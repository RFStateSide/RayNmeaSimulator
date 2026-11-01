# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
import importlib.util,json,tempfile,unittest,zipfile,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('update_metadata',ROOT/'deployment/update/metadata.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
class MetadataTests(unittest.TestCase):
 def test_actual_version_and_platform_receipts(self):
  for arch,os,variant,key,suffix in [('AMD64','Windows','native','windows-x86_64','Windows-x64.zip'),('x86_64','Linux','native','linux-x86_64','Linux-x86_64.tar.gz'),('aarch64','Linux','native','linux-arm64','Linux-aarch64.tar.gz'),('aarch64','Linux','Debian12','linux-arm64-debian12','Linux-aarch64-Debian12.tar.gz')]:
   with self.subTest(key=key),tempfile.TemporaryDirectory(dir=ROOT/'build') as d:
    p=Path(d)/f'RayNmeaSimulator-{module.version()}-{suffix}';p.write_bytes(b'private package fixture');metadata={'version':module.version(),'architecture':arch,'os':os,'package_variant':variant}
    if os=='Windows':
     with zipfile.ZipFile(p,'w') as archive:archive.writestr(f'RayNmeaSimulator-{module.version()}-Windows-x64-Setup.exe',b'unchanged installer fixture')
    if os=='Linux':metadata['required_abi']={'GLIBC':'2.36','GLIBCXX':'3.4.30','CXXABI':'1.3.13'}
    item=module.record(metadata,p);self.assertIn(key,item['downloads']);self.assertEqual(module.merge([item]),item);self.assertEqual(item['downloads'][key]['bytes'],p.stat().st_size)
    with self.assertRaises(ValueError):module.merge([item,item])
    metadata['version']='99.0.0'
    with self.assertRaises(ValueError):module.record(metadata,p)
 def test_windows_zip_receipt_rejects_renames_and_unrelated_contents(self):
  with tempfile.TemporaryDirectory(dir=ROOT/'build') as d:
   folder=Path(d);name=f'RayNmeaSimulator-{module.version()}-Windows-x64.zip';p=folder/name;meta={'version':module.version(),'architecture':'AMD64','os':'Windows','package_variant':'native'}
   p.write_bytes(b'not a ZIP')
   with self.assertRaises(ValueError):module.record(meta,p)
   for member in ['../installer.exe','folder/installer.exe','unrelated.exe']:
    with zipfile.ZipFile(p,'w') as archive:archive.writestr(member,b'installer fixture')
    with self.assertRaises(ValueError):module.record(meta,p)
   exe=f'RayNmeaSimulator-{module.version()}-Windows-x64-Setup.exe'
   with zipfile.ZipFile(p,'w') as archive:archive.writestr(exe,b'installer fixture');archive.writestr('extra.txt',b'unexpected')
   with self.assertRaises(ValueError):module.record(meta,p)
   with zipfile.ZipFile(p,'w') as archive:archive.writestr(exe,b'installer fixture')
   receipt=module.record(meta,p);entry=receipt['downloads']['windows-x86_64'];self.assertEqual(entry['sha256'],hashlib.sha256(p.read_bytes()).hexdigest());self.assertEqual(entry['name'],name)
   wrong=folder/exe;wrong.write_bytes(b'installer fixture')
   with self.assertRaises(ValueError):module.record(meta,wrong)
 def test_actual_repository_manifest(self):
  value=json.loads((ROOT/'deployment/update/update.json').read_text());merged=module.merge([value]);self.assertEqual(set(merged['downloads']),module.KEYS);self.assertEqual(merged['downloads'],value['downloads'])
 def test_hostile_repository_manifest_receipts(self):
  original=json.loads((ROOT/'deployment/update/update.json').read_text())
  for field,bad in [('schema',True),('repository','RFStateSide/RayNmeaSimulator-Archive')]:
   value=json.loads(json.dumps(original));value[field]=bad
   with self.assertRaises(ValueError):module.merge([value])
  value=json.loads(json.dumps(original));value['downloads']['linux-arm64-debian12']['required_abi']['GLIBC']='2.38'
  with self.assertRaises(ValueError):module.merge([value])
 def test_no_placeholder_receipts(self):
  with self.assertRaises(ValueError):module.merge([])
  with self.assertRaises(ValueError):module.merge([{'schema':1,'repository':'RFStateSide/RayNmeaSimulator-Archive','version':module.version(),'downloads':{}}])
 def test_debian12_rejects_newer_glibc(self):
  with tempfile.TemporaryDirectory(dir=ROOT/'build') as d:
   p=Path(d)/f'RayNmeaSimulator-{module.version()}-Linux-aarch64-Debian12.tar.gz';p.write_bytes(b'fixture');metadata={'version':module.version(),'architecture':'aarch64','os':'Linux','package_variant':'Debian12','required_abi':{'GLIBC':'2.38','GLIBCXX':'3.4.30','CXXABI':'1.3.13'}}
   with self.assertRaises(ValueError):module.record(metadata,p)
if __name__=='__main__':unittest.main(verbosity=2)
