#!/usr/bin/env python3
# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Package current source with an existing qualified Debian 12 ARM64 Qt runtime.

No bootstrap, package installation or host/target OS changes. All baseline inputs
are mounted read-only; only the explicitly selected output directory is writable.
"""
import argparse
import os
from pathlib import Path
import platform
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def sandbox(args):
    return ['bwrap', '--unshare-all', '--uid', '0', '--gid', '0', '--die-with-parent', '--new-session',
            '--ro-bind', str(args.rootfs), '/', '--proc', '/proc', '--dev', '/dev',
            '--tmpfs', '/tmp', '--tmpfs', '/run', '--tmpfs', '/work',
            '--ro-bind', str(ROOT), '/src', '--ro-bind', str(args.deps), '/deps',
            '--ro-bind', str(args.qt_sources), '/qt-src', '--ro-bind', str(args.cmake_tools), '/tools/cmake',
            '--ro-bind', str(args.qt), '/work/qt', '--bind', str(args.build), '/work/output',
            '--clearenv', '--setenv', 'PATH', '/tools/cmake/bin:/work/qt/bin:/usr/bin:/bin',
            '--setenv', 'HOME', '/tmp/raynmea-build', '--setenv', 'LANG', 'C.UTF-8',
            '--setenv', 'GIT_OPTIONAL_LOCKS', '0', '--chdir', '/work/output', '--']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('rootfs', 'qt', 'qt-sources', 'deps', 'cmake-tools', 'build'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    if platform.system() != 'Linux' or platform.machine() != 'aarch64':
        parser.error('Run natively on a Linux ARM64 development host; Windows/x86_64 cross-packaging is not implemented')
    if not shutil.which('bwrap'): parser.error('Missing Bubblewrap; prepare host prerequisites separately')
    for name, value in vars(args).items():
        if value.absolute() != value.resolve(): parser.error('Use physical absolute paths: --' + name.replace('_', '-'))
        setattr(args, name, value.resolve())
    for path in (args.qt / 'bin/qmake', args.cmake_tools / 'bin/cmake', args.rootfs / 'etc/os-release'):
        if not path.is_file(): parser.error('Missing baseline input: ' + str(path))
    release = (args.rootfs / 'etc/os-release').read_text()
    if 'ID=debian' not in release or 'VERSION_ID="12"' not in release:
        parser.error('--rootfs must contain Debian 12 userspace')
    for name in ('src', 'deps', 'qt-src', 'tools/cmake', 'work', 'proc', 'dev', 'tmp', 'run'):
        if not (args.rootfs / name).is_dir(): parser.error('Baseline mount directory missing: ' + name + '; see README-Debian12-ARM64.md')
    protected = (ROOT, args.rootfs, args.qt, args.qt_sources, args.deps, args.cmake_tools)
    if any(args.build == p or args.build.is_relative_to(p) or p.is_relative_to(args.build) for p in protected):
        # An ignored source build directory is safe, but never mount the source root itself writable.
        if not args.build.is_relative_to(ROOT / 'build') or any(args.build == p or args.build.is_relative_to(p) or p.is_relative_to(args.build) for p in protected[1:]):
            parser.error('Use a dedicated output directory separate from baseline inputs and source')
    args.build.mkdir(parents=True, exist_ok=True)
    base = sandbox(args)
    configure = ['cmake', '-S', '/src', '-B', '/work/output', '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
                 '-DBUILD_TESTING=OFF', '-DCMAKE_CXX_FLAGS_RELEASE=-O2 -DNDEBUG',
                 '-DCMAKE_PREFIX_PATH=/work/qt', '-DDEPS_ROOT=/deps',
                 '-DRAYNMEA_QT_SOURCES=/qt-src', '-DRAYNMEA_PACKAGE_VARIANT=Debian12']
    subprocess.run(base + configure, check=True)
    subprocess.run(base + ['cmake', '--build', '/work/output', '--target', 'Deploy'], check=True)
    print('Host output:', args.build / 'deploy/installer')
    return 0


if __name__ == '__main__': raise SystemExit(main())
