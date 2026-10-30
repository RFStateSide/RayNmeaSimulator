# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Private native Release packages; RF StateSide BrickSuite architecture adaptation.

Fresh build-tree staging, matching Qt tools/notices, explicit ELF closure/RPATH,
Inno Setup on Windows and relocatable archives on Linux. Never installs or publishes.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import tarfile
import time
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
MODULES = ['qtbase', 'qtcharts', 'qtlocation', 'qtpositioning', 'qtserialport',
           'qtwebsockets', 'qtmultimedia', 'qtdeclarative', 'qtsvg', 'qtimageformats',
           'qtshadertools', 'qtwayland', 'qt5compat']
PRIVATE = ('libQt6', 'libicu', 'libSDL2', 'libssl.so', 'libcrypto.so', 'libz.so',
           'libzstd.so', 'libbrotli', 'libpcre2-16')
ICU_LICENSE = ('https://raw.githubusercontent.com/unicode-org/icu/release-73-2/icu4c/LICENSE',
               'f3005e195ff74d8812cc1f182a1c446fab678d70a10e3dada497585befee5416')


def run(*args, **kwargs):
    return subprocess.check_output(list(map(str, args)), text=True, **kwargs)


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def debian_package(path):
    path = Path(path)
    candidates = [path]
    # Debian 12's merged-/usr filesystem retains some /lib ownership records.
    for folder in ['lib', 'bin', 'sbin']:
        prefix = Path('/usr') / folder
        if path.is_relative_to(prefix):
            alias = Path('/') / folder / path.relative_to(prefix)
            if alias.resolve() == path.resolve():
                candidates.append(alias)
    for candidate in candidates:
        try:
            return run('dpkg-query', '-S', candidate, stderr=subprocess.DEVNULL).split(': ')[0]
        except subprocess.CalledProcessError:
            pass
    raise RuntimeError('No Debian package owns runtime file: ' + str(path))


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def inno_configuration(version, stage, output):
    def quoted(path): return str(path).replace('"', '""')
    return ('; Generated private development installer configuration.\n'
            f'#define AppVersion "{version}"\n#define StageDir "{quoted(stage)}"\n'
            f'#define OutputDir "{quoted(output)}"\n'
            + (ROOT / 'deployment/windows/RayNmeaSimulator.iss').read_text(encoding='utf-8'))


def check_variant(variant, architecture, os_release, ceilings):
    if variant == 'native': return
    if variant != 'Debian12' or architecture != 'aarch64':
        raise RuntimeError('Debian12 packaging requires native ARM64 inside a Debian 12 build environment')
    release = dict(line.split('=', 1) for line in os_release.splitlines() if '=' in line)
    if release.get('ID', '').strip('"') != 'debian' or release.get('VERSION_ID', '').strip('"') != '12':
        raise RuntimeError('Debian12 variant requires rebuilding application AND Qt runtime inside Debian 12')
    for name, maximum in {'GLIBC': '2.35', 'GLIBCXX': '3.4.30', 'CXXABI': '1.3.13'}.items():
        if name not in ceilings or tuple(map(int, ceilings[name].split('.'))) > tuple(map(int, maximum.split('.'))):
            raise RuntimeError('Debian12 qualified ABI ceiling exceeded or unavailable: ' + name + ' <= ' + maximum)


def cache(build):
    return {line.split(':', 1)[0]: line.split('=', 1)[1]
            for line in (build / 'CMakeCache.txt').read_text().splitlines()
            if '=' in line and ':' in line.split('=', 1)[0] and not line.startswith('//')}


def copy(source, dest):
    if not source.is_file() or not source.stat().st_size:
        raise RuntimeError('Missing/empty package input: ' + str(source))
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, dest)


def qt_inventory(qt):
    result = {}
    for path in (qt / 'sbom').glob('*.spdx.json'):
        data = json.loads(path.read_text())
        for entry in data.get('files', []):
            name = Path(entry['fileName']).name
            info = {
                'license': entry.get('licenseConcluded', 'NOASSERTION'),
                'source': 'https://code.qt.io/cgit/qt/' + path.name.split('-6.')[0] + '.git',
                'sbom': path.name}
            result[name] = info
            if '.so.' in name:
                result[name.split('.so.')[0] + '.so.6'] = info
    return result


def notices(stage, args, config):
    licenses = stage / 'licenses'
    for name in ['LICENSE', 'THIRD_PARTY_NOTICES.md']:
        copy(ROOT / name, stage / name)
    for folder in ['LICENSES', 'resources/nmea2000', 'icons/controls']:
        for src in (ROOT / folder).rglob('*'):
            if src.is_file() and any(s in src.name.lower() for s in ['license', 'notice']):
                copy(src, licenses / 'Application' / src.relative_to(ROOT))
    for module in MODULES:
        source = args.qt_sources / module
        if not source.exists():
            if module == 'qtwayland' and os.name == 'nt':
                continue
            raise RuntimeError('Missing Qt notice source: ' + str(source))
        if f'"{config["Qt6Core_VERSION"]}"' not in (source / '.cmake.conf').read_text():
            raise RuntimeError('Qt source/SDK version mismatch: ' + module)
        selected = set()
        for src in source.rglob('*'):
            if src.is_file() and ('LICENSES' in src.relative_to(source).parts
                                  or re.match(r'^(LICENSE|LICENCE|COPYING|COPYRIGHT|NOTICE)($|[._-])', src.name, re.I)
                                  or src.name == 'qt_attribution.json'):
                selected.add(src)
                if src.name == 'qt_attribution.json':
                    # Some shipped Qt attribution records contain literal newlines in strings.
                    data = json.loads(src.read_text(encoding='utf-8'), strict=False)
                    for entry in data if isinstance(data, list) else [data]:
                        names = entry.get('LicenseFile', '')
                        for name in names if isinstance(names, list) else [names]:
                            if name:
                                for part in name.split(' '):
                                    candidate = src.parent / part
                                    if candidate.is_file():
                                        selected.add(candidate)
        if not selected:
            raise RuntimeError('Missing Qt module license material: ' + module)
        for src in selected:
            copy(src, licenses / 'Qt' / module / src.relative_to(source))
    for src in (args.qt_sources / 'LICENSES').glob('*'):
        if src.is_file():
            copy(src, licenses / 'Qt/LICENSES' / src.name)
    for module in MODULES:
        for src in (args.qt / 'sbom').glob(module + '-*.spdx.json'):
            copy(src, licenses / 'Qt/sbom' / src.name)
    if list(stage.glob('lib/libicu*.so.73')):
        # Same ICU 73.2 notice source/hash reviewed by BrickSuite's source-lock.
        cached = args.build / 'icu-73.2-LICENSE'
        if not cached.exists():
            payload = urllib.request.urlopen(ICU_LICENSE[0], timeout=30).read()
            if hashlib.sha256(payload).hexdigest() != ICU_LICENSE[1]:
                raise RuntimeError('ICU notice download checksum mismatch')
            cached.write_bytes(payload)
        if digest(cached) != ICU_LICENSE[1]:
            raise RuntimeError('ICU notice cache checksum mismatch')
        copy(cached, licenses / 'ICU/LICENSE')
    deps = Path(config['DEPS_ROOT'])
    for name in ['NMEA2000', 'NMEA0183', 'NMEA2000_socketCAN']:
        # Full upstream MIT headers, not a hand-written license summary.
        filename = {'NMEA2000': 'src/NMEA2000.h', 'NMEA0183': 'NMEA0183.h',
                    'NMEA2000_socketCAN': 'NMEA2000_SocketCAN.h'}[name]
        source = Path(config.get({'NMEA2000_socketCAN': 'SOCKETCAN'}.get(name, name) + '_ROOT') or deps / name)
        copy(source / filename, licenses / name / Path(filename).name)
    copy(ROOT / 'dependencies/pins.json', licenses / 'dependency-pins.json')
    copy(ROOT / 'dependencies/patches/nmea0183-windows-printf.patch', licenses / 'nmea0183-approved.patch')
    if os.name == 'nt':
        compiler = Path(config['CMAKE_CXX_COMPILER']).parent.parent
        for src in (compiler / 'licenses').rglob('*'):
            if src.is_file():
                copy(src, licenses / 'MinGW' / src.relative_to(compiler / 'licenses'))
        sdk = Path(config['SDL2_ROOT'])
        candidates = list(sdk.parent.glob('COPYING*')) + list(sdk.parent.glob('LICENSE*')) + list(sdk.glob('share/licenses/SDL2/*'))
        if not candidates:
            candidates = list(sdk.parent.glob('*/share/licenses/SDL2/*'))
        for src in candidates:
            if src.is_file():
                copy(src, licenses / 'SDL2' / src.name)
        if not (licenses / 'SDL2').exists():
            raise RuntimeError('SDL2 full license not found')
    copy(ROOT / 'packaging/DISTRIBUTION.txt', stage / 'DISTRIBUTION.txt')


def linux(stage, args, config, origins):
    qt = args.qt.resolve()
    binary = stage / 'bin/RayNmeaSimulator'
    copy(args.build / 'RayNmeaSimulator', binary)
    plugins = ['platforms/libqxcb.so', 'platforms/libqoffscreen.so',
               'imageformats/libqjpeg.so', 'imageformats/libqgif.so', 'imageformats/libqico.so',
               'imageformats/libqsvg.so', 'iconengines/libqsvgicon.so', 'tls/libqopensslbackend.so']
    for group in ['xcbglintegrations', 'wayland-graphics-integration-client',
                  'wayland-shell-integration', 'wayland-decoration-client', 'platforminputcontexts']:
        plugins.extend(str(p.relative_to(qt / 'plugins')) for p in (qt / 'plugins' / group).glob('*.so'))
    plugins.extend(str(p.relative_to(qt / 'plugins')) for p in (qt / 'plugins/platforms').glob('*wayland*.so'))
    for name in plugins:
        copy(qt / 'plugins' / name, stage / 'plugins' / name)
        origins['plugins/' + name] = str(qt / 'plugins' / name)
    env = os.environ.copy()
    env['LD_LIBRARY_PATH'] = str(qt / 'lib')
    scanned, system = set(), {}
    while True:
        pending = [p for p in stage.rglob('*') if p.is_file() and p.read_bytes()[:4] == b'\x7fELF' and p not in scanned]
        if not pending:
            break
        for binary in pending:
            scanned.add(binary)
            listing = run('ldd', binary, env=env)
            if 'not found' in listing:
                raise RuntimeError('Unresolved dependency: ' + listing)
            for name, location in re.findall(r'^\s*(\S+) => (/\S+)', listing, re.M):
                source = Path(location).resolve()
                if name.startswith(PRIVATE):
                    if name.startswith('libQt6') and not source.is_relative_to(qt):
                        raise RuntimeError('Mixed Qt runtime: ' + str(source))
                    dest = stage / 'lib' / name
                    if not dest.exists():
                        copy(source, dest)
                        origins[dest.relative_to(stage).as_posix()] = str(source)
                else:
                    system[name] = str(source)
    for binary in scanned:
        run('strip', '--strip-unneeded', binary)
        relative = os.path.relpath(stage / 'lib', binary.parent)
        run('patchelf', '--set-rpath', '$ORIGIN/' + relative, binary)
    (stage / 'bin/qt.conf').write_text('[Paths]\nPrefix=..\nPlugins=plugins\nLibraries=lib\n')
    (stage / 'RayNmeaSimulator').write_text('#!/bin/sh\nset -eu\nroot=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\nexec "$root/bin/RayNmeaSimulator" "$@"\n')
    (stage / 'RayNmeaSimulator').chmod(0o755)
    for name in ['install.sh', 'uninstall.sh', 'installer.py']:
        copy(ROOT / 'packaging/linux' / name, stage / name)
        (stage / name).chmod(0o755)
    copy(ROOT / 'images/RayNmeaSimulator_1024.png', stage / 'RayNmeaSimulator.png')
    # Record external baseline separately. Never bundle libc, graphics drivers, systemd or keyrings.
    external = {}
    for name, path in system.items():
        owner = debian_package(path)
        external[name] = {'path': path, 'package': owner,
                          'version': run('dpkg-query', '-W', '-f=${Version}', owner).strip()}
    write(stage / 'system-runtime.json', external)
    for name, path in {**system, **{Path(p).name: origin for p, origin in origins.items()}}.items():
        if Path(path).is_relative_to(qt):
            continue
        owner = debian_package(path)
        package = owner.split(':')[0]
        copyright_file = Path('/usr/share/doc') / package / 'copyright'
        if copyright_file.is_file():
            copy(copyright_file, stage / 'licenses/System' / package / 'copyright')
    audit = {}
    for binary in scanned:
        deps = run('ldd', binary, env={**os.environ, 'LD_LIBRARY_PATH': ''})
        if 'not found' in deps or str(qt) in deps:
            raise RuntimeError('Packaged closure resolves outside stage: ' + deps)
        audit[str(binary.relative_to(stage))] = {'dynamic': run('readelf', '-d', binary), 'ldd': deps}
    write(stage.parent / 'loader-audit.json', audit)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['build', 'qt', 'qt-sources', 'iscc']:
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--output', type=Path, help='Fresh internal generation directory; must not already exist')
    parser.add_argument('--variant', choices=['native', 'Debian12'], default='native')
    args = parser.parse_args()
    args.build = args.build.resolve()
    config = cache(args.build)
    if (config.get('CMAKE_BUILD_TYPE') != 'Release' or config.get('BUILD_TESTING') != 'OFF'
            or config.get('RAYNMEA_DEPLOYMENT') != 'ON'):
        raise RuntimeError('Deploy requires Release, BUILD_TESTING=OFF and RAYNMEA_DEPLOYMENT=ON; use a new deployment build')
    if Path(config['CMAKE_HOME_DIRECTORY']).resolve() != ROOT:
        raise RuntimeError('Build belongs to a different source tree')
    if (args.build / 'deploy').is_symlink():
        raise RuntimeError('Symlink deployment directory rejected')
    # Every invocation has fresh output, including failures; never mistake an old installer for success.
    output = args.output or args.build / 'deploy' / time.strftime('%Y%m%d-%H%M%S')
    output.mkdir(parents=True, exist_ok=False)
    stage = output / 'RayNmeaSimulator'
    stage.mkdir()
    version = re.search(r'project\(RayNmeaSimulator VERSION ([0-9.]+)', (ROOT / 'CMakeLists.txt').read_text())[1]
    config['Qt6Core_VERSION'] = run(args.qt / 'bin/qmake', '-query', 'QT_VERSION').strip()
    # Verify the application's actual linked resource objects, not just a source
    # allowlist. The probe is a build tool and is never copied into the package.
    probe = args.build / ('raynmea_bundled_content_probe.exe' if os.name == 'nt' else 'raynmea_bundled_content_probe')
    probe_env = {**os.environ, 'PATH': str(args.qt / 'bin') + os.pathsep
                 + str(Path(config['CMAKE_CXX_COMPILER']).parent) + os.pathsep + os.environ.get('PATH', ''),
                 'LD_LIBRARY_PATH': str(args.qt / 'lib')}
    bundled_content = json.loads(run(probe, env=probe_env, encoding='utf-8'))
    origins = {}
    if os.name == 'nt':
        copy(args.build / 'RayNmeaSimulator.exe', stage / 'RayNmeaSimulator.exe')
        copy(Path(config['SDL2_ROOT']) / 'bin/SDL2.dll', stage / 'SDL2.dll')
        copy(ROOT / 'images/RayNmeaSimulator.ico', stage / 'RayNmeaSimulator.ico')
        run(args.qt / 'bin/windeployqt.exe', '--release', '--compiler-runtime', '--no-translations',
            '--no-opengl-sw', '--no-system-d3d-compiler', '--no-system-dxc-compiler',
            '--dir', stage, stage / 'RayNmeaSimulator.exe')
        (stage / 'qt.conf').write_text('[Paths]\nPrefix=.\nPlugins=.\n')
        # Trace every deployed DLL to its matching SDK or compiler input, after Qt relocation.
        compiler = Path(config['CMAKE_CXX_COMPILER']).parent
        for dest in stage.rglob('*.dll'):
            matches = [p for root in [args.qt / 'bin', args.qt / 'plugins', compiler,
                                      Path(config['SDL2_ROOT']) / 'bin'] for p in root.rglob(dest.name)]
            if not matches:
                raise RuntimeError('Unreviewed deployed DLL: ' + dest.name)
            origins[dest.relative_to(stage).as_posix()] = str(matches[0])
    elif platform.system() == 'Linux' and platform.machine() in ['x86_64', 'aarch64']:
        linux(stage, args, config, origins)
    else:
        raise RuntimeError('Supported package platforms: Windows x64, Linux x86_64/aarch64')
    notices(stage, args, config)
    snapshot = ROOT / '.raynmea-source.json'
    source_record = json.loads(snapshot.read_text()) if snapshot.exists() else {
        'source_sha': run('git', '-C', ROOT, 'rev-parse', 'HEAD').strip(),
        'source_diff_sha256': hashlib.sha256(run('git', '-C', ROOT, 'diff', 'HEAD').encode()).hexdigest()}
    linux_metadata = {}
    if platform.system() == 'Linux':
        symbols = ''.join(run('objdump', '-T', p) for p in stage.rglob('*')
                          if p.is_file() and p.read_bytes()[:4] == b'\x7fELF')
        ceilings = {}
        for name in ['GLIBC', 'GLIBCXX', 'CXXABI']:
            values = re.findall(r'\b' + name + r'_(\d+(?:\.\d+)+)', symbols)
            if values:
                ceilings[name] = max(values, key=lambda s: tuple(map(int, s.split('.'))))
        linux_metadata = {'build_os_release': Path('/etc/os-release').read_text(),
                          'required_abi': ceilings, 'python_minimum': '3.11'}
        check_variant(args.variant, platform.machine(), linux_metadata['build_os_release'], ceilings)
    elif args.variant != 'native':
        raise RuntimeError('Debian12 variant is only available in a Debian 12 ARM64 build environment')
    write(stage / 'build-metadata.json', {'version': version, 'qt': config['Qt6Core_VERSION'],
          'architecture': platform.machine(), 'os': platform.platform(), 'private_candidate': True,
          'package_variant': args.variant, 'deployment_workflow': 'stable-build-local-v1',
          'clean_machine_acceptance': 'pending', 'public_distribution': 'NO-GO',
          **source_record,
          **linux_metadata,
          'packaging_tools': {str(p.relative_to(ROOT)): digest(p) for p in
              [Path(__file__).resolve(), ROOT / 'scripts/bundled-content-probe.cpp', ROOT / 'deployment/deploy.py', ROOT / 'deployment/linux/package_debian12.py', ROOT / 'deployment/windows/RayNmeaSimulator.iss', *(ROOT / 'packaging').rglob('*')] if p.is_file()},
          'compiler': run(config['CMAKE_CXX_COMPILER'], '--version').splitlines()[0],
          'template_catalog': 'six owner-approved v0.1.0 Templates',
          'bundled_content': bundled_content})
    manifest = []
    qt_files = qt_inventory(args.qt)
    for path in sorted(stage.rglob('*')):
        if path.is_file():
            name = path.relative_to(stage).as_posix()
            original = Path(origins[name]) if name in origins else None
            origin = 'RayNmeaSimulator source / matching notice sources'
            license_name, component_version, source = 'AGPL-3.0-or-later; notices retain upstream terms', version, 'RFStateSide/RayNmeaSimulator'
            if original:
                if os.name == 'nt' and path.name in ['libstdc++-6.dll', 'libgcc_s_seh-1.dll', 'libwinpthread-1.dll']:
                    origin = 'MinGW runtime/' + original.name
                    license_name = 'MIT and BSD' if 'winpthread' in path.name else 'GPL-3.0-or-later WITH GCC-exception-3.1'
                    component_version = run(config['CMAKE_CXX_COMPILER'], '-dumpfullversion').strip()
                    if 'winpthread' in path.name:
                        headers = Path(config['CMAKE_CXX_COMPILER']).parent.parent / 'x86_64-w64-mingw32/include/_mingw_mac.h'
                        component_version = '.'.join(re.search(r'#define __MINGW64_VERSION_' + field + r' (\d+)', headers.read_text())[1]
                                                     for field in ['MAJOR', 'MINOR', 'BUGFIX'])
                    source = 'https://mingw-w64.org / https://gcc.gnu.org'
                elif original.is_relative_to(args.qt.resolve()):
                    origin = 'Qt SDK/' + original.relative_to(args.qt.resolve()).as_posix()
                    info = qt_files.get(path.name, {})
                    if path.name.startswith('libicu') and path.name.endswith('.73'):
                        info = {'license': 'ICU and Unicode-DFS-2016 (full ICU LICENSE)',
                                'source': 'https://github.com/unicode-org/icu/tree/release-73-2'}
                    license_name = info.get('license', 'UNRESOLVED: review SDK binary terms')
                    source = info.get('source', 'Qt SDK redistribution source obligation pending')
                    component_version = config['Qt6Core_VERSION']
                    if path.name.startswith('libicu'):
                        component_version = '73.2'
                elif os.name == 'nt':
                    origin = 'SDL2 SDK/' + original.name if path.name == 'SDL2.dll' else 'MinGW runtime/' + original.name
                    license_name = 'Zlib' if path.name == 'SDL2.dll' else ('MIT and BSD' if 'winpthread' in path.name else 'GPL-3.0-or-later WITH GCC-exception-3.1')
                    component_version = '2.30.9' if path.name == 'SDL2.dll' else run(config['CMAKE_CXX_COMPILER'], '-dumpfullversion').strip()
                    source = 'https://libsdl.org' if path.name == 'SDL2.dll' else 'https://mingw-w64.org / https://gcc.gnu.org'
                else:
                    owner = debian_package(original)
                    component_version = run('dpkg-query', '-W', '-f=${Version}', owner).strip()
                    origin = 'OS package ' + owner
                    source = run('dpkg-query', '-W', '-f=${source:Package} ${source:Version}', owner).strip()
                    copyright_file = stage / 'licenses/System' / owner.split(':')[0] / 'copyright'
                    text = copyright_file.read_text(errors='replace')
                    license_name = '; '.join(sorted(set(re.findall(r'^License: (.+)', text, re.M)))) or 'See ' + str(copyright_file.relative_to(stage))
            manifest.append({'path': name, 'sha256': digest(path), 'bytes': path.stat().st_size,
                             'origin': origin, 'source': source, 'license': license_name,
                             'version': component_version,
                             **({'input_sha256': digest(original)} if original else {})})
    write(stage / 'manifest.json', manifest)
    if os.name == 'nt':
        generated = output / 'RayNmeaSimulator.iss'
        generated.write_text(inno_configuration(version, stage, output), encoding='utf-8')
        print(run(args.iscc, generated))
        artifact = next(output.glob('*Setup.exe'))
    else:
        variant = '-Debian12' if args.variant == 'Debian12' else ''
        artifact = output / f'RayNmeaSimulator-{version}-Linux-{platform.machine()}{variant}.tar.gz'
        with tarfile.open(artifact, 'w:gz') as archive:
            archive.add(stage, arcname='RayNmeaSimulator')
    (output / (artifact.name + '.sha256')).write_text(digest(artifact) + '  ' + artifact.name + '\n')
    print('Private candidate:', artifact, '\nSHA256:', digest(artifact))


if __name__ == '__main__':
    main()
