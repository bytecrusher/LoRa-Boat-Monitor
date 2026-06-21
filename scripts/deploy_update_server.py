#!/usr/bin/env python3
"""Upload firmware and web files to the configured update server via FTP."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import os
import posixpath
import re
import ssl
import subprocess
import sys
import tempfile
from ftplib import FTP, FTP_TLS, error_perm
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DATA_DIR = PROJECT_ROOT / "data"
FIRMWARE_BIN = PROJECT_ROOT / "firmware.bin"
WEBUI_PACKAGE = PROJECT_ROOT / "webui-package.tar"
CONFIGURATION_H = PROJECT_ROOT / "src" / "Configuration.h"
DEFAULT_ENV_FILE = PROJECT_ROOT / ".env"

EXCLUDED_DATA_FILES = {
    ".DS_Store",
}

STABLE_MARKERS = ("latestVersion.txt", "latestStableVersion.txt", "ActualVersion.txt")
BETA_MARKERS = ("latestBetaVersion.txt",)
MANIFEST_FILE = "firmware-manifest.json"
DEFAULT_MDS_OTA_SSH_TARGET = "hosting157867@derguntmar.de:httpdocs/mds-git.derguntmar.de/var/ota/bin"
DEFAULT_MDS_PUBLIC_OTA_SSH_TARGET = "hosting157867@derguntmar.de:httpdocs/mds-git.derguntmar.de/public/ota/bin"


def load_project_env() -> None:
    if not DEFAULT_ENV_FILE.exists():
        return

    for raw_line in DEFAULT_ENV_FILE.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue

        key, value = line.split("=", 1)
        key = key.strip()
        if not key or key in os.environ:
            continue

        os.environ[key] = value.strip().strip('"').strip("'")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Deploy LoRa Boat Monitor update files via FTP.")
    parser.add_argument("--host", default=os.environ.get("UPDATE_SERVER_FTP_HOST", ""), help="FTP host name")
    parser.add_argument("--user", default=os.environ.get("UPDATE_SERVER_FTP_USER", ""), help="FTP user name")
    parser.add_argument("--password", default=os.environ.get("UPDATE_SERVER_FTP_PASSWORD", ""), help="FTP password")
    parser.add_argument(
        "--remote-dir",
        default=os.environ.get("UPDATE_SERVER_FTP_DIR", ""),
        help="Remote directory, e.g. /files_for_esp_webserver",
    )
    parser.add_argument(
        "--channel",
        choices=("stable", "beta"),
        default="stable",
        help="Which OTA marker files to update.",
    )
    parser.add_argument(
        "--version",
        default="",
        help="Override firmware version. Defaults to fversion from src/Configuration.h",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=int(os.environ.get("UPDATE_SERVER_FTP_PORT", "21")),
        help="FTP port.",
    )
    parser.add_argument(
        "--tls",
        action="store_true",
        default=os.environ.get("UPDATE_SERVER_FTP_TLS", "true").lower() not in {"0", "false", "no"},
        help="Use explicit FTPS (FTP over TLS).",
    )
    parser.add_argument(
        "--allow-insecure-ftp",
        action="store_true",
        default=os.environ.get("UPDATE_SERVER_ALLOW_INSECURE_FTP", "").lower() in {"1", "true", "yes"},
        help="Allow plain FTP. Not recommended.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print actions without uploading files.",
    )
    parser.add_argument(
        "--mds-ota-dir",
        default=os.environ.get("MDS_OTA_FTP_DIR", ""),
        help="Optional FTP directory for MDS OTA firmware files, e.g. var/ota/bin.",
    )
    parser.add_argument(
        "--mds-ota-target",
        default=os.environ.get("MDS_OTA_SSH_TARGET", DEFAULT_MDS_OTA_SSH_TARGET),
        help="Optional SSH/SCP target for MDS OTA firmware files, e.g. user@host:path/to/var/ota/bin.",
    )
    parser.add_argument(
        "--mds-public-target",
        default=os.environ.get("MDS_PUBLIC_OTA_SSH_TARGET", DEFAULT_MDS_PUBLIC_OTA_SSH_TARGET),
        help="Optional SSH/SCP target for public MDS OTA web files and metadata, e.g. user@host:path/to/public/ota/bin.",
    )
    parser.add_argument(
        "--skip-mds-ota",
        action="store_true",
        default=os.environ.get("SKIP_MDS_OTA_UPLOAD", "").lower() in {"1", "true", "yes"},
        help="Skip the additional MDS OTA firmware upload.",
    )
    return parser.parse_args()


def read_version() -> str:
    content = CONFIGURATION_H.read_text(encoding="utf-8")
    match = re.search(r'fversion\[\d+\]\s*=\s*"([^"]+)"', content)
    if not match:
        raise RuntimeError("Could not read firmware version from src/Configuration.h")
    return match.group(1)


def collect_web_files() -> list[Path]:
    return sorted(
        path
        for path in DATA_DIR.rglob("*")
        if path.is_file() and path.name not in EXCLUDED_DATA_FILES
    )


def has_ftp_config(args: argparse.Namespace) -> bool:
    return bool(args.host and args.user and args.password and args.remote_dir)


def ensure_ftp_config(args: argparse.Namespace) -> None:
    if not args.host:
        raise ValueError("Missing --host or UPDATE_SERVER_FTP_HOST")
    if not args.user:
        raise ValueError("Missing --user or UPDATE_SERVER_FTP_USER")
    if not args.password:
        raise ValueError("Missing --password or UPDATE_SERVER_FTP_PASSWORD")
    if not args.remote_dir:
        raise ValueError("Missing --remote-dir or UPDATE_SERVER_FTP_DIR")
    if not args.tls and not args.allow_insecure_ftp:
        raise ValueError("Plain FTP is disabled. Use FTPS or pass --allow-insecure-ftp explicitly.")


def connect_ftp(args: argparse.Namespace) -> FTP:
    ftp: FTP
    if args.tls:
        ftp = FTP_TLS(context=ssl.create_default_context())
    else:
        ftp = FTP()

    ftp.connect(args.host, args.port, timeout=20)
    ftp.login(args.user, args.password)

    if args.tls and isinstance(ftp, FTP_TLS):
        ftp.prot_p()

    return ftp


def split_remote_path(remote_path: str) -> list[str]:
    cleaned = remote_path.replace("\\", "/").strip("/")
    if not cleaned:
        return []
    return [part for part in cleaned.split("/") if part]


def ensure_remote_dir(ftp: FTP, remote_path: str) -> None:
    ftp.cwd("/")
    for part in split_remote_path(remote_path):
        try:
            ftp.cwd(part)
        except error_perm as exc:
            message = str(exc)
            if not message.startswith("550"):
                raise
            ftp.mkd(part)
            ftp.cwd(part)


def ftp_cwd(ftp: FTP, remote_path: str) -> None:
    ftp.cwd("/")
    if remote_path in ("", "/"):
        return
    for part in split_remote_path(remote_path):
        ftp.cwd(part)


def upload_file(ftp: FTP, local_path: Path, remote_path: str, dry_run: bool) -> None:
    print(f"+ upload {local_path} -> {remote_path}")
    if dry_run:
        return

    remote_dir = posixpath.dirname(remote_path)
    ensure_remote_dir(ftp, remote_dir)
    ftp_cwd(ftp, remote_dir)
    with local_path.open("rb") as source:
        ftp.storbinary(f"STOR {posixpath.basename(remote_path)}", source)


def upload_text(ftp: FTP, text: str, remote_path: str, dry_run: bool) -> None:
    print(f"+ write marker {remote_path} = {text.strip()}")
    if dry_run:
        return

    remote_dir = posixpath.dirname(remote_path)
    ensure_remote_dir(ftp, remote_dir)
    ftp_cwd(ftp, remote_dir)
    payload = io.BytesIO(text.encode("utf-8"))
    ftp.storbinary(f"STOR {posixpath.basename(remote_path)}", payload)


def download_text(ftp: FTP, remote_path: str) -> str:
    remote_dir = posixpath.dirname(remote_path)
    remote_name = posixpath.basename(remote_path)
    ensure_remote_dir(ftp, remote_dir)
    ftp_cwd(ftp, remote_dir)

    payload = io.BytesIO()
    ftp.retrbinary(f"RETR {remote_name}", payload.write)
    return payload.getvalue().decode("utf-8")


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def build_web_file_hashes(release_files: list[tuple[Path, str]], version: str) -> dict[str, str]:
    hashes: dict[str, str] = {}
    version_prefix = f"{version}/"
    for local_path, remote_path in release_files:
        if remote_path == posixpath.join(version, "firmware.bin"):
            continue
        if remote_path.startswith(version_prefix):
            hashes[remote_path[len(version_prefix):]] = file_sha256(local_path)
    return hashes


def build_manifest(
    existing_manifest: str,
    channel: str,
    version: str,
    firmware_sha256: str,
    web_file_hashes: dict[str, str],
) -> str:
    try:
        manifest = json.loads(existing_manifest) if existing_manifest.strip() else {}
    except json.JSONDecodeError:
        manifest = {}

    manifest[channel] = {
        "version": version,
        "firmware": f"{version}/firmware.bin",
        "sha256": firmware_sha256,
        "webFileHashes": web_file_hashes,
        "webFiles": f"{version}/",
    }

    return json.dumps(manifest, indent=2, sort_keys=True) + "\n"


def upload_mds_ota_files(
    ftp: FTP,
    remote_root: str,
    version: str,
    firmware_sha256: str,
    dry_run: bool,
) -> None:
    if not remote_root:
        print("MDS OTA upload skipped: MDS_OTA_FTP_DIR is not configured.")
        return

    cleaned_root = remote_root.strip("/")
    print(f"Preparing MDS OTA firmware files in {cleaned_root}")
    upload_file(
        ftp,
        FIRMWARE_BIN,
        posixpath.join(cleaned_root, "firmware.bin"),
        dry_run,
    )
    upload_file(
        ftp,
        FIRMWARE_BIN,
        posixpath.join(cleaned_root, f"{version}.bin"),
        dry_run,
    )
    upload_text(
        ftp,
        version + "\n",
        posixpath.join(cleaned_root, "firmware.version"),
        dry_run,
    )
    upload_text(
        ftp,
        firmware_sha256 + "\n",
        posixpath.join(cleaned_root, "firmware.sha256"),
        dry_run,
    )


def split_scp_target(target: str) -> tuple[str, str]:
    if ":" not in target:
        raise ValueError("MDS OTA SSH target must use the form user@host:path")
    host, remote_dir = target.split(":", 1)
    if not host or not remote_dir:
        raise ValueError("MDS OTA SSH target must include host and remote path")
    return host, remote_dir.rstrip("/")


def run_command(command: list[str], dry_run: bool) -> None:
    print("+ " + " ".join(command))
    if dry_run:
        return
    subprocess.run(command, check=True)


def download_text_ssh(target: str, remote_path: str) -> str:
    host, remote_dir = split_scp_target(target)
    full_path = f"{remote_dir.rstrip('/')}/{remote_path.lstrip('/')}"
    result = subprocess.run(
        ["ssh", host, "cat", full_path],
        check=True,
        capture_output=True,
        text=True,
    )
    return result.stdout


def upload_mds_ota_files_ssh(
    target: str,
    public_target: str,
    version: str,
    firmware_sha256: str,
    manifest_text: str,
    dry_run: bool,
) -> None:
    if not target:
        print("MDS OTA SSH upload skipped: MDS_OTA_SSH_TARGET is not configured.")
        return

    host, remote_dir = split_scp_target(target)
    public_host, public_remote_dir = split_scp_target(public_target or target)
    print(f"Preparing MDS OTA firmware files via SSH in {host}:{remote_dir}")
    print(f"Preparing public MDS OTA web files via SSH in {public_host}:{public_remote_dir}")
    run_command(["ssh", host, "mkdir", "-p", remote_dir], dry_run)
    run_command(["scp", str(FIRMWARE_BIN), f"{host}:{remote_dir}/firmware.bin"], dry_run)
    run_command(["scp", str(FIRMWARE_BIN), f"{host}:{remote_dir}/{version}.bin"], dry_run)

    with tempfile.TemporaryDirectory(prefix="loraboatmonitor-ota-") as temp_dir_name:
        temp_dir = Path(temp_dir_name)
        version_file = temp_dir / "firmware.version"
        sha256_file = temp_dir / "firmware.sha256"
        manifest_file = temp_dir / MANIFEST_FILE
        version_file.write_text(version + "\n", encoding="utf-8")
        sha256_file.write_text(firmware_sha256 + "\n", encoding="utf-8")
        manifest_file.write_text(manifest_text, encoding="utf-8")
        run_command(["scp", str(version_file), f"{host}:{remote_dir}/firmware.version"], dry_run)
        run_command(["scp", str(sha256_file), f"{host}:{remote_dir}/firmware.sha256"], dry_run)
        run_command(["ssh", public_host, "mkdir", "-p", public_remote_dir], dry_run)
        run_command(["scp", str(version_file), f"{public_host}:{public_remote_dir}/firmware.version"], dry_run)
        run_command(["scp", str(sha256_file), f"{public_host}:{public_remote_dir}/firmware.sha256"], dry_run)
        run_command(["scp", str(FIRMWARE_BIN), f"{public_host}:{public_remote_dir}/firmware.bin"], dry_run)
        run_command(["scp", str(FIRMWARE_BIN), f"{public_host}:{public_remote_dir}/{version}.bin"], dry_run)
        run_command(["ssh", public_host, "mkdir", "-p", f"{public_remote_dir}/web/{version}"], dry_run)
        run_command(["scp", str(manifest_file), f"{public_host}:{public_remote_dir}/web/{MANIFEST_FILE}"], dry_run)

        for web_file in collect_web_files():
            remote_web_path = f"{public_host}:{public_remote_dir}/web/{version}/{web_file.relative_to(DATA_DIR).as_posix()}"
            run_command(["scp", str(web_file), remote_web_path], dry_run)

        if WEBUI_PACKAGE.exists():
            run_command(["scp", str(WEBUI_PACKAGE), f"{public_host}:{public_remote_dir}/web/{version}/webui-package.tar"], dry_run)
            run_command(["scp", str(WEBUI_PACKAGE), f"{public_host}:{public_remote_dir}/web/webui-package.tar"], dry_run)
        else:
            print(f"Warning: {WEBUI_PACKAGE} does not exist. Skipping MDS web package upload.")


def collect_release_files(version: str) -> list[tuple[Path, str]]:
    if not FIRMWARE_BIN.exists():
        raise FileNotFoundError(f"Missing firmware image: {FIRMWARE_BIN}")

    release_files: list[tuple[Path, str]] = [(FIRMWARE_BIN, posixpath.join(version, "firmware.bin"))]

    for web_file in collect_web_files():
        relative_path = web_file.relative_to(DATA_DIR).as_posix()
        release_files.append((web_file, posixpath.join(version, relative_path)))

    return release_files


def main() -> int:
    load_project_env()
    args = parse_args()
    version = args.version or read_version()
    firmware_sha256 = file_sha256(FIRMWARE_BIN)
    marker_files = STABLE_MARKERS if args.channel == "stable" else BETA_MARKERS
    remote_root = args.remote_dir.strip("/")
    release_files = collect_release_files(version)
    web_file_hashes = build_web_file_hashes(release_files, version)
    manifest_text = build_manifest("", args.channel, version, firmware_sha256, web_file_hashes)

    print(f"Preparing release {version} for channel '{args.channel}'")
    if has_ftp_config(args):
        print(f"FTP target: {args.host}:{args.port}/{remote_root}")
        print(f"Files to upload: {len(release_files)}")
    else:
        print("Legacy FTP target not configured. Skipping legacy web root deployment.")

    ftp = None
    if has_ftp_config(args):
        ensure_ftp_config(args)
    if not args.dry_run and has_ftp_config(args):
        ftp = connect_ftp(args)

    try:
        if has_ftp_config(args):
            for local_file, remote_relative_path in release_files:
                upload_file(
                    ftp,  # type: ignore[arg-type]
                    local_file,
                    posixpath.join(remote_root, remote_relative_path),
                    args.dry_run,
                )

            for marker in marker_files:
                upload_text(
                    ftp,  # type: ignore[arg-type]
                    version + "\n",
                    posixpath.join(remote_root, marker),
                    args.dry_run,
                )

            manifest_remote_path = posixpath.join(remote_root, MANIFEST_FILE)
            existing_manifest = ""
            if not args.dry_run and ftp is not None:
                try:
                    existing_manifest = download_text(ftp, manifest_remote_path)
                except error_perm:
                    existing_manifest = ""

            manifest_text = build_manifest(existing_manifest, args.channel, version, firmware_sha256, web_file_hashes)
            upload_text(
                ftp,  # type: ignore[arg-type]
                manifest_text,
                manifest_remote_path,
                args.dry_run,
            )

        if args.skip_mds_ota:
            print("MDS OTA upload skipped by --skip-mds-ota.")
        elif args.mds_ota_dir:
            upload_mds_ota_files(
                ftp,  # type: ignore[arg-type]
                args.mds_ota_dir,
                version,
                firmware_sha256,
                args.dry_run,
            )
        else:
            if not has_ftp_config(args) and not args.dry_run:
                try:
                    existing_mds_manifest = download_text_ssh(args.mds_ota_target, f"web/{MANIFEST_FILE}")
                except Exception:
                    existing_mds_manifest = ""
                manifest_text = build_manifest(existing_mds_manifest, args.channel, version, firmware_sha256, web_file_hashes)
            upload_mds_ota_files_ssh(
                args.mds_ota_target,
                args.mds_public_target,
                version,
                firmware_sha256,
                manifest_text,
                args.dry_run,
            )
    finally:
        if ftp is not None:
            ftp.quit()

    print(f"Release {version} prepared for channel '{args.channel}'.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:  # noqa: BLE001
        print(f"Error: {exc}", file=sys.stderr)
        raise SystemExit(1)
