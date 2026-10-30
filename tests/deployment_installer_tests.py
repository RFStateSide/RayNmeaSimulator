# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Exercise the actual packaged Linux installer in a disposable home, without launch."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

bundle = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='raynmea-install-tests-') as temporary:
    root = Path(temporary)
    prefix = root / 'program files/RayNmeaSimulator'
    env = {**os.environ, 'HOME': str(root), 'XDG_DATA_HOME': str(root / 'share')}
    user = root / '.config/RFStateSide/RayNmeaSimulator.ini'
    user.parent.mkdir(parents=True)
    user.write_text('synthetic preferred units, theme and geometry')
    before = user.read_bytes()
    def run(action, success=True):
        result = subprocess.run([sys.executable, str(bundle / 'installer.py'), action,
                                 '--prefix', str(prefix)], env=env, text=True, capture_output=True)
        assert (result.returncode == 0) == success, result.stderr
    run('install')
    assert (prefix / 'bin/RayNmeaSimulator').is_file()
    run('install')
    assert user.read_bytes() == before
    custom = prefix / 'owner.txt'
    custom.write_text('preserve unrecognized file')
    run('install', False)
    run('uninstall', False)
    assert custom.read_text() == 'preserve unrecognized file'
    custom.unlink()
    run('uninstall')
    assert not prefix.exists() and user.read_bytes() == before
    assert not list((root / 'share/applications').glob('*.desktop'))
    prefix.mkdir()
    (prefix / 'owner.txt').write_text('unmanaged')
    run('install', False)
    assert (prefix / 'owner.txt').read_text() == 'unmanaged'
print('PASS install, same-version upgrade, preservation, uninstall, modified/unmanaged prefix refusal')
