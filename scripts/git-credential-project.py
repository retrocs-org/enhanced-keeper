#!/usr/bin/env python3
"""Repository-local Git credential bridge to concealed 1Password token fields."""
from pathlib import Path
import shlex
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
REPOSITORY='retrocs-org/keeper-enhanced'
HOSTS={'github.com':'x-access-token','gitlab.kladhest.se':'oauth2'}

def credentials(request):
    host=request.get('host','')
    path=request.get('path','').strip('/').removesuffix('.git')
    if request.get('protocol')!='https' or host not in HOSTS or path!=REPOSITORY:return {}
    try:
        result=subprocess.run(['op','item','get',host,'--vault','Private','--fields','label='+REPOSITORY+'-rw','--reveal'],capture_output=True,text=True)
    except OSError:return {'quit':'1'}
    token=result.stdout.strip()
    if result.returncode or not token or any(c.isspace() for c in token):return {'quit':'1'}
    return {'username':HOSTS[host],'password':token}

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
