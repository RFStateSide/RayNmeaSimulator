# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Production macOS settings isolation, without GUI, Keychain or normal-store reads."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

probe = sys.argv[1]
with tempfile.TemporaryDirectory(prefix='raynmea-native-profile-') as temporary:
    root = Path(temporary).resolve()
    def run(arguments, success=True):
        result = subprocess.run([probe, *map(str, arguments)], capture_output=True, text=True, timeout=10)
        assert (result.returncode == 0) == success, result.stderr
        return json.loads(result.stdout) if success else None

    a, b = root / 'one', root / 'two'
    one = run(['--no-nmea', '--isolated-profile', a, '--write'])
    assert one == {'file': str(a / 'settings/RFStateSide/RayNmeaSimulator.ini'),
                   'ini': True, 'fallbacks': False, 'value': 'private'}
    assert run(['--no-nmea', '--isolated-profile', a])['value'] == 'private'
    assert run(['--no-nmea', '--isolated-profile', b])['value'] == 'missing'
    missing = root / 'no-standalone'
    run(['--isolated-profile', missing], success=False)
    assert not missing.exists()
    run(['--no-nmea', '--isolated-profile'], success=False)
    run(['--no-nmea', '--isolated-profile', 'relative'], success=False)
    run(['--no-nmea', '--isolated-profile', Path.home()], success=False)
    (root / 'link').symlink_to(a, target_is_directory=True)
    run(['--no-nmea', '--isolated-profile', root / 'link'], success=False)
    outside = root / 'preserved.ini'
    outside.write_text('preserve this fixture')
    redirected = root / 'redirected/settings/RFStateSide/RayNmeaSimulator.ini'
    redirected.parent.mkdir(parents=True)
    redirected.symlink_to(outside)
    run(['--no-nmea', '--isolated-profile', root / 'redirected', '--write'], success=False)
    assert outside.read_text() == 'preserve this fixture'
print('PASS production INI persistence, cross-profile isolation, disabled fallback and fail-closed argument/path guards')
