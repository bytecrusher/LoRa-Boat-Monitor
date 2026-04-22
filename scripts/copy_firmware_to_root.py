from pathlib import Path
from shutil import copy2

Import("env")


def copy_firmware_to_project_root(source, target, env):
    build_dir = Path(env.subst("$BUILD_DIR"))
    project_dir = Path(env.subst("$PROJECT_DIR"))
    firmware_bin = build_dir / "firmware.bin"
    target_bin = project_dir / "firmware.bin"

    if firmware_bin.exists():
        for old_firmware in project_dir.glob("firmware *.bin"):
            old_firmware.unlink(missing_ok=True)
        copy2(firmware_bin, target_bin)
        print(f"Copied {firmware_bin} -> {target_bin}")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", copy_firmware_to_project_root)
