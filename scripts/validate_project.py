#!/usr/bin/env python3

from __future__ import annotations

import re
import sys
import tarfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "data"
CONFIG_SOURCE = ROOT / "src" / "Configuration.h"
WEB_BUNDLE = ROOT / "webui-package.tar"
PLATFORMIO_INI = ROOT / "platformio.ini"


def fail(errors: list[str], message: str) -> None:
    errors.append(message)


def firmware_version(errors: list[str]) -> str:
    match = re.search(r'fversion\[\d+\]\s*=\s*"([^"]+)"', CONFIG_SOURCE.read_text(encoding="utf-8"))
    if not match:
        fail(errors, "Could not read firmware version from Configuration.h")
        return ""
    version = match.group(1)
    if not re.fullmatch(r"V\d+\.\d+[a-z]+", version):
        fail(errors, f"Firmware version has an unexpected format: {version}")
    return version


def validate_web_bundle(errors: list[str], version: str) -> None:
    if not WEB_BUNDLE.exists():
        fail(errors, "webui-package.tar is missing")
        return

    expected = {path.name for path in DATA.iterdir() if path.is_file() and path.name != ".DS_Store"}
    with tarfile.open(WEB_BUNDLE, "r") as archive:
        members = [member for member in archive.getmembers() if member.isfile()]
        names = [member.name.removeprefix("./") for member in members]
        if len(names) != len(set(names)):
            fail(errors, "webui-package.tar contains duplicate file names")
        actual = set(names)
        missing = sorted(expected - actual)
        unexpected = sorted(actual - expected)
        if missing:
            fail(errors, "Web bundle is missing: " + ", ".join(missing))
        if unexpected:
            fail(errors, "Web bundle contains unexpected files: " + ", ".join(unexpected))

        for name in sorted(expected & actual):
            if name == "webfiles-version.txt":
                continue
            archived = archive.extractfile(name)
            archived_bytes = archived.read() if archived else b""
            source_bytes = (DATA / name).read_bytes()
            if archived_bytes != source_bytes:
                fail(errors, f"Web bundle contains stale content for {name}")

        try:
            marker = archive.extractfile("webfiles-version.txt")
            marker_value = marker.read().decode("utf-8").strip() if marker else ""
        except KeyError:
            marker_value = ""
        marker_version = marker_value.split("|", 1)[0]
        if marker_version != version:
            fail(errors, f"Web bundle version is {marker_value or 'missing'}, expected {version}")


def validate_data_marker(errors: list[str], version: str) -> None:
    marker = (DATA / "webfiles-version.txt").read_text(encoding="utf-8").strip()
    marker_version = marker.split("|", 1)[0]
    if marker_version != version:
        fail(errors, f"data/webfiles-version.txt contains {marker_version}, expected {version}")


def validate_frontend_contract(errors: list[str]) -> None:
    server_text = "\n".join(path.read_text(encoding="utf-8") for path in (ROOT / "src").glob("*.cpp"))
    routes = set(re.findall(r'(?:httpServer\.|server->)on\("([^"]+)"', server_text))
    routes.update({"/", "/logged-out"})
    data_paths = {"/" + path.name for path in DATA.iterdir() if path.is_file()}
    known_paths = routes | data_paths

    referenced_paths: set[str] = set()
    for source in list(DATA.glob("*.js")) + list(DATA.glob("*.html")):
        text = source.read_text(encoding="utf-8", errors="replace")
        for match in re.findall(r'["\'](/[^"\'\s<>]*)["\']', text):
            path = match.split("?", 1)[0]
            if path and not any(token in path for token in ("${", "%", "*")):
                referenced_paths.add(path)

    ignored = {"/logged-out", "/"}
    missing = sorted(path for path in referenced_paths if path not in known_paths and path not in ignored)
    if missing:
        fail(errors, "Frontend references unregistered paths: " + ", ".join(missing))

    settings_html = (DATA / "settings.html").read_text(encoding="utf-8")
    settings_handler = (ROOT / "src" / "func_webServerHandler.cpp").read_text(encoding="utf-8")
    sensor_name_fields = (
        "MdsSensorNameBattery",
        "MdsSensorNameTanks",
        "MdsSensorNameStatus",
        "MdsSensorNameTemperature",
        "MdsSensorNameGps",
        "MdsSensorNameEnv",
        "MdsSensorNameDewpoint",
        "MdsSensorNameVedirect",
    )
    for field in sensor_name_fields:
        if settings_html.count(f"name='{field}'") != 1:
            fail(errors, f"Settings must contain exactly one editable field for {field}")
        if f'var == "{field}"' not in settings_handler or f'fieldName == "{field}"' not in settings_handler:
            fail(errors, f"Sensor name field is not fully handled by the firmware: {field}")


def validate_route_registry(errors: list[str]) -> None:
    registrations: list[tuple[str, str]] = []
    pattern = re.compile(r'(?:httpServer\.|server->)on\("([^"]+)",\s*(HTTP_[A-Z]+)')
    for source in (ROOT / "src").glob("*.cpp"):
        text = source.read_text(encoding="utf-8", errors="replace")
        registrations.extend((method, path) for path, method in pattern.findall(text))

    duplicates = sorted({entry for entry in registrations if registrations.count(entry) > 1})
    if duplicates:
        formatted = ", ".join(f"{method} {path}" for method, path in duplicates)
        fail(errors, "Duplicate HTTP route registrations: " + formatted)

    required = {
        ("HTTP_GET", "/health"),
        ("HTTP_GET", "/index.html"),
        ("HTTP_GET", "/settings.html"),
        ("HTTP_POST", "/savesettings"),
        ("HTTP_GET", "/firmware.html"),
        ("HTTP_POST", "/startRemoteUpdate"),
        ("HTTP_POST", "/doUpdate"),
        ("HTTP_POST", "/updatefiles"),
        ("HTTP_GET", "/otaprogress"),
        ("HTTP_POST", "/testMdsUpload"),
        ("HTTP_GET", "/testMdsUploadStatus"),
        ("HTTP_GET", "/lora/status"),
        ("HTTP_POST", "/lora/send"),
    }
    missing = sorted(required - set(registrations))
    if missing:
        formatted = ", ".join(f"{method} {path}" for method, path in missing)
        fail(errors, "Required HTTP routes are missing: " + formatted)


def validate_runtime_state_boundaries(errors: list[str]) -> None:
    sources = list((ROOT / "src").glob("*.cpp")) + list((ROOT / "include").glob("*.h"))
    combined = "\n".join(path.read_text(encoding="utf-8", errors="replace") for path in sources)
    legacy_globals = {
        "remoteOtaPending",
        "remoteOtaInProgress",
        "remoteOtaUrl",
        "remoteOtaSha256",
        "webFilesDownloadStatusMessage",
        "webFilesDownloadCurrentName",
    }
    remaining = sorted(name for name in legacy_globals if re.search(rf"\b{name}\b", combined))
    if remaining:
        fail(errors, "Unprotected legacy update state remains: " + ", ".join(remaining))

    storage_source = (ROOT / "src" / "func_myFunctions.cpp").read_text(encoding="utf-8")
    save_start = storage_source.find("saveEEPROMConfig(")
    if save_start < 0:
        fail(errors, "saveEEPROMConfig implementation is missing")
        return
    save_end = storage_source.find("configData loadEEPROMConfig", save_start)
    save_body = storage_source[save_start:save_end]
    if "delay(" in save_body:
        fail(errors, "saveEEPROMConfig must not contain a fixed blocking delay")
    if "ConfigStorageGuard" not in save_body or "EEPROM.commit()" not in save_body:
        fail(errors, "saveEEPROMConfig must lock storage and commit explicitly")


def validate_reproducible_dependencies(errors: list[str]) -> None:
    content = PLATFORMIO_INI.read_text(encoding="utf-8")
    platform_match = re.search(r"^platform\s*=\s*(.+)$", content, re.MULTILINE)
    if not platform_match or "@" not in platform_match.group(1):
        fail(errors, "PlatformIO platform must be pinned to an explicit version")

    lib_match = re.search(r"^lib_deps\s*=\s*\n((?:[ \t]+.*(?:\n|$))*)", content, re.MULTILINE)
    if not lib_match:
        fail(errors, "PlatformIO lib_deps section is missing")
        return
    for line in lib_match.group(1).splitlines():
        dependency = line.strip()
        if not dependency or dependency.startswith((";", "[")):
            continue
        if dependency.startswith(("http://", "https://", "git@")):
            if not re.search(r"[#@][0-9a-f]{7,40}$", dependency):
                fail(errors, f"Git dependency is not pinned to a commit: {dependency}")
        elif "@" not in dependency or "^" in dependency or "~" in dependency:
            fail(errors, f"Library dependency is not exactly pinned: {dependency}")


def validate_lora_runtime_safety(errors: list[str]) -> None:
    lora_source = (ROOT / "src" / "LoRa.h").read_text(encoding="utf-8")
    main_source = (ROOT / "src" / "LoRa_boat_monitor_abp.cpp").read_text(encoding="utf-8")
    webclient_source = (ROOT / "src" / "func_webclient.cpp").read_text(encoding="utf-8")

    if re.search(r"\bLMIC\s*=\s*RTC_LMIC\b", lora_source):
        fail(errors, "LMIC transient scheduler state must not be restored from RTC")

    queue_lines = [line for line in lora_source.splitlines() if "LMIC_setTxData2(" in line]
    unchecked_queue_lines = [line.strip() for line in queue_lines if "=" not in line.split("LMIC_setTxData2(", 1)[0]]
    if unchecked_queue_lines:
        fail(errors, "Every LMIC_setTxData2 call must capture and validate its return value")

    required_status_fields = (
        'response["txQueued"]',
        'response["queueResult"]',
        'response["opmode"]',
        'response["queuedAgeMillis"]',
        'response["confirmedTxAttempts"]',
        'response["rx1DelaySeconds"]',
    )
    missing_fields = [field for field in required_status_fields if field not in main_source]
    if missing_fields:
        fail(errors, "LoRa diagnostics are missing: " + ", ".join(missing_fields))

    if "manualUplink || currentLoraPacketUsesWifiFallback || currentLoraPacketIncludedDeviceEvent" not in lora_source:
        fail(errors, "WakeupLog LoRa packets must use confirmed uplinks")
    if "currentLoraPacketIncludedDeviceEvent\n        ? acknowledged" not in lora_source:
        fail(errors, "WakeupLog events must remain queued until a LoRa network ACK is received")
    if "actconf.MdsSensorIdStatus <= 0" in webclient_source:
        fail(errors, "WakeupLog WiFi delivery must not depend on a legacy sensor ID")
    if "insertedSensorRows <= 0 || skippedSensorRows > 0" not in webclient_source:
        fail(errors, "MDS delivery must reject responses that did not store every sensor row")

    state0_start = main_source.find("void state0(){")
    state0_end = main_source.find("// S1 = Battery On", state0_start)
    state0_source = main_source[state0_start:state0_end]
    if state0_start < 0 or state0_end < 0:
        fail(errors, "Could not inspect the standby state implementation")
    else:
        guarded_loop = re.search(
            r"if\s*\(standbyUseLoraThisWake\)\s*\{\s*lora_loop\(\);\s*\}",
            state0_source,
        )
        if not guarded_loop:
            fail(errors, "Standby must not run the LMIC scheduler before LoRa initialization")

        guarded_cancel = re.search(
            r"if\s*\(standbyUseLoraThisWake\)\s*\{\s*LMIC_clrTxData\(\);\s*os_clearCallback\(&sendjob\);",
            state0_source,
        )
        if not guarded_cancel:
            fail(errors, "Standby must not cancel LMIC work before LoRa initialization")


def main() -> int:
    errors: list[str] = []
    version = firmware_version(errors)
    validate_data_marker(errors, version)
    validate_web_bundle(errors, version)
    validate_frontend_contract(errors)
    validate_route_registry(errors)
    validate_runtime_state_boundaries(errors)
    validate_reproducible_dependencies(errors)
    validate_lora_runtime_safety(errors)

    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1

    print(f"Project validation passed for {version}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
