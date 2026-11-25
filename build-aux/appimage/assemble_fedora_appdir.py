#!/usr/bin/env -S uv run --script
"""Assemble an x86-64 AppDir from a Fedora 44 native CMake install."""

import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

from assemble_appdir import (
    GLIBC_EXCLUDE,
    HERE,
    bundle_qt_plugins,
    bundle_runtime,
    desktop_integration,
    gui_selection,
    record_modules,
    system_libdir,
    walk_closure,
)
from set_runpaths import set_runpaths


def main(appdir: Path) -> None:
    usr = appdir / "usr"
    libdir = usr / "lib"
    if not (usr / "bin" / "simulcast-studio").is_file():
        raise SystemExit("Stage a Fedora native CMake install into a fresh AppDir first.")
    (usr / "lib64").rename(libdir)
    (usr / "lib64").symlink_to("lib")
    gui_name, app_id, data_root, module_root = gui_selection(appdir, libdir)

    syslib = system_libdir()
    bundle_qt_plugins(appdir, libdir, syslib, "")
    bundle_runtime(libdir, syslib, 0)
    (libdir / "gio" / "modules").mkdir(parents=True, exist_ok=True)
    pactl = shutil.which("pactl")
    if not pactl:
        raise SystemExit("Install pulseaudio-utils on the build host.")
    shutil.copy2(pactl, usr / "bin" / "pactl")
    record_modules(data_root, module_root)

    # Fedora's native FFmpeg and Mesa use the host VA-API and graphics drivers.
    # Keep glibc and libstdc++ on the host too. This image targets Fedora 44.
    old_path = os.environ.get("LD_LIBRARY_PATH")
    os.environ["LD_LIBRARY_PATH"] = str(libdir)
    try:
        walk_closure(appdir, libdir, re.compile(GLIBC_EXCLUDE))
    finally:
        if old_path is None:
            del os.environ["LD_LIBRARY_PATH"]
        else:
            os.environ["LD_LIBRARY_PATH"] = old_path

    set_runpaths(appdir, "")
    desktop_integration(appdir, HERE, app_id, gui_name, "", 0)
    desktop = appdir / f"{app_id}.desktop"
    desktop.write_text(re.sub(r"(?m)^Name=.*$", "Name=Simulcast Studio", desktop.read_text()))
    subprocess.run([appdir / "AppRun", "--version"], check=True)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: assemble_fedora_appdir.py APPDIR")
    main(Path(sys.argv[1]).resolve())
