#!/usr/bin/env python3
"""Install checksum-pinned standalone build helpers under ignored tools/local/."""
import argparse
import hashlib
import io
from pathlib import Path
import platform
import tarfile
import tempfile
import urllib.request
import json

ROOT=Path(__file__).resolve().parents[1]
VERSION='26.06'
PACKAGES={
    ('Linux','x86_64'):('7z2606-linux-x64.tar.xz','2b539cf067b2ad48e1baa5093dbaa8818b73542d1222a0ffde9dc5f2922ea62e','7zzs'),
    ('Linux','aarch64'):('7z2606-linux-arm64.tar.xz','edcc408aacce77da5223728f2eceb5e9f6dc713b8a9df934ad21e2a7f7560627','7zzs'),
    ('Darwin','arm64'):('7z2606-mac.tar.xz','b6f17920b4240940461dac8ef4009891881ac44d2a5ead625ede2ec8472185dd','7zz'),
    ('Darwin','x86_64'):('7z2606-mac-x64.tar.xz','3af68b124052da7fc2a246c96b0bea6a68bfee389baa788f4cead1a133445586','7zz'),
}

def sha(data):return hashlib.sha256(data).hexdigest()

def install(prefix):
    machine=platform.machine().lower()
    if machine=='amd64':machine='x86_64'
    if platform.system()=='Linux' and machine=='arm64':machine='aarch64'
    key=(platform.system(),machine)
    if key not in PACKAGES:raise ValueError('No pinned local 7-Zip helper for '+str(key))
    filename,expected,member=PACKAGES[key]
    if prefix.is_symlink():raise ValueError('Local tools prefix must not be a symlink')
    cache=prefix/'downloads';cache.mkdir(parents=True,exist_ok=True)
    if cache.is_symlink():raise ValueError('Tool download cache must not be a symlink')
    archive=cache/filename
    if archive.is_symlink():raise ValueError('Tool archive must not be a symlink')
    data=archive.read_bytes() if archive.exists() else b''
    if sha(data)!=expected:
        url=f'https://github.com/ip7z/7zip/releases/download/{VERSION}/{filename}'
        print('Downloading standalone 7-Zip '+VERSION+' for '+str(key),flush=True)
        request=urllib.request.Request(url,headers={'User-Agent':'Keeper-Enhanced-tools/0.1'})
        with urllib.request.urlopen(request,timeout=90) as response:data=response.read(20*1024*1024+1)
        if len(data)>20*1024*1024 or sha(data)!=expected:raise ValueError('7-Zip archive checksum/size validation failed')
        with tempfile.NamedTemporaryFile(dir=cache,delete=False) as stream:
            part=Path(stream.name)
            try:stream.write(data);stream.close();part.replace(archive)
            finally:part.unlink(missing_ok=True)
    # Do not extract paths from archives: copy only explicitly named regular files.
    with tarfile.open(fileobj=io.BytesIO(data),mode='r:xz') as tar:
        def content(name):
            entry=tar.getmember(name)
            if not entry.isfile() or entry.size>16*1024*1024:raise ValueError('Invalid tool archive member')
            return tar.extractfile(entry).read()
        binary=content(member);license=content('License.txt')
    for folder in (prefix/'bin',prefix/'licenses/7zip'):
        if folder.is_symlink():raise ValueError('Tool installation directory must not be a symlink')
        folder.mkdir(parents=True,exist_ok=True)
    target=prefix/'bin/7zz'
    if target.is_symlink():raise ValueError('Tool executable must not be a symlink')
    if not target.exists() or sha(target.read_bytes())!=sha(binary):
        with tempfile.NamedTemporaryFile(dir=target.parent,delete=False) as stream:
            part=Path(stream.name)
            try:stream.write(binary);stream.close();part.chmod(0o755);part.replace(target)
            finally:part.unlink(missing_ok=True)
    (prefix/'licenses/7zip/License.txt').write_bytes(license)
    (prefix/'7zip.json').write_text(json.dumps({'version':VERSION,'archive_sha256':expected,'binary_sha256':sha(binary)},indent=2)+'\n')
    print('Local 7-Zip ready: '+str(target),flush=True)
    return target

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--prefix',type=Path,default=ROOT/'tools/local')
    a=p.parse_args()
    try:install(a.prefix)
    except (ValueError,OSError,tarfile.TarError,KeyError) as error:p.exit(1,str(error)+'\n')

if __name__=='__main__':main()
