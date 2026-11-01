# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Create/verify a private companion source-and-verification ZIP. Never publishes.

Four installers + this companion = five uploaded assets; update.json is repository-hosted.
The companion retains all original published bytes as well as current source/support.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import zipfile
ROOT=Path(__file__).resolve().parents[1]
VERSION='0.1.0'
COMMIT='87cd0912b3e10777e610a82a78156484f66a2c12'
BINARY_NAMES={f'RayNmeaSimulator-{VERSION}-'+suffix for suffix in ['Windows-x64.zip','Linux-x86_64.tar.gz','Linux-aarch64.tar.gz','Linux-aarch64-Debian12.tar.gz']}
METADATA='update.json'
LEGACY_METADATA='RayNmeaSimulator-update.json'
COMPANION='RayNmeaSimulator-0.1.0-Corrected-Source-and-Verification.zip'

def digest(path):
    with Path(path).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()

def canonical(value):return (json.dumps(value,sort_keys=True,indent=2)+chr(10)).encode()

def inputs(path,metadata_name=METADATA):
    rows=json.loads(Path(path).read_text())['assets'];result={}
    for row in rows:
        name=row['name'];p=Path(row['path']).absolute()
        if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]*',name) or name in result or p!=p.resolve() or not p.is_file():raise ValueError('Unsafe/duplicate support input')
        if p.stat().st_size!=row['bytes'] or digest(p)!=row['sha256']:raise ValueError('Support input checksum mismatch: '+name)
        result[name]=row
    if not BINARY_NAMES|{metadata_name} <= result.keys():raise ValueError('Four installation packages and metadata required')
    return result

def traceability(current):
    metadata=json.loads(Path(current[METADATA]['path']).read_text())
    if metadata['repository']!='RFStateSide/RayNmeaSimulator' or metadata['version']!=VERSION or len(metadata['downloads'])!=4:raise ValueError('Update metadata identity mismatch')
    keys={'windows-x86_64','linux-x86_64','linux-arm64','linux-arm64-debian12'}
    if set(metadata['downloads'])!=keys:raise ValueError('Unknown or missing update platform')
    ready=True;identities=set()
    for platform,entry in metadata['downloads'].items():
        if entry['name'] not in BINARY_NAMES or entry['url']!='https://github.com/RFStateSide/RayNmeaSimulator/releases/download/v0.1.0/'+entry['name']:raise ValueError('Nonofficial package URL or name')
        binary=current[entry['name']]
        if (binary['bytes'],binary['sha256'])!=(entry['bytes'],entry['sha256']):raise ValueError('Metadata/package mismatch')
        receipt=json.loads(Path(current[f'RayNmeaSimulator-{VERSION}-{platform}-release-set.json']['path']).read_text())
        source=current[receipt['corresponding_source']['name']]
        if receipt['installer']!=entry or (source['sha256'],source['bytes'])!=(receipt['corresponding_source']['sha256'],receipt['corresponding_source']['bytes']):raise ValueError('Source/package receipt mismatch')
        identities.add((receipt['base_commit'],receipt['source_tree_sha256'],receipt['clean_commit']))
        if not re.fullmatch('[a-f0-9]{40}',receipt['base_commit']) or not re.fullmatch('[a-f0-9]{64}',receipt['source_tree_sha256']):raise ValueError('Invalid corrected build source identity')
        ready=ready and receipt.get('publication_ready',receipt.get('clean_commit',False))
    if len(identities)!=1:raise ValueError('Platform source identities differ')
    identity=next(iter(identities))
    return metadata,ready,{'base_commit':identity[0],'source_tree_sha256':identity[1],'clean_commit':identity[2]}

def member(archive,name,path=None,data=None):
    info=zipfile.ZipInfo(name,date_time=(1980,1,1,0,0,0));info.create_system=3;info.external_attr=0o100644<<16;info.compress_type=zipfile.ZIP_STORED
    if path is None:archive.writestr(info,data);return {'name':name,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}
    p=Path(path)
    with p.open('rb') as src,archive.open(info,'w',force_zip64=True) as dst:
        while chunk:=src.read(1024*1024):dst.write(chunk)
    return {'name':name,'bytes':p.stat().st_size,'sha256':digest(p)}

def create(current_inventory,original_inventory,output):
    current=inputs(current_inventory);original=inputs(original_inventory,LEGACY_METADATA)
    metadata,ready,source=traceability(current)
    if any(current[name]['sha256']==original[name]['sha256'] for name in BINARY_NAMES):raise ValueError('Corrected updater requires all four rebuilt application packages')
    output=Path(output).absolute()
    if output!=output.resolve() or not output.is_relative_to(ROOT/'build') or output.exists():raise ValueError('Use fresh physical build/ output')
    estimated=sum(v['bytes'] for k,v in current.items() if k not in BINARY_NAMES)+sum(v['bytes'] for v in original.values())
    if estimated+1024*1024>=2*1024**3:raise ValueError('Companion would exceed GitHub single-asset limit')
    output.parent.mkdir(parents=True,exist_ok=True);files=[]
    readme=('Complete corresponding source, notices and verification for RayNmeaSimulator v0.1.0.\n'
            'Normal installation downloads are the four separate packages on the Release page.\n'
            'current/ contains all four complete matching platform Source kits, notices, receipts,\n'
            'checksums and a snapshot of update metadata. Choose the platform Source kit named\n'
            'by its release-set receipt; extract that kit and follow REBUILD.md.\n'
            'GitHub automatic source archives do not replace these complete kits.\n'
            'original-v0.1.0/ retains EVERY original published artifact unchanged, including original\n'
            'binary/source kits and verification files. Do not confuse original and corrected hashes.\n'
            'contents.json verifies every member except itself. The outer ZIP SHA is recorded separately\n'
            'on the Release page. Run python3 packaging-tools/release_support.py verify --archive THIS.zip\n'
            'to verify all internal bytes. GPL/AGPL sources/notices remain available at no additional charge.\n'
            'Private candidates marked publication_ready=false require owner acceptance and clean packaging\n'
            'source sealing before any public replacement.\n').replace('\\n','\n').encode()
    with zipfile.ZipFile(output,'w',allowZip64=True) as z:
        files.append(member(z,'README-SOURCE.txt',data=readme))
        files.append(member(z,'LICENSE',path=ROOT/'LICENSE'))
        files.append(member(z,'packaging-tools/release_support.py',path=Path(__file__)))
        for scope,items in [('current',current),('original-v0.1.0',original)]:
            for name,row in sorted(items.items()):
                if scope=='current' and name in BINARY_NAMES:continue
                files.append(member(z,scope+'/'+name,path=row['path']))
        manifest={'schema':1,'version':VERSION,'original_tagged_source_commit':COMMIT,'corrected_build_source':source,'publication_ready':ready,
                  'current_downloads':metadata['downloads'],'files':files,
                  'uploaded_asset_count_proposed':5,'original_artifacts_retained':len(original)}
        member(z,'contents.json',data=canonical(manifest))
    verify(output)
    receipt={'name':output.name,'bytes':output.stat().st_size,'sha256':digest(output),'publication_ready':ready,'member_count':len(files)+1}
    output.with_name(output.name+'.sha256').write_text(receipt['sha256']+'  '+output.name+chr(10))
    output.with_name(output.name+'.receipt.json').write_bytes(canonical(receipt));return receipt

def verify(path):
    with zipfile.ZipFile(path) as z:
        names=z.namelist()
        if len(names)!=len(set(names)) or any(PurePosixPath(n).is_absolute() or '..' in PurePosixPath(n).parts or chr(92) in n or any(not re.fullmatch(r'[A-Za-z0-9_.-]+',part) for part in PurePosixPath(n).parts) for n in names):raise ValueError('Unsafe/duplicate ZIP member')
        manifest=json.loads(z.read('contents.json'));rows={r['name']:r for r in manifest['files']}
        if len(rows)!=len(manifest['files']) or set(names)!=set(rows)|{'contents.json'}:raise ValueError('Companion inventory differs')
        for name,row in rows.items():
            with z.open(name) as data:actual=hashlib.file_digest(data,'sha256').hexdigest()
            if actual!=row['sha256'] or z.getinfo(name).file_size!=row['bytes']:raise ValueError('Companion member checksum mismatch: '+name)
    return manifest

def main():
    p=argparse.ArgumentParser(description=__doc__);sub=p.add_subparsers(dest='action',required=True)
    c=sub.add_parser('create');c.add_argument('--current-inventory',type=Path,required=True);c.add_argument('--original-inventory',type=Path,required=True);c.add_argument('--output',type=Path,required=True)
    v=sub.add_parser('verify');v.add_argument('--archive',type=Path,required=True);a=p.parse_args()
    if a.action=='create':print(json.dumps(create(a.current_inventory,a.original_inventory,a.output),indent=2))
    else:
        m=verify(a.archive);print(json.dumps({'verified_members':len(m['files']),'version':m['version'],'publication_ready':m['publication_ready']},indent=2))
if __name__=='__main__':main()
