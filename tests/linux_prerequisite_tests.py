# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Loader, consent and package-plan regressions; never run a real package manager."""
import contextlib
import importlib.util
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('runtime_prerequisites', ROOT/'packaging/linux/runtime_prerequisites.py')
p = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p)
CURSOR = 'libxcb-cursor.so.0'

class PrerequisiteTests(unittest.TestCase):
    def info(self, name='linuxmint', version='22.3'):
        return {'ID': name, 'VERSION_ID': version}

    def test_supported_debian_ubuntu_mint_providers(self):
        for distro,version,t64 in [('debian','12',False),('debian','13',True),('ubuntu','24.04',True),('linuxmint','22.3',True)]:
            info=self.info(distro,version)
            self.assertTrue(p.supported(info));self.assertEqual(p.provider(CURSOR,info),'libxcb-cursor0')
            self.assertEqual(p.provider('libglib-2.0.so.0',info),'libglib2.0-0'+('t64' if t64 else ''))
            self.assertEqual(p.provider('libssl.so.3',info),'libssl3'+('t64' if t64 else ''))
            self.assertEqual(p.provider('libasound.so.2',info),'libasound2'+('t64' if t64 else ''))
            self.assertEqual(p.provider('libpng16.so.16',info),'libpng16-16'+('t64' if t64 else ''))
        self.assertFalse(p.supported(self.info('fedora','43')))
        for name in ['libc.so.6','libstdc++.so.6','libQt6Gui.so.6']:
            self.assertIsNone(p.provider(name,self.info()))

    def test_plugin_closure_not_only_main_executable(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d)
            for name in p.TARGETS:
                f=root/name;f.parent.mkdir(parents=True,exist_ok=True);f.write_bytes(b'fixture')
            def run(cmd,**kwargs):
                output=CURSOR+' => not found\n' if cmd[-1].endswith('libqxcb.so') else ''
                return subprocess.CompletedProcess(cmd,0,output,'')
            with mock.patch.object(p,'run',side_effect=run) as runner,mock.patch.object(p.shutil,'which',return_value='/usr/bin/ldd'):
                self.assertEqual(p.missing_libraries(root),[CURSOR])
                self.assertTrue(any(str(root/'plugins/platforms/libqxcb.so') in c.args[0] for c in runner.call_args_list))

    def test_abi_failure_does_not_offer_os_upgrade(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);f=root/p.TARGETS[0];f.parent.mkdir();f.write_bytes(b'fixture')
            with mock.patch.object(p,'run',return_value=subprocess.CompletedProcess([],1,'',"version `GLIBCXX_3.4.32' not found")),mock.patch.object(p.shutil,'which',return_value='/usr/bin/ldd'):
                with self.assertRaisesRegex(RuntimeError,'do not replace system glibc or Qt'):p.missing_libraries(root)

    def test_safe_plan_and_forbidden_plans(self):
        command=['/usr/bin/apt-get','install','libxcb-cursor0']
        for output,valid in [('Inst libxcb-cursor0 (0.1.4 Ubuntu:24.04 [amd64])\n',True),('Remv owner-package [1.0]\n',False),('Inst libfoo [1.0] (2.0)\n',False),('Inst libc6 (2.39)\n',False),('Inst libqt6gui6 (6.4.2)\n',False)]:
            with mock.patch.object(p,'run',return_value=subprocess.CompletedProcess([],0,output,'')):
                if valid:self.assertEqual(p.checked_plan(command),['libxcb-cursor0'])
                else:
                    with self.assertRaises(RuntimeError):p.checked_plan(command)

    def test_offline_plan_has_actionable_failure(self):
        with mock.patch.object(p,'run',return_value=subprocess.CompletedProcess([],100,'','Network unavailable')):
            with self.assertRaisesRegex(RuntimeError,'offline'):p.checked_plan(['/usr/bin/apt-get','install','libxcb-cursor0'])

    def fixture(self, missing=None):
        stack=contextlib.ExitStack()
        stack.enter_context(mock.patch.object(p,'missing_libraries',side_effect=missing or [[CURSOR],[]]))
        stack.enter_context(mock.patch.object(p,'os_info',return_value=self.info()))
        stack.enter_context(mock.patch.object(p.shutil,'which',side_effect=lambda name,**kw:'/usr/bin/'+name))
        stack.enter_context(contextlib.redirect_stdout(io.StringIO()))
        return stack

    def test_default_noninteractive_and_check_never_elevate(self):
        for policy in ['ask','check']:
            with self.fixture(),mock.patch.object(p,'run') as runner:
                with self.assertRaisesRegex(RuntimeError,'No packages installed'):p.ensure(Path('/fixture'),policy,True)
                runner.assert_not_called()

    def test_declined_interactive_install_never_elevates(self):
        with self.fixture(),mock.patch.object(p,'checked_plan',return_value=['libxcb-cursor0']),mock.patch.object(p.sys.stdin,'isatty',return_value=True),mock.patch.object(p.sys.stdout,'isatty',return_value=True),mock.patch('builtins.input',return_value=''),mock.patch.object(p,'run') as runner:
            with self.assertRaisesRegex(RuntimeError,'declined'):p.ensure(Path('/fixture'))
            runner.assert_not_called()

    def test_explicit_noninteractive_consent_and_recheck(self):
        with self.fixture(),mock.patch.object(p,'checked_plan',return_value=['libxcb-cursor0']),mock.patch.object(p.os,'geteuid',return_value=1000,create=True),mock.patch.object(p,'run',return_value=subprocess.CompletedProcess([],0)) as runner:
            p.ensure(Path('/fixture'),'install',True)
            self.assertEqual(runner.call_args.args[0][:4],['/usr/bin/sudo','-n','/usr/bin/apt-get','-y'])
            self.assertIn('--no-remove',runner.call_args.args[0]);self.assertIn('--no-upgrade',runner.call_args.args[0])

    def test_failed_install_and_unresolved_after_install(self):
        for rc,missing,pattern in [(100,[[CURSOR]],'failed'),(0,[[CURSOR],[CURSOR]],'remain unavailable')]:
            with self.fixture(missing),mock.patch.object(p,'checked_plan',return_value=['libxcb-cursor0']),mock.patch.object(p.os,'geteuid',return_value=0,create=True),mock.patch.object(p,'run',return_value=subprocess.CompletedProcess([],rc)):
                with self.assertRaisesRegex(RuntimeError,pattern):p.ensure(Path('/fixture'),'install',True)

    def test_unmapped_or_unsupported_does_not_run_apt(self):
        for missing,info in [(['libQt6Gui.so.6'],self.info()),([CURSOR],self.info('fedora','43'))]:
            with self.fixture([missing]),mock.patch.object(p,'os_info',return_value=info),mock.patch.object(p,'run') as runner:
                with self.assertRaisesRegex(RuntimeError,'not supported'):p.ensure(Path('/fixture'),'install',True)
                runner.assert_not_called()

    @unittest.skipUnless(sys.platform.startswith('linux'), 'real Linux installer entry point')
    def test_help_does_not_modify_extracted_package(self):
        import shutil
        with tempfile.TemporaryDirectory() as d:
            root=Path(d)
            for name in ['installer.py','runtime_prerequisites.py']:
                shutil.copy2(ROOT/'packaging/linux'/name,root/name)
            env=dict(p.os.environ);env.pop('PYTHONDONTWRITEBYTECODE',None)
            result=subprocess.run([sys.executable,str(root/'installer.py'),'--help'],env=env,capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertEqual({x.name for x in root.iterdir()},{'installer.py','runtime_prerequisites.py'})

    def test_sanitized_environment(self):
        with mock.patch.dict(p.os.environ,{'LD_LIBRARY_PATH':'/developer','QT_PLUGIN_PATH':'/sdk','APT_CONFIG':'/owner/config'}):
            env=p.environment();self.assertEqual(env['PATH'],p.PATH)
            self.assertNotIn('LD_LIBRARY_PATH',env);self.assertNotIn('QT_PLUGIN_PATH',env);self.assertNotIn('APT_CONFIG',env)

if __name__=='__main__':unittest.main()
