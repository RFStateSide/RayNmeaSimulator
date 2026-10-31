# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Offline source delivery and package traceability. Does not publish or authorize release."""
import argparse
import csv
import gzip
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import tarfile
import zipfile
import tempfile
import importlib.util
ROOT=Path(__file__).resolve().parents[1]

def digest(path):
    with Path(path).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def canonical(value):return json.dumps(value,sort_keys=True,separators=(',',':')).encode()
def write(path,value):Path(path).write_bytes(json.dumps(value,indent=2,sort_keys=True).encode()+b'\n')
def git(root,*args):return subprocess.check_output(['git','-C',str(root),*args])
def physical(path):
    p=Path(path).absolute()
    if p!=p.resolve():raise ValueError('Redirected path rejected')
    return p

def snapshot(root=ROOT):
    root=Path(root);private=root/'.raynmea-source.json';modes={}
    if private.exists():
        record=json.loads(private.read_text());names=record['source_files'];head=record['source_sha'];clean=False
        modes=record.get('source_modes',{})
    else:
        head=git(root,'rev-parse','HEAD').decode().strip();clean=not git(root,'status','--porcelain').strip()
        names=git(root,'ls-files','-z').decode().split('\0');names=[n for n in names if n]
        for line in git(root,'ls-files','-s').decode().splitlines():info,name=line.split('\t',1);modes[name]=info.split()[0]
        # Only explicitly provenance-registered additions; never sweep owner files.
        with (root/'docs/provenance-inventory.csv').open(encoding='utf-8-sig',newline='') as f:
            names+= [r['path'] for r in csv.DictReader(f) if (root/r['path']).is_file() and r['disposition']!='removed from current tree']
    files=[]
    for n in sorted(set(names)):
        p=root/n
        if n.startswith(('build/','.git/','.qtcreator/','.dependencies/')) or '..' in PurePosixPath(n).parts or p.is_symlink() or not p.is_file():raise ValueError('Invalid application source path: '+n)
        files.append({'path':n,'sha256':digest(p),'mode':modes.get(n,'100755' if n in ['packaging/linux/install.sh','packaging/linux/uninstall.sh','packaging/linux/installer.py'] else '100644')})
    if private.exists():clean=bool(record.get('source_clean_commit')) and hashlib.sha256(canonical(files)).hexdigest()==record.get('source_tree_sha256')
    result={'schema':1,'repository':'RFStateSide/RayNmeaSimulator','base_commit':head,'clean_commit':clean,'files':files}
    result['source_tree_sha256']=hashlib.sha256(canonical(files)).hexdigest();return result

def qt_modules(stage,qt):
    # Supplier file records, not a fixed list of developer-requested Qt modules.
    records={}
    for p in sorted((Path(qt)/'sbom').glob('*.spdx.json')):
        d=json.loads(p.read_text());module=p.name.split('-6.')[0]
        if not re.fullmatch('qt[a-z0-9]+',module):continue
        supplier=next((v for v in d.get('packages',[]) if v.get('name')==module),{})
        origin=supplier.get('downloadLocation','');pin=origin.rsplit('@',1)[-1] if '@' in origin else ''
        supplier_version=supplier.get('versionInfo','')
        tag=Path(qt).parent/'Src'/module/'.tag'
        evidence='supplier SPDX source commit' if pin else 'supplier release version and matching source component'
        if not pin and tag.exists():pin=tag.read_text().strip()
        for f in d.get('files',[]):
            name=PurePosixPath(f['fileName']).name;records[name]={'module':module,'revision':pin,'sbom':p.name,'license':f.get('licenseConcluded','NOASSERTION'),'supplier_version':supplier_version,'revision_evidence':evidence}
            if '.so.' in name:records[name.split('.so.')[0]+'.so.6']=records[name]
    found={}
    for p in Path(stage).rglob('*'):
        if not p.is_file():continue
        rel=p.relative_to(stage).as_posix();candidate=p.name.startswith(('Qt6','libQt6')) or rel.startswith(('plugins/','platforms/','imageformats/','iconengines/','tls/','styles/','position/','generic/','networkinformation/'))
        if not candidate or not (p.suffix=='.dll' or '.so' in p.name):continue
        if p.name not in records:raise ValueError('Missing supplier SPDX identity for '+rel)
        r=records[p.name]
        if not re.fullmatch('[a-f0-9]{40}',r['revision']) and not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+',r['supplier_version']):raise ValueError('Missing supplier source identity for '+rel)
        found[r['module']]={k:v for k,v in r.items() if k!='license'}
    return found

def inventory(stage,qt):
    stage=Path(stage);meta=json.loads((stage/'build-metadata.json').read_text());modules=qt_modules(stage,qt);groups={}
    for r in json.loads((stage/'manifest.json').read_text()):
        if r['path'].startswith('licenses/') or not (r['path'].endswith('.dll') or '.so' in Path(r['path']).name):continue
        key=(r['source'],r['version'],r['license']);g=groups.setdefault(key,{'source':key[0],'version':key[1],'upstream_license_expression':key[2],'files':[]});g['files'].append({'path':r['path'],'sha256':r['sha256']})
    return {'schema':1,'application_version':meta['version'],'source_snapshot_sha256':meta.get('source_snapshot_sha256'),'qt_version':meta['qt'],'installer_components':meta.get('installer_components',[]),'qt_modules':modules,'components':list(groups.values()),'marine_pins':json.loads((ROOT/'dependencies/pins.json').read_text()),'system_prerequisites':json.loads((stage/'system-runtime.json').read_text()) if (stage/'system-runtime.json').exists() else {},'license_selection':'Qt module code uses the applicable GPLv3 option; embedded third-party terms remain separate. Application AGPLv3 and GPLv3 combination retains each license under section 13.','linux_tls_requirement':'OpenSSL 3 libssl/libcrypto supplied by the target system when the OpenSSL TLS backend is used; no claim of universal system-library compatibility.' if meta.get('required_abi') else None}

def verify_snapshot(record,root):
    seen=set()
    for r in record['files']:
        name=PurePosixPath(r['path'])
        if name.is_absolute() or '..' in name.parts or str(name) in seen or r['mode'] not in ('100644','100755') or str(name).startswith(('.git/','.qtcreator/','.dependencies/','build/')):raise ValueError('Invalid source receipt path')
        seen.add(str(name));p=Path(root)/r['path']
        if p.absolute()!=p.resolve() or not p.resolve().is_relative_to(Path(root).resolve()):raise ValueError('Redirected source receipt path')
        if not p.is_file() or p.is_symlink() or digest(p)!=r['sha256']:raise ValueError('Source changed since compilation: '+r['path'])
    if hashlib.sha256(canonical(record['files'])).hexdigest()!=record['source_tree_sha256']:raise ValueError('Invalid source receipt')

def add_bytes(tar,name,data,mode=0o644):
    info=tarfile.TarInfo(name);info.size=len(data);info.mode=mode;info.mtime=0;info.uid=info.gid=0;info.uname=info.gname='';tar.addfile(info,io.BytesIO(data))
def archive_tag(path):
    with tarfile.open(path,'r:*') as t:
        matches=[m for m in t.getmembers() if PurePosixPath(m.name).name=='.tag' and len(PurePosixPath(m.name).parts)==2]
        if len(matches)!=1:raise ValueError('Qt source archive has no unique supplier .tag')
        return t.extractfile(matches[0]).read().decode().strip()

def verify_qt_notices(stage,inputs,modules):
    for module in modules:
        notices=Path(stage)/'licenses/Qt'/module
        files=[p for p in notices.rglob('*') if p.is_file()]
        if not files:raise ValueError('Missing deployed Qt notices: '+module)
        with tarfile.open(inputs['Qt/'+module]['path']) as t:
            roots={PurePosixPath(m.name).parts[0] for m in t.getmembers() if PurePosixPath(m.name).name=='.tag' and len(PurePosixPath(m.name).parts)==2}
            if len(roots)!=1:raise ValueError('Ambiguous Qt notice source')
            prefix=next(iter(roots))+'/'
            for p in files:
                name=prefix+p.relative_to(notices).as_posix()
                try:data=t.extractfile(name).read()
                except (KeyError,AttributeError):raise ValueError('Qt notice absent from matching source: '+name)
                if data.replace(b'\r\n',b'\n')!=p.read_bytes().replace(b'\r\n',b'\n'):raise ValueError('Qt notice differs from matching source: '+name)

def verify_distribution_source(path,identity):
    package,version=identity.split(' ',1)
    with tarfile.open(path) as t:
        members=t.getmembers();names=[m.name for m in members]
        if len(names)!=len(set(names)) or any(not m.isfile() or PurePosixPath(m.name).name!=m.name for m in members):raise ValueError('Invalid distribution source archive')
        descriptors=[n for n in names if n.endswith('.dsc')]
        if len(descriptors)!=1:raise ValueError('One distribution source descriptor required')
        text=t.extractfile(descriptors[0]).read().decode()
        if not re.search(r'^Source: '+re.escape(package)+r'$',text,re.M) or not re.search(r'^Version: '+re.escape(version)+r'$',text,re.M):raise ValueError('Distribution source identity mismatch')
        match=re.search(r'Checksums-Sha256:\n((?: [^\n]+\n)+)',text)
        if not match:raise ValueError('Missing distribution source checksums')
        expected={descriptors[0]}
        for line in match[1].splitlines():
            sha,size,name=line.split();expected.add(name)
            if name not in names:raise ValueError('Incomplete distribution source')
            data=t.extractfile(name).read()
            if hashlib.sha256(data).hexdigest()!=sha or len(data)!=int(size):raise ValueError('Distribution source checksum mismatch')
        if set(names)!=expected:raise ValueError('Unlisted distribution source files')

def verify_marine_sources(inputs,pins):
    if not pins:return
    spec=importlib.util.spec_from_file_location('source_marine_verifier',ROOT/'scripts/dependencies.py');v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
    with tempfile.TemporaryDirectory(prefix='source-verify-',dir=ROOT/'build') as tmp:
        for name in pins:
            dest=Path(tmp)/name;dest.mkdir()
            with tarfile.open(inputs['Marine/'+name]['path']) as t:
                for m in t.getmembers():
                    path=PurePosixPath(m.name)
                    if not m.isfile() or path.is_absolute() or '..' in path.parts or not path.parts or path.parts[0]!='dependency':raise ValueError('Unsafe marine source archive')
                    target=dest/m.name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(t.extractfile(m).read())
            config=(dest/'dependency/.git/config').read_text()
            for line in config.splitlines():
                text=line.strip()
                if text and text!='[core]' and text.split('=')[0].strip() not in ['repositoryformatversion','bare','filemode']:raise ValueError('Unexpected Git source configuration')
            v.verify(name,dest/'dependency')

def assemble(stage,artifact,qt,sources,output,allow_dirty=False):
    stage=physical(stage);artifact=physical(artifact);output=physical(output)
    if not output.is_relative_to(ROOT/'build') or output.exists():raise ValueError('Use a fresh physical build/ output directory')
    meta=json.loads((stage/'build-metadata.json').read_text());record=json.loads((stage/'source-snapshot.json').read_text());verify_snapshot(record,ROOT)
    if not record['clean_commit'] and not allow_dirty:raise ValueError('Release source must be a clean approved commit; --allow-dirty is for local qualification only')
    listed=json.loads((stage/'manifest.json').read_text());actual={p.relative_to(stage).as_posix():digest(p) for p in stage.rglob('*') if p.is_file() and p.name!='manifest.json'}
    if actual!={r['path']:r['sha256'] for r in listed}:raise ValueError('Staged manifest mismatch')
    receipt=json.loads((artifact.parent/'update-package.json').read_text());entry=next(iter(receipt['downloads'].values()))
    if entry['name']!=artifact.name or entry['sha256']!=digest(artifact) or receipt['version']!=meta['version']:raise ValueError('Artifact/update receipt mismatch')
    container=None
    if artifact.suffix=='.zip':
        container=json.loads(artifact.with_name(artifact.name+'.contents.json').read_text())
        expected=f'RayNmeaSimulator-{meta["version"]}-Windows-x64-Setup.exe'
        with zipfile.ZipFile(artifact) as archive:
            if archive.namelist()!=[expected] or archive.testzip() is not None:raise ValueError('Unexpected Windows ZIP contents')
            embedded=archive.read(expected)
        measured={'schema':1,'archive':{'name':artifact.name,'bytes':artifact.stat().st_size,'sha256':digest(artifact)},'files':[{'name':expected,'bytes':len(embedded),'sha256':hashlib.sha256(embedded).hexdigest()}]}
        if container!=measured:raise ValueError('Windows ZIP/installer receipt mismatch')
    inv=inventory(stage,qt)
    if canonical(inv)!=canonical(json.loads((stage/'redistribution.json').read_text())):raise ValueError('Supplier/runtime inventory changed since packaging')
    if meta.get('source_snapshot_sha256') and meta['source_snapshot_sha256']!=digest(stage/'source-snapshot.json'):raise ValueError('Build/source receipt mismatch')
    inputs=json.loads(Path(sources).read_text());needed={'Qt/'+m for m in inv['qt_modules']}
    if inv['installer_components']:needed.add('Installer/InnoSetup')
    if 'qtdeclarative' in inv['qt_modules']:needed|={'Qt/qtshadertools','Qt/qtlanguageserver','Qt/qttools'}
    for c in inv['components']:
        if c['source'].startswith(('https://code.qt.io','RFStateSide/')) or c['files'][0]['path'].startswith(('Qt6','libgcc','libstdc++')):continue
        needed.add('Runtime/'+c['source']+'@'+c['version'])
    # Marine source snapshots are explicit inputs; validated by the unchanged verifier.
    needed|={'Marine/'+n for n in inv['marine_pins']}
    if (ROOT/'resources/nmea2000/lookups.json').is_file():needed.add('Data/CANboat')
    missing=sorted(needed-set(inputs))
    if missing:raise ValueError('Missing matching source materials: '+', '.join(missing))
    for key in needed:
        item=inputs[key];p=physical(item['path'])
        if not p.is_file() or digest(p)!=item['sha256']:raise ValueError('Source hash mismatch: '+key)
        if not tarfile.is_tarfile(p):raise ValueError('Complete source archive required: '+key)
        if key.startswith('Qt/'):
            expected=inv['qt_modules'].get(key.split('/')[1],{}).get('revision')
            if expected and archive_tag(p)!=expected:raise ValueError('Source/SDK commit mismatch: '+key)
        if not item.get('origin') or not item.get('version'):raise ValueError('Missing source identity: '+key)
        if key=='Installer/InnoSetup' and item['version']!=inv['installer_components'][0]['version']:raise ValueError('Installer source version mismatch')
        if key.startswith('Qt/') and item['version']!=inv['qt_version']:raise ValueError('Qt source version mismatch: '+key)
        if key.startswith('Runtime/'):
            if item['version']!=key.rsplit('@',1)[1]:raise ValueError('Runtime source version mismatch: '+key)
            identity=key[len('Runtime/'):].rsplit('@',1)[0]
            if re.fullmatch(r'[a-z0-9+.-]+ [^ ]+',identity):
                if item.get('distribution_source')!=identity:raise ValueError('Exact distribution source required: '+key)
                verify_distribution_source(p,identity)
        if key=='Data/CANboat':
            expected=json.loads((ROOT/'resources/nmea2000/lookups.json').read_text())['provenance']
            with tarfile.open(p) as t:data=t.extractfile('canboat.json').read()
            if hashlib.sha256(data).hexdigest()!=expected['sha256'] or item.get('revision')!=expected['revision']:raise ValueError('CANboat source identity mismatch')
        if key.startswith('Marine/') and item['version']!=inv['marine_pins'][key.split('/')[1]]['revision']:raise ValueError('Marine source revision mismatch: '+key)
    verify_qt_notices(stage,inputs,inv['qt_modules'])
    verify_marine_sources(inputs,inv['marine_pins'])
    output.mkdir(parents=True);platform=next(iter(receipt['downloads']));name=f'RayNmeaSimulator-{meta["version"]}-{platform}-Source.tar.gz';bundle=output/name
    source_index={k:{**inputs[k],'path':'sources/'+k.split('/')[0]+'/'+Path(inputs[k]['path']).name} for k in sorted(needed)}
    with bundle.open('wb') as raw,gzip.GzipFile(filename='',fileobj=raw,mode='wb',mtime=0) as gz,tarfile.open(fileobj=gz,mode='w|') as tar:
        for r in record['files']:add_bytes(tar,'application/'+r['path'],(ROOT/r['path']).read_bytes(),0o755 if r['mode']=='100755' else 0o644)
        descriptor={'source_sha':record['base_commit'],'source_files':[r['path'] for r in record['files']],'source_modes':{r['path']:r['mode'] for r in record['files']},'source_tree_sha256':record['source_tree_sha256'],'source_clean_commit':record['clean_commit']}
        add_bytes(tar,'application/.raynmea-source.json',canonical(descriptor))
        add_bytes(tar,'build-metadata.json',(stage/'build-metadata.json').read_bytes())
        add_bytes(tar,'package-manifest.json',(stage/'manifest.json').read_bytes())
        add_bytes(tar,'update-package.json',(artifact.parent/'update-package.json').read_bytes())
        if container:add_bytes(tar,'container-contents.json',canonical(container))
        if (stage/'system-runtime.json').exists():add_bytes(tar,'system-runtime.json',(stage/'system-runtime.json').read_bytes())
        add_bytes(tar,'source-snapshot.json',canonical(record));add_bytes(tar,'source-index.json',canonical(source_index));add_bytes(tar,'redistribution.json',canonical(inv))
        add_bytes(tar,'REBUILD.md',(ROOT/'docs/source-delivery.md').read_bytes())
        for key in sorted(needed):add_bytes(tar,source_index[key]['path'],Path(inputs[key]['path']).read_bytes())
        for p in sorted((stage/'licenses').rglob('*')):
            if p.is_file():add_bytes(tar,'notices/'+p.relative_to(stage/'licenses').as_posix(),p.read_bytes())
        for p in sorted((Path(qt)/'mkspecs').glob('q*.pri')):add_bytes(tar,'supplier-configuration/'+p.name,p.read_bytes())
    (output/(name+'.sha256')).write_text(digest(bundle)+'  '+name+'\n')
    sealed={'schema':1,'version':meta['version'],'platform':platform,'base_commit':record['base_commit'],'clean_commit':record['clean_commit'],'source_tree_sha256':record['source_tree_sha256'],'installer':entry,'corresponding_source':{'name':name,'sha256':digest(bundle),'bytes':bundle.stat().st_size},'staged_manifest_sha256':digest(stage/'manifest.json'),'redistribution_sha256':digest(stage/'redistribution.json')}
    if container:sealed['container_contents']=container
    write(output/f'RayNmeaSimulator-{meta["version"]}-{platform}-release-set.json',sealed);return sealed

def main():
    p=argparse.ArgumentParser(description=__doc__);sub=p.add_subparsers(dest='action',required=True)
    c=sub.add_parser('capture');c.add_argument('--output',type=Path,required=True)
    a=sub.add_parser('assemble')
    for n in ['stage','artifact','qt','sources','output']:a.add_argument('--'+n,type=Path,required=True)
    a.add_argument('--allow-dirty',action='store_true');args=p.parse_args()
    if args.action=='capture':
        value=snapshot();data=canonical(value)+b'\n';args.output.parent.mkdir(parents=True,exist_ok=True)
        if not args.output.exists() or args.output.read_bytes()!=data:args.output.write_bytes(data)
    else:print(json.dumps(assemble(args.stage,args.artifact,args.qt,args.sources,args.output,args.allow_dirty),indent=2))
if __name__=='__main__':main()
