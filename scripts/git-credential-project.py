#!/usr/bin/env python3
"""Repository-local Git credential bridge to concealed 1Password token fields."""
from pathlib import Path
import shlex
import json
import os
from urllib.parse import urlparse
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
REPOSITORY='retrocs-org/keeper-enhanced'
def destinations():
    result={'github.com':{'repository':REPOSITORY,'username':'x-access-token','vault':'Private','item':'github.com','field':REPOSITORY+'-rw'}}
    profile=Path(os.environ.get('KFX_RELEASE_CONFIG',str(Path.home()/'Development/tools/git-release/projects/keeper-enhanced.json')))
    if not profile.is_file() or profile.is_symlink():return result
    try:config=json.loads(profile.read_text())
    except (ValueError,OSError):return result
    for service,user in [('github','x-access-token'),('gitlab','oauth2')]:
        url=urlparse(config.get(service,''));repository=url.path.strip('/')
        if url.scheme!='https' or not url.hostname or url.username or url.password or not repository:continue
        selector=config.get('credentials',{}).get(service,{})
        result[url.netloc]={'repository':repository,'username':user,'vault':selector.get('vault','Private'),'item':selector.get('item',url.hostname),'field':selector.get('field',repository+'-rw')}
    return result

def credentials(request):
    host=request.get('host','')
    path=request.get('path','').strip('/').removesuffix('.git')
    selected=destinations().get(host)
    if request.get('protocol')!='https' or not selected or path!=selected['repository']:return {}
    try:
        result=subprocess.run(['op','item','get',selected['item'],'--vault',selected['vault'],'--fields','label='+selected['field'],'--reveal'],capture_output=True,text=True)
    except OSError:return {'quit':'1'}
    token=result.stdout.strip()
    if result.returncode or not token or any(c.isspace() for c in token):return {'quit':'1'}
    return {'username':selected['username'],'password':token}

def main():
    action=sys.argv[1] if len(sys.argv)>1 else ''
    if action=='--install':
        for args in [('credential.useHttpPath','true'),('--replace-all','credential.helper',''),('--add','credential.helper','!'+shlex.join(['python3',str(Path(__file__).resolve())]))]:
            subprocess.run(['git','-C',str(ROOT),'config','--local',*args],check=True)
        print('Configured repository-local Git authentication through 1Password. No token stored in Git configuration.')
        return
    if action!='get':return
    request=dict(line.rstrip('\n').split('=',1) for line in sys.stdin if '=' in line)
    for key,value in credentials(request).items():print(key+'='+value)
    print()
if __name__=='__main__':main()
