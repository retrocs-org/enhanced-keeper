#!/usr/bin/env python3
"""Fetch a pinned upstream runtime asset base; never install downloaded executables."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import tempfile
import urllib.request

ROOT=Path(__file__).resolve().parents[1]
URL='https://github.com/dkfans/keeperfx/releases/download/v1.4.0/keeperfx_1_4_0_complete.7z'
SHA256='82d9d5634e8ea6cabb8f62fbd4830b758919893863cd313517af256a3040a279'
SIZE=374587275
ASSETS={'campgns','creatrs','data','fxdata','ldata','levels','multiplayer','mods','music','sound'}

def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda:stream.read(1024*1024),b''):h.update(chunk)
    return h.hexdigest()

def validate_listing(text):
    entries=text.partition('----------')[2]
    if not entries:raise ValueError('Cannot validate runtime archive listing')
    for line in entries.splitlines():
        if line.startswith(('Symbolic Link =','Hard Link =')):raise ValueError('Linked archive entry rejected')
        if line.startswith('Path = '):
            name=line[7:].replace('\\','/')
            path=PurePosixPath(name)
            if path.is_absolute() or '..' in path.parts or ':' in name:raise ValueError('Unsafe archive path')

def bootstrap(output,cache):
    if output.exists() or output.is_symlink():raise ValueError('Runtime base already exists; refusing replacement')
    subprocess.run([__import__('sys').executable,str(ROOT/'scripts/bootstrap-tools.py')],check=True)
    tool=str(ROOT/'tools/local/bin/7zz')
    cache.mkdir(parents=True,exist_ok=True)
    archive=cache/'keeperfx_1_4_0_complete.7z'
    if archive.is_symlink():raise ValueError('Runtime cache must not contain linked archives')
    if not archive.exists() or archive.stat().st_size!=SIZE or digest(archive)!=SHA256:
        print('Downloading pinned KeeperFX 1.4.0 runtime assets (about 375 MB)...',flush=True)
        with tempfile.NamedTemporaryFile(prefix='.runtime-',dir=cache,delete=False) as stream:
            partial=Path(stream.name)
            try:
                request=urllib.request.Request(URL,headers={'User-Agent':'Keeper-Enhanced-importer/0.1'})
                with urllib.request.urlopen(request,timeout=90) as response:
                    count=0
                    while True:
                        chunk=response.read(1024*1024)
                        if not chunk:break
                        count+=len(chunk)
                        if count>SIZE:raise ValueError('Runtime download exceeds pinned size')
                        stream.write(chunk)
                stream.close()
                if partial.stat().st_size!=SIZE or digest(partial)!=SHA256:raise ValueError('Runtime archive checksum mismatch')
                partial.replace(archive)
            finally:partial.unlink(missing_ok=True)
    print('Verified runtime archive SHA-256.',flush=True)
    listing=subprocess.check_output([tool,'l','-slt',str(archive)],text=True)
    validate_listing(listing)
    output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.runtime-base-',dir=output.parent) as temp:
        temp=Path(temp);unpacked=temp/'archive';unpacked.mkdir()
        subprocess.run([tool,'x','-y','-o'+str(unpacked),str(archive)],check=True,stdout=subprocess.DEVNULL)
        candidates=[unpacked]+[p for p in unpacked.iterdir() if p.is_dir()]
        base=next((p for p in candidates if (p/'keeperfx.cfg').is_file() or (p/'keeper-enhanced.cfg').is_file()),None)
        if base is None:raise ValueError('Runtime archive is missing its main configuration')
        if any(p.is_symlink() for p in unpacked.rglob('*')):raise ValueError('Runtime archive contains a link')
        stage=temp/'base';stage.mkdir()
        for name in ASSETS:
            source=base/name
            if source.is_dir():
                shutil.copytree(source,stage/name,ignore=shutil.ignore_patterns('*.exe','*.dll','*.so','*.dylib','*.log','community-session.cfg'))
        config=base/'keeper-enhanced.cfg'
        if not config.exists():config=base/'keeperfx.cfg'
        text=config.read_text()
        # Use the project's default borderless fullscreen presentation.
        import re
        text=re.sub(r'(?mi)^FRONTEND_RES\s*=.*$', 'FRONTEND_RES=640x480w32 DESKTOP DESKTOP',text)
        text=re.sub(r'(?mi)^INGAME_RES\s*=.*$', 'INGAME_RES=DESKTOP DESKTOP_FULL 1920x1080x32',text)
        (stage/'keeper-enhanced.cfg').write_text(text)
        (stage/'runtime-source.json').write_text(json.dumps({'url':URL,'sha256':SHA256,'version':'1.4.0'},indent=2)+'\n')
        if output.exists() or output.is_symlink():raise ValueError('Runtime destination appeared during bootstrap')
        stage.rename(output)
    print('Prepared runtime asset base: '+str(output),flush=True)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,default=ROOT/'.local/keeperfx')
    p.add_argument('--cache',type=Path,default=ROOT/'.local/runtime-cache')
    a=p.parse_args()
    try:bootstrap(a.output,a.cache)
    except (ValueError,OSError,subprocess.CalledProcessError) as error:p.exit(1,str(error)+'\n')

if __name__=='__main__':main()
