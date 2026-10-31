# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Build-independent regressions for current-result publication and ABI policy."""
import importlib.util
import json
import zipfile
import hashlib
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('deploy', ROOT / 'deployment/deploy.py')
deploy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(deploy)


class WorkflowTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='raynmea-deploy-test-')
        self.addCleanup(self.temporary.cleanup)
        self.build = Path(self.temporary.name).resolve()

    def complete(self, attempt):
        output = attempt.generation / 'package'
        stage = output / 'RayNmeaSimulator'
        stage.mkdir(parents=True)
        (stage / 'build-metadata.json').write_text('{"version":"0.1.0"}')
        (stage / 'application').write_text('synthetic application')
        artifact = output / ('test-Windows-x64.zip' if sys.platform == 'win32' else 'test.tar.gz')
        artifact.write_bytes(b'synthetic private package')
        Path(str(artifact) + '.sha256').write_text(deploy.package.digest(artifact) + '  ' + artifact.name)
        (output / 'update-package.json').write_text('{"schema":1,"private_fixture":true}')
        attempt.publish(output, artifact)

    def test_stable_outputs_and_matching_hash(self):
        attempt = deploy.Attempt(self.build)
        self.complete(attempt)
        state = json.loads(attempt.state.read_text())
        self.assertEqual(state['status'], 'succeeded')
        self.assertEqual(Path(state['artifact']).parent, self.build / 'deploy/installer')
        self.assertEqual(state['sha256'], deploy.package.digest(Path(state['artifact'])))
        self.assertTrue((self.build / 'deploy/RayNmeaSimulator/application').exists())
        self.assertTrue((self.build / 'deploy/installer/update-package.json').is_file())

    def test_failed_replacement_hides_old_success_and_retains_evidence(self):
        first = deploy.Attempt(self.build); self.complete(first)
        old = self.build / 'deploy/RayNmeaSimulator/application'
        old.write_text('preserve modified stage too')
        second = deploy.Attempt(self.build)
        second.failed(RuntimeError('simulated compiler failure'))
        state = json.loads(second.state.read_text())
        self.assertEqual(state['status'], 'failed')
        self.assertNotIn('artifact', state)
        self.assertFalse(old.exists())
        self.assertFalse((self.build / 'deploy/installer').exists())
        self.assertEqual((second.generation / 'previous/RayNmeaSimulator/application').read_text(), 'preserve modified stage too')
        self.assertTrue(first.generation.exists())

    def test_compile_failure_invalidates_outputs_before_command(self):
        self.complete(deploy.Attempt(self.build))
        config = {'CMAKE_HOME_DIRECTORY': str(ROOT), 'CMAKE_BUILD_TYPE': 'Release',
                  'CMAKE_GENERATOR': 'Ninja', 'Qt6_DIR': str(self.build / 'qt/lib/cmake/Qt6'),
                  'DEPS_ROOT': str(self.build / 'qualified-deps')}
        def fail(args, log):
            self.assertFalse((self.build / 'deploy/installer').exists())
            self.assertIn('-DRAYNMEA_DEPLOYMENT=ON', args)
            self.assertIn('-DBUILD_TESTING=OFF', args)
            self.assertIn('-DDEPS_ROOT=' + config['DEPS_ROOT'], args)
            self.assertIn(self.build / 'deployment/build', args)
            raise RuntimeError('compiler unavailable')
        with patch.object(deploy.package, 'cache', return_value=config), patch.object(deploy, 'command', side_effect=fail):
            with self.assertRaisesRegex(RuntimeError, 'compiler unavailable'):
                deploy.deploy(self.build, 'cmake')
        self.assertEqual(json.loads((self.build / 'deploy/deployment-result.json').read_text())['status'], 'failed')

    def test_unmanaged_output_preserved(self):
        owner = self.build / 'deploy/RayNmeaSimulator'
        owner.mkdir(parents=True); (owner / 'owner.txt').write_text('retain')
        with self.assertRaisesRegex(RuntimeError, 'Unmanaged'):
            deploy.Attempt(self.build)
        self.assertEqual((owner / 'owner.txt').read_text(), 'retain')

    def test_symlink_output_rejected(self):
        outside = self.build / 'elsewhere'; outside.mkdir()
        (self.build / 'deploy').symlink_to(outside, target_is_directory=True)
        with self.assertRaisesRegex(RuntimeError, 'Redirected'):
            deploy.Attempt(self.build)
        self.assertEqual(list(outside.iterdir()), [])

    def test_path_outside_build_rejected(self):
        with self.assertRaisesRegex(RuntimeError, 'outside'):
            deploy.checked(self.build.parent, self.build)

    def test_in_source_build_rejected_before_writes(self):
        with self.assertRaisesRegex(RuntimeError, 'shadow build'):
            deploy.deploy(ROOT, 'cmake')

    def test_debug_refused_without_stale_success(self):
        self.complete(deploy.Attempt(self.build))
        with patch.object(deploy.package, 'cache', return_value={'CMAKE_HOME_DIRECTORY': str(ROOT), 'CMAKE_BUILD_TYPE': 'Debug'}):
            with self.assertRaisesRegex(RuntimeError, 'requires Release'):
                deploy.deploy(self.build, 'cmake')
        self.assertFalse((self.build / 'deploy/installer').exists())
        self.assertEqual(json.loads((self.build / 'deploy/deployment-result.json').read_text())['status'], 'failed')

    def test_concurrent_deploy_refused(self):
        with deploy.locked(self.build):
            with self.assertRaises(OSError):
                with deploy.locked(self.build): self.fail('second deploy acquired lock')

    def test_debian12_qualified_policy(self):
        good = {'GLIBC': '2.35', 'GLIBCXX': '3.4.30', 'CXXABI': '1.3.13'}
        release = 'ID=debian\nVERSION_ID="12"\n'
        deploy.package.check_variant('Debian12', 'aarch64', release, good)
        for architecture, os_release, ceilings in [('x86_64', release, good), ('aarch64', 'ID=debian\nVERSION_ID=13', good),
                *[('aarch64', release, {**good, key: value}) for key, value in [('GLIBC', '2.36'), ('GLIBCXX', '3.4.32'), ('CXXABI', '1.3.15')]],
                ('aarch64', release, {})]:
            with self.subTest(architecture=architecture, release=os_release, ceilings=ceilings):
                with self.assertRaises(RuntimeError): deploy.package.check_variant('Debian12', architecture, os_release, ceilings)
        deploy.package.check_variant('native', 'x86_64', 'ID=ubuntu', {'GLIBC': '2.38'})

    def test_windows_zip_preserves_installer_and_only_publishes_container(self):
        installer = self.build / 'RayNmeaSimulator-0.1.0-Windows-x64-Setup.exe'
        original = b'MZ synthetic installer bytes with no installation side effects'
        installer.write_bytes(original)
        artifact = deploy.package.windows_zip(installer, self.build, '0.1.0')
        self.assertEqual(installer.read_bytes(), original)
        with zipfile.ZipFile(artifact) as archive:
            self.assertEqual(archive.namelist(), [installer.name])
            self.assertIsNone(archive.testzip())
            self.assertEqual(archive.read(installer.name), original)
        receipt = json.loads(artifact.with_name(artifact.name + '.contents.json').read_text())
        self.assertEqual(receipt['files'][0]['sha256'], hashlib.sha256(original).hexdigest())
        self.assertEqual(receipt['archive']['sha256'], deploy.package.digest(artifact))
        # Exercise actual stable-output publication with both EXE and ZIP present.
        attempt = deploy.Attempt(self.build)
        output = attempt.generation / 'package'; output.mkdir()
        stage = output / 'RayNmeaSimulator'; stage.mkdir()
        (stage / 'build-metadata.json').write_text('{"version":"0.1.0"}')
        (output / installer.name).write_bytes(original)
        packaged = deploy.package.windows_zip(output / installer.name, output, '0.1.0')
        packaged.with_name(packaged.name + '.sha256').write_text(deploy.package.digest(packaged))
        (output / 'update-package.json').write_text('{}')
        attempt.publish(output, packaged)
        selected = self.build / 'deploy/installer'
        self.assertTrue((selected / packaged.name).is_file())
        self.assertTrue((selected / (packaged.name + '.contents.json')).is_file())
        self.assertFalse((selected / installer.name).exists())

    def test_generated_inno_retains_installer_policy(self):
        text = deploy.package.inno_configuration('0.1.0', self.build / 'stage with spaces', self.build / 'installer')
        self.assertIn('#define AppVersion "0.1.0"', text)
        for line in ('AppId={{E3048447-B053-4C82-8A90-45C05B214BC7}', 'PrivilegesRequired=lowest',
                     'RestartApplications=no', 'UsePreviousAppDir=yes', 'Flags: unchecked'):
            self.assertIn(line, text)
        self.assertNotIn('[Run]', text)
        self.assertNotIn('[UninstallDelete]', text)

    def test_debian12_wrapper_mounts_only_output_writable(self):
        from argparse import Namespace
        spec = importlib.util.spec_from_file_location('baseline', ROOT / 'deployment/linux/package_debian12.py')
        baseline = importlib.util.module_from_spec(spec); spec.loader.exec_module(baseline)
        args = Namespace(**{name: self.build / name for name in ('rootfs', 'deps', 'qt_sources', 'cmake_tools', 'qt', 'build')})
        command = baseline.sandbox(args)
        self.assertIn('--unshare-all', command)
        self.assertIn('--clearenv', command)
        self.assertEqual(command.count('--bind'), 1)
        at = command.index('--bind')
        self.assertEqual(command[at + 1:at + 3], [str(args.build), '/work/output'])
        for name in ('rootfs', 'deps', 'qt_sources', 'cmake_tools', 'qt'):
            at = command.index(str(getattr(args, name)))
            self.assertEqual(command[at - 1], '--ro-bind')


if __name__ == '__main__': unittest.main()
