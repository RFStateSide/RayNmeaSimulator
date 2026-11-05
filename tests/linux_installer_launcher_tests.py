# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Actual installer transactions in disposable files; no live launcher or executable launch."""
import contextlib
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import platform
import stat
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
if sys.platform.startswith('linux'):
    sys.path.insert(0, str(ROOT / 'packaging/linux'))
    spec = importlib.util.spec_from_file_location('launcher_installer', ROOT / 'packaging/linux/installer.py')
    installer = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(installer)


@unittest.skipUnless(sys.platform.startswith('linux'), 'Linux installer filesystem semantics')
class LauncherTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='raynmea-launcher-tests-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / 'new-package'
        self.source.mkdir()
        (self.source / 'installer.py').write_text('fixture installer')
        (self.source / 'build-metadata.json').write_text(json.dumps({'architecture': platform.machine()}))
        (self.source / 'bin').mkdir()
        (self.source / 'bin/RayNmeaSimulator').write_bytes(b'new executable fixture; never run')
        (self.source / 'RayNmeaSimulator').write_text('new wrapper fixture; never run')
        (self.source / 'manifest.json').write_text(json.dumps([
            {'path': k, 'sha256': v} for k, v in installer.inventory(self.source).items()]))
        self.prefix = self.root / 'program files/RayNmeaSimulator'
        self.data = self.root / 'data'
        self.desktop = self.data / 'applications' / (installer.IDENTITY + '.desktop')
        self.desktop.parent.mkdir(parents=True)
        self.old_binary = self.root / 'historical development/RayNmeaSimulator'
        self.old_binary.parent.mkdir()
        self.old_binary.write_text('historical executable fixture; never run')
        self.old_binary.chmod(0o755)
        self.old = ('[Desktop Entry]\nType=Application\nName=RayNmeaSimulator\n'
                    f'Exec="{self.old_binary}"\nTryExec={self.old_binary}\n'
                    'StartupWMClass=RayNmeaSimulator\n').encode()
        self.user = self.root / 'settings'
        self.user.write_bytes(b'preserved settings/templates/layout/credential fixture')
        self.env = patch.dict(os.environ, {'HOME': str(self.root), 'XDG_DATA_HOME': str(self.data)})
        self.env.start()
        self.addCleanup(self.env.stop)

    def legacy(self):
        self.desktop.write_bytes(self.old)
        self.desktop.chmod(0o640)

    def run_install(self, *options, action='install'):
        with patch.object(installer, '__file__', str(self.source / 'installer.py')), \
             patch.object(sys, 'argv', ['installer.py', action, '--prefix', str(self.prefix), *options]), \
             patch.object(installer.runtime_prerequisites, 'ensure') as ensure, \
             contextlib.redirect_stdout(io.StringIO()):
            self.last_ensure = ensure
            installer.main()
            return ensure

    def preserved(self):
        self.assertEqual(self.old_binary.read_text(), 'historical executable fixture; never run')
        self.assertEqual(self.user.read_bytes(), b'preserved settings/templates/layout/credential fixture')

    def test_managed_upgrade_uninstall_and_idempotence(self):
        self.run_install()
        first = json.loads((self.prefix / installer.RECEIPT).read_text())
        self.run_install('--non-interactive')
        self.assertEqual(json.loads((self.prefix / installer.RECEIPT).read_text()), first)
        self.assertEqual(list(self.desktop.parent.glob('*.bak')), [])
        self.run_install(action='uninstall')
        self.assertFalse(self.prefix.exists())
        self.assertFalse(self.desktop.exists())
        self.preserved()

    def test_previous_installer_managed_launcher_upgrades_without_prompt(self):
        self.run_install()
        old_content = ('[Desktop Entry]\nType=Application\nName=RayNmeaSimulator\n'
                       f'Exec="{self.prefix}/RayNmeaSimulator"\nIcon={self.prefix}/RayNmeaSimulator.png\n'
                       'Terminal=false\nCategories=Utility;\n').encode()
        self.desktop.write_bytes(old_content)
        receipt_path = self.prefix / installer.RECEIPT
        receipt = json.loads(receipt_path.read_text())
        receipt['desktop_sha256'] = hashlib.sha256(old_content).hexdigest()
        receipt_path.write_text(json.dumps(receipt))
        with patch('builtins.input', side_effect=AssertionError('Managed upgrade must not prompt')):
            self.run_install('--non-interactive')
        self.assertIn('TryExec=', self.desktop.read_text())
        self.assertFalse(list(self.desktop.parent.glob('*.bak')))

    def test_explicit_approval_backup_mode_and_uninstall_preservation(self):
        self.legacy()
        unrelated = self.desktop.with_name('other.desktop'); unrelated.write_text('unrelated')
        self.run_install('--desktop-replacement', 'replace', '--non-interactive')
        backup, = self.desktop.parent.glob('*.bak')
        self.assertEqual(backup.read_bytes(), self.old)
        self.assertEqual(stat.S_IMODE(backup.stat().st_mode), 0o640)
        self.assertIn('Exec="' + str(self.prefix / 'RayNmeaSimulator') + '"', self.desktop.read_text())
        self.run_install()
        self.assertEqual(list(self.desktop.parent.glob('*.bak')), [backup])
        self.run_install(action='uninstall')
        self.assertEqual(backup.read_bytes(), self.old)
        self.assertEqual(unrelated.read_text(), 'unrelated')
        self.preserved()

    def test_interactive_yes(self):
        self.legacy()
        with installer_context(self.desktop) as reg, patch.object(sys.stdin, 'isatty', return_value=True), patch.object(sys.stdout, 'isatty', return_value=True), patch('builtins.input', return_value='yes') as prompt:
            reg.consent('ask', False)
            prompt.assert_called_once()
        self.run_install('--desktop-replacement', 'replace')
        self.assertTrue(self.prefix.exists())

    def test_interactive_no_and_eof(self):
        self.legacy()
        for answer in ['no', '', EOFError()]:
            with self.subTest(answer=repr(answer)), installer_context(self.desktop) as reg, \
                 patch.object(sys.stdin, 'isatty', return_value=True), patch.object(sys.stdout, 'isatty', return_value=True):
                with patch('builtins.input', side_effect=answer if isinstance(answer, Exception) else None, return_value=answer):
                    with self.assertRaisesRegex(RuntimeError, 'not approved'):
                        reg.consent('ask', False)
        self.assertEqual(self.desktop.read_bytes(), self.old)
        self.assertFalse(self.prefix.exists())

    def test_noninteractive_default_and_preserve_decline(self):
        self.legacy()
        for options in [(), ('--non-interactive',), ('--desktop-replacement', 'preserve')]:
            with self.subTest(options=options), self.assertRaisesRegex(RuntimeError, 'not approved'):
                self.run_install(*options)
            self.assertEqual(self.desktop.read_bytes(), self.old)
            self.assertFalse(self.prefix.exists())
            self.assertFalse(list(self.desktop.parent.glob('*.bak')))

    def test_decline_precedes_dependency_installation(self):
        self.legacy()
        with self.assertRaises(RuntimeError):
            self.run_install('--desktop-replacement', 'preserve', '--dependencies', 'install')
        self.last_ensure.assert_not_called()

    def test_unrelated_ambiguous_malformed_and_stale_refused_even_with_consent(self):
        for data in [self.old.replace(b'Name=RayNmeaSimulator', b'Name=Other'),
                     self.old.replace(b'RayNmeaSimulator"', b'Other"'),
                     self.old.replace(b'Type=Application', b'Type=Link'),
                     self.old + b'Name=RayNmeaSimulator\n', b'broken',
                     self.old + b'Actions=Unsafe;\n', self.old + b'\n[Desktop Action Unsafe]\nExec=/bin/true\n',
                     self.old.replace(b'StartupWMClass=RayNmeaSimulator', b'StartupWMClass=Other'),
                     self.old.replace(str(self.old_binary).encode(), b'/no/such/RayNmeaSimulator'),
                     self.old.replace(b'Exec="', b'Exec=env "', 1), self.old.replace(b'Exec="', b'Exec=sh "', 1)]:
            with self.subTest(data=data):
                self.desktop.write_bytes(data)
                with self.assertRaisesRegex(RuntimeError, 'cannot be safely identified'):
                    self.run_install('--desktop-replacement', 'replace')
                self.assertEqual(self.desktop.read_bytes(), data)
                self.assertFalse(self.prefix.exists())
                self.assertFalse(list(self.desktop.parent.glob('*.bak')))

    def test_modified_managed_entry_preserved(self):
        self.run_install()
        self.desktop.write_text('owner edited managed launcher')
        for action in ['install', 'uninstall']:
            with self.assertRaisesRegex(RuntimeError, 'Desktop registration changed'):
                self.run_install('--desktop-replacement', 'replace', action=action)
        self.assertEqual(self.desktop.read_text(), 'owner edited managed launcher')
        self.assertTrue(self.prefix.exists())

    def test_file_symlink_hardlink_fifo_directory_refused(self):
        for kind in ['symlink', 'hardlink', 'fifo', 'directory']:
            with self.subTest(kind=kind):
                target = self.root / 'target'; target.write_bytes(self.old)
                if kind == 'symlink': self.desktop.symlink_to(target)
                if kind == 'hardlink': os.link(target, self.desktop)
                if kind == 'fifo': os.mkfifo(self.desktop)
                if kind == 'directory': self.desktop.mkdir()
                with self.assertRaises((OSError, RuntimeError)):
                    self.run_install('--desktop-replacement', 'replace')
                self.assertEqual(target.read_bytes(), self.old)
                self.assertFalse(self.prefix.exists())
                if kind == 'directory': self.desktop.rmdir()
                else: self.desktop.unlink()

    def test_ancestor_symlink_and_writable_directory_refused(self):
        self.desktop.parent.rmdir(); self.data.rmdir()
        elsewhere = self.root / 'elsewhere'; elsewhere.mkdir()
        self.data.symlink_to(elsewhere, target_is_directory=True)
        with self.assertRaises(OSError): self.run_install()
        self.assertEqual(list(elsewhere.iterdir()), [])
        self.data.unlink(); self.desktop.parent.mkdir(parents=True)
        self.desktop.parent.chmod(0o777)
        with self.assertRaisesRegex(RuntimeError, 'not writable by others'): self.run_install()

    def test_relative_traversal_and_empty_xdg(self):
        for name in ['relative', str(self.data / '../escape')]:
            with patch.dict(os.environ, {'XDG_DATA_HOME': name}), self.assertRaises(RuntimeError):
                self.run_install()
        with patch.dict(os.environ, {'XDG_DATA_HOME': ''}):
            self.assertEqual(installer.desktop_target(), self.root / '.local/share/applications' / self.desktop.name)

    def test_world_writable_launcher_refused(self):
        self.legacy(); self.desktop.chmod(0o666)
        with self.assertRaises(RuntimeError): self.run_install('--desktop-replacement', 'replace')
        self.assertEqual(self.desktop.read_bytes(), self.old)

    def test_executable_symlink_and_unsafe_mode_refused(self):
        self.legacy(); self.old_binary.chmod(0o777)
        with self.assertRaises(RuntimeError): self.run_install('--desktop-replacement', 'replace')
        self.old_binary.unlink(); self.old_binary.symlink_to('/bin/true')
        with self.assertRaises(RuntimeError): self.run_install('--desktop-replacement', 'replace')

    def test_backup_collision_does_not_overwrite(self):
        self.legacy()
        backup = self.desktop.with_name(self.desktop.name + '.fixed.bak'); backup.write_text('preserve prior backup')
        with patch.object(installer.uuid, 'uuid4') as value:
            value.return_value.hex = 'fixed'
            with self.assertRaises(FileExistsError): self.run_install('--desktop-replacement', 'replace')
        self.assertEqual(backup.read_text(), 'preserve prior backup')
        self.assertEqual(self.desktop.read_bytes(), self.old)
        self.assertFalse(self.prefix.exists())

    def test_replacement_failure_restores_original_and_application(self):
        self.legacy()
        with patch.object(installer.os, 'replace', side_effect=OSError('injected atomic replace failure')):
            with self.assertRaisesRegex(RuntimeError, 'original launcher/application state restored'):
                self.run_install('--desktop-replacement', 'replace')
        self.assertEqual(self.desktop.read_bytes(), self.old)
        self.assertEqual(stat.S_IMODE(self.desktop.stat().st_mode), 0o640)
        self.assertFalse(self.prefix.exists())
        self.assertEqual(next(self.desktop.parent.glob('*.bak')).read_bytes(), self.old)
        self.preserved()

    def test_post_publication_failure_rolls_back(self):
        self.legacy()
        publish = installer.DesktopRegistration.publish
        def fail(reg, content):
            publish(reg, content)
            raise OSError('injected after publication')
        with patch.object(installer.DesktopRegistration, 'publish', fail):
            with self.assertRaisesRegex(RuntimeError, 'state restored'):
                self.run_install('--desktop-replacement', 'replace')
        self.assertEqual(self.desktop.read_bytes(), self.old)
        self.assertEqual(stat.S_IMODE(self.desktop.stat().st_mode), 0o640)
        self.assertFalse(self.prefix.exists())

    def test_managed_upgrade_failure_restores_old_installation(self):
        self.run_install(); before = installer.inventory(self.prefix); desktop = self.desktop.read_bytes()
        with patch.object(installer.os, 'replace', side_effect=OSError('injected')):
            with self.assertRaisesRegex(RuntimeError, 'state restored'): self.run_install()
        self.assertEqual(installer.inventory(self.prefix), before)
        self.assertEqual(self.desktop.read_bytes(), desktop)
        installer.check_installed(self.prefix)

    def test_concurrent_edit_is_preserved_and_rollback_failure_is_reported(self):
        self.legacy(); copy = installer.shutil.copytree
        def edit(*args, **kwargs):
            result = copy(*args, **kwargs)
            self.desktop.write_text('concurrent owner edit')
            return result
        with patch.object(installer.shutil, 'copytree', edit):
            with self.assertRaisesRegex(RuntimeError, 'launcher rollback failed'):
                self.run_install('--desktop-replacement', 'replace')
        self.assertEqual(self.desktop.read_text(), 'concurrent owner edit')
        self.assertFalse(self.prefix.exists())
        self.assertEqual(next(self.desktop.parent.glob('*.bak')).read_bytes(), self.old)

    def test_extended_acl_refused(self):
        self.legacy()
        actual = installer.os.listxattr
        def attributes(fd):
            if stat.S_ISREG(os.fstat(fd).st_mode):
                return ['system.posix_acl_access']
            return actual(fd)
        with patch.object(installer.os, 'listxattr', attributes):
            with self.assertRaisesRegex(RuntimeError, 'permissions/ACL'):
                self.run_install('--desktop-replacement', 'replace')
        self.assertEqual(self.desktop.read_bytes(), self.old)

    def test_unexpected_owner_refused(self):
        self.legacy()
        with patch.object(installer.os, 'geteuid', return_value=os.geteuid() + 10000):
            with self.assertRaisesRegex(RuntimeError, 'unexpected owner'):
                self.run_install('--desktop-replacement', 'replace')
        self.assertEqual(self.desktop.read_bytes(), self.old)

    def test_sync_failure_after_atomic_publication_restores_original(self):
        self.legacy()
        sync = installer.os.fsync
        calls = 0
        def fail_once(fd):
            nonlocal calls
            calls += 1
            # Backup file + directory, then new desktop file + directory.
            if calls == 4:
                raise OSError('injected directory sync failure')
            return sync(fd)
        with patch.object(installer.os, 'fsync', fail_once):
            with self.assertRaisesRegex(RuntimeError, 'state restored'):
                self.run_install('--desktop-replacement', 'replace')
        self.assertEqual(self.desktop.read_bytes(), self.old)
        self.assertEqual(stat.S_IMODE(self.desktop.stat().st_mode), 0o640)
        self.assertFalse(self.prefix.exists())

    def test_rollback_failure_keeps_backup_and_reports_failure(self):
        self.legacy()
        publish = installer.DesktopRegistration.publish
        def fail(reg, content):
            publish(reg, content)
            raise OSError('injected after publication')
        with patch.object(installer.DesktopRegistration, 'publish', fail), patch.object(installer.DesktopRegistration, 'rollback', side_effect=OSError('injected rollback failure')):
            with self.assertRaisesRegex(RuntimeError, 'launcher rollback failed.*preserved launcher backup'):
                self.run_install('--desktop-replacement', 'replace')
        self.assertEqual(next(self.desktop.parent.glob('*.bak')).read_bytes(), self.old)
        self.assertFalse(self.prefix.exists())

    def test_directory_swap_cannot_redirect_publication(self):
        self.legacy()
        outside = self.root / 'outside'; outside.mkdir()
        marker = outside / self.desktop.name; marker.write_text('unrelated outside launcher')
        with installer_context(self.desktop) as reg:
            moved = self.data / 'preserved-applications'
            self.desktop.parent.rename(moved)
            self.desktop.parent.symlink_to(outside, target_is_directory=True)
            with self.assertRaises(OSError):
                reg.publish(installer.launcher_content(self.prefix))
        self.assertEqual(marker.read_text(), 'unrelated outside launcher')
        self.assertEqual((moved / self.desktop.name).read_bytes(), self.old)

    def test_shared_group_write_is_not_private_group_write(self):
        self.legacy(); self.desktop.chmod(0o660)
        with installer_context(self.desktop) as reg, patch.object(installer.grp, 'getgrgid') as group:
            group.return_value.gr_mem = ['another-user']
            self.assertFalse(installer.private_writable(reg.original[1]))

    def test_gio_launch_treats_special_path_characters_literally(self):
        try:
            from gi.repository import GioUnix
            desktop_class = GioUnix.DesktopAppInfo
        except ImportError:
            try:
                from gi.repository import Gio
                desktop_class = Gio.DesktopAppInfo
            except ImportError:
                self.skipTest('Optional system GIO bindings unavailable')
        prefix = self.root / 'Space $cash`tick"quote\\slash'
        prefix.mkdir()
        binary = prefix / 'RayNmeaSimulator'
        marker = self.root / 'literal-launch.txt'
        binary.write_text('#!/usr/bin/python3\nfrom pathlib import Path\nPath(' + repr(str(marker)) + ").write_text('launched')\n")
        binary.chmod(0o755)
        self.desktop.write_bytes(installer.launcher_content(prefix))
        app = desktop_class.new_from_filename(str(self.desktop))
        self.assertIsNotNone(app)
        self.assertTrue(app.launch([], None))
        for _ in range(40):
            if marker.exists(): break
            time.sleep(0.05)
        self.assertEqual(marker.read_text(), 'launched')

    def test_safe_exec_escaping(self):
        text = installer.launcher_content(Path('/tmp/Space $cash`tick"quote\\slash')).decode()
        self.assertIn('Exec="/tmp/Space \\\\$cash\\\\`tick\\\\"quote\\\\\\\\slash/RayNmeaSimulator"', text)
        for name in ['/tmp/bad%field', '/tmp/bad\nline', '/tmp/bad=value']:
            with self.assertRaises(RuntimeError): installer.launcher_content(Path(name))


@contextlib.contextmanager
def installer_context(path):
    reg = installer.DesktopRegistration(path)
    try: yield reg
    finally: reg.close()


if __name__ == '__main__':
    unittest.main()
