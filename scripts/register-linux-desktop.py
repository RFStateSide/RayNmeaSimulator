# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Register an existing development or installed binary for the current user.

No sudo, application launch, desktop preference changes or icon regeneration.
Existing differing files are backed up; identical registration is a no-op.
"""
import argparse
from datetime import datetime, timezone
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
DESKTOP_ID = "com.rfstateside.RayNmeaSimulator.desktop"
ICON_NAME = "RayNmeaSimulator_1024.png"


def data_home():
    path = Path(os.environ.get("XDG_DATA_HOME") or Path.home() / ".local/share")
    if not path.is_absolute():
        raise ValueError("XDG_DATA_HOME must be an absolute path")
    return path


def desktop_entry(executable, icon=None):
    path = str(executable)
    # GIO treats percent field codes in executable names inconsistently, even
    # when escaped. Fail explicitly rather than writing an unusable launcher.
    if any(ord(c) < 32 for c in path) or any(c in path for c in "=%"):
        raise ValueError("Executable path contains an unsupported desktop-entry character")
    # Desktop Entry Exec quoting and string escaping are two separate layers.
    quoted = "".join("\\" + c if c in '\\"`$' else c for c in path)
    quoted = '"' + quoted.replace("\\", "\\\\") + '"'
    template = (ROOT / "packaging" / DESKTOP_ID).read_text(encoding="utf-8")
    return "\n".join(
        "Exec=" + quoted if line.startswith("Exec=") else
        "TryExec=" + path.replace("\\", "\\\\") if line.startswith("TryExec=") else
        "Icon=" + str(icon).replace("\\", "\\\\") if icon is not None and line.startswith("Icon=") else line
        for line in template.splitlines()
    ) + "\n"


def write_preserving(path, content):
    if path.is_symlink() or (path.exists() and not path.is_file()):
        raise ValueError("Refusing to replace a symlink or non-file: " + str(path))
    if path.exists() and path.read_bytes() == content:
        return None
    path.parent.mkdir(parents=True, exist_ok=True)
    backup = None
    if path.exists():
        stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
        backup = path.with_name(path.name + "." + stamp + ".bak")
        shutil.copy2(path, backup)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, prefix="." + path.name,
                                         suffix=".tmp", delete=False) as file:
            temporary = Path(file.name)
            file.write(content)
        temporary.chmod(0o644)
        os.replace(temporary, path)
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()
    return backup


def register(executable, destination):
    executable = executable.resolve(strict=True)
    if not executable.is_file() or not os.access(executable, os.X_OK):
        raise ValueError("Select an existing executable application binary")
    if not destination.is_absolute():
        raise ValueError("Application data location must be absolute")
    desktop = destination / "applications" / DESKTOP_ID
    # Ubuntu's hicolor index does not declare 1024x1024/apps. Unthemed icons
    # in the user icon search root accept the approved PNG at its actual size.
    icon = destination / "icons" / ICON_NAME
    # Prepare/validate inputs before changing either destination.
    entry = desktop_entry(executable, icon).encode("utf-8")
    artwork = (ROOT / "images" / ICON_NAME).read_bytes()
    for path in (desktop, icon):
        if path.is_symlink() or (path.exists() and not path.is_file()):
            raise ValueError("Refusing to replace a symlink or non-file: " + str(path))
    backups = [backup for backup in (write_preserving(icon, artwork),
                                    write_preserving(desktop, entry)) if backup]
    return desktop, icon, backups


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True,
                        help="Existing Qt Creator build or installed application executable")
    args = parser.parse_args()
    if not sys.platform.startswith("linux"):
        parser.error("Desktop registration is for Linux only")
    try:
        desktop, icon, backups = register(args.executable, data_home())
    except (OSError, ValueError) as error:
        parser.exit(1, "Desktop registration failed: " + str(error) + "\n")
    tool = shutil.which("update-desktop-database")
    if tool and subprocess.run([tool, str(desktop.parent)], check=False).returncode:
        print("Warning: user desktop database refresh failed", file=sys.stderr)
    print("Desktop entry: " + str(desktop))
    print("Icon: " + str(icon))
    for backup in backups:
        print("Preserved previous file: " + str(backup))
    print("Restart the application if GNOME still shows its previous running-app icon.")


if __name__ == "__main__":
    main()
