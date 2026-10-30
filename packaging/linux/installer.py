#!/usr/bin/env python3
# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Per-user atomic install/upgrade; uninstall never accesses application user data.

Adapted from RF StateSide BrickSuite's managed-prefix/desktop/rollback architecture.
No sudo, apt, service management or application launch. Python3 is a runtime prerequisite.
"""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile
import platform
import subprocess

IDENTITY = 'com.rfstateside.RayNmeaSimulator'
RECEIPT = '.raynmea-install.json'


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def inventory(root):
    result = {}
    for path in root.rglob('*'):
        if path.is_symlink():
            raise RuntimeError('Symlink in installation: ' + str(path))
        if path.is_file() and path.name != RECEIPT:
            result[path.relative_to(root).as_posix()] = digest(path)
    return result


def checked_prefix(path):
    path = path.expanduser().absolute()
    for part in [path, *path.parents]:
        if part.is_symlink():
            raise RuntimeError('Symlink prefix rejected')
    if path in [Path.home(), Path('/')] or len(path.parts) < 4:
        raise RuntimeError('Use a dedicated application directory')
    return path


def check_installed(prefix):
    receipt = json.loads((prefix / RECEIPT).read_text())
    if receipt['identity'] != IDENTITY or receipt['prefix'] != str(prefix):
        raise RuntimeError('Unmanaged or moved installation; preserving it')
    if inventory(prefix) != receipt['files']:
        raise RuntimeError('Installed files changed or extra files exist; preserving them for owner review')
    for exe in Path('/proc').glob('[0-9]*/exe'):
        try:
            if exe.resolve() == prefix / 'bin/RayNmeaSimulator':
                raise RuntimeError('Close RayNmeaSimulator before installing or uninstalling')
        except (PermissionError, FileNotFoundError, ProcessLookupError):
            pass
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['install', 'uninstall'])
    parser.add_argument('--prefix', type=Path, default=Path.home() / '.local/opt/RayNmeaSimulator')
    args = parser.parse_args()
    prefix = checked_prefix(args.prefix)
    prefix.parent.mkdir(parents=True, exist_ok=True)
    lock = prefix.parent / '.raynmea-install.lock'
    if lock.is_symlink():
        raise RuntimeError('Symlink lock rejected')
    with lock.open('a') as handle:
        fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
        existing = check_installed(prefix) if prefix.exists() else None
        desktop = Path(os.environ.get('XDG_DATA_HOME', str(Path.home() / '.local/share'))) / 'applications' / (IDENTITY + '.desktop')
        if desktop.is_symlink() or desktop.parent.is_symlink():
            raise RuntimeError('Symlink desktop path rejected')
        if existing and (str(desktop) != existing['desktop'] or not desktop.is_file()
                         or digest(desktop) != existing['desktop_sha256']):
            raise RuntimeError('Desktop registration changed; preserving it')
        if args.action == 'uninstall':
            if not existing:
                raise RuntimeError('No managed installation at prefix')
            # Read all inputs before deleting the installed copy of this script.
            desktop.unlink()
            shutil.rmtree(prefix)
            print('Removed application files. Settings, credentials, themes, routes and logs preserved.')
            return
        source = Path(__file__).resolve().parent
        if source == prefix:
            raise RuntimeError('Run upgrade from the newly extracted archive')
        manifest = json.loads((source / 'manifest.json').read_text())
        expected = {entry['path']: entry['sha256'] for entry in manifest}
        actual = inventory(source)
        actual.pop('manifest.json', None)
        if actual != expected:
            raise RuntimeError('Package manifest mismatch; no installation changed')
        metadata = json.loads((source / 'build-metadata.json').read_text())
        if platform.machine() != metadata['architecture']:
            raise RuntimeError('Package architecture does not match this machine')
        required = metadata.get('required_abi', {}).get('GLIBC')
        if required and tuple(map(int, os.confstr('CS_GNU_LIBC_VERSION').split()[-1].split('.'))) < tuple(map(int, required.split('.'))):
            raise RuntimeError('Package requires glibc ' + required + ' or newer')
        loader = subprocess.check_output(['ldd', str(source / 'bin/RayNmeaSimulator')], text=True,
                                         env={**os.environ, 'LD_LIBRARY_PATH': ''})
        if 'not found' in loader:
            raise RuntimeError('Missing system runtime libraries:\n' + loader)
        if not existing and desktop.exists():
            raise RuntimeError('Existing desktop registration; preserving it')
        desktop.parent.mkdir(parents=True, exist_ok=True)
        temporary = Path(tempfile.mkdtemp(prefix='.raynmea-new-', dir=prefix.parent))
        backup = None
        published = False
        old_desktop = desktop.read_bytes() if desktop.exists() else None
        try:
            shutil.copytree(source, temporary, dirs_exist_ok=True)
            # Desktop Entry escaping for paths with spaces, quotes and field-code characters.
            target = str(prefix / 'RayNmeaSimulator').replace('\\', '\\\\').replace('"', '\\"').replace('`', '\\`').replace('$', '\\$').replace('%', '%%')
            if '\n' in target or '\r' in target:
                raise RuntimeError('Invalid desktop prefix')
            content = ('[Desktop Entry]\nType=Application\nName=RayNmeaSimulator\n'
                       f'Exec="{target}"\nIcon={prefix}/RayNmeaSimulator.png\n'
                       'Terminal=false\nCategories=Utility;\n').encode()
            receipt = {'identity': IDENTITY, 'prefix': str(prefix), 'files': inventory(temporary),
                       'desktop': str(desktop), 'desktop_sha256': hashlib.sha256(content).hexdigest()}
            (temporary / RECEIPT).write_text(json.dumps(receipt, indent=2))
            if existing:
                backup = Path(tempfile.mkdtemp(prefix='.raynmea-old-', dir=prefix.parent))
                backup.rmdir()
                prefix.rename(backup)
            temporary.rename(prefix)
            published = True
            desktop.write_bytes(content)
        except BaseException:
            if published:
                shutil.rmtree(prefix)
            if backup:
                backup.rename(prefix)
            if old_desktop is not None:
                desktop.write_bytes(old_desktop)
            elif desktop.exists():
                desktop.unlink()
            raise
        finally:
            if temporary.exists():
                shutil.rmtree(temporary)
        if backup:
            shutil.rmtree(backup)
        print('Installed:', prefix, '\nLauncher:', desktop, '\nUser settings and data preserved.')


if __name__ == '__main__':
    main()
