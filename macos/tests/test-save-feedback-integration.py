from pathlib import Path
import os,subprocess,time,signal,socket,json,re,hashlib,tempfile,plistlib
root=Path(__file__).resolve().parents[2];prefs=Path(tempfile.mkdtemp(prefix='kfx-save-feedback-'));runtime=prefs/'GameData'
app=Path(os.environ.get('KFX_TEST_APP','/Applications/Keeper-Enhanced.app'));launcher=app/'Contents/MacOS'/plistlib.loads((app/'Contents/Info.plist').read_bytes())['CFBundleExecutable']
env=dict(os.environ,KFX_USER_DATA_DIR=str(prefs),KFX_AUTOSAVE_SECONDS='0',KFX_APP_RESOURCES=str(app/'Contents/Resources'))
subprocess.run([str(launcher),'--import',str(root/'.local/keeperfx')],env=env,check=True);subprocess.run([str(launcher),'--prepare-only'],env=env,check=True)
sock=socket.socket();sock.bind(('127.0.0.1',0));port=sock.getsockname()[1];sock.close();cfg=runtime/'keeperfx.cfg';text=cfg.read_text();text=re.sub(r'(?m)^API_PORT=.*$',f'API_PORT={port}',text);text=re.sub(r'(?m)^API_ENABLED=.*$','API_ENABLED=TRUE',text);text=re.sub(r'(?m)^FRONTEND_RES=.*$','FRONTEND_RES=640x480w32 800x600w32 800x600w32',text);text=re.sub(r'(?m)^INGAME_RES=.*$','INGAME_RES=800x600w32',text);cfg.write_text(text)
pending=prefs/'save/fx1g0020.sav.pending';pending.mkdir(exist_ok=True)
p=subprocess.Popen([str(runtime/'keeperfx'),'-nointro','-level','1','-log','polish-save-test.log'],executable=str(root/'build-mac-universal/keeperfx'),env=env,cwd=runtime,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,start_new_session=True)
try:
 connection=None
 for _ in range(100):
  assert p.poll() is None
  try:connection=socket.create_connection(('127.0.0.1',port),timeout=.2);break
  except OSError:time.sleep(.2)
 assert connection;connection.settimeout(5);time.sleep(3)
 def cmd(value):
  connection.sendall((json.dumps({'action':'console_command','command':value,'ack':1})+'\n').encode());return json.loads(connection.recv(8192))
 assert cmd('game.save 19 Polish_QA').get('success');time.sleep(1);saved=prefs/'save/fx1g0019.sav';assert saved.exists();digest=hashlib.sha256(saved.read_bytes()).hexdigest();assert (prefs/'save/fx1g0019.sav.png').exists()
 assert cmd('game.load 19').get('success'),'Newly written qualified-campaign save did not reload'
 assert not cmd('game.save 20 Failure_QA').get('success');assert hashlib.sha256(saved.read_bytes()).hexdigest()==digest
 preview=prefs/'save/options-preview.png';preview.unlink(missing_ok=True);assert cmd('mac.options 14 capture').get('success')
 for _ in range(100):
  if preview.exists() and preview.stat().st_size>1000:break
  time.sleep(.1)
 assert preview.exists();Path('/tmp/kfx-polish-save-error.png').write_bytes(preview.read_bytes());print('PASS: real manual save + preview; blocked write reports failure and preserves the existing save; recovery page rendered.',flush=True)
finally:
 if p.poll() is None:
  os.killpg(p.pid,signal.SIGTERM)
  try:p.wait(timeout=5)
  except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait()
 pending.rmdir()
