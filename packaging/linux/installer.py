#!/usr/bin/env python3
# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Per-user atomic install/upgrade; uninstall never accesses application user data.

Adapted from RF StateSide BrickSuite's managed-prefix/desktop/rollback architecture.
System runtime dependency installation requires explicit consent; the application remains per-user.
No service changes or application launch. Python 3.11 or newer is a prerequisite.
"""
import argparse
import configparser
import grp
import pwd
import shlex
import stat
import uuid
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile
import platform
import subprocess
import sys
# Checking or uninstalling a verified package must not create unlisted bytecode files.
sys.dont_write_bytecode = True
if sys.version_info < (3, 11):
    raise SystemExit('Installer requires Python 3.11 or newer; no application or system packages changed.')
import runtime_prerequisites

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
    if '..' in path.parts or path in [Path.home(), Path('/')] or len(path.parts) < 4:
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



def desktop_target():
    data = Path(os.environ.get('XDG_DATA_HOME') or Path.home() / '.local/share')
    if not data.is_absolute() or '..' in data.parts:
        raise RuntimeError('Desktop data location must be absolute without parent traversal')
    return data / 'applications' / (IDENTITY + '.desktop')


def private_writable(info):
    """Permit the normal Ubuntu private-user-group umask, not shared write access."""
    if info.st_mode & 0o002:
        return False
    if info.st_mode & 0o020:
        try:
            group = grp.getgrgid(info.st_gid)
            owner = pwd.getpwuid(os.geteuid())
        except KeyError:
            return False
        if group.gr_mem and set(group.gr_mem) - {owner.pw_name}:
            return False
        if any(user.pw_gid == info.st_gid and user.pw_uid != os.geteuid() for user in pwd.getpwall()):
            return False
        if info.st_gid != owner.pw_gid:
            return False
    return True


def open_directory(path, create=False):
    """Walk without following symlinks, retaining a descriptor for scoped operations."""
    if not path.is_absolute() or '..' in path.parts:
        raise RuntimeError('Unsafe desktop directory')
    fd = os.open('/', os.O_RDONLY | os.O_DIRECTORY)
    try:
        for part in path.parts[1:]:
            if create:
                try:
                    os.mkdir(part, 0o755, dir_fd=fd)
                except FileExistsError:
                    pass
            next_fd = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=fd)
            os.close(fd)
            fd = next_fd
            info = os.fstat(fd)
            if info.st_uid not in (0, os.geteuid()):
                raise RuntimeError('Desktop directory has an unexpected owner')
        info = os.fstat(fd)
        if (any(name.startswith('system.posix_acl_') for name in os.listxattr(fd))
                or info.st_uid != os.geteuid() or not private_writable(info)):
            raise RuntimeError('Desktop registration directory must be owned by this user and not writable by others')
        return fd
    except BaseException:
        os.close(fd)
        raise


class DesktopRegistration:
    """Only the fixed application ID in a checked, descriptor-held directory is mutable.

    Backups are unique non-desktop files beside the launcher, outside prefix ownership.
    No executable referenced by an old launcher is run, modified or removed.
    """
    def __init__(self, path):
        self.path = path
        self.fd = open_directory(path.parent, create=True)
        try:
            self.original = self.read()
        except BaseException:
            os.close(self.fd)
            raise
        self.backup = None
        self.published = None
        self.attempted = None

    def close(self):
        os.close(self.fd)

    def read(self):
        try:
            fd = os.open(self.path.name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=self.fd)
        except FileNotFoundError:
            return None
        try:
            info = os.fstat(fd)
            if (not stat.S_ISREG(info.st_mode) or info.st_uid != os.geteuid()
                    or info.st_nlink != 1 or not private_writable(info) or info.st_size > 65536
                    or 'system.posix_acl_access' in os.listxattr(fd)):
                raise RuntimeError('Unsafe desktop file type, owner, links, permissions/ACL or size; preserving it')
            with os.fdopen(os.dup(fd), 'rb') as stream:
                data = stream.read(65537)
            if len(data) > 65536:
                raise RuntimeError('Desktop registration too large')
            return data, info
        finally:
            os.close(fd)

    @staticmethod
    def signature(snapshot):
        if snapshot is None:
            return None
        data, info = snapshot
        return (data, info.st_dev, info.st_ino, info.st_uid, info.st_gid, info.st_mode,
                info.st_nlink, info.st_mtime_ns, info.st_ctime_ns)

    def unchanged(self, expected):
        # Also reject a replaced/renamed registration directory before mutation.
        check = open_directory(self.path.parent)
        try:
            if (os.fstat(check).st_dev, os.fstat(check).st_ino) != (os.fstat(self.fd).st_dev, os.fstat(self.fd).st_ino):
                raise RuntimeError('Desktop directory changed during installation')
        finally:
            os.close(check)
        if self.signature(self.read()) != self.signature(expected):
            raise RuntimeError('Desktop registration changed during installation; preserving it')

    def recognized_unmanaged(self):
        try:
            text = self.original[0].decode('utf-8')
            parser = configparser.ConfigParser(interpolation=None, strict=True)
            parser.optionxform = str
            parser.read_string(text)
            if parser.defaults() or parser.sections() != ['Desktop Entry']:
                return False
            entry = parser['Desktop Entry']
            if entry.get('Type') != 'Application' or entry.get('Name') != 'RayNmeaSimulator':
                return False
            if any(k in entry for k in ('Actions', 'DBusActivatable', 'Hidden')):
                return False
            # Deliberately accept only an unambiguous direct executable, no shell,
            # environment wrapper, arguments, field codes or complex escaping.
            command = entry.get('Exec', '')
            if any(c in command for c in '\\$`%\n\r\x00'):
                return False
            parts = shlex.split(command)
            if len(parts) != 1:
                return False
            executable = Path(parts[0])
            if not executable.is_absolute() or '..' in executable.parts or executable.name != 'RayNmeaSimulator':
                return False
            if entry.get('StartupWMClass', 'RayNmeaSimulator') != 'RayNmeaSimulator':
                return False
            if 'TryExec' in entry and entry['TryExec'] != str(executable):
                return False
            if 'TryExec' not in entry and entry.get('StartupWMClass') != 'RayNmeaSimulator':
                return False
            fd = open_directory(executable.parent)
            try:
                info = os.stat(executable.name, dir_fd=fd, follow_symlinks=False)
                return (stat.S_ISREG(info.st_mode) and info.st_uid == os.geteuid()
                        and info.st_mode & 0o100 and private_writable(info))
            finally:
                os.close(fd)
        except (OSError, ValueError, KeyError, configparser.Error, RuntimeError):
            return False

    def consent(self, policy, non_interactive):
        if not self.recognized_unmanaged():
            raise RuntimeError('Unmanaged desktop entry is unrelated or cannot be safely identified as RayNmeaSimulator; preserving it')
        print('Unmanaged RayNmeaSimulator launcher:', self.path, flush=True)
        interactive = not non_interactive and sys.stdin.isatty() and sys.stdout.isatty()
        approved = policy == 'replace'
        if policy == 'ask' and interactive:
            try:
                approved = input('Back up and replace this launcher with the new installation? [y/N] ').strip().lower() in ('y', 'yes')
            except EOFError:
                approved = False
        if not approved:
            raise RuntimeError('Unmanaged launcher replacement not approved; no application installation changed. '
                               'Use --desktop-replacement replace to explicitly approve backup and replacement.')

    def save_backup(self):
        self.unchanged(self.original)
        name = self.path.name + '.' + uuid.uuid4().hex + '.bak'
        self.write_new(name, self.original[0], self.original[1])
        self.backup = self.path.with_name(name)
        os.fsync(self.fd)
        print('Preserved original launcher backup:', self.backup, flush=True)

    def write_new(self, name, content, info=None):
        fd = os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600, dir_fd=self.fd)
        try:
            with os.fdopen(os.dup(fd), 'wb') as stream:
                stream.write(content)
                stream.flush()
            os.fchmod(fd, stat.S_IMODE(info.st_mode) if info else 0o644)
            if info:
                os.utime(fd, ns=(info.st_atime_ns, info.st_mtime_ns))
            os.fsync(fd)
        except BaseException:
            os.unlink(name, dir_fd=self.fd)
            raise
        finally:
            os.close(fd)

    def atomic_write(self, content, info=None):
        name = '.' + self.path.name + '.' + uuid.uuid4().hex + '.tmp'
        self.write_new(name, content, info)
        try:
            os.replace(name, self.path.name, src_dir_fd=self.fd, dst_dir_fd=self.fd)
        finally:
            try:
                os.unlink(name, dir_fd=self.fd)
            except FileNotFoundError:
                pass

    def publish(self, content):
        self.unchanged(self.original)
        self.attempted = content
        self.atomic_write(content)
        self.published = self.read()
        os.fsync(self.fd)

    def rollback(self):
        current = self.read()
        if self.signature(current) == self.signature(self.original):
            return  # A failed atomic replacement leaves the original intact.
        if self.attempted is None:
            raise RuntimeError('Launcher changed outside this transaction; refusing rollback overwrite')
        if self.published is None:
            if current is None or current[0] != self.attempted:
                raise RuntimeError('Launcher changed outside this transaction; refusing rollback overwrite')
            self.unchanged(current)
        else:
            self.unchanged(self.published)
        if self.original:
            self.atomic_write(*self.original)
        else:
            os.unlink(self.path.name, dir_fd=self.fd)
        os.fsync(self.fd)

    def remove_managed(self):
        self.unchanged(self.original)
        os.unlink(self.path.name, dir_fd=self.fd)
        os.fsync(self.fd)


def launcher_content(prefix):
    path = str(prefix / 'RayNmeaSimulator')
    if any(ord(c) < 32 for c in path) or any(c in path for c in '=%'):
        raise RuntimeError('Invalid desktop prefix')
    # Exec quoting and desktop string escaping are distinct layers, as in the
    # existing development registration helper. No shell expansion is involved.
    quoted = ''.join('\\' + c if c in '\\"`$' else c for c in path)
    quoted = '"' + quoted.replace('\\', '\\\\') + '"'
    icon = str(prefix / 'RayNmeaSimulator.png').replace('\\', '\\\\')
    return ('[Desktop Entry]\nType=Application\nName=RayNmeaSimulator\n'
            f'Exec={quoted}\nTryExec={path.replace(chr(92), chr(92)*2)}\nIcon={icon}\n'
            'Terminal=false\nCategories=Utility;\nStartupWMClass=RayNmeaSimulator\n').encode()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['install', 'uninstall'])
    parser.add_argument('--prefix', type=Path, default=Path.home() / '.local/opt/RayNmeaSimulator')
    parser.add_argument('--dependencies', choices=['ask', 'check', 'install'], default='ask',
                        help='ask interactively (default), check only, or explicitly consent to installing missing runtime packages')
    parser.add_argument('--non-interactive', action='store_true', help='never prompt; missing dependencies or unmanaged launcher conflicts fail unless explicitly opted in')
    parser.add_argument('--desktop-replacement', choices=['ask', 'replace', 'preserve'], default='ask',
                        help='ask before replacing an identified unmanaged launcher; replace explicitly consents to backup/replacement, including noninteractive use; preserve refuses replacement')
    args = parser.parse_args()
    prefix = checked_prefix(args.prefix)
    prefix.parent.mkdir(parents=True, exist_ok=True)
    lock = prefix.parent / '.raynmea-install.lock'
    if lock.is_symlink():
        raise RuntimeError('Symlink lock rejected')
    with os.fdopen(os.open(lock, os.O_WRONLY | os.O_CREAT | os.O_NOFOLLOW, 0o600), 'a') as handle:
        fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
        existing = check_installed(prefix) if prefix.exists() else None
        desktop = desktop_target()
        registration = DesktopRegistration(desktop)
        try:
            return perform(args, prefix, existing, desktop, registration)
        finally:
            registration.close()


def perform(args, prefix, existing, desktop, registration):
    if existing and (str(desktop) != existing['desktop'] or registration.original is None
                     or hashlib.sha256(registration.original[0]).hexdigest() != existing['desktop_sha256']):
        raise RuntimeError('Desktop registration changed; preserving it')
    if args.action == 'uninstall':
        if not existing:
            raise RuntimeError('No managed installation at prefix')
        # Read all inputs before deleting the installed copy of this script.
        registration.remove_managed()
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
    content = launcher_content(prefix)
    unmanaged = not existing and registration.original is not None
    if unmanaged:
        registration.consent(args.desktop_replacement, args.non_interactive)
    runtime_prerequisites.ensure(source, args.dependencies, args.non_interactive)
    if unmanaged:
        registration.save_backup()
    temporary = Path(tempfile.mkdtemp(prefix='.raynmea-new-', dir=prefix.parent))
    backup = None
    published = False
    try:
        shutil.copytree(source, temporary, dirs_exist_ok=True)
        receipt = {'identity': IDENTITY, 'prefix': str(prefix), 'files': inventory(temporary),
                   'desktop': str(desktop), 'desktop_sha256': hashlib.sha256(content).hexdigest()}
        (temporary / RECEIPT).write_text(json.dumps(receipt, indent=2))
        if existing:
            backup = Path(tempfile.mkdtemp(prefix='.raynmea-old-', dir=prefix.parent))
            backup.rmdir()
            prefix.rename(backup)
        temporary.rename(prefix)
        published = True
        registration.publish(content)
    except BaseException as error:
        failures = []
        try:
            registration.rollback()
        except (OSError, RuntimeError) as rollback_error:
            failures.append('launcher rollback failed: ' + str(rollback_error))
        try:
            if published:
                shutil.rmtree(prefix)
            if backup:
                backup.rename(prefix)
        except OSError as rollback_error:
            failures.append('application rollback failed: ' + str(rollback_error))
        if failures:
            raise RuntimeError(str(error) + '; ' + '; '.join(failures)
                               + '; preserved launcher backup: ' + str(registration.backup)) from error
        raise RuntimeError(str(error) + '; original launcher/application state restored. '
                           + ('Preserved launcher backup: ' + str(registration.backup) if registration.backup else '')) from error
    finally:
        if temporary.exists():
            shutil.rmtree(temporary)
    if backup:
        shutil.rmtree(backup)
    print('Installed:', prefix, '\nLauncher:', desktop, '\nUser settings and data preserved.')


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, OSError, ValueError) as error:
        print('Installation stopped: ' + str(error), file=sys.stderr)
        raise SystemExit(1)
