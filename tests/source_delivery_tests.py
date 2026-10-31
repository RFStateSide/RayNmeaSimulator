# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
import hashlib,importlib.util,json,sys,tarfile,tempfile,unittest,io
from unittest import mock
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'deployment'))
import source_delivery as sd
import acquire_sources as acquisition
class DeliveryTests(unittest.TestCase):
 def test_release_wording_is_durable(self):
  for name in ['README.md','docs/online-help/README.md','deployment/README.md','deployment/windows/README.md','deployment/linux/README.md','packaging/DISTRIBUTION.txt','deployment/windows/RayNmeaSimulator.iss']:
   text=(ROOT/name).read_text().lower()
   for phrase in ['not published yet','it is not published','source remains private','public downloads are not available yet','private acceptance candidate','private acceptance installer','public release no-go']:
    self.assertNotIn(phrase,text,name)
 def test_actual_qt_supplier_inventory(self):
  stage=ROOT/'build/m258/windows-release/deploy/RayNmeaSimulator';qt=Path('C:/Qt6.5/6.10.3/mingw_64')
  if not stage.exists() or not qt.exists():self.skipTest('optional existing Windows package inventory')
  modules=sd.qt_modules(stage,qt);self.assertIn('qtcharts',modules);self.assertIn('qtimageformats',modules);self.assertNotIn('qtmultimedia',modules)
 def test_snapshot_source_changes_and_path_rejection(self):
  with tempfile.TemporaryDirectory(dir=ROOT/'build') as d:
   root=Path(d);(root/'app.txt').write_text('source');record={'schema':1,'base_commit':'a'*40,'clean_commit':True,'files':[{'path':'app.txt','mode':'100644','sha256':sd.digest(root/'app.txt')}]};record['source_tree_sha256']=hashlib.sha256(sd.canonical(record['files'])).hexdigest();sd.verify_snapshot(record,root)
   (root/'app.txt').write_text('modified')
   with self.assertRaisesRegex(ValueError,'changed since compilation'):sd.verify_snapshot(record,root)
   record['files'][0]['path']='../outside'
   with self.assertRaisesRegex(ValueError,'Invalid source receipt'):sd.verify_snapshot(record,root)
 def test_git_archive_existing_tag_is_not_duplicated(self):
  pin='b'*40;data=io.BytesIO()
  with tarfile.open(fileobj=data,mode='w') as t:sd.add_bytes(t,'qtfixture/.tag',pin.encode());sd.add_bytes(t,'qtfixture/src.cpp',b'preferred source')
  def output(args,**kwargs):
   if 'archive' in args:return data.getvalue()
   if 'ls-tree' in args:return b'' if not kwargs.get('text') else ''
   value=pin if args[-1]=='FETCH_HEAD' else 'c'*40
   return value if kwargs.get('text') else value.encode()
  with tempfile.TemporaryDirectory(dir=ROOT/'build') as d,mock.patch.object(acquisition.subprocess,'run'),mock.patch.object(acquisition.subprocess,'check_output',side_effect=output):
   archive=acquisition.qt_commit_archive('qtfixture',pin,Path(d));self.assertEqual(sd.archive_tag(archive),pin)
   with tarfile.open(archive) as t:self.assertEqual(t.getnames().count('qtfixture/.tag'),1);self.assertIn('qtfixture/src.cpp',t.getnames())
 def test_qt_notice_mismatch_is_rejected(self):
  with tempfile.TemporaryDirectory(dir=ROOT/'build') as d:
   root=Path(d);archive=root/'qt.tar';notice=root/'stage/licenses/Qt/qtfixture/LICENSE';notice.parent.mkdir(parents=True);notice.write_bytes(b'wrong notice')
   with tarfile.open(archive,'w') as t:sd.add_bytes(t,'module/.tag',b'b'*40);sd.add_bytes(t,'module/LICENSE',b'full upstream notice\n')
   inputs={'Qt/qtfixture':{'path':str(archive)}}
   with self.assertRaisesRegex(ValueError,'notice differs'):sd.verify_qt_notices(root/'stage',inputs,['qtfixture'])
   notice.write_bytes(b'full upstream notice\r\n');sd.verify_qt_notices(root/'stage',inputs,['qtfixture'])
 def test_distribution_source_identity_and_completeness(self):
  with tempfile.TemporaryDirectory(dir=ROOT/'build') as d:
   p=Path(d)/'distro.tar';data=b'preferred original source';sha=hashlib.sha256(data).hexdigest();descriptor=('Source: sample\nVersion: 1.0-2\nChecksums-Sha256:\n '+sha+' '+str(len(data))+' sample.orig.tar\n').encode()
   with tarfile.open(p,'w') as t:sd.add_bytes(t,'sample.dsc',descriptor);sd.add_bytes(t,'sample.orig.tar',data)
   sd.verify_distribution_source(p,'sample 1.0-2')
   with self.assertRaisesRegex(ValueError,'identity mismatch'):sd.verify_distribution_source(p,'sample 1.0-1')
   with tarfile.open(p,'w') as t:sd.add_bytes(t,'sample.dsc',descriptor)
   with self.assertRaisesRegex(ValueError,'Incomplete'):sd.verify_distribution_source(p,'sample 1.0-2')
 def test_real_format_source_kit_and_negative_closure(self):
  previous=sd.ROOT
  with tempfile.TemporaryDirectory(dir=ROOT/'build') as d:
   root=Path(d);sd.ROOT=root
   try:
    (root/'build').mkdir();(root/'docs').mkdir();(root/'dependencies').mkdir();(root/'docs/source-delivery.md').write_text('Rebuild instructions');sd.write(root/'dependencies/pins.json',{})
    (root/'app.txt').write_text('source');files=[{'path':'app.txt','mode':'100644','sha256':sd.digest(root/'app.txt')}];record={'base_commit':'a'*40,'clean_commit':False,'files':files,'source_tree_sha256':hashlib.sha256(sd.canonical(files)).hexdigest()}
    stage=root/'build/stage';stage.mkdir();qt=root/'qt';(qt/'sbom').mkdir(parents=True);(qt/'mkspecs').mkdir()
    sd.write(stage/'source-snapshot.json',record);sd.write(stage/'build-metadata.json',{'version':'0.1.0','qt':'6.10.3'});(stage/'Qt6VirtualKeyboard.dll').write_bytes(b'fixture library')
    sd.write(qt/'sbom/qtvirtualkeyboard-6.10.3.spdx.json',{'packages':[{'name':'qtvirtualkeyboard','versionInfo':'6.10.3','downloadLocation':'git://code.qt.io/qt/qtvirtualkeyboard.git@'+'b'*40}],'files':[{'fileName':'./bin/Qt6VirtualKeyboard.dll','licenseConcluded':'GPL-3.0-only'}]})
    self.assertIn('qtvirtualkeyboard',sd.qt_modules(stage,qt))
    notice=stage/'licenses/Qt/qtvirtualkeyboard/LICENSE';notice.parent.mkdir(parents=True);notice.write_bytes(b'GPLv3')
    rows=[{'path':p.relative_to(stage).as_posix(),'sha256':sd.digest(p),'bytes':p.stat().st_size,'source':'https://code.qt.io/cgit/qt/qtvirtualkeyboard.git','version':'6.10.3','license':'GPL-3.0-only'} for p in stage.rglob('*') if p.is_file()];sd.write(stage/'manifest.json',rows);sd.write(stage/'redistribution.json',sd.inventory(stage,qt));rows.append({'path':'redistribution.json','sha256':sd.digest(stage/'redistribution.json'),'bytes':1,'source':'RFStateSide/RayNmeaSimulator','version':'0.1.0','license':'AGPL-3.0-or-later'});sd.write(stage/'manifest.json',rows)
    artifact=root/'build/installer.exe';artifact.write_bytes(b'fixture installer');sd.write(artifact.parent/'update-package.json',{'version':'0.1.0','downloads':{'windows-x86_64':{'name':artifact.name,'sha256':sd.digest(artifact)}}});catalog=root/'inputs.json';sd.write(catalog,{})
    with self.assertRaisesRegex(ValueError,'clean approved commit'):sd.assemble(stage,artifact,qt,catalog,root/'build/release')
    with self.assertRaisesRegex(ValueError,'Missing matching source'):sd.assemble(stage,artifact,qt,catalog,root/'build/no-source',True)
    archive=root/'qtvirtualkeyboard.tar.gz'
    with tarfile.open(archive,'w:gz') as t:sd.add_bytes(t,'module/.tag',b'b'*40);sd.add_bytes(t,'module/LICENSE',b'GPLv3');sd.add_bytes(t,'module/src/keyboard.cpp',b'preferred source')
    sd.write(catalog,{'Qt/qtvirtualkeyboard':{'path':str(archive),'sha256':sd.digest(archive),'origin':'https://download.qt.io/fixture','version':'6.10.3'}})
    a=sd.assemble(stage,artifact,qt,catalog,root/'build/a',True);b=sd.assemble(stage,artifact,qt,catalog,root/'build/b',True);self.assertEqual(a['corresponding_source']['sha256'],b['corresponding_source']['sha256'])
    with tarfile.open(root/'build/a'/a['corresponding_source']['name']) as t:self.assertIn('application/.raynmea-source.json',t.getnames());self.assertNotIn('application/.git/config',t.getnames())
    data=json.loads(catalog.read_text());data['Qt/qtvirtualkeyboard']['sha256']='0'*64;sd.write(catalog,data)
    with self.assertRaisesRegex(ValueError,'Source hash mismatch'):sd.assemble(stage,artifact,qt,catalog,root/'build/wrong',True)
   finally:sd.ROOT=previous
if __name__=='__main__':unittest.main(verbosity=2)
