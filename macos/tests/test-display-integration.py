#!/usr/bin/env python3
"""Optional live display integration: opens a separate game and switches fullscreen."""
from pathlib import Path
import tempfile,os,subprocess,time,signal,socket,json,re,struct,plistlib
root=Path(__file__).resolve().parents[2];engine=root/'build-mac-universal/keeperfx';prefs=Path(tempfile.mkdtemp(prefix='kfx-display-integration-'));env=dict(os.environ,KFX_USER_DATA_DIR=str(prefs),KFX_AUTOSAVE_SECONDS='0')
app=Path(os.environ.get('KFX_TEST_APP','/Applications/Keeper-Enhanced.app' if Path('/Applications/Keeper-Enhanced.app').exists() else '/Applications/KeeperFX.app'))
launcher=app/'Contents/MacOS'/plistlib.loads((app/'Contents/Info.plist').read_bytes())['CFBundleExecutable']
subprocess.run([str(launcher),'--import',str(root/'.local/keeperfx')],env=env,check=True);subprocess.run([str(launcher),'--prepare-only'],env=env,check=True)
runtime=prefs/'GameData';cfg=runtime/'keeperfx.cfg';text=cfg.read_text();sock=socket.socket();sock.bind(('127.0.0.1',0));port=sock.getsockname()[1];sock.close()
for key,value in {'API_ENABLED':'TRUE','API_PORT':str(port),'FRONTEND_RES':'640x480w32 1280x800w32 1280x800w32','INGAME_RES':'1280x800w32'}.items():text=re.sub(r'(?m)^'+key+r'=.*$',key+'='+value,text)
cfg.write_text(text)
def run(frontend):
 args=[str(runtime/'keeperfx'),'-nointro','-log','display-frontend.log' if frontend else 'display-game.log']+([] if frontend else ['-level','1'])
 p=subprocess.Popen(args,executable=str(engine),env=env,cwd=runtime,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,start_new_session=True)
 try:
  connection=None
  for _ in range(120):
   assert p.poll() is None,('early exit',p.returncode)
   try:connection=socket.create_connection(('127.0.0.1',port),timeout=.2);break
   except OSError:time.sleep(.2)
  assert connection;connection.settimeout(5);time.sleep(4)
  def cmd(command):
   connection.sendall((json.dumps({'action':'console_command','player':0,'command':command,'ack':1})+'\n').encode());reply=json.loads(connection.recv(8192));assert reply.get('success'),reply;return reply
  if frontend:
   connection.sendall((json.dumps({'action':'get_kfx_info','ack':9})+'\n').encode());assert json.loads(connection.recv(8192)).get('success');assert p.poll() is None;print('PASS: frontend starts with persisted display settings and remains responsive',flush=True);return
  for index,(mode,width,height) in enumerate([(0,800,600),(1,1280,800),(0,1280,800)]):
   cmd(f'mac.display {mode} {width} {height}');time.sleep(3);assert p.poll() is None;cmd('mac.display.keep')
   prefs_text=(prefs/'save/macos-options.cfg').read_text();assert f'display_mode={mode}' in prefs_text
   cmd('stats')
   if not frontend:
    cmd(f'game.save {index+6} Display_QA');time.sleep(.5);png=(prefs/f'save/fx1g{index+6:04d}.sav.png').read_bytes();dimensions=struct.unpack('>II',png[16:24]);assert dimensions==(width,height) if mode==0 else dimensions[0]>=800;print('Real rendered dimensions:',dimensions,flush=True)
  if not frontend:
   cmd('mac.display 0 800 600');time.sleep(17);assert p.poll() is None;cmd('game.save 9 Rollback_QA');time.sleep(.5);png=(prefs/'save/fx1g0009.sav.png').read_bytes();assert struct.unpack('>II',png[16:24])==(1280,800);print('PASS: real 15-second display timeout restores the previous 1280x800 mode',flush=True)
  print('PASS: actual '+('frontend menus' if frontend else 'dungeon')+' switches windowed 800x600 → desktop fullscreen → windowed 1280x800, stays responsive, and persists display settings',flush=True)
 finally:
  if p.poll() is None:
   os.killpg(p.pid,signal.SIGTERM)
   try:p.wait(timeout=5)
   except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait()
run(False)
placement=prefs/'save/window-position.cfg';assert placement.is_file();coordinates=placement.read_text().split();assert len(coordinates)==2;assert all(-100000<int(value)<100000 for value in coordinates)
run(True)
print('PASS: window placement is persisted in the isolated profile and frontend restoration starts successfully',flush=True)
print('QA profile:',prefs)
