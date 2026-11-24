#!/usr/bin/env python3
"""Real loopback hub + headless host + game browser join + read-only spectator."""
from pathlib import Path
import importlib.util,json,os,re,signal,socket,subprocess,tempfile,threading,time,traceback,plistlib,shutil,urllib.request,urllib.error,sys
from http.cookies import SimpleCookie
root=Path(__file__).resolve().parents[2]
def module(name,file):
 s=importlib.util.spec_from_file_location(name,file);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
sys.path.insert(0,str(root/'services/keeperhub'))
from relay import Relay
from relay_worker import serve_control
ded=module('dedicated',root/'services/keeperhub/dedicated.py')
work=Path(tempfile.mkdtemp(prefix='kfx-community-e2e-'));Path('/tmp/kfx-community-e2e-profile').write_text(str(work));relay=Relay();admin_key='test-'+os.urandom(24).hex();control_key='control-'+os.urandom(24).hex();http=serve_control(relay,control_key);threading.Thread(target=http.serve_forever,daemon=True).start()
with socket.socket() as sock:sock.bind(('127.0.0.1',0));web_port=sock.getsockname()[1]
base='http://127.0.0.1:'+str(web_port);web=root/'public-web';web_process=subprocess.Popen([shutil.which('php'),'-S','127.0.0.1:'+str(web_port),'-t',str(web),str(web/'router.php')],env=dict(os.environ,KFX_DB_CONFIG='',KFX_DB_DSN='',KFX_DB_USER='',KFX_DB_PASSWORD='',KFX_HUB_DB='',KFX_PUBLIC_ORIGIN=base,KFX_HUB_STATE=str(work/'service'),KFX_HUB_ADMIN_KEY=admin_key,KFX_RELAY_CONTROL='http://127.0.0.1:'+str(http.server_port),KFX_RELAY_CONTROL_KEY=control_key),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
class PublicPHP:
 admin_key=admin_key
 def handle(self,method,path,data,token=''):
  headers={'Content-Type':'application/json'}
  if token:headers['Authorization']='Bearer '+token
  request=urllib.request.Request(base+path,data=json.dumps(data).encode() if method=='POST' else None,headers=headers,method=method)
  with urllib.request.urlopen(request,timeout=6) as response:
   value=json.load(response)
   if path in ('/api/register','/api/login'):
    jar=SimpleCookie();jar.load(response.headers.get('Set-Cookie',''));value['token']=jar['kfx_session'].value
   return value
hub=PublicPHP()
for attempt in range(100):
 try:
  assert hub.handle('GET','/api/health',{})['http_backend']=='php';break
 except (OSError,AssertionError):time.sleep(.05)
else:raise RuntimeError('Native PHP API did not start')
processes=[];supervisor=None;hostthread=None

app=Path(os.environ.get('KFX_TEST_APP','/Applications/KeeperFX-E.app'));launcher=app/'Contents/MacOS'/plistlib.loads((app/'Contents/Info.plist').read_bytes())['CFBundleExecutable'];engine=root/'build-mac-universal/keeperfx'
def port(udp=False):
 with socket.socket(socket.AF_INET,socket.SOCK_DGRAM if udp else socket.SOCK_STREAM) as s:s.bind(('127.0.0.1',0));return s.getsockname()[1]
def profile(name):
 p=work/name;env=dict(os.environ,KFX_USER_DATA_DIR=str(p));subprocess.run([str(launcher),'--import',str(root/'.local/keeperfx')],env=env,check=True,stdout=subprocess.DEVNULL);subprocess.run([str(launcher),'--prepare-only'],env=env,check=True,stdout=subprocess.DEVNULL);return p
def config_api(p,apiport):
 f=p/'GameData/keeperfx.cfg';s=f.read_text();s=re.sub(r'(?m)^API_ENABLED=.*$','API_ENABLED=TRUE',s);s=re.sub(r'(?m)^API_PORT=.*$',f'API_PORT={apiport}',s);f.write_text(s)
connections={};buffers={};serial=0
def rpc(apiport,action='get_network_info',**kwargs):
 global serial
 if apiport not in connections:connections[apiport]=socket.create_connection(('127.0.0.1',apiport),timeout=2);buffers[apiport]=b''
 c=connections[apiport];c.settimeout(8);serial+=1;ack=serial;c.sendall((json.dumps(dict(action=action,ack=ack,**kwargs))+'\n').encode())
 while True:
  while b'\n' not in buffers[apiport]:
   part=c.recv(4096)
   if not part:raise ConnectionError('Engine API disconnected')
   buffers[apiport]+=part
   assert len(buffers[apiport])<65536
  line,buffers[apiport]=buffers[apiport].split(b'\n',1);v=json.loads(line)
  if v.get('ack')!=ack:continue
  assert v.get('success'),v;return v.get('data',{})

def until(fn,seconds=40):
 deadline=time.monotonic()+seconds;last=None
 while time.monotonic()<deadline:
  try:
   last=fn()
   if last:return last
  except (OSError,ValueError,AssertionError) as e:last=str(e)
  time.sleep(.3)
 raise AssertionError('Timed out: '+str(last))
def launch(p,apiport,token=None):
 config_api(p,apiport)
 if token:
  me=hub.handle('GET','/api/me',{},token);(p/'save/community-session.cfg').write_text(base+'\n'+token+'\n'+me['name']+'\n'+me['id']+'\n');(p/'save/community-session.cfg').chmod(0o600)
 else:(p/'save/community-url.cfg').write_text(base)
 env=dict(os.environ,KFX_USER_DATA_DIR=str(p),KFX_HEADLESS='1',KFX_LOCAL_NETWORK_ONLY='1',SDL_VIDEODRIVER='dummy',SDL_RENDER_DRIVER='software',SDL_AUDIODRIVER='dummy',ALSOFT_DRIVERS='null',KFX_APP_RESOURCES=str(app/'Contents/Resources'))
 log=open(p/'stdout.log','w');proc=subprocess.Popen([str(p/'GameData/keeperfx'),'-nointro','-nosound','-nick','LocalTester','-log','community.log'],executable=str(engine),env=env,cwd=p/'GameData',stdout=log,stderr=subprocess.STDOUT,start_new_session=True);processes.append((proc,log));until(lambda:rpc(apiport,'community_status'));return proc
try:
 hostprofile=profile('host');hostport=port(True);cfg=dict(name='Local Dedicated QA',engine_binary=str(engine),user_data=str(hostprofile),hub_url=base,endpoint='127.0.0.1:1',port=hostport,rotation=[{'campaign':'classic','level':52}],humans=1,spectators=True,app_resources=str(app/'Contents/Resources'));os.environ['KFX_HUB_ADMIN_KEY']=hub.admin_key;supervisor=ded.Host(cfg);hosterrors=[]
 def run_host():
  try:supervisor.run()
  except Exception:hosterrors.append(traceback.format_exc())
 hostthread=threading.Thread(target=run_host,daemon=True);hostthread.start();until(lambda:hub.handle('GET','/api/servers',{},'')['servers']);until(lambda:supervisor.status.get('active'))
 with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as query:
  query.settimeout(2);query.sendto(b'KFXQ1 local-probe',('127.0.0.1',hostport));assert query.recvfrom(100)[0]==b'KFXA1 local-probe'
 print('PASS: server latency query does not allocate a player',flush=True)
 user=hub.handle('POST','/api/register',{'name':'LocalKeeper','password':'local game test password'});clientprofile=profile('client');clientport=port();client=launch(clientprofile,clientport,user['token']);rpc(clientport,'community_action',command=2000);until(lambda:rpc(clientport,'community_status').get('servers')==1);rpc(clientport,'community_action',command=2010);rpc(clientport,'community_action',command=2040)
 # Advertised direct port is deliberately unreachable: the same game browser must fall back to relay.
 until(lambda:rpc(clientport).get('turn',0)>=20,seconds=90);status=rpc(clientport);assert status['active'] and status['dedicated_player']==0,status;until(lambda:any(v.get('account') for v in supervisor.status.get('users',[])));assert relay.channels,'No relay channel was created';print('PASS: actual browser direct attempt falls back to authenticated ENet relay; dedicated AI and cloud identity agree',flush=True)
 if os.environ.get('KFX_TEST_RECONNECT')=='1':
  old=rpc(clientport);old_users=[(v.get('peer'),v.get('account')) for v in supervisor.status.get('users',[]) if v.get('account')]
  os.kill(client.pid,signal.SIGSTOP)
  try:time.sleep(35)
  finally:os.kill(client.pid,signal.SIGCONT)
  restored=until(lambda:(lambda value:value if value.get('active') and value.get('turn',0)>old['turn']+10 else None)(rpc(clientport)),seconds=90)
  assert supervisor.status.get('players')==2,supervisor.status
  assert old_users==[(v.get('peer'),v.get('account')) for v in supervisor.status.get('users',[]) if v.get('account')]
  print('PASS: 35-second lost client resumes its authenticated player slot and simulation without restarting the match',flush=True)
 until(lambda:hub.handle('GET','/api/servers/'+supervisor.status['server_id']+'/spectate',{},'').get('size',0)>0)
 spectatorprofile=profile('spectator');spectatorport=port();spectator=launch(spectatorprofile,spectatorport);rpc(spectatorport,'community_action',command=2000);until(lambda:rpc(spectatorport,'community_status').get('servers')==1);rpc(spectatorport,'community_action',command=2010);rpc(spectatorport,'community_action',command=2045)
 watch=until(lambda:(lambda x:x if x.get('spectating') and x.get('turn',0)>=10 else None)(rpc(spectatorport)),seconds=60);assert not watch['active'],watch;before=watch['turn'];until(lambda:rpc(spectatorport).get('turn',0)>before+10,seconds=30);assert supervisor.status.get('players')==2,supervisor.status;print('PASS: real replay spectator advances without consuming a player slot',flush=True);assert not hosterrors,hosterrors
 print('Evidence profile:',work,flush=True)
finally:
 if __import__('sys').exc_info()[0]:
  try:print('Client status:',rpc(clientport,'community_status') if 'clientport' in globals() else {},flush=True)
  except OSError:pass
  print('Host errors:',hosterrors if 'hosterrors' in globals() else [],flush=True)
  print('Relay diagnostic:',[(bool(c['host']),bool(c['client']),c['bytes']) for c in relay.channels.values()],flush=True)
 for c in connections.values():c.close()
 if supervisor:supervisor.stop=True
 for proc,log in processes:
  if proc.poll() is None:
   os.killpg(proc.pid,signal.SIGTERM)
   try:proc.wait(10)
   except subprocess.TimeoutExpired:os.killpg(proc.pid,signal.SIGKILL);proc.wait(5)
  log.close()
 if supervisor:supervisor.finish_child()
 if hostthread:hostthread.join(10)
 http.shutdown();http.server_close();relay.close();web_process.terminate();web_process.wait(timeout=5)
