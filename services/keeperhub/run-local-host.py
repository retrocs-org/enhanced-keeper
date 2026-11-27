#!/usr/bin/env python3
"""Start a local dedicated host without putting its administrator key in argv."""
import argparse,json,os,subprocess,sys
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('config',type=Path);p.add_argument('--stop',action='store_true');p.add_argument('--next',action='store_true');p.add_argument('--status',action='store_true');p.add_argument('--state',type=Path,default=Path('.local/keeperhub'));a=p.parse_args();config=json.loads(a.config.read_text());control=Path(config['user_data']).resolve()/'dedicated'
if a.stop or a.next:
    control.mkdir(mode=0o700,parents=True,exist_ok=True);(control/('stop.request' if a.stop else 'next.request')).touch();print('Local host control requested.');raise SystemExit(0)
if a.status:
    file=control/'status.json';print(file.read_text() if file.exists() else 'No local host status yet.');raise SystemExit(0)
key=a.state/'admin.key'
env=dict(os.environ)
if config.get('visibility','public')!='private' and key.is_file():env['KFX_HUB_ADMIN_KEY']=key.read_text().strip()
raise SystemExit(subprocess.call([sys.executable,str(Path(__file__).with_name('dedicated.py')),str(a.config)],env=env))
