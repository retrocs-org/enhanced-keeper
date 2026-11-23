#!/usr/bin/env python3
"""Refresh website downloads from published GitHub releases; never advertise drafts."""
import argparse
import json
from pathlib import Path
import re
import urllib.request
import urllib.error
from urllib.parse import quote
from release_support import atomic_json

REPO = 'https://github.com/retrocs-org/KeeperFX-E'
API = 'https://api.github.com/repos/retrocs-org/KeeperFX-E/releases/latest'


def download(url, limit=32768):
    request = urllib.request.Request(url, headers={'User-Agent':'KeeperFX-E-website','Accept':'application/vnd.github+json'})
    with urllib.request.urlopen(request, timeout=15) as response:
        if not response.url.startswith('https://'):
            raise ValueError('HTTPS required')
        value = response.read(limit+1)
    if len(value)>limit:
        raise ValueError('Release response too large')
    return value


def manifest(release, fetch=download):
    tag = release.get('tag_name', '')
    version = tag.removeprefix('v')
    if release.get('draft',True) or release.get('prerelease',True) or not re.fullmatch(r'\d+\.\d+\.\d+',version):
        raise ValueError('Choose a stable published release')
    assets = {}
    for item in release.get('assets',[]):
        if item['name'] in assets:
            raise ValueError('Duplicate release asset')
        assets[item['name']] = item
    result = {'version':version,'release_url':REPO+'/releases/tag/'+quote(tag,safe=''),'assets':[]}
    for kind in ('dmg','zip'):
        name = 'KeeperFX-E-'+version+'-macOS-Universal.'+kind
        expected = REPO+'/releases/download/'+quote(tag,safe='')+'/'+name
        for candidate in (name,name+'.sha256'):
            item=assets.get(candidate,{})
            if item.get('browser_download_url') != expected+('.sha256' if candidate.endswith('.sha256') else ''):
                raise ValueError('Missing or unexpected release download')
        checksum=fetch(expected+'.sha256',1024).decode().strip()
        match=re.fullmatch(r'([a-f0-9]{64})  '+re.escape(name),checksum)
        if not match:
            raise ValueError('Invalid checksum file')
        # The release tool verifies the uploaded bytes; this check also uses GitHub's digest when available.
        digest=assets[name].get('digest')
        if digest and digest!='sha256:'+match[1]:
            raise ValueError('Release checksum differs from host digest')
        result['assets'].append({'type':kind,'url':expected,'sha256':match[1]})
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path(__file__).resolve().parents[1]/'.local/web/releases.json')
    args=parser.parse_args()
    try:value=manifest(json.loads(download(API,262144)))
    except urllib.error.HTTPError as error:
        if error.code==404:
            print('No stable release is published yet; existing metadata left unchanged.')
            return
        raise
    args.output.parent.mkdir(mode=0o700,parents=True,exist_ok=True)
    atomic_json(args.output,value)
    print('Website downloads updated for '+value['version'])

if __name__=='__main__':main()
