# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Production profile persistence, unsafe-path rejection and credential backend exclusion."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

probe = sys.argv[1]
with tempfile.TemporaryDirectory(prefix='raynmea-deployment-profile-') as temporary:
    root = Path(temporary).resolve()
    def run(arguments, success=True):
        result = subprocess.run([probe, *map(str, arguments)], capture_output=True, text=True, timeout=10)
        assert (result.returncode == 0) == success, result.stderr
        return json.loads(result.stdout) if success else None
    a, b = root / 'one', root / 'two'
    one = run(['--no-nmea', '--isolated-profile', a, '--write'])
    assert Path(one.pop('file')) == a / 'settings/RFStateSide/RayNmeaSimulator.ini'
    assert one == {'ini': True, 'fallbacks': False, 'value': 'private', 'credentialsBlocked': True}
    assert run(['--no-nmea', '--isolated-profile', a])['value'] == 'private'
    assert run(['--no-nmea', '--isolated-profile', b])['value'] == 'missing'
    missing = root / 'no-standalone'
    for args in [['--isolated-profile', missing], ['--no-nmea', '--isolated-profile'],
                 ['--no-nmea', '--isolated-profile', 'relative'], ['--no-nmea', '--isolated-profile', Path.home()]]:
        run(args, success=False)
    assert not missing.exists()
    (root / 'link').symlink_to(a, target_is_directory=True)
    run(['--no-nmea', '--isolated-profile', root / 'link'], success=False)
    outside = root / 'preserved.ini'
    outside.write_text('preserve this fixture')
    redirected = root / 'redirected/settings/RFStateSide/RayNmeaSimulator.ini'
    redirected.parent.mkdir(parents=True)
    redirected.symlink_to(outside)
    run(['--no-nmea', '--isolated-profile', root / 'redirected', '--write'], success=False)
    assert outside.read_text() == 'preserve this fixture'
print('PASS persistence, cross-profile isolation, disabled fallback, fail-closed paths and credential exclusion')
