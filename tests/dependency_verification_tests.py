# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Exercise pin/patch rejection against disposable clones, never historical repositories."""
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
APP = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('dependencies', APP / 'scripts/dependencies.py')
deps = importlib.util.module_from_spec(spec)
spec.loader.exec_module(deps)


class DependencyVerificationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='raynmea-pin-test-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.repo = self.root / 'NMEA0183'
        subprocess.run(['git', '-c', 'core.autocrlf=false', 'clone', '--no-hardlinks',
                        str(Path(os.environ.get('RAYNMEA_TEST_DEPS_ROOT', str(APP / '.dependencies'))) / 'NMEA0183'), str(self.repo)],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        deps.git(self.repo, 'apply', str(APP / 'dependencies' / deps.PINS['NMEA0183']['patch']))

    def test_approved_patch(self):
        deps.verify('NMEA0183', self.repo)

    def test_wrong_revision(self):
        # Remove the approved overlay in this disposable clone before switching revisions.
        deps.git(self.repo, 'apply', '--reverse', str(APP / 'dependencies' / deps.PINS['NMEA0183']['patch']))
        deps.git(self.repo, 'checkout', '--detach', 'HEAD~1')
        with self.assertRaisesRegex(RuntimeError, 'revision mismatch'):
            deps.verify('NMEA0183', self.repo)

    def test_staged_change(self):
        path = self.repo / 'NMEA0183.h'
        path.write_bytes(path.read_bytes() + b'\n// unintended change\n')
        deps.git(self.repo, 'add', 'NMEA0183.h')
        with self.assertRaisesRegex(RuntimeError, 'source differs'):
            deps.verify('NMEA0183', self.repo)

    def test_extra_source(self):
        (self.repo / 'extra.h').write_text('// unapproved source')
        with self.assertRaisesRegex(RuntimeError, 'untracked'):
            deps.verify('NMEA0183', self.repo)

    def test_missing_checkout(self):
        with self.assertRaisesRegex(RuntimeError, 'missing Git checkout'):
            deps.verify('NMEA0183', self.root / 'missing')

    def test_refuses_unmanaged_acquisition_directory(self):
        result = subprocess.run([sys.executable, str(APP / 'scripts/dependencies.py'),
                                 'acquire', '--root', str(self.root)], capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Refusing a nonempty directory', result.stderr)
        self.assertFalse((self.root / deps.MARKER).exists())


if __name__ == '__main__':
    unittest.main()
