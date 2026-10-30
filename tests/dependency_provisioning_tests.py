# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Offline provisioning safety fixtures; no owner dependencies or network access."""
import copy
import hashlib
import importlib.util
import io
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import unittest

sys.dont_write_bytecode = True
APP = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('dependencies', APP / 'scripts/dependencies.py')
deps = importlib.util.module_from_spec(spec)
spec.loader.exec_module(deps)


class ProvisioningTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='raynmea-provision-test-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.pin = copy.deepcopy(deps.SDL2)
        self.calls = 0
        self.archive()

    def archive(self, extras=(), omit=()):
        data = io.BytesIO()
        prefix = self.pin['directory'] + '/' + self.pin['sdk'] + '/'
        names = ['include/SDL2/SDL.h', 'lib/libSDL2.dll.a', 'bin/SDL2.dll',
                 'lib/cmake/SDL2/SDL2Config.cmake', 'lib/cmake/SDL2/SDL2ConfigVersion.cmake']
        with tarfile.open(fileobj=data, mode='w:gz') as archive:
            for name in names:
                if name in omit:
                    continue
                payload = ('synthetic ' + name).encode()
                member = tarfile.TarInfo(prefix + name)
                member.size = len(payload)
                archive.addfile(member, io.BytesIO(payload))
            for name, kind in extras:
                member = tarfile.TarInfo(name)
                member.type = kind
                member.linkname = '../../outside'
                archive.addfile(member)
        self.payload = data.getvalue()
        self.pin['sha256'] = hashlib.sha256(self.payload).hexdigest()

    def opener(self, url, timeout):
        self.assertEqual(url, self.pin['url'])
        self.assertEqual(timeout, 30)
        self.calls += 1
        return io.BytesIO(self.payload)

    @property
    def sdk(self):
        return self.root / self.pin['directory'] / self.pin['sdk']

    def acquire(self):
        deps.acquire_sdl2(self.root, self.pin, self.opener)

    def test_acquisition_and_idempotent_verification(self):
        self.acquire()
        self.assertEqual((self.root / deps.SDL2_MARKER).read_text(), self.pin['sha256'])
        before = {str(p): p.read_bytes() for p in self.root.rglob('*') if p.is_file()}
        self.acquire()
        self.assertEqual(self.calls, 1)
        self.assertEqual(before, {str(p): p.read_bytes() for p in self.root.rglob('*') if p.is_file()})

    def test_conflicting_receipt_is_preserved(self):
        self.acquire()
        receipt = self.root / deps.SDL2_MARKER
        receipt.write_text('preserve unexpected receipt')
        with self.assertRaisesRegex(RuntimeError, 'receipt mismatch'):
            self.acquire()
        self.assertEqual(receipt.read_text(), 'preserve unexpected receipt')

    def test_wrong_download_is_not_published(self):
        self.pin['sha256'] = '0' * 64
        with self.assertRaisesRegex(RuntimeError, 'checksum'):
            self.acquire()
        self.assertFalse(self.sdk.exists())
        self.assertEqual(list((self.root / '.downloads').iterdir()), [])

    def test_corrupted_cache_is_not_replaced(self):
        cache = self.root / '.downloads'
        cache.mkdir()
        archive = cache / self.pin['archive']
        archive.write_bytes(b'preserve corrupt cache for inspection')
        with self.assertRaisesRegex(RuntimeError, 'checksum'):
            self.acquire()
        self.assertEqual(self.calls, 0)
        self.assertEqual(archive.read_bytes(), b'preserve corrupt cache for inspection')

    def test_modified_sdk_is_never_overwritten(self):
        self.acquire()
        header = self.sdk / 'include/SDL2/SDL.h'
        header.write_bytes(b'preserve local change')
        with self.assertRaisesRegex(RuntimeError, 'SDK differs'):
            self.acquire()
        self.assertEqual(header.read_bytes(), b'preserve local change')
        self.assertEqual(self.calls, 1)

    def test_unexpected_sdk_file_is_rejected(self):
        self.acquire()
        (self.sdk / 'extra.txt').write_text('not in pinned archive')
        with self.assertRaisesRegex(RuntimeError, 'SDK differs'):
            self.acquire()

    def test_missing_sdk_shape_is_not_published(self):
        self.archive(omit=('include/SDL2/SDL.h',))
        with self.assertRaisesRegex(RuntimeError, 'required MinGW'):
            self.acquire()
        self.assertFalse((self.root / self.pin['directory']).exists())

    def test_unsafe_members_are_rejected_before_extraction(self):
        for name, kind in [('SDL2-2.30.9/../../escape', tarfile.REGTYPE),
                           ('SDL2-2.30.9/link', tarfile.SYMTYPE),
                           ('SDL2-2.30.9/link', tarfile.LNKTYPE),
                           ('C:/escape', tarfile.REGTYPE),
                           ('SDL2-2.30.9\\escape', tarfile.REGTYPE)]:
            with self.subTest(name=name, kind=kind), tempfile.TemporaryDirectory() as temporary:
                self.archive(extras=((name, kind),))
                with self.assertRaisesRegex(RuntimeError, 'unsafe'):
                    deps.acquire_sdl2(Path(temporary), self.pin, self.opener)
                self.assertFalse((Path(temporary) / self.pin['directory']).exists())

    def test_extraction_size_limit(self):
        member = tarfile.TarInfo(self.pin['directory'] + '/large')
        member.size = deps.MAX_EXPANDED + 1
        class Archive:
            def getmembers(self): return [member]
        with self.assertRaisesRegex(RuntimeError, 'limits'):
            deps.sdl_members(Archive(), self.pin)

    def test_read_only_overrides_rejected_for_acquisition(self):
        result = subprocess.run([sys.executable, str(APP / 'scripts/dependencies.py'),
                                 'acquire', '--root', str(self.root), '--sdl2-root', str(self.sdk)],
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('overrides are read-only', result.stderr)
        self.assertFalse((self.root / deps.MARKER).exists())


if __name__ == '__main__':
    unittest.main()
