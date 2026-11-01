# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Negative companion inventory/receipt/archive checks, without network or publication."""
import importlib.util,json,sys,tempfile,unittest,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('release_support',ROOT/'deployment/release_support.py');s=importlib.util.module_from_spec(spec);spec.loader.exec_module(s)
class SupportTests(unittest.TestCase):
 def test_inventory_rejects_tamper_and_redirected_name(self):
  with tempfile.TemporaryDirectory(dir=ROOT/'build') as d:
   root=Path(d);f=root/'source';f.write_bytes(b'actual');inv=root/'inventory.json'
   for name,checksum in [('safe','0'*64),('../outside',s.digest(f)),('bad'+chr(92)+'name',s.digest(f))]:
    inv.write_text(json.dumps({'assets':[{'name':name,'path':str(f),'bytes':6,'sha256':checksum}]}))
    with self.assertRaises(ValueError):s.inputs(inv)
 def test_companion_member_hash_and_path_security(self):
  with tempfile.TemporaryDirectory(dir=ROOT/'build') as d:
   for name,data,checksum in [('source.txt',b'preferred source','0'*64),('../outside',b'x','0'*64),('C:/outside',b'x','0'*64)]:
    p=Path(d)/'support.zip'
    with zipfile.ZipFile(p,'w') as z:
     z.writestr(name,data);z.writestr('contents.json',json.dumps({'files':[{'name':name,'bytes':len(data),'sha256':checksum}]}))
    with self.assertRaises(ValueError):s.verify(p)
 def test_real_private_companion_if_available(self):
  p=ROOT/'build/m264/repository-update/release-candidate'/s.COMPANION
  if not p.exists():self.skipTest('private companion not constructed yet')
  m=s.verify(p);self.assertEqual(m['original_artifacts_retained'],26);self.assertEqual(m['uploaded_asset_count_proposed'],5)
  self.assertEqual(len(m['current_downloads']),4)
  self.assertTrue(all(any(row['name']=='original-v0.1.0/'+name for row in m['files']) for name in s.BINARY_NAMES))
if __name__=='__main__':unittest.main()
