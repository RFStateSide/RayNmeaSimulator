# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Offline native-stage rejection tests: no compiler, download or owner SDK writes."""
import importlib.util
import io
import json
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch

sys.dont_write_bytecode = True
APP = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(APP / 'scripts'))
spec = importlib.util.spec_from_file_location('sdl_macos', APP / 'scripts/sdl2-macos.py')
sdl = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdl)


class NativeStageTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='raynmea-native-sdl-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        pin = dict(sdl.PIN)
        payload = b'untouched upstream source'
        with tarfile.open(self.root / pin['archive'], 'w:gz') as archive:
            member = tarfile.TarInfo(pin['directory'] + '/source.c')
            member.size = len(payload)
            archive.addfile(member, io.BytesIO(payload))
        pin['sha256'] = sdl.sha256(self.root / pin['archive'])
        self.addCleanup(patch.stopall)
        patch.object(sdl, 'PIN', pin).start()
        self.lipo = patch.object(sdl.subprocess, 'check_output', return_value='arm64\n').start()
        source = self.root / pin['directory']
        source.mkdir()
        (source / 'source.c').write_bytes(payload)
        files = ['include/SDL2/SDL.h', 'lib/libSDL2.a', 'lib/cmake/SDL2/SDL2Config.cmake',
                 'lib/cmake/SDL2/SDL2ConfigVersion.cmake']
        for name in files:
            path = self.root / 'install' / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(name)
        self.receipt = {'pin': pin, 'architecture': 'arm64',
                        'installed_sha256': sdl.inventory(self.root / 'install')}
        (self.root / sdl.RECEIPT).write_text(json.dumps(self.receipt))

    def test_valid_stage_is_read_only(self):
        before = sdl.inventory(self.root)
        sdl.verify(self.root)
        self.assertEqual(before, sdl.inventory(self.root))

    def test_corrupt_archive(self):
        (self.root / sdl.PIN['archive']).write_bytes(b'corrupt')
        with self.assertRaisesRegex(RuntimeError, 'checksum'):
            sdl.verify(self.root)

    def test_modified_source(self):
        (self.root / sdl.PIN['directory'] / 'source.c').write_text('modified')
        with self.assertRaisesRegex(RuntimeError, 'source differs'):
            sdl.verify(self.root)

    def test_modified_install(self):
        (self.root / 'install/lib/libSDL2.a').write_text('modified')
        with self.assertRaisesRegex(RuntimeError, 'SDK differs'):
            sdl.verify(self.root)

    def test_unexpected_install_file(self):
        (self.root / 'install/extra').write_text('unexpected')
        with self.assertRaisesRegex(RuntimeError, 'SDK differs'):
            sdl.verify(self.root)

    def test_wrong_architecture_even_with_valid_hashes(self):
        self.lipo.return_value = 'x86_64 arm64\n'
        with self.assertRaisesRegex(RuntimeError, 'only arm64'):
            sdl.verify(self.root)

    def test_wrong_receipt_identity(self):
        self.receipt['architecture'] = 'x86_64'
        (self.root / sdl.RECEIPT).write_text(json.dumps(self.receipt))
        with self.assertRaisesRegex(RuntimeError, 'identity/architecture'):
            sdl.verify(self.root)

    @unittest.skipIf(sys.platform == 'win32', 'Windows symlink creation may require additional privileges')
    def test_symlink_install_root(self):
        (self.root / 'install').rename(self.root / 'elsewhere')
        (self.root / 'install').symlink_to(self.root / 'elsewhere', target_is_directory=True)
        with self.assertRaisesRegex(RuntimeError, 'real directory'):
            sdl.verify(self.root)


if __name__ == '__main__':
    unittest.main()
