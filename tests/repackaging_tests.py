# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Verify packaging-only reconstruction and real private candidate/source invariants."""
import hashlib
import importlib.util
import io
import json
import shutil
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest
from unittest import mock
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('repackage_linux',ROOT/'deployment/repackage_linux.py')
r=importlib.util.module_from_spec(spec);spec.loader.exec_module(r)

class RepackagingTests(unittest.TestCase):
    def fixture(self,path,commit=r.APPLICATION_COMMIT,tamper=False):
        data={'bin/RayNmeaSimulator':b'\x7fELFapproved application',
              'build-metadata.json':r.canonical({'source_sha':commit}),
              'source-snapshot.json':r.canonical({'base_commit':commit,'clean_commit':True})}
        rows=[{'path':n,'sha256':r.sha(v)} for n,v in data.items()]
        data['manifest.json']=r.canonical(rows)
        if tamper:data['bin/RayNmeaSimulator']=b'tampered'
        with tarfile.open(path,'w:gz') as t:
            for name,value in data.items():r.add(t,'RayNmeaSimulator/'+name,value,0o755 if name.startswith('bin/') else 0o644)

    def test_valid_original_and_manifest_tamper(self):
        with tempfile.TemporaryDirectory() as d:
            f=Path(d)/'original.tar.gz';self.fixture(f);payload,rows,meta,source=r.read_package(f)
            self.assertEqual(payload['bin/RayNmeaSimulator'][0],b'\x7fELFapproved application')
            self.fixture(f,tamper=True)
            with self.assertRaisesRegex(ValueError,'manifest mismatch'):r.read_package(f)

    def test_wrong_application_revision(self):
        with tempfile.TemporaryDirectory() as d:
            f=Path(d)/'original.tar.gz';self.fixture(f,commit='f'*40)
            with self.assertRaisesRegex(ValueError,'approved clean application'):r.read_package(f)

    def test_unsafe_archive_and_duplicate_paths(self):
        for names in [['../outside'],['/absolute'],['same','same']]:
            buf=io.BytesIO()
            with tarfile.open(fileobj=buf,mode='w') as t:
                for name in names:r.add(t,name,b'fixture')
            buf.seek(0)
            with tarfile.open(fileobj=buf) as t:
                with self.assertRaisesRegex(ValueError,'Unsafe or duplicate'):list(r.checked_members(t))
        buf=io.BytesIO()
        with tarfile.open(fileobj=buf,mode='w') as t:
            item=tarfile.TarInfo('link');item.type=tarfile.SYMTYPE;item.linkname='../owner';t.addfile(item)
        buf.seek(0)
        with tarfile.open(fileobj=buf) as t:
            with self.assertRaises(ValueError):list(r.checked_members(t))

    def test_uncommitted_packaging_is_explicit_private_only(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'build') as d:
            root=Path(d);(root/'.git').mkdir()
            for name in r.OVERLAY:
                target=root/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(ROOT/name,target)
            with mock.patch.object(r,'ROOT',root),mock.patch.object(r.subprocess,'check_output',side_effect=['a'*40,' M packaging/linux/installer.py']):
                with self.assertRaisesRegex(ValueError,'private qualification only'):r.packaging_source(False)
            with mock.patch.object(r,'ROOT',root),mock.patch.object(r.subprocess,'check_output',side_effect=['a'*40,' M packaging/linux/installer.py']):
                record=r.packaging_source(True);self.assertIsNone(record['packaging_commit']);self.assertFalse(record['packaging_clean_commit'])
                self.assertEqual(record['application_commit'],r.APPLICATION_COMMIT)
                self.assertEqual(record['packaging_source_tree_sha256'],r.sha(r.canonical(record['files'])))

    def test_actual_private_candidates_and_complete_sources(self):
        candidates=ROOT/'build/m264/candidates'
        receipts=sorted(candidates.glob('*/RayNmeaSimulator-*-release-set.json'))
        if not receipts:self.skipTest('private candidate artifacts not present; fixture invariants still run')
        self.assertEqual(len(receipts),3)
        for receipt in receipts:
            record=json.loads(receipt.read_text());artifact=receipt.parent/record['installer']['name']
            self.assertEqual(r.digest(artifact),record['installer']['sha256'])
            payload,rows,meta,source=r.read_package(artifact)
            self.assertEqual(source['base_commit'],r.APPLICATION_COMMIT)
            self.assertEqual({n:r.sha(v[0]) for n,v in payload.items() if v[0].startswith(b'\x7fELF')},record['unchanged_elf_sha256'])
            kit=receipt.parent/record['corresponding_source']['name'];self.assertEqual(r.digest(kit),record['corresponding_source']['sha256'])
            with tarfile.open(kit) as t:
                app=json.load(t.extractfile('source-snapshot.json'))
                for file in app['files']:
                    member=t.getmember('application/'+file['path']);self.assertEqual(r.sha(t.extractfile(member).read()),file['sha256'])
                    self.assertEqual(member.mode,0o755 if file['mode']=='100755' else 0o644)
                self.assertEqual(json.load(t.extractfile('packaging-source.json')),record['packaging_source'])
                for file in record['packaging_source']['files']:
                    self.assertEqual(r.sha(t.extractfile('packaging-overlay/'+file['path']).read()),file['sha256'])
                self.assertEqual(json.load(t.extractfile('package-manifest.json')),rows)
                for key,item in json.load(t.extractfile('source-index.json')).items():
                    with t.extractfile(item['path']) as data:self.assertEqual(hashlib.file_digest(data,'sha256').hexdigest(),item['sha256'],key)

if __name__=='__main__':unittest.main()
