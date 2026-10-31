# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Private packaging receipts and explicit future release metadata assembly. Never publishes."""
import argparse
import hashlib
import json
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[2]
REPOSITORY='RFStateSide/RayNmeaSimulator'
NAME='RayNmeaSimulator-update.json'
KEYS={'windows-x86_64','linux-x86_64','linux-arm64','linux-arm64-debian12'}
def version():
    return re.search(r'project\(RayNmeaSimulator VERSION ([0-9]+\.[0-9]+\.[0-9]+)',(ROOT/'CMakeLists.txt').read_text())[1]
def record(metadata,artifact):
    """Called only after packaging; hash actual package bytes and measured ABI receipt."""
    artifact=Path(artifact)
    if metadata['version']!=version():raise ValueError('Package/application version mismatch')
    arch=metadata['architecture'].lower()
    if str(metadata['os']).lower().startswith('windows') and arch in ('amd64','x86_64'):
        key='windows-x86_64';expected=f'RayNmeaSimulator-{version()}-Windows-x64-Setup.exe'
    elif 'required_abi' in metadata and arch in ('x86_64','aarch64','arm64'):
        key='linux-x86_64' if arch=='x86_64' else 'linux-arm64'
        variant=metadata['package_variant']
        if variant=='Debian12':
            if key!='linux-arm64':raise ValueError('Debian12 requires ARM64')
            key+='-debian12'
        elif variant!='native':raise ValueError('Unknown package variant')
        expected=f'RayNmeaSimulator-{version()}-Linux-{"x86_64" if arch=="x86_64" else "aarch64"}{"-Debian12" if variant=="Debian12" else ""}.tar.gz'
    else:raise ValueError('Unsupported package OS/architecture')
    if artifact.name!=expected or not artifact.is_file() or artifact.is_symlink():raise ValueError('Unexpected package identity')
    with artifact.open('rb') as stream: checksum=hashlib.file_digest(stream,'sha256').hexdigest()
    entry={'name':artifact.name,'url':f'https://github.com/{REPOSITORY}/releases/download/v{version()}/{artifact.name}','bytes':artifact.stat().st_size,'sha256':checksum}
    if key.startswith('linux'):
        abi=metadata['required_abi']
        if set(abi)!={'GLIBC','GLIBCXX','CXXABI'} or any(not re.fullmatch(r'\d+(?:\.\d+){1,3}',v) for v in abi.values()):raise ValueError('Missing measured Linux ABI requirements')
        if key.endswith('debian12') and any(tuple(map(int,abi[k].split('.')))>tuple(map(int,ceiling.split('.'))) for k,ceiling in {'GLIBC':'2.36','GLIBCXX':'3.4.30','CXXABI':'1.3.13'}.items()):raise ValueError('Not Debian12 compatible')
        entry['required_abi']=abi
    return {'schema':1,'repository':REPOSITORY,'version':version(),'downloads':{key:entry}}
def merge(records):
    output={'schema':1,'repository':REPOSITORY,'version':version(),'downloads':{}}
    for item in records:
        if (item.get('schema'),item.get('repository'),item.get('version'))!=(1,REPOSITORY,version()):raise ValueError('Release identity mismatch')
        for key,entry in item['downloads'].items():
            if key not in KEYS or key in output['downloads']:raise ValueError('Unknown or duplicate platform')
            expected_url=f'https://github.com/{REPOSITORY}/releases/download/v{version()}/{entry["name"]}'
            if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]*',entry['name']) or entry['url']!=expected_url or not re.fullmatch('[a-f0-9]{64}',entry['sha256']) or type(entry['bytes']) is not int or entry['bytes']<=0:raise ValueError('Invalid package receipt')
            suffix={'windows-x86_64':'Windows-x64-Setup.exe','linux-x86_64':'Linux-x86_64.tar.gz','linux-arm64':'Linux-aarch64.tar.gz','linux-arm64-debian12':'Linux-aarch64-Debian12.tar.gz'}[key]
            if entry['name']!=f'RayNmeaSimulator-{version()}-{suffix}':raise ValueError('Platform/package identity mismatch')
            if key.startswith('linux'):
                abi=entry.get('required_abi',{})
                if set(abi)!={'GLIBC','GLIBCXX','CXXABI'} or any(not isinstance(v,str) or not re.fullmatch(r'\d+(?:\.\d+){1,3}',v) for v in abi.values()):raise ValueError('Missing Linux ABI receipt')
            output['downloads'][key]=entry
    if not output['downloads']:raise ValueError('No packages')
    return output
def main():
    parser=argparse.ArgumentParser(description='Assemble release information locally; does not publish anything')
    parser.add_argument('--receipt',type=Path,action='append',required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    output=args.output.absolute()
    if output.resolve()!=output or not output.is_relative_to(ROOT/'build') or output.name!=NAME:raise ValueError('Use a physical build/ output named '+NAME)
    data=merge([json.loads(p.read_text()) for p in args.receipt]);output.parent.mkdir(parents=True,exist_ok=True)
    if output.exists():raise ValueError('Preserve prior metadata; choose a fresh output location')
    output.write_text(json.dumps(data,indent=2)+'\n');print('Local metadata only:',output)
if __name__=='__main__':main()
