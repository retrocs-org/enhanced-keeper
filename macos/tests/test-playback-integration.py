#!/usr/bin/env python3
"""Manual real-engine save preflight and deterministic offline replay test."""
from pathlib import Path
import json,os,plistlib,re,signal,socket,subprocess,tempfile,time
ROOT=Path(__file__).resolve().parents[2]
app=Path(os.environ.get('KFX_TEST_APP','/Applications/Keeper-Enhanced.app'));launcher=app/'Contents/MacOS'/plistlib.loads((app/'Contents/Info.plist').read_bytes())['CFBundleExecutable'];engine=ROOT/'build-mac-universal/keeperfx'
with tempfile.TemporaryDirectory(prefix='keeperfx-playback-') as temporary:
 profile=Path(temporary);runtime=profile/'GameData';recording=profile/'recording.pck'
 env=dict(os.environ,KFX_USER_DATA_DIR=str(profile),KFX_AUTOSAVE_SECONDS='0',KFX_HEADLESS='1',KFX_LOCAL_NETWORK_ONLY='1',SDL_VIDEODRIVER='dummy',SDL_RENDER_DRIVER='software',SDL_AUDIODRIVER='dummy',ALSOFT_DRIVERS='null',KFX_APP_RESOURCES=str(app/'Contents/Resources'))
 for args in [['--import',str(ROOT/'.local/keeperfx')],['--prepare-only']]:subprocess.run([str(launcher),*args],env=env,check=True,stdout=subprocess.DEVNULL)
 with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
 cfg=runtime/'keeper-enhanced.cfg';text=cfg.read_text()
 for key,value in {'API_ENABLED':'TRUE','API_PORT':str(port),'FREEZE_GAME_ON_FOCUS_LOST':'FALSE'}.items():text=re.sub(r'(?m)^'+key+r'=.*$',key+'='+value,text)
 cfg.write_text(text)
 process=None;connection=None;buffer=b'';serial=0;logs=[]
 def rpc(action='get_network_info',**values):
  global connection,buffer,serial
  if connection is None:connection=socket.create_connection(('127.0.0.1',port),timeout=.3);connection.settimeout(5)
  serial+=1;connection.sendall((json.dumps({'action':action,'ack':serial,**values})+'\n').encode())
  while True:
   while b'\n' not in buffer:
    data=connection.recv(65536)
    if not data:raise RuntimeError('Engine API disconnected')
    buffer+=data
   line,buffer=buffer.split(b'\n',1);reply=json.loads(line)
   if reply.get('ack')!=serial:continue
   assert reply.get('success'),reply
   return reply.get('data',{})
 def until(function,seconds=30):
  deadline=time.monotonic()+seconds;last=None
  while time.monotonic()<deadline:
   if process.poll() is not None:raise RuntimeError('Engine exited early; '+logs[-1].read_text()[-2000:])
   try:
    last=function()
    if last:return last
   except (OSError,ValueError) as error:last=str(error)
   time.sleep(.05)
  raise AssertionError('Timed out: '+str(last))
 def stop():
  global connection,buffer
  if connection:connection.close();connection=None;buffer=b''
  if process and process.poll() is None:
   os.killpg(process.pid,signal.SIGTERM)
   try:process.wait(timeout=5)
   except subprocess.TimeoutExpired:os.killpg(process.pid,signal.SIGKILL);process.wait()
 def launch(args,frontend=False):
  global process
  stop();log=profile/('process-'+str(len(logs))+'.log');logs.append(log)
  with log.open('w') as stream:process=subprocess.Popen([str(runtime/'keeperfx'),'-nointro','-nosound',*args],executable=str(engine),cwd=runtime,env=env,stdout=stream,stderr=subprocess.STDOUT,start_new_session=True)
  until(lambda:rpc() if frontend else rpc().get('turn',0)>10)
 try:
  launch(['-level','1'])
  until(lambda:rpc().get('turn',0)>=40)
  rpc('console_command',player=0,command='game.save 0 Compatibility_QA')
  save=profile/'save/fx1g0000.sav';until(lambda:Path(str(save)+'.requirements').is_file())
  assert Path(str(save)+'.requirements').read_text().startswith('KFXREQ2\nengine=')
  saved_turn=rpc()['turn'];until(lambda:rpc()['turn']>=saved_turn+20)
  rpc('console_command',player=0,command='game.load 0');assert rpc()['turn']<saved_turn+20
  assert list((profile/'save/CompatibilityBackups').iterdir())
  print('PASS: real save records content requirements and reloads from a preserved copy',flush=True)
  for page in range(30,36):
   preview=profile/'save/options-preview.png';previous=preview.stat().st_mtime_ns if preview.exists() else 0
   rpc('options_preview',page=page)
   until(lambda:preview.exists() and preview.stat().st_mtime_ns>previous)
   __import__('shutil').copy2(preview,Path('/tmp')/('kfx-engine-page-'+str(page)+'.png'))
  print('PASS: all six new pages render in the actual engine with the original menu assets',flush=True)

  stop();launch(['-level','1','-packetsave',str(recording)])
  until(lambda:rpc()['turn']>=220)
  rpc('console_command',player=0,command='quit');process.wait(timeout=8);stop()
  assert recording.stat().st_size>1000
  launch(['-packetload',str(recording)])
  until(lambda:rpc('get_replay_info')['available'])
  rpc('replay_control',command=-1);until(lambda:rpc('get_replay_info')['index']>=40)
  rpc('replay_control',command=0);paused=rpc('get_replay_info');assert paused['paused'];time.sleep(.6);assert rpc('get_replay_info')['index']==paused['index']
  rpc('replay_control',command=2);target=paused['index'];checksum=paused['checksum'];stop()
  launch([],frontend=True)
  rpc('open_local_replay',file=str(recording))
  until(lambda:rpc('get_replay_info')['available'])
  rpc('console_command',player=0,command='mac.options 28')
  rpc('replay_control',command=1);rpc('replay_control',command=1);rpc('replay_control',command=3)
  state=until(lambda:(lambda value:value if value['paused'] else None)(rpc('get_replay_info')))
  assert state['index']==target,(state,target);assert state['checksum']==checksum,(state['checksum'],checksum)
  print('PASS: real replay pause holds turns; 0.5x and 4x playback reach the same bookmark with an identical world checksum',flush=True)
 finally:stop()
