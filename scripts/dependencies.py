# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Acquire pinned public sources only in a dedicated staging area, or verify read-only."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request

APP = Path(__file__).resolve().parents[1]
PINS = json.loads((APP / 'dependencies/pins.json').read_text())
MARKER = '.raynmea-dependency-stage'
SDL2 = json.loads((APP / 'dependencies/sdl2-mingw.json').read_text())
SDL2_MARKER = '.raynmea-sdl2-stage'
MAX_ARCHIVE = 64 * 1024 * 1024
MAX_EXPANDED = 256 * 1024 * 1024


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def sdl_members(archive, pin):
    """Validate the pinned archive before extracting any members."""
    members = archive.getmembers()
    if len(members) > 10000 or sum(m.size for m in members) > MAX_EXPANDED:
        raise RuntimeError('SDL2: archive exceeds extraction limits')
    seen = set()
    for member in members:
        path = PurePosixPath(member.name)
        if (path.is_absolute() or '..' in path.parts or not path.parts
                or path.parts[0] != pin['directory'] or '\\' in member.name
                or ':' in member.name or not (member.isfile() or member.isdir())
                or member.name.casefold().rstrip('/') in seen):
            raise RuntimeError('SDL2: unsafe or duplicate archive member')
        seen.add(member.name.casefold().rstrip('/'))
    return members


def verify_sdl2(sdk, archive_path, pin=None):
    pin = pin or SDL2
    if (not archive_path.is_file() or archive_path.is_symlink()
            or archive_path.stat().st_size > MAX_ARCHIVE
            or sha256(archive_path) != pin['sha256']):
        raise RuntimeError('SDL2: missing archive or archive checksum mismatch')
    if not sdk.is_dir() or sdk.is_symlink():
        raise RuntimeError('SDL2: missing SDK or symbolic link')
    expected = {}
    prefix = pin['directory'] + '/' + pin['sdk'] + '/'
    with tarfile.open(archive_path, 'r:gz') as archive:
        for member in sdl_members(archive, pin):
            if member.isfile() and member.name.startswith(prefix):
                stream = archive.extractfile(member)
                expected[member.name[len(prefix):]] = hashlib.sha256(stream.read()).hexdigest()
    required = {'include/SDL2/SDL.h', 'lib/libSDL2.dll.a', 'bin/SDL2.dll',
                'lib/cmake/SDL2/SDL2Config.cmake', 'lib/cmake/SDL2/SDL2ConfigVersion.cmake'}
    if not required.issubset(expected):
        raise RuntimeError('SDL2: archive lacks the required MinGW x64 SDK')
    actual = {}
    for path in sdk.rglob('*'):
        if path.is_symlink():
            raise RuntimeError('SDL2: SDK contains a symbolic link')
        if path.is_file():
            actual[path.relative_to(sdk).as_posix()] = sha256(path)
    if actual != expected:
        raise RuntimeError('SDL2: SDK differs from the pinned archive; existing files preserved')
    print(f'Verified SDL2 {pin["version"]} MinGW x64: archive SHA256 and {len(expected)} SDK files')


def acquire_sdl2(root, pin=None, opener=None):
    pin = pin or SDL2
    opener = opener or urllib.request.urlopen
    downloads = root / '.downloads'
    if downloads.is_symlink():
        raise RuntimeError('SDL2: downloads directory is a symbolic link')
    downloads.mkdir(exist_ok=True)
    archive_path = downloads / pin['archive']
    destination = root / pin['directory']
    if destination.is_symlink():
        raise RuntimeError('SDL2: SDK destination is a symbolic link')
    # Never replace an existing cache or SDK. A failed stage can be inspected or
    # superseded with --root pointing at a new empty directory.
    with tempfile.TemporaryDirectory(prefix='.sdl2-prepare-', dir=root) as temporary:
        temporary = Path(temporary)
        if not archive_path.exists():
            pending = temporary / 'download.tar.gz'
            with opener(pin['url'], timeout=30) as response, pending.open('wb') as stream:
                size = 0
                for chunk in iter(lambda: response.read(1024 * 1024), b''):
                    size += len(chunk)
                    if size > MAX_ARCHIVE:
                        raise RuntimeError('SDL2: download exceeds size limit')
                    stream.write(chunk)
            if sha256(pending) != pin['sha256']:
                raise RuntimeError('SDL2: downloaded archive checksum mismatch')
            if archive_path.exists():
                raise RuntimeError('SDL2: archive appeared during acquisition; retry verification')
            pending.rename(archive_path)
        if (archive_path.is_symlink() or not archive_path.is_file()
                or archive_path.stat().st_size > MAX_ARCHIVE
                or sha256(archive_path) != pin['sha256']):
            raise RuntimeError('SDL2: existing archive checksum mismatch; cache preserved')
        if not destination.exists():
            with tarfile.open(archive_path, 'r:gz') as archive:
                for member in sdl_members(archive, pin):
                    target = temporary.joinpath(*PurePosixPath(member.name).parts)
                    if member.isdir():
                        target.mkdir(parents=True, exist_ok=True)
                    else:
                        target.parent.mkdir(parents=True, exist_ok=True)
                        with archive.extractfile(member) as source, target.open('xb') as output:
                            shutil.copyfileobj(source, output)
            verify_sdl2(temporary / pin['directory'] / pin['sdk'], archive_path, pin)
            (temporary / pin['directory']).rename(destination)
        else:
            verify_sdl2(destination / pin['sdk'], archive_path, pin)
    receipt = root / SDL2_MARKER
    if receipt.exists():
        if receipt.is_symlink() or receipt.read_text() != pin['sha256']:
            raise RuntimeError('SDL2: managed-stage receipt mismatch; existing receipt preserved')
    else:
        with receipt.open('x') as stream:
            stream.write(pin['sha256'])


def git(root, *args):
    return subprocess.check_output(
        ['git', '-c', 'core.autocrlf=false', '-C', str(root), *args],
        stderr=subprocess.STDOUT)


def patch_bytes(pin):
    if 'patch' not in pin:
        return b''
    patch = (APP / 'dependencies' / pin['patch']).read_bytes()
    if hashlib.sha256(patch).hexdigest() != pin['patch_sha256']:
        raise RuntimeError('Tracked patch checksum mismatch')
    return patch


def verify(name, root):
    pin = PINS[name]
    if not (root / '.git').exists():
        raise RuntimeError(f'{name}: missing Git checkout at {root}')
    if git(root, 'rev-parse', 'HEAD').decode().strip() != pin['revision']:
        raise RuntimeError(f'{name}: revision mismatch at {root}')
    # Compare all tracked changes to HEAD, including staged changes. Reject extra files.
    if git(root, 'diff', '--no-ext-diff', '--binary', 'HEAD') != patch_bytes(pin):
        raise RuntimeError(f'{name}: source differs from the approved patch state')
    if git(root, 'ls-files', '--others'):
        raise RuntimeError(f'{name}: unexpected untracked/ignored files')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=['acquire', 'verify'])
    parser.add_argument('--root', type=Path, default=APP / '.dependencies')
    parser.add_argument('--nmea2000', type=Path)
    parser.add_argument('--nmea0183', type=Path)
    parser.add_argument('--socketcan', type=Path)
    parser.add_argument('--with-sdl2', action='store_true', help='Also acquire/verify the pinned Windows MinGW x64 SDK')
    parser.add_argument('--sdl2-root', type=Path, help='Read-only verification override for the SDL2 SDK')
    parser.add_argument('--sdl2-archive', type=Path, help='Read-only verification override for the pinned SDL2 archive')
    args = parser.parse_args()
    root = args.root.resolve()
    overrides = {'NMEA2000': args.nmea2000, 'NMEA0183': args.nmea0183,
                 'NMEA2000_socketCAN': args.socketcan}
    with_sdl2 = args.with_sdl2 or args.sdl2_root is not None or args.sdl2_archive is not None
    if args.command == 'acquire':
        if any(overrides.values()) or args.sdl2_root or args.sdl2_archive:
            raise RuntimeError('Acquisition uses --root only; overrides are read-only verification')
        # Never reset, patch, or clean existing user repositories.
        if root.exists() and any(root.iterdir()) and not (root / MARKER).is_file():
            raise RuntimeError('Refusing a nonempty directory without the staging marker')
        root.mkdir(parents=True, exist_ok=True)
        (root / MARKER).touch(exist_ok=True)
    for name, pin in PINS.items():
        target = (overrides[name] or root / name).resolve()
        patch_bytes(pin)  # Validate the tracked patch before any clone.
        if args.command == 'acquire' and not target.exists():
            subprocess.run(['git', '-c', 'core.autocrlf=false', 'clone', '--no-checkout',
                            pin['url'], str(target)], check=True)
            git(target, 'checkout', '--detach', pin['revision'])
            if 'patch' in pin:
                git(target, 'apply', '--check', str(APP / 'dependencies' / pin['patch']))
                git(target, 'apply', str(APP / 'dependencies' / pin['patch']))
        verify(name, target)
        print(f'Verified {name} {pin["revision"]}')
    if with_sdl2:
        if args.command == 'acquire':
            acquire_sdl2(root)
        else:
            verify_sdl2(args.sdl2_root or root / SDL2['directory'] / SDL2['sdk'],
                        args.sdl2_archive or root / '.downloads' / SDL2['archive'])


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, subprocess.CalledProcessError, OSError, tarfile.TarError) as exc:
        print(f'Dependency check failed: {exc}', file=sys.stderr)
        sys.exit(1)
