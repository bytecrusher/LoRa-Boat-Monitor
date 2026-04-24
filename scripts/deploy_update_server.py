#!/usr/bin/env python3
"""Upload firmware and web files to the configured update server via FTP."""

from __future__ import annotations

import argparse
import io
import os
import posixpath
import re
import sys
from ftplib import FTP, FTP_TLS, error_perm
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DATA_DIR = PROJECT_ROOT / "data"
FIRMWARE_BIN = PROJECT_ROOT / "firmware.bin"
CONFIGURATION_H = PROJECT_ROOT / "src" / "Configuration.h"
DEFAULT_ENV_FILE = PROJECT_ROOT / ".env"

EXCLUDED_DATA_FILES = {
    ".DS_Store",
}

STABLE_MARKERS = ("latestVersion.txt", "latestStableVersion.txt", "ActualVersion.txt")
BETA_MARKERS = ("latestBetaVersion.txt",)


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
        default=os.environ.get("UPDATE_SERVER_FTP_TLS", "").lower() in {"1", "true", "yes"},
        help="Use explicit FTPS (FTP over TLS).",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print actions without uploading files.",
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


def ensure_ftp_config(args: argparse.Namespace) -> None:
    if not args.host:
        raise ValueError("Missing --host or UPDATE_SERVER_FTP_HOST")
    if not args.user:
        raise ValueError("Missing --user or UPDATE_SERVER_FTP_USER")
    if not args.password:
        raise ValueError("Missing --password or UPDATE_SERVER_FTP_PASSWORD")
    if not args.remote_dir:
        raise ValueError("Missing --remote-dir or UPDATE_SERVER_FTP_DIR")


def connect_ftp(args: argparse.Namespace) -> FTP:
    ftp: FTP
    if args.tls:
        ftp = FTP_TLS()
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
    ensure_ftp_config(args)
    version = args.version or read_version()
    marker_files = STABLE_MARKERS if args.channel == "stable" else BETA_MARKERS
    remote_root = args.remote_dir.strip("/")
    release_files = collect_release_files(version)

    print(f"Preparing release {version} for channel '{args.channel}'")
    print(f"FTP target: {args.host}:{args.port}/{remote_root}")
    print(f"Files to upload: {len(release_files)}")

    ftp = None
    if not args.dry_run:
        ftp = connect_ftp(args)

    try:
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
