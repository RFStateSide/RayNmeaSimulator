# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Focused CMake/CTest development; no downloads, normal profiles or live CAN.

Use a NEW dedicated build directory. Test inventory and executable prerequisites
come from CTest JSON, not a second suite allowlist. Existing IDE builds are never
reconfigured by this runner. Normal application development uses CMake presets.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['list', 'test', 'build', 'run'])
    parser.add_argument('--build-root', required=True, type=Path)
    parser.add_argument('--qt-root', type=Path)
    parser.add_argument('--deps-root', type=Path)
    parser.add_argument('--sdl-root', type=Path)
    parser.add_argument('--compiler-root', type=Path, help='Optional compiler prefix; bin is added to PATH')
    parser.add_argument('--config', choices=['Debug', 'Release'], default='Debug')
    parser.add_argument('--cmake', default='cmake')
    parser.add_argument('--jobs', type=int, default=2)
    parser.add_argument('--suites', nargs='+', help='Exact CTest names')
    parser.add_argument('--label', help='CTest label regular expression')
    parser.add_argument('--match', help='CTest name regular expression')
    parser.add_argument('--target', nargs='+', help='Explicit CMake build/helper targets')
    parser.add_argument('--platform', help='Qt platform for tests/helpers (offscreen, windows, xcb, ...)')
    parser.add_argument('--smoke', action='store_true', help='Bounded startup or route-fixture smoke instead of manual UI')
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    if args.action == 'test' and not any([args.suites, args.label, args.match]):
        parser.error('Select --suites, --label or --match; no implicit Run All')
    if args.action == 'build' and not args.target:
        parser.error('build requires --target')
    if args.action == 'run' and (not args.target or len(args.target) != 1 or args.target[0] not in
                                ['RayNmeaSimulator-isolated', 'route_acceptance_fixture']):
        parser.error('run requires --target RayNmeaSimulator-isolated or route_acceptance_fixture; production runs are excluded')
    build = args.build_root.resolve()
    if build == ROOT or ROOT.is_relative_to(build):
        parser.error('Use a dedicated build directory, not the source or its parent')
    marker = build / '.raynmea-cmake-dev.json'
    if not marker.exists() and build.exists() and any(build.iterdir()):
        parser.error('Use a NEW build directory; existing Qt Creator/preset outputs are not repurposed')
    identity = {'source': str(ROOT), 'config': args.config}
    if marker.exists() and json.loads(marker.read_text()) != identity:
        parser.error('Source/configuration differs from this runner directory; choose a new one')
    env = os.environ.copy()
    paths = []
    if args.compiler_root:
        paths.append(str(args.compiler_root.resolve() / 'bin'))
    if args.qt_root:
        paths.append(str(args.qt_root.resolve() / 'bin'))
    env['PATH'] = os.pathsep.join(paths + [env.get('PATH', '')])
    cmake = shutil.which(args.cmake, path=env['PATH'])
    if not cmake:
        parser.error('CMake not found; put CMake/Ninja/compiler on PATH or use --cmake')
    ctest = str(Path(cmake).with_name('ctest.exe' if os.name == 'nt' else 'ctest'))

    def run(command, capture=False):
        result = subprocess.run(list(map(str, command)), env=env, text=True,
                                stdout=subprocess.PIPE if capture else None, check=True)
        return result.stdout if capture else None

    build.mkdir(parents=True, exist_ok=True)
    marker.write_text(json.dumps(identity, indent=2))
    # Ask CMake for authoritative executable locations. Qt's Windows targets
    # need not live under tests/ even though they are declared there.
    query = build / '.cmake/api/v1/query/codemodel-v2'
    query.parent.mkdir(parents=True, exist_ok=True)
    query.touch()
    command = [cmake, '-S', ROOT, '-B', build, '-G', 'Ninja',
               '-DCMAKE_BUILD_TYPE=' + args.config, '-DBUILD_TESTING=ON']
    if sys.platform == 'darwin':
        command.append('-DCMAKE_OSX_ARCHITECTURES=arm64')
    for key, value in [('CMAKE_PREFIX_PATH', args.qt_root), ('DEPS_ROOT', args.deps_root), ('SDL2_ROOT', args.sdl_root)]:
        if value:
            command.append('-D' + key + '=' + str(value.resolve()))
    if args.config == 'Release':
        command.append('-DCMAKE_CXX_FLAGS_RELEASE=-O2 -DNDEBUG')
    run(command)
    cache = {}
    for line in (build / 'CMakeCache.txt').read_text().splitlines():
        if '=' in line and ':' in line.split('=', 1)[0]:
            key, value = line.split('=', 1)
            cache[key.split(':', 1)[0]] = value
    qt = Path(cache['Qt6_DIR']).resolve().parents[2]
    if args.platform:
        env['RAYNMEA_TEST_PLATFORM'] = args.platform

    def build_targets(targets):
        if targets:
            run([cmake, '--build', build, '--parallel', args.jobs, '--target', *sorted(set(targets))])

    if args.action == 'build':
        build_targets(args.target)
        return 0
    if args.action == 'run':
        target = args.target[0]
        if target == 'route_acceptance_fixture' and os.name != 'nt':
            parser.error('The route acceptance fixture uses Windows HKCU isolation; use route_service_tests on Linux')
        target = 'cmake_startup_smoke' if args.smoke and target == 'RayNmeaSimulator-isolated' else target
        build_targets([target])
        reply = build / '.cmake/api/v1/reply'
        index = json.loads(max(reply.glob('index-*.json'), key=lambda p: p.stat().st_mtime_ns).read_text())
        model = json.loads((reply / index['reply']['codemodel-v2']['jsonFile']).read_text())
        entry = next(t for t in model['configurations'][0]['targets'] if t['name'] == target)
        details = json.loads((reply / entry['jsonFile']).read_text())
        executable = build / details['artifacts'][0]['path']
        env['RAYNMEA_TEST_PLATFORM'] = args.platform or ('windows' if os.name == 'nt' else 'cocoa' if sys.platform == 'darwin' else 'xcb')
        launcher = [sys.executable, ROOT / 'scripts/run-cmake-test.py', '--name', target,
                    '--root', ROOT, '--build', build, '--qt-root', qt,
                    '--sdl-root', cache.get('SDL2_ROOT', '')]
        if not args.smoke:
            launcher.append('--interactive')
        run([*launcher, '--', executable, *(['--smoke'] if target == 'route_acceptance_fixture' and args.smoke else ['--no-nmea'])])
        return 0
    tests = json.loads(run([ctest, '--test-dir', build, '--show-only=json-v1'], capture=True))['tests']
    names = {test['name'] for test in tests}
    if args.suites and set(args.suites) - names:
        parser.error('Unknown CTest names: ' + ', '.join(sorted(set(args.suites) - names)))
    selected = []
    targets = []
    for test in tests:
        properties = {p['name']: p['value'] for p in test.get('properties', [])}
        labels = properties.get('LABELS', [])
        if args.suites and test['name'] not in args.suites:
            continue
        if args.match and not re.search(args.match, test['name']):
            continue
        if args.label and not any(re.search(args.label, label) for label in labels):
            continue
        selected.append(test['name'])
        required = properties.get('REQUIRED_FILES', [])
        targets.extend(Path(path).stem for path in required)
        if args.action == 'list':
            print(test['name'] + ' [' + ', '.join(labels) + ']')
    if not selected:
        parser.error('Selection matches no tests')
    if args.action == 'test':
        build_targets(targets)
        pattern = '^(' + '|'.join(re.escape(name) for name in selected) + ')$'
        run([ctest, '--test-dir', build, '-R', pattern, '--output-on-failure'])
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        print('CMake development command failed:', error, file=sys.stderr)
        sys.exit(1)
