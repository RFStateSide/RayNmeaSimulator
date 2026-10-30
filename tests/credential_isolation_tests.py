# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Linux integration checks for the disposable real Secret Service boundary."""
import argparse
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

parser = argparse.ArgumentParser()
parser.add_argument('--root', required=True, type=Path)
parser.add_argument('--build', required=True, type=Path)
args = parser.parse_args()
sys.path.insert(0, str(args.root / 'scripts'))
from test_secret_service import SecretServiceSession


class IsolationTests(unittest.TestCase):
    def verify_cleanup(self, record):
        self.assertTrue(record['owned_processes_stopped'])
        self.assertTrue(record['private_directory_removed'])
        for key in ('bus_pid', 'provider_pid'):
            if key in record:
                self.assertFalse(Path('/proc', str(record[key])).exists())

    def test_real_provider_and_environment(self):
        ambient = dict(os.environ, DBUS_SESSION_BUS_ADDRESS='unix:path=/do-not-use-owner-session',
                       GNOME_KEYRING_CONTROL='/do-not-use-owner-control')
        before = ambient.copy()
        with tempfile.TemporaryDirectory() as evidence:
            session = SecretServiceSession(ambient, evidence)
            try:
                env = session.start()
                self.assertEqual(ambient, before)
                self.assertNotEqual(env['DBUS_SESSION_BUS_ADDRESS'], ambient['DBUS_SESSION_BUS_ADDRESS'])
                private = Path(session.directory.name)
                self.assertEqual(private.stat().st_mode & 0o777, 0o700)
                for key in ('HOME', 'XDG_DATA_HOME', 'XDG_STATE_HOME', 'XDG_RUNTIME_DIR', 'GNOME_KEYRING_CONTROL'):
                    self.assertTrue(Path(env[key]).is_relative_to(private))
                self.assertTrue(list((private / 'data/keyrings').glob('*.keyring')))
                self.assertTrue(session.record['owner_pid_verified'])
                self.assertTrue(session.record['synthetic_create_read_replace_remove'])
            finally:
                session.close()
            self.verify_cleanup(session.record)

    def test_missing_prerequisite_fails_without_fallback(self):
        with tempfile.TemporaryDirectory() as evidence:
            session = SecretServiceSession(dict(os.environ, PATH=''), evidence)
            try:
                with self.assertRaisesRegex(RuntimeError, 'prerequisites missing'):
                    session.start()
                self.assertFalse(session.processes)
            finally:
                session.close()
            self.verify_cleanup(session.record)

    def test_provider_start_failure_cleans_bus(self):
        with tempfile.TemporaryDirectory() as evidence:
            session = SecretServiceSession(os.environ, evidence)
            import shutil
            which = shutil.which
            with patch('test_secret_service.shutil.which', side_effect=lambda name, **kw:
                       '/bin/false' if name == 'gnome-keyring-daemon' else which(name, **kw)):
                try:
                    with self.assertRaisesRegex(RuntimeError, 'exited before readiness'):
                        session.start()
                finally:
                    session.close()
            self.assertEqual(len(session.processes), 2)
            self.verify_cleanup(session.record)

    def test_launcher_exit_timeout_and_termination(self):
        # Actual launcher paths, not a mock cleanup implementation. Child has
        # only synthetic state, no network or native application profile.
        evidence_root = args.build / 'test-evidence'
        evidence_root.mkdir(exist_ok=True)
        for case in ('failure', 'timeout', 'termination'):
            with self.subTest(case=case), tempfile.TemporaryDirectory() as scratch:
                marker = Path(scratch) / 'ready'
                child = ('from pathlib import Path;import sys,time;Path(sys.argv[1]).touch();'
                         + ('sys.exit(7)' if case == 'failure' else 'time.sleep(20)'))
                previous = set(evidence_root.glob('credential-lifecycle-*'))
                command = [sys.executable, str(args.root / 'scripts/run-cmake-test.py'),
                           '--root', str(args.root), '--build', str(args.build), '--qt-root', str(args.build),
                           '--name', 'credential-lifecycle', '--credentials', '--timeout', '1' if case == 'timeout' else '30',
                           '--', sys.executable, '-c', child, str(marker)]
                process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                try:
                    if case == 'termination':
                        deadline = time.monotonic() + 15
                        while not marker.exists() and process.poll() is None and time.monotonic() < deadline:
                            time.sleep(0.05)
                        self.assertTrue(marker.exists(), 'Fixture child did not start')
                        process.send_signal(signal.SIGTERM)
                    output, _ = process.communicate(timeout=25)
                finally:
                    if process.poll() is None:
                        process.kill()
                        process.wait(timeout=3)
                self.assertEqual(process.returncode, {'failure': 7, 'timeout': 124, 'termination': 1}[case], output.decode())
                created = set(evidence_root.glob('credential-lifecycle-*')) - previous
                self.assertEqual(len(created), 1)
                record = json.loads((created.pop() / 'result.json').read_text())
                self.verify_cleanup(record['credential_isolation'])


unittest.main(argv=[sys.argv[0]], verbosity=2)
