#!/usr/bin/env python3
"""Exercise refresh swaps/rollback with fake build and systemd commands in a temp tree."""
import json,os,re,shutil,subprocess,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
FAKE=r'''#!/usr/bin/env python3
import json,os,sys
from pathlib import Path
name=Path(sys.argv[0]).name;a=sys.argv[1:];r=Path(os.environ['TEST_ROOT']);events=r/'events'
if name=='id':print('0')
elif name=='uname':print('Linux')
elif name=='git':
 if '--show-current' in a:print('main')
elif name=='pkg-config':pass
elif name=='ldd':
 if os.environ.get('FAIL_MEDIA'):print('libopenal.so => /lib/libopenal.so')
elif name=='flock':pass
elif name=='nproc':print('2')
elif name=='runuser':os.execvp(a[3],a[3:])
elif name=='cmake':
 if '--build' in a:
  assert a[a.index('--target')+1]=='keeper-enhanced', 'Wrong server build target'
  assert a[a.index('--config')+1]=='Release', 'Wrong build configuration'
 if '--build' in a and os.environ.get('FAIL_BUILD'):sys.exit(1)
 if '--install' in a:
  dest=Path(a[a.index('--prefix')+1]);dest.mkdir(parents=True);p=dest/'keeper-enhanced';p.write_text('#!/bin/sh\n# new-engine\n[ "$1" = --version ] || exit 1\n[ -z "$FAIL_VERSION" ] || exit 1\necho "Keeper Enhanced dedicated server"\n');p.chmod(0o755)
elif name=='systemctl':
 with events.open('a') as f:f.write(' '.join(a)+'\n')
 if a[0]=='show':
  if 'FragmentPath' in a:print(r/'etc/systemd/system/keeper-enhanced-server.service')
  elif 'User' in a:print('keeper-enhanced')
  else:print('ActiveState=active\nMainPID=123')
 elif a[0]=='start':
  marker=r/'failed-once'
  if os.environ.get('FAIL_START') and not marker.exists():marker.touch();sys.exit(1)
  state=r/'var/lib/keeper-enhanced/dedicated';state.mkdir(exist_ok=True);(state/'status.json').write_text(json.dumps({'active':True}))
'''
class RefreshTests(unittest.TestCase):
 def run_refresh(self,fail=None):
  self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);r=Path(self.tmp.name);self.root=r
  for folder in ('bin','run/lock','opt/keeper-enhanced-server/engine','etc/keeper-enhanced','etc/systemd/system','var/lib/keeper-enhanced/GameData'):(r/folder).mkdir(parents=True,exist_ok=True)
  (r/'var/lib/keeper-enhanced/GameData/keeper-enhanced.cfg').write_text('game-data')
  self.dest=r/'opt/keeper-enhanced-server';(self.dest/'engine/keeper-enhanced').write_text('old-engine');(self.dest/'engine/keeper-enhanced').chmod(0o755);(self.dest/'dedicated.py').write_text('old-supervisor')
  self.unit=r/'etc/systemd/system/keeper-enhanced-server.service';self.unit.write_text('custom-unit')
  self.config=r/'etc/keeper-enhanced/server.json';self.config.write_text(json.dumps({'engine_binary':str(self.dest/'engine/keeper-enhanced'),'user_data':str(r/'var/lib/keeper-enhanced'),'visibility':'private','name':'test','rotation':[{'campaign':'classic','level':52}]}))
  self.secret=r/'etc/keeper-enhanced/server.env';self.secret.write_text('private-secret')
  (r/'source/.git').mkdir(parents=True);(r/'source/services/keeperhub').mkdir(parents=True);(r/'source/linux').mkdir();(r/'source/docs').mkdir()
  shutil.copy(ROOT/'services/keeperhub/dedicated.py',r/'source/services/keeperhub/dedicated.py')
  for p in (ROOT/'linux').iterdir():
   if p.is_file():shutil.copy(p,r/'source/linux'/p.name)
  shutil.copy(ROOT/'docs/LINUX-SERVER.md',r/'source/docs/LINUX-SERVER.md');shutil.copy(ROOT/'LICENSE',r/'source/LICENSE')
  for tool in ('id','uname','git','pkg-config','ldd','flock','nproc','runuser','cmake','systemctl','patchelf'):
   p=r/'bin'/tool;p.write_text(FAKE);p.chmod(0o755)
  script=(ROOT/'scripts/refresh-keeper-enhanced.sh').read_text()
  script=re.sub(r'/(?:opt|etc|var|run)/',lambda m:str(r)+m[0],script)
  path=r/'refresh.sh';path.write_text(script)
  env={**os.environ,'PATH':str(r/'bin')+':'+os.environ['PATH'],'TEST_ROOT':str(r),'KFX_SERVER_SOURCE':str(r/'source')}
  if fail:env[fail]='1'
  result=subprocess.run(['bash',str(path)],env=env,text=True,capture_output=True,timeout=20)
  return result
 def test_success_preserves_config_data_and_unit(self):
  result=self.run_refresh();self.assertEqual(result.returncode,0,result.stderr)
  self.assertIn('# new-engine', (self.dest/'engine/keeper-enhanced').read_text());self.assertEqual(self.unit.read_text(),'custom-unit');self.assertEqual(self.secret.read_text(),'private-secret')
  self.assertEqual((self.root/'var/lib/keeper-enhanced/GameData/keeper-enhanced.cfg').read_text(),'game-data')
 def test_media_dependency_is_rejected_before_stop(self):
  result=self.run_refresh('FAIL_MEDIA');self.assertNotEqual(result.returncode,0)
  self.assertEqual((self.dest/'engine/keeper-enhanced').read_text(),'old-engine');self.assertNotIn('stop ',(self.root/'events').read_text())
 def test_version_failure_leaves_running_installation(self):
  result=self.run_refresh('FAIL_VERSION');self.assertNotEqual(result.returncode,0)
  self.assertEqual((self.dest/'engine/keeper-enhanced').read_text(),'old-engine');self.assertNotIn('stop ',(self.root/'events').read_text())
 def test_build_failure_leaves_running_installation(self):
  result=self.run_refresh('FAIL_BUILD');self.assertNotEqual(result.returncode,0)
  self.assertEqual((self.dest/'engine/keeper-enhanced').read_text(),'old-engine');self.assertNotIn('stop ',(self.root/'events').read_text())
 def test_start_failure_restores_old_bundle(self):
  result=self.run_refresh('FAIL_START');self.assertNotEqual(result.returncode,0)
  self.assertEqual((self.dest/'engine/keeper-enhanced').read_text(),'old-engine');self.assertEqual((self.dest/'dedicated.py').read_text(),'old-supervisor');self.assertEqual(self.unit.read_text(),'custom-unit');self.assertEqual(self.secret.read_text(),'private-secret')
if __name__=='__main__':unittest.main()
