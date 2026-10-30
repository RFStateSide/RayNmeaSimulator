# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Explicit local SDL2 source provisioning; verify never downloads or builds.

The release archive is SHA256 pinned. Source bytes are compared to that archive;
installed bytes are checked against a local build receipt, not upstream binaries.
Only an empty stage may be acquired; failed/changed stages are preserved.
"""
import argparse
import json
from pathlib import Path, PurePosixPath
import platform
import shutil
import subprocess
import sys
import tarfile
import urllib.request

sys.dont_write_bytecode = True
from dependencies import sha256, sdl_members, MAX_ARCHIVE

APP = Path(__file__).resolve().parents[1]
PIN = json.loads((APP / 'dependencies/sdl2-macos.json').read_text())
RECEIPT = 'receipt.json'


def inventory(root):
    if root.is_symlink() or not root.is_dir():
        raise RuntimeError('SDL2: inventory root must be an existing real directory')
    result = {}
    for path in root.rglob('*'):
        if path.is_symlink():
            raise RuntimeError('SDL2: symbolic links are not permitted in this static stage')
        if path.is_file():
            result[path.relative_to(root).as_posix()] = sha256(path)
    return result


def verify_archive(path):
    if (path.is_symlink() or not path.is_file() or path.stat().st_size > MAX_ARCHIVE
            or sha256(path) != PIN['sha256']):
        raise RuntimeError('SDL2: archive checksum mismatch or missing pinned archive')


def verify(root):
    archive_path = root / PIN['archive']
    verify_archive(archive_path)
    expected = {}
    with tarfile.open(archive_path, 'r:gz') as archive:
        for member in sdl_members(archive, PIN):
            if member.isfile():
                import hashlib
                expected[PurePosixPath(member.name).relative_to(PIN['directory']).as_posix()] = hashlib.sha256(archive.extractfile(member).read()).hexdigest()
    if inventory(root / PIN['directory']) != expected:
        raise RuntimeError('SDL2: source differs from pinned release archive')
    receipt = json.loads((root / RECEIPT).read_text())
    if receipt['pin'] != PIN or receipt['architecture'] != 'arm64':
        raise RuntimeError('SDL2: receipt identity/architecture mismatch')
    actual = inventory(root / 'install')
    required = {'include/SDL2/SDL.h', 'lib/libSDL2.a', 'lib/cmake/SDL2/SDL2Config.cmake',
                'lib/cmake/SDL2/SDL2ConfigVersion.cmake'}
    if not required.issubset(actual) or actual != receipt['installed_sha256']:
        raise RuntimeError('SDL2: installed SDK differs from local build receipt')
    arches = subprocess.check_output(['/usr/bin/lipo', '-archs', str(root / 'install/lib/libSDL2.a')], text=True).split()
    if arches != ['arm64']:
        raise RuntimeError('SDL2: static library must contain only arm64')
    print('Verified SDL2 2.30.9: pinned source archive, exact source/install inventory, native arm64 static library')


def acquire(args, root):
    if sys.platform != 'darwin' or platform.machine() != 'arm64':
        raise RuntimeError('SDL2: acquire requires native macOS arm64')
    if root.exists() and any(root.iterdir()):
        verify(root)
        return
    root.mkdir(parents=True, exist_ok=True)
    archive_path = root / PIN['archive']
    if args.archive:
        verify_archive(args.archive)
        shutil.copyfile(args.archive, archive_path)
    else:
        pending = root / 'download.pending'
        with urllib.request.urlopen(PIN['url'], timeout=30) as response, pending.open('xb') as output:
            size = 0
            for chunk in iter(lambda: response.read(1024 * 1024), b''):
                size += len(chunk)
                if size > MAX_ARCHIVE:
                    raise RuntimeError('SDL2: download exceeds limit')
                output.write(chunk)
        verify_archive(pending)
        pending.rename(archive_path)
    with tarfile.open(archive_path, 'r:gz') as archive:
        for member in sdl_members(archive, PIN):
            target = root.joinpath(*PurePosixPath(member.name).parts)
            if member.isdir():
                target.mkdir(parents=True, exist_ok=True)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                with archive.extractfile(member) as source, target.open('xb') as output:
                    shutil.copyfileobj(source, output)
    configure = [args.cmake, '-S', str(root / PIN['directory']), '-B', str(root / 'build'),
                 '-G', 'Ninja', '-DCMAKE_MAKE_PROGRAM=' + args.ninja,
                 '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_OSX_ARCHITECTURES=arm64',
                 '-DCMAKE_INSTALL_PREFIX=' + str(root / 'install'),
                 '-DSDL_SHARED=OFF', '-DSDL_STATIC=ON', '-DSDL_TEST=OFF',
                 '-DSDL_TESTS=OFF', '-DSDL2_DISABLE_SDL2MAIN=ON']
    subprocess.run(configure, check=True)
    subprocess.run([args.cmake, '--build', str(root / 'build'), '--parallel', str(args.jobs)], check=True)
    subprocess.run([args.cmake, '--install', str(root / 'build')], check=True)
    receipt = {'pin': PIN, 'architecture': 'arm64', 'configure': configure,
               'compiler': subprocess.check_output(['/usr/bin/clang', '--version'], text=True),
               'installed_sha256': inventory(root / 'install')}
    (root / RECEIPT).write_text(json.dumps(receipt, indent=2) + '\n')
    verify(root)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=['acquire', 'verify'])
    parser.add_argument('--root', type=Path, default=APP / '.dependencies/sdl2-macos-arm64')
    parser.add_argument('--archive', type=Path, help='Acquire from an already downloaded pinned archive')
    parser.add_argument('--cmake', default='cmake')
    parser.add_argument('--ninja', default=shutil.which('ninja') or 'ninja')
    parser.add_argument('--jobs', type=int, default=2)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    if args.root.is_symlink():
        raise RuntimeError('SDL2: stage root must not be a symbolic link')
    root = args.root.resolve()
    if args.command == 'acquire':
        acquire(args, root)
    else:
        verify(root)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.CalledProcessError, tarfile.TarError) as error:
        print('SDL2 provisioning failed:', error, file=sys.stderr)
        sys.exit(1)
