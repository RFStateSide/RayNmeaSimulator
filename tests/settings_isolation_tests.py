# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Subprocess regression: no native settings values are read or written."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile



def settings_constructor_violations(data, test_source=False):
    """Reject native-store bypasses, retaining explicit temporary INI fixtures."""
    import re
    temporary = re.compile(rb'QTemporaryFile\s+(\w+)\s*;.{0,600}?(QSettings\s+\w+\s*\(\s*\1\.fileName\(\)\s*,\s*QSettings::IniFormat\s*\)\s*;)', re.S)
    for match in reversed(list(temporary.finditer(data))):
        start, end = match.span(2)
        data = data[:start] + b' ' * (end-start) + data[end:]
    if test_source:
        # Existing Template tests resolve settings in a method-local temporary
        # directory. Accept a literal INI basename, never arbitrary/native paths.
        constructors = re.compile(rb'QSettings\s+\w+\s*\(\s*(\w+)\.filePath\("[A-Za-z0-9_-]+\.ini"\)\s*,\s*QSettings::IniFormat\s*\)\s*;')
        for match in reversed(list(constructors.finditer(data))):
            prefix = data[:match.start()]
            methods = list(re.finditer(rb'\bvoid\s+\w+\s*\([^)]*\)\s*\{', prefix))
            scope = prefix[methods[-1].end():] if methods else b''
            if re.search(rb'\bQTemporaryDir\s+' + re.escape(match[1]) + rb'\s*[;({]', scope):
                data = data[:match.start()] + b' ' * (match.end()-match.start()) + data[match.end():]
        # The route picker deliberately tests Qt's own native dialog history.
        # Its process-scoped HKCU override / Unix XDG isolation predates CMake.
        native_guards = (b'IsolatedUserRegistry registry;', b'RegOverridePredefKey(HKEY_CURRENT_USER,hive)',
                         b'RegOverridePredefKey(HKEY_CURRENT_USER,nullptr)', b'qputenv("XDG_CONFIG_HOME"')
        if all(marker in data for marker in native_guards):
            data = re.sub(rb'QSettings\s+qt\(QSettings::UserScope,"QtProject"\);', b'/* isolated Qt dialog fixture */', data)
    return re.search(rb'\bQSettings\s*(?:\w+\s*)?[({]', data)


def check_source_guard():
    valid = b'QTemporaryFile settingsFile; if(!settingsFile.open())return false; QSettings fixture(settingsFile.fileName(),QSettings::IniFormat);'
    assert not settings_constructor_violations(valid)
    directory = b'void check(){QTemporaryDir dir; QSettings fixture(dir.filePath("safe.ini"),QSettings::IniFormat);}'
    assert not settings_constructor_violations(directory, test_source=True)
    assert settings_constructor_violations(directory)
    assert settings_constructor_violations(directory.replace(b'QTemporaryDir', b'QDir'), test_source=True)
    native = b'QSettings qt(QSettings::UserScope,"QtProject");'
    assert settings_constructor_violations(native, test_source=True)
    isolated = native + b' IsolatedUserRegistry registry; RegOverridePredefKey(HKEY_CURRENT_USER,hive); RegOverridePredefKey(HKEY_CURRENT_USER,nullptr); qputenv("XDG_CONFIG_HOME",path);'
    assert not settings_constructor_violations(isolated, test_source=True)
    assert settings_constructor_violations(isolated.replace(b'RegOverridePredefKey(HKEY_CURRENT_USER,hive)', b'missing'), test_source=True)
    for invalid in [b'QSettings s("Org","App");', b'QSettings("Org","App");', b'QSettings fixture(path,QSettings::IniFormat);', valid+b'QSettings bad("Org","App");', b'QTemporaryFile f; QSettings bad("Org","App"); QSettings good(f.fileName(),QSettings::IniFormat);']:
        assert settings_constructor_violations(invalid), invalid


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--isolated', required=True, type=Path)
    parser.add_argument('--production', required=True, type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='raynmea-settings-') as directory:
        root = Path(directory)
        def run(profile, value=None, production=False, valid=True):
            env = os.environ.copy()
            env.pop('RAYNMEA_TEST_HOME', None)
            if profile is not None:
                env['RAYNMEA_TEST_HOME'] = str(profile)
            command = [str(args.production if production else args.isolated)]
            if value is not None:
                command.append(value)
            result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=30)
            if not valid:
                assert result.returncode != 0, 'Invalid profile silently accepted'
                return
            assert result.returncode == 0, result.stderr
            return json.loads(result.stdout)

        for profile in (None, '', 'relative-profile', Path.home()):
            run(profile, valid=False)
        blocked = root/'blocked'
        blocked.write_text('not a directory')
        run(blocked, valid=False)
        invalid_ini = root/'invalid-ini/settings/RFStateSide/RayNmeaSimulator.ini'
        invalid_ini.mkdir(parents=True)
        run(root/'invalid-ini', valid=False)
        first, second = root/'one', root/'two'
        a = run(first, 'first')
        assert a['ini'] and not a['fallbacks']
        assert Path(a['file']).resolve() == first/'settings/RFStateSide/RayNmeaSimulator.ini'
        assert run(first)['value'] == 'first', 'Same-profile process did not restore settings'
        assert run(second)['value'] == 'missing', 'Profiles leaked settings'
        run(second, 'second')
        assert run(first)['value'] == 'first'
        assert run(second)['value'] == 'second'
        for profile in (None, 'invalid-relative-profile', first):
            normal = run(profile, production=True)
            assert not normal['ini'] and normal['fallbacks'] and normal['value'] == 'not-read'
        print('PASS: explicit INI location, disabled fallback, same-profile persistence, distinct-process isolation')
        print('PASS: missing/relative/home/blocked profiles fail closed; production metadata unchanged')

    check_source_guard()
    # Ensure newly added application paths cannot accidentally bypass the wrapper.
    source = Path(__file__).resolve().parents[1]
    for folder in ('src', 'tests'):
        for path in (source/folder).rglob('*.cpp'):
            if path.name == 'settings_probe.cpp':
                continue
            assert not settings_constructor_violations(path.read_bytes(), test_source=folder == 'tests'), path
    print('PASS: native settings guard and explicit isolated-fixture exceptions')


if __name__ == '__main__':
    main()
