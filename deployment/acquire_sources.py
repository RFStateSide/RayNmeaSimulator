# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Explicit source acquisition for offline release kits; never runs in a normal build."""
import argparse
import concurrent.futures
import configparser
import gzip
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import re
import subprocess
import tempfile
import tarfile
import urllib.request
import urllib.parse
from source_delivery import ROOT, digest, qt_modules, archive_tag, write, physical, add_bytes

def fetch(url,path,expected=None):
    path=Path(path)
    if not url.startswith('https://'):raise ValueError('HTTPS source required')
    if not path.exists():
        temporary=path.with_suffix(path.suffix+'.partial')
        try:
            with urllib.request.urlopen(url,timeout=90) as response,temporary.open('wb') as out:
                while data:=response.read(1024*1024):out.write(data)
            if expected and digest(temporary)!=expected:raise ValueError('Supplier checksum mismatch')
            temporary.replace(path)
        finally:
            if temporary.exists():temporary.unlink()
    if expected and digest(path)!=expected:raise ValueError('Cached supplier checksum mismatch')
    return path

def qt_commit_archive(module,revision,cache,origin=None):
    archive=cache/(module+'-'+revision+'-source.tar.gz')
    if archive.exists():
        if archive_tag(archive)!=revision:raise ValueError('Cached Qt commit mismatch')
        return archive
    with tempfile.TemporaryDirectory(prefix='qt-source-',dir=cache) as temporary:
        records=[]
        def collect(url,commit,prefix,dest,depth=0):
            if depth>5 or not re.fullmatch('[a-f0-9]{40}',commit):raise ValueError('Invalid Qt source recursion')
            parsed=urllib.parse.urlparse(url)
            if parsed.scheme!='https' or parsed.hostname not in ('code.qt.io','github.com') or parsed.username:raise ValueError('Unapproved Qt source origin')
            checkout=Path(temporary)/('repo-'+str(len(records)));subprocess.run(['git','init','--quiet','--template=',str(checkout)],check=True)
            subprocess.run(['git','-C',str(checkout),'fetch','--quiet','--depth=1',url,commit],check=True)
            actual=subprocess.check_output(['git','-C',str(checkout),'rev-parse','FETCH_HEAD'],text=True).strip()
            if actual!=commit:raise ValueError('Qt source commit mismatch')
            tree=subprocess.check_output(['git','-C',str(checkout),'rev-parse',commit+'^{tree}'],text=True).strip();records.append({'path':prefix,'origin':url,'commit':commit,'tree':tree})
            data=subprocess.check_output(['git','-C',str(checkout),'archive','--prefix='+prefix+'/',commit])
            with tarfile.open(fileobj=io.BytesIO(data)) as source:
                for member in source.getmembers():
                    if member.name==module+'/.tag':continue
                    if member.isfile():add_bytes(dest,member.name,source.extractfile(member).read(),member.mode)
                    elif member.issym():dest.addfile(member)
            listing=subprocess.check_output(['git','-C',str(checkout),'ls-tree','-r',commit],text=True)
            links=[line.split(chr(9),1) for line in listing.splitlines() if line.startswith('160000 ')]
            if links:
                parser=configparser.ConfigParser();parser.read_string(subprocess.check_output(['git','-C',str(checkout),'show',commit+':.gitmodules'],text=True))
                urls={parser[section]['path']:parser[section]['url'] for section in parser.sections()}
                for identity,path in links:
                    if Path(path).is_absolute() or '..' in Path(path).parts or path not in urls:raise ValueError('Invalid Qt submodule path')
                    childurl=urllib.parse.urljoin(url+'/',urls[path]);collect(childurl,identity.split()[2],prefix+'/'+path,dest,depth+1)
        temporary_archive=Path(temporary)/'complete-source.tar.gz'
        with temporary_archive.open('wb') as raw,gzip.GzipFile(filename='',fileobj=raw,mode='wb',mtime=0) as gz,tarfile.open(fileobj=gz,mode='w|') as dest:
            collect(origin or 'https://code.qt.io/qt/'+module+'.git',revision,module,dest)
            add_bytes(dest,module+'/.tag',(revision+chr(10)).encode())
            add_bytes(dest,module+'/SOURCE-ACQUISITION.json',json.dumps({'method':'exact Git object IDs, including submodules; generated .tag identifies the supplier commit','objects':records},indent=2).encode())
        temporary_archive.replace(archive)
    return archive

def marine_snapshot(root,pin,output):
    # One public upstream commit/tree only, no owner history/remotes/credentials.
    # A shallow Git snapshot preserves the existing unchanged dependency verifier.
    root=Path(root);head=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()
    if head!=pin['revision']:raise ValueError('Marine pin mismatch')
    objects=subprocess.check_output(['git','-C',str(root),'rev-list','--objects',head+'^{tree}'],text=True)
    ids=[line.split()[0] for line in objects.splitlines()]+[head]
    pack=subprocess.check_output(['git','-C',str(root),'pack-objects','--stdout'],input=('\n'.join(sorted(set(ids)))+'\n').encode())
    names=subprocess.check_output(['git','-C',str(root),'ls-files','-z']).decode().split('\0')
    modes={line.split(chr(9),1)[1]:line.split()[0] for line in subprocess.check_output(['git','-C',str(root),'ls-files','--stage'],text=True).splitlines()}
    with tempfile.TemporaryDirectory(prefix='marine-source-',dir=Path(output).parent) as temporary:
        temp=Path(temporary);dep=temp/'dependency';dep.mkdir();subprocess.run(['git','init','--quiet','--template=','--initial-branch=source',str(dep)],check=True)
        packname='pack-'+pack[-20:].hex();packpath=dep/'.git/objects/pack'/ (packname+'.pack');packpath.write_bytes(pack);subprocess.run(['git','index-pack',str(packpath)],check=True,stdout=subprocess.DEVNULL)
        (dep/'.git/refs/heads/source').write_bytes((head+chr(10)).encode());(dep/'.git/shallow').write_bytes((head+chr(10)).encode());subprocess.run(['git','-C',str(dep),'read-tree','HEAD'],check=True);(dep/'.git/config').write_bytes(b'[core]'+bytes([10])+b'repositoryformatversion = 0'+bytes([10])+b'bare = false'+bytes([10])+b'filemode = false'+bytes([10]))
        with Path(output).open('wb') as raw,gzip.GzipFile(filename='',fileobj=raw,mode='wb',mtime=0) as gz,tarfile.open(fileobj=gz,mode='w|') as tar:
            for n in sorted(n for n in names if n):
                p=root/n
                if p.is_symlink():raise ValueError('Unexpected marine symlink')
                add_bytes(tar,'dependency/'+n,p.read_bytes(),0o755 if modes[n]=='100755' else 0o644)
            for n in ['HEAD','config','index','shallow','refs/heads/source','objects/pack/'+packname+'.pack','objects/pack/'+packname+'.idx']:add_bytes(tar,'dependency/.git/'+n,(dep/'.git'/n).read_bytes())
    return {'path':str(output),'sha256':digest(output),'origin':pin['url'],'version':head,'patch_sha256':pin.get('patch_sha256')}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ['stage','qt','deps','cache','output']:p.add_argument('--'+key,type=Path,required=True)
    args=p.parse_args();cache=physical(args.cache)
    if not cache.is_relative_to(ROOT/'build'):raise ValueError('Cache must be a physical build/ directory')
    cache.mkdir(parents=True,exist_ok=True);meta=json.loads((args.stage/'build-metadata.json').read_text());version=meta['qt'];modules=qt_modules(args.stage,args.qt)
    if 'qtdeclarative' in modules:
        for n in ['qttools','qtlanguageserver','qtshadertools']:modules.setdefault(n,{})
    result={}
    def acquire(module):
        name=f'{module}-everywhere-src-{version}.tar.xz';url=f'https://download.qt.io/official_releases/qt/{".".join(version.split(".")[:2])}/{version}/submodules/{name}'
        checksum=urllib.request.urlopen(url+'.sha256',timeout=45).read().decode().split()[0]
        if not re.fullmatch('[a-f0-9]{64}',checksum):raise ValueError('Invalid supplier SHA-256')
        archive=fetch(url,cache/name,checksum);tag=archive_tag(archive)
        if modules[module].get('revision') and tag!=modules[module]['revision']:
            archive=qt_commit_archive(module,modules[module]['revision'],cache);tag=modules[module]['revision'];url='https://code.qt.io/qt/'+module+'.git@'+tag;checksum=digest(archive)
        return 'Qt/'+module,{'path':str(archive),'origin':url,'sha256':checksum,'version':version,'revision':tag}
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
        for key,value in pool.map(acquire,sorted(modules)):result[key]=value;write(args.output,result);print('Verified',key,flush=True)
    if meta.get('installer_components'):
        version=meta['installer_components'][0]['version'];tag='is-'+version.replace('.','_')
        info=json.load(urllib.request.urlopen('https://api.github.com/repos/jrsoftware/issrc/commits/'+tag,timeout=30));revision=info['sha']
        archive=qt_commit_archive('innosetup',revision,cache,'https://github.com/jrsoftware/issrc.git')
        result['Installer/InnoSetup']={'path':str(archive),'origin':'https://github.com/jrsoftware/issrc/tree/'+revision,'sha256':digest(archive),'version':version,'revision':revision};write(args.output,result)
    pins=json.loads((ROOT/'dependencies/pins.json').read_text())
    spec=importlib.util.spec_from_file_location('marine_verifier',ROOT/'scripts/dependencies.py');verifier=importlib.util.module_from_spec(spec);spec.loader.exec_module(verifier)
    for name,pin in pins.items():
        verifier.verify(name,args.deps/name)
        result['Marine/'+name]=marine_snapshot(args.deps/name,pin,cache/(name+'-'+pin['revision']+'.tar.gz'))
    for r in json.loads((args.stage/'manifest.json').read_text()):
        if r['path']=='SDL2.dll':
            pin=json.loads((ROOT/'dependencies/sdl2-macos.json').read_text())
            if pin['version']!=r['version']:raise ValueError('SDL source/runtime version mismatch')
            archive=fetch(pin['url'],cache/pin['archive'],pin['sha256']);result['Runtime/'+r['source']+'@'+r['version']]={'path':str(archive),'origin':pin['url'],'sha256':digest(archive),'version':r['version']}
    for r in json.loads((args.stage/'manifest.json').read_text()):
        if r['path']=='libwinpthread-1.dll':
            version=r['version'];info=json.load(urllib.request.urlopen('https://api.github.com/repos/mingw-w64/mingw-w64/commits/v'+version,timeout=30));revision=info['sha']
            archive=qt_commit_archive('mingw-w64',revision,cache,'https://github.com/mingw-w64/mingw-w64.git')
            result['Runtime/'+r['source']+'@'+version]={'path':str(archive),'origin':'https://github.com/mingw-w64/mingw-w64/tree/'+revision,'sha256':digest(archive),'version':version,'revision':revision};write(args.output,result)
    # Exact distribution source descriptors include distribution patches, not just upstream tarballs.
    components={}
    for r in json.loads((args.stage/'manifest.json').read_text()):
        if r['path'].startswith('lib/') and re.fullmatch(r'[a-z0-9+.-]+ [^ ]+',r['source']):components[r['source']]=r
    for source,r in components.items():
        package,version=source.split(' ',1);fileversion=version.split(':')[-1];filename=package+'_'+fileversion+'.dsc';folder=package[:4] if package.startswith('lib') else package[0]
        candidates=[f'https://launchpad.net/ubuntu/+archive/primary/+sourcefiles/{package}/{urllib.parse.quote(version,safe="")}/{filename}']
        candidates += [f'https://archive.ubuntu.com/ubuntu/pool/{section}/{folder}/{package}/{filename}' for section in ['main','universe']]
        candidates += [f'https://{host}/debian/pool/main/{folder}/{package}/{filename}' for host in ['deb.debian.org','archive.debian.org']]
        candidates += [f'https://security.debian.org/debian-security/pool/updates/main/{folder}/{package}/{filename}']
        descriptor=None
        for url in candidates:
            try:descriptor=fetch(url,cache/filename);break
            except Exception:continue
        if descriptor is None:raise RuntimeError('Matching distribution .dsc unavailable: '+source)
        text=descriptor.read_text();match=re.search(r'Checksums-Sha256:\n((?: [^\n]+\n)+)',text)
        if not match:raise ValueError('Missing .dsc SHA-256 list')
        members=[descriptor]
        for line in match[1].splitlines():
            sha,size,name=line.split()
            if Path(name).name!=name:raise ValueError('Unsafe .dsc filename')
            members.append(fetch(url.rsplit('/',1)[0]+'/'+name,cache/name,sha))
        archive=cache/(package+'-'+fileversion+'-source.tar')
        with tarfile.open(archive,'w') as tar:
            for f in members:add_bytes(tar,f.name,f.read_bytes())
        result['Runtime/'+source+'@'+r['version']]={'path':str(archive),'origin':url,'sha256':digest(archive),'version':r['version'],'distribution_source':source};write(args.output,result)
    if any(r.get('source')=='https://github.com/unicode-org/icu/tree/release-73-2' for r in json.loads((args.stage/'manifest.json').read_text())):
        url='https://github.com/unicode-org/icu/releases/download/release-73-2/icu4c-73_2-src.tgz';archive=fetch(url,cache/'icu4c-73_2-src.tgz')
        key='Runtime/https://github.com/unicode-org/icu/tree/release-73-2@73.2';result[key]={'path':str(archive),'origin':url,'sha256':digest(archive),'version':'73.2'}
    provenance=json.loads((ROOT/'resources/nmea2000/lookups.json').read_text())['provenance'];raw=fetch(provenance['url'],cache/('canboat-'+provenance['revision']+'.json'),provenance['sha256']);archive=cache/('canboat-'+provenance['revision']+'-source.tar')
    with tarfile.open(archive,'w') as tar:
        add_bytes(tar,'canboat.json',raw.read_bytes());add_bytes(tar,'LICENSE',(ROOT/'resources/nmea2000/LICENSE-canboat.txt').read_bytes());add_bytes(tar,'NOTICE',(ROOT/'resources/nmea2000/NOTICE.txt').read_bytes())
    result['Data/CANboat']={'path':str(archive),'origin':provenance['url'],'version':provenance['version'],'revision':provenance['revision'],'sha256':digest(archive)}
    write(args.output,result);print('Source input catalog:',args.output)
if __name__=='__main__':main()
