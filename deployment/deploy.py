#!/usr/bin/env python3
# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Qt Creator/CLI private Deploy adapter; stable outputs, fresh internal generations.

Build-local organization follows RF StateSide BrickSuite. The existing native
packager owns runtime selection, notices, ABI measurement and installer behavior.
"""
import argparse
from contextlib import contextmanager
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import uuid

ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT / 'scripts'))
import package

IDENTITY = 'com.rfstateside.RayNmeaSimulator.deploy.v1'
FORWARDED = ('CMAKE_CXX_COMPILER', 'CMAKE_C_COMPILER', 'CMAKE_MAKE_PROGRAM',
             'CMAKE_TOOLCHAIN_FILE', 'CMAKE_PREFIX_PATH', 'Qt6_DIR',
             'CMAKE_CXX_FLAGS', 'CMAKE_CXX_FLAGS_RELEASE', 'CMAKE_C_FLAGS',
             'CMAKE_C_FLAGS_RELEASE', 'CMAKE_EXE_LINKER_FLAGS', 'CMAKE_EXE_LINKER_FLAGS_RELEASE',
             'DEPS_ROOT', 'NMEA2000_ROOT', 'NMEA0183_ROOT', 'SOCKETCAN_ROOT',
             'SDL2_ROOT', 'SDL2_DIR', 'SDL2_ARCHIVE')


def checked(path, build):
    """All managed moves/writes stay inside the selected physical build directory."""
    path = Path(path).absolute()
    if not path.is_relative_to(build) or path.resolve() != path:
        raise RuntimeError('Redirected/outside deployment path rejected: ' + str(path))
    return path


@contextmanager
def locked(build):
    folder = checked(build / 'deployment', build)
    folder.mkdir(exist_ok=True)
    lock = checked(folder / '.deploy.lock', build)
    with lock.open('a+b') as stream:
        if os.name == 'nt':
            import msvcrt
            stream.seek(0); stream.write(b'0'); stream.flush(); stream.seek(0)
            msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
        else:
            import fcntl
            fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
        yield


class Attempt:
    def __init__(self, build):
        self.build = build
        self.root = checked(build / 'deploy', build)
        self.root.mkdir(exist_ok=True)
        self.state = checked(self.root / 'deployment-result.json', build)
        previous = json.loads(self.state.read_text()) if self.state.exists() else None
        paths = [self.root / 'RayNmeaSimulator', self.root / 'installer',
                 build / 'deployment/windows/RayNmeaSimulator.iss']
        for p in paths:
            checked(p, build)
        if any(p.exists() for p in paths) and (not previous or previous.get('identity') != IDENTITY):
            raise RuntimeError('Unmanaged output exists; preserve it and choose a new Release build directory')
        self.generation = checked(self.root / '.runs' / uuid.uuid4().hex, build)
        self.generation.mkdir(parents=True)
        # Invalidate last success BEFORE configure/compile, not merely before packaging.
        self.record('running')
        for p in paths:
            if p.exists():
                dest = self.generation / 'previous' / ('generated.iss' if p.suffix == '.iss' else p.name)
                dest.parent.mkdir(exist_ok=True)
                checked(p, build).rename(checked(dest, build))

    def record(self, status, **details):
        temporary = checked(self.root / ('.deployment-result-' + uuid.uuid4().hex + '.tmp'), self.build)
        package.write(temporary, {'identity': IDENTITY, 'status': status,
                                 'generation': str(self.generation), **details})
        temporary.replace(self.state)

    def publish(self, output, artifact):
        installer = self.generation / 'installer'
        installer.mkdir()
        for p in (artifact, Path(str(artifact) + '.sha256'), output / 'RayNmeaSimulator/build-metadata.json', output / 'update-package.json'):
            shutil.copy2(p, installer / p.name)
        audit = output / 'loader-audit.json'
        if audit.exists(): shutil.copy2(audit, installer / audit.name)
        checked(output / 'RayNmeaSimulator', self.build).rename(checked(self.root / 'RayNmeaSimulator', self.build))
        checked(installer, self.build).rename(checked(self.root / 'installer', self.build))
        if os.name == 'nt':
            metadata = json.loads((self.root / 'RayNmeaSimulator/build-metadata.json').read_text())
            generated = checked(self.build / 'deployment/windows/RayNmeaSimulator.iss', self.build)
            generated.parent.mkdir(parents=True, exist_ok=True)
            generated.write_text(package.inno_configuration(metadata['version'], self.root / 'RayNmeaSimulator', self.root / 'installer'), encoding='utf-8')
        final = self.root / 'installer' / artifact.name
        self.record('succeeded', artifact=str(final), sha256=package.digest(final),
                    bytes=final.stat().st_size, stage=str(self.root / 'RayNmeaSimulator'))
        print('\nDeployment complete', flush=True)
        print('Staged application:', self.root / 'RayNmeaSimulator', flush=True)
        print('Package output:', final, flush=True)
        print('SHA-256:', package.digest(final), flush=True)

    def failed(self, error):
        # A partially published tree is evidence, never a current successful result.
        for p in [self.root / 'RayNmeaSimulator', self.root / 'installer',
                  self.build / 'deployment/windows/RayNmeaSimulator.iss']:
            if p.exists():
                dest = self.generation / 'failed-publish' / ('generated.iss' if p.suffix == '.iss' else p.name)
                dest.parent.mkdir(exist_ok=True)
                checked(p, self.build).rename(checked(dest, self.build))
        self.record('failed', error=str(error))


def command(args, log):
    print('+', subprocess.list2cmdline(list(map(str, args))), flush=True)
    with subprocess.Popen(list(map(str, args)), stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          text=True, encoding='utf-8', errors='replace') as process:
        for line in process.stdout:
            print(line, end='', flush=True); log.write(line); log.flush()
        if process.wait(): raise RuntimeError('Command failed; see deployment/deploy.log')


def deploy(build, cmake):
    build = Path(build).absolute()
    if build.resolve() != build: raise RuntimeError('Use a physical build directory')
    if build == ROOT: raise RuntimeError('Deploy requires a shadow build directory, not the source root')
    config = package.cache(build)
    if Path(config['CMAKE_HOME_DIRECTORY']).resolve() != ROOT:
        raise RuntimeError('Build belongs to a different source checkout')
    with locked(build):
        attempt = Attempt(build)
        try:
            if config.get('CMAKE_BUILD_TYPE') != 'Release':
                raise RuntimeError('Deploy requires Release. Select MinGW Release in Qt Creator, then build the Deploy target (deployment/README.md).')
            if config.get('CMAKE_GENERATOR') != 'Ninja':
                raise RuntimeError('Deploy currently requires a single-config Ninja Release build')
            runtime = checked(build / 'deployment/build', build)
            qt = Path(config['Qt6_DIR']).resolve().parents[2]
            qt_sources = config.get('RAYNMEA_QT_SOURCES') or str(qt.parent / 'Src')
            iscc = config.get('RAYNMEA_ISCC') or 'C:/Program Files (x86)/Inno Setup 6/ISCC.exe'
            variant = config.get('RAYNMEA_PACKAGE_VARIANT', 'native')
            with (build / 'deployment/deploy.log').open('w', encoding='utf-8') as log:
                configure = [cmake, '-S', ROOT, '-B', runtime, '-G', 'Ninja',
                             '-DCMAKE_BUILD_TYPE=Release', '-DBUILD_TESTING=OFF', '-DRAYNMEA_DEPLOYMENT=ON']
                configure += ['-D' + k + '=' + config[k] for k in FORWARDED if config.get(k)]
                command(configure, log)
                command([cmake, '--build', runtime, '--target', 'RayNmeaSimulator', '--parallel', '4'], log)
                output = attempt.generation / 'package'
                command([sys.executable, '-B', ROOT / 'scripts/package.py', '--build', runtime,
                         '--qt', qt, '--qt-sources', qt_sources, '--iscc', iscc,
                         '--output', output, '--variant', variant], log)
            suffix = '*Setup.exe' if os.name == 'nt' else '*.tar.gz'
            artifacts = list(output.glob(suffix))
            if len(artifacts) != 1: raise RuntimeError('Expected exactly one completed package')
            attempt.publish(output, artifacts[0])
        except BaseException as error:
            attempt.failed(error)
            raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', required=True, type=Path, help='Selected CMake Release build directory')
    parser.add_argument('--cmake', default='cmake', help='CMake executable from the selected kit')
    args = parser.parse_args()
    try: deploy(args.build, args.cmake)
    except (OSError, ValueError, KeyError, RuntimeError, KeyboardInterrupt) as error:
        print('ERROR: Deploy failed:', error, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__': raise SystemExit(main())
