# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Reseal packaging-only Linux corrections without rebuilding approved application/Qt bytes.

No network, Git writes or publication. Requires independently accepted input hashes.
Full matching source retains the tagged application and adds the exact packaging overlay.
"""
import argparse
import copy
import gzip
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
import tarfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'deployment/update'))
import metadata as updates

APPLICATION_COMMIT = '87cd0912b3e10777e610a82a78156484f66a2c12'
OVERLAY = ['packaging/linux/installer.py', 'packaging/linux/runtime_prerequisites.py',
           'scripts/package.py', 'deployment/repackage_linux.py',
           'tests/linux_prerequisite_tests.py', 'tests/repackaging_tests.py',
           'tests/CMakeLists.txt', 'docs/packaging-corrections.md',
           'deployment/release_support.py', 'tests/release_support_tests.py']


def canonical(value):
    return (json.dumps(value, sort_keys=True, indent=2)+'\n').encode()


def sha(data):
    return hashlib.sha256(data).hexdigest()


def digest(path):
    with Path(path).open('rb') as f:
        return hashlib.file_digest(f,'sha256').hexdigest()


def physical(path):
    p=Path(path).absolute()
    if p!=p.resolve() or p.is_symlink():raise ValueError('Use physical input/output paths')
    return p


def checked_members(archive):
    seen=set()
    for member in archive:
        p=PurePosixPath(member.name)
        if p.is_absolute() or '..' in p.parts or str(p) in seen or not (member.isfile() or member.isdir()):
            raise ValueError('Unsafe or duplicate archive member: '+member.name)
        seen.add(str(p))
        yield member


def read_package(path):
    payload={}
    with tarfile.open(path) as t:
        for member in checked_members(t):
            p=PurePosixPath(member.name)
            if not p.parts or p.parts[0]!='RayNmeaSimulator':raise ValueError('Unexpected package root')
            if member.isfile():
                name='/'.join(p.parts[1:])
                if not name:raise ValueError('Invalid package file')
                payload[name]=(t.extractfile(member).read(),member.mode)
    rows=json.loads(payload['manifest.json'][0])
    expected={r['path']:r['sha256'] for r in rows}
    actual={n:sha(data) for n,(data,mode) in payload.items() if n!='manifest.json'}
    if len(expected)!=len(rows) or expected!=actual:raise ValueError('Original package manifest mismatch')
    meta=json.loads(payload['build-metadata.json'][0]);source=json.loads(payload['source-snapshot.json'][0])
    if meta['source_sha']!=APPLICATION_COMMIT or source['base_commit']!=APPLICATION_COMMIT or not source['clean_commit']:
        raise ValueError('Only the approved clean application revision may be repackaged')
    return payload,rows,meta,source


def packaging_source(allow_uncommitted):
    files=[]
    for name in OVERLAY:
        p=physical(ROOT/name)
        files.append({'path':name,'sha256':digest(p),'mode':'100755' if name=='packaging/linux/installer.py' else '100644'})
    seed=ROOT/'packaging-source.json'
    if not (ROOT/'.git').exists() and seed.is_file():
        result=json.loads(seed.read_text())
        if result['files']!=files:raise ValueError('Packaging overlay differs from supplied source receipt')
    else:
        head=subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip()
        dirty=bool(subprocess.check_output(['git','-C',str(ROOT),'status','--porcelain','--',*OVERLAY],text=True))
        result={'schema':1,'application_commit':APPLICATION_COMMIT,'packaging_base_commit':head,
                'packaging_commit':None if dirty else head,'packaging_clean_commit':not dirty,
                'files':files,'packaging_source_tree_sha256':sha(canonical(files))}
    if not result['packaging_clean_commit'] and not allow_uncommitted:
        raise ValueError('Packaging source is uncommitted; --allow-uncommitted is private qualification only')
    return result


def add(t,name,data,mode=0o644):
    info=tarfile.TarInfo(name);info.size=len(data);info.mode=mode;info.mtime=0
    info.uid=info.gid=0;info.uname=info.gname=''
    t.addfile(info,io.BytesIO(data))


def tar_writer(path):
    # ExitStack in callers closes all layers and finishes deterministic gzip trailers.
    import contextlib
    stack=contextlib.ExitStack();raw=stack.enter_context(path.open('wb'))
    gz=stack.enter_context(gzip.GzipFile(filename='',fileobj=raw,mode='wb',mtime=0))
    archive=stack.enter_context(tarfile.open(fileobj=gz,mode='w|'))
    return stack,archive


def repackage(binary,source_kit,expected_binary,expected_source,output,allow_uncommitted=False):
    binary=physical(binary);source_kit=physical(source_kit);output=physical(output)
    if not output.is_relative_to(ROOT/'build') or output.exists():raise ValueError('Choose fresh private build/ output')
    for path,expected in [(binary,expected_binary),(source_kit,expected_source)]:
        if not re.fullmatch('[a-f0-9]{64}',expected) or digest(path)!=expected:raise ValueError('Approved input SHA-256 mismatch: '+path.name)
    original,rows,meta,source=read_package(binary)
    with tarfile.open(source_kit) as t:
        # Trusted input hash is also bound to these application/package source records.
        if json.load(t.extractfile('source-snapshot.json'))!=source:raise ValueError('Original source kit does not match application source')
        if json.load(t.extractfile('package-manifest.json'))!=rows:raise ValueError('Original source kit does not match original package')
        if json.load(t.extractfile('build-metadata.json'))!=meta:raise ValueError('Original source kit build metadata mismatch')
    packaging=packaging_source(allow_uncommitted)
    payload=copy.copy(original)
    for name in ['installer.py','runtime_prerequisites.py']:
        payload[name]=((ROOT/'packaging/linux'/name).read_bytes(),0o755 if name=='installer.py' else 0o644)
    meta={**meta,'packaging_source':packaging,'original_distribution':{'name':binary.name,'sha256':expected_binary,'bytes':binary.stat().st_size}}
    payload['build-metadata.json']=(canonical(meta),0o644)
    payload['packaging-source.json']=(canonical(packaging),0o644)
    by_name={r['path']:r for r in rows}
    manifest=[]
    for name,(data,mode) in sorted(payload.items()):
        if name=='manifest.json':continue
        row=dict(by_name.get(name,{'path':name,'origin':'packaging source overlay','source':'RFStateSide/RayNmeaSimulator','license':'AGPL-3.0-or-later','version':'0.1.0'}))
        row.update(sha256=sha(data),bytes=len(data));manifest.append(row)
    payload['manifest.json']=(canonical(manifest),0o644)
    # Every original file except the explicit packaging/metadata allowlist must be identical.
    permitted={'installer.py','build-metadata.json','manifest.json'}
    for name,value in original.items():
        if name not in permitted and payload[name]!=value:raise ValueError('Application/runtime/notices changed: '+name)
    elf={n:sha(v[0]) for n,v in original.items() if v[0].startswith(b'\x7fELF')}
    if not elf or 'bin/RayNmeaSimulator' not in elf:raise ValueError('Approved ELF application is required')
    if any(sha(payload[n][0])!=h for n,h in elf.items()):raise ValueError('ELF application/runtime substitution rejected')
    output.mkdir(parents=True)
    artifact=output/binary.name;stack,t=tar_writer(artifact)
    with stack:
        for name,(data,mode) in sorted(payload.items()):add(t,'RayNmeaSimulator/'+name,data,mode)
    update=updates.record(meta,artifact);entry=next(iter(update['downloads'].values()));platform=next(iter(update['downloads']))
    (output/'update-package.json').write_bytes(canonical(update))
    source_name=f'RayNmeaSimulator-0.1.0-{platform}-Source.tar.gz';corrected=output/source_name
    replacements={'build-metadata.json':canonical(meta),'package-manifest.json':canonical(manifest),'update-package.json':canonical(update)}
    instructions=("""# Packaging correction: v0.1.0

Application/Qt/runtime ELF bytes remain the approved application commit 87cd0912b3e10777e610a82a78156484f66a2c12.
The final installer scripts are in packaging-overlay/, not solely the tagged application tree.
Read REBUILD.md for the original application/toolchain/dependency build. To reproduce the correction:

1. Rebuild the original application/package using application/ and the complete matching sources/.
2. Make a separate copy of application/. Copy packaging-overlay/ over that copy, including packaging-source.json.
3. From that copy run python3 deployment/repackage_linux.py with the original binary archive, original complete
   source kit and their independently verified --expected-binary-sha256 / --expected-source-sha256 values.
   Select a fresh --output build/... directory. Use --allow-uncommitted only for a source receipt explicitly
   marked private/uncommitted. Exact files/hashes are in packaging-source.json and build-metadata.json.
4. Compare expanded file hashes and modes with package-manifest.json; ELF files must be byte-identical.

The source kit preserves the complete original application and upstream source inputs. New packaging files
retain AGPL-3.0-or-later; no new binary dependency or replacement system Qt/glibc is distributed.
The original source archive and its SHA are recorded in the external release-set receipt. Recompression may
change archive bytes with a different Python/zlib toolchain; use the recorded distribution SHA for downloads.
""").encode()
    stack,dest=tar_writer(corrected)
    with stack,tarfile.open(source_kit) as original_sources:
        for member in checked_members(original_sources):
            if member.isdir():continue
            data=original_sources.extractfile(member).read()
            if member.name=='REBUILD.md':data=b'Read PACKAGING-CORRECTION.md first for the final packaging overlay.\n\n'+data
            add(dest,member.name,replacements.get(member.name,data),member.mode)
        add(dest,'PACKAGING-CORRECTION.md',instructions)
        add(dest,'packaging-source.json',canonical(packaging))
        add(dest,'packaging-overlay/packaging-source.json',canonical(packaging))
        for row in packaging['files']:add(dest,'packaging-overlay/'+row['path'],(ROOT/row['path']).read_bytes(),0o755 if row['mode']=='100755' else 0o644)
    sealed={'schema':2,'version':'0.1.0','platform':platform,'base_commit':APPLICATION_COMMIT,
            'source_tree_sha256':source['source_tree_sha256'],'clean_commit':source['clean_commit'],
            'packaging_source':packaging,'original_distribution':meta['original_distribution'],
            'original_corresponding_source':{'name':source_kit.name,'bytes':source_kit.stat().st_size,'sha256':expected_source},
            'installer':entry,'corresponding_source':{'name':source_name,'bytes':corrected.stat().st_size,'sha256':digest(corrected)},
            'staged_manifest_sha256':sha(canonical(manifest)),'redistribution_sha256':sha(payload['redistribution.json'][0]),
            'unchanged_elf_sha256':elf,'publication_ready':packaging['packaging_clean_commit']}
    (output/f'RayNmeaSimulator-0.1.0-{platform}-release-set.json').write_bytes(canonical(sealed))
    for path in [artifact,corrected]:(output/(path.name+'.sha256')).write_text(digest(path)+'  '+path.name+chr(10))
    (output/'packaging-source.json').write_bytes(canonical(packaging))
    return sealed


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['binary','source-kit','output']:p.add_argument('--'+name,type=Path,required=True)
    for name in ['expected-binary-sha256','expected-source-sha256']:p.add_argument('--'+name,required=True)
    p.add_argument('--allow-uncommitted',action='store_true');a=p.parse_args()
    result=repackage(a.binary,a.source_kit,a.expected_binary_sha256,a.expected_source_sha256,a.output,a.allow_uncommitted)
    print(json.dumps({'platform':result['platform'],'installer':result['installer'],'corresponding_source':result['corresponding_source'],'publication_ready':result['publication_ready']},indent=2))
if __name__=='__main__':main()
