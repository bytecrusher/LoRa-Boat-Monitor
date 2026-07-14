#!/usr/bin/env python3

from __future__ import annotations

import io
import argparse
import re
import tarfile
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DATA_DIR = PROJECT_ROOT / "data"
CONFIGURATION_H = PROJECT_ROOT / "src" / "Configuration.h"
OUTPUT_PATH = PROJECT_ROOT / "webui-package.tar"


def read_version() -> str:
    content = CONFIGURATION_H.read_text(encoding="utf-8")
    match = re.search(r'fversion\[\d+\]\s*=\s*"([^"]+)"', content)
    if not match:
        raise RuntimeError("Could not read firmware version from src/Configuration.h")
    return match.group(1)


def read_channel() -> str:
    content = CONFIGURATION_H.read_text(encoding="utf-8")
    match = re.search(r'FIRMWARE_RELEASE_CHANNEL\s*=\s*"([^"]+)"', content)
    return match.group(1).lower() if match else "beta"


def build_tar_package(channel: str) -> None:
    version = read_version()
    files = sorted(
        path
        for path in DATA_DIR.iterdir()
        if path.is_file() and path.name not in {".DS_Store", "webfiles-version.txt"}
    )

    with tarfile.open(OUTPUT_PATH, "w", format=tarfile.USTAR_FORMAT) as archive:
        for file_path in files:
            data = file_path.read_bytes()
            info = tarfile.TarInfo(name=file_path.name)
            info.size = len(data)
            archive.addfile(info, io.BytesIO(data))

        version_bytes = (version + "|" + channel + "\n").encode("utf-8")
        version_info = tarfile.TarInfo(name="webfiles-version.txt")
        version_info.size = len(version_bytes)
        archive.addfile(version_info, io.BytesIO(version_bytes))

    print(f"Created {OUTPUT_PATH} for {version}|{channel}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Build the LittleFS web interface package.")
    parser.add_argument("--channel", choices=("beta", "stable"), default=read_channel())
    args = parser.parse_args()
    configured_channel = read_channel()
    if args.channel != configured_channel:
        raise SystemExit(
            f"Requested {args.channel} package, but firmware is configured for {configured_channel}. "
            "Set FIRMWARE_RELEASE_CHANNEL consistently before building."
        )
    build_tar_package(args.channel)
