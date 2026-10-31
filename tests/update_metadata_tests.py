# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
import importlib.util,json,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('update_metadata',ROOT/'deployment/update/metadata.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
class MetadataTests(unittest.TestCase):
 def test_actual_version_and_platform_receipts(self):
  for arch,os,variant,key,suffix in [('AMD64','Windows','native','windows-x86_64','Windows-x64-Setup.exe'),('x86_64','Linux','native','linux-x86_64','Linux-x86_64.tar.gz'),('aarch64','Linux','native','linux-arm64','Linux-aarch64.tar.gz'),('aarch64','Linux','Debian12','linux-arm64-debian12','Linux-aarch64-Debian12.tar.gz')]:
   with self.subTest(key=key),tempfile.TemporaryDirectory(dir=ROOT/'build') as d:
    p=Path(d)/f'RayNmeaSimulator-{module.version()}-{suffix}';p.write_bytes(b'private package fixture');metadata={'version':module.version(),'architecture':arch,'os':os,'package_variant':variant}
    if os=='Linux':metadata['required_abi']={'GLIBC':'2.36','GLIBCXX':'3.4.30','CXXABI':'1.3.13'}
    item=module.record(metadata,p);self.assertIn(key,item['downloads']);self.assertEqual(module.merge([item]),item);self.assertEqual(item['downloads'][key]['bytes'],p.stat().st_size)
    with self.assertRaises(ValueError):module.merge([item,item])
    metadata['version']='99.0.0'
    with self.assertRaises(ValueError):module.record(metadata,p)
 def test_no_placeholder_receipts(self):
  with self.assertRaises(ValueError):module.merge([])
  with self.assertRaises(ValueError):module.merge([{'schema':1,'repository':'RFStateSide/RayNmeaSimulator-Archive','version':module.version(),'downloads':{}}])
 def test_debian12_rejects_newer_glibc(self):
  with tempfile.TemporaryDirectory(dir=ROOT/'build') as d:
   p=Path(d)/f'RayNmeaSimulator-{module.version()}-Linux-aarch64-Debian12.tar.gz';p.write_bytes(b'fixture');metadata={'version':module.version(),'architecture':'aarch64','os':'Linux','package_variant':'Debian12','required_abi':{'GLIBC':'2.38','GLIBCXX':'3.4.30','CXXABI':'1.3.13'}}
   with self.assertRaises(ValueError):module.record(metadata,p)
if __name__=='__main__':unittest.main(verbosity=2)
