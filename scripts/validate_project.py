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


def main() -> int:
    errors: list[str] = []
    version = firmware_version(errors)
    validate_data_marker(errors, version)
    validate_web_bundle(errors, version)
    validate_frontend_contract(errors)
    validate_route_registry(errors)
    validate_runtime_state_boundaries(errors)

    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1

    print(f"Project validation passed for {version}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
