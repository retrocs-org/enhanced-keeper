#!/usr/bin/env python3
"""Real loopback hub + headless host + game browser join + read-only spectator."""
from pathlib import Path
import importlib.util,json,os,re,signal,socket,subprocess,tempfile,threading,time,traceback,plistlib,shutil,urllib.request,urllib.error,sys
from http.cookies import SimpleCookie
root=Path(__file__).resolve().parents[2]
def module(name,file):
 s=importlib.util.spec_from_file_location(name,file);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
sys.path.insert(0,str(root/'services/keeperhub'))
sys.path.insert(0,str(Path(__file__).resolve().parent/'relay-fixtures'))
from relay import Relay
from relay_worker import serve_control
ded=module('dedicated',root/'services/keeperhub/dedicated.py')
work=Path(tempfile.mkdtemp(prefix='kfx-community-e2e-'));Path('/tmp/kfx-community-e2e-profile').write_text(str(work));relay=Relay();admin_key='test-'+os.urandom(24).hex();control_key='control-'+os.urandom(24).hex();http=serve_control(relay,control_key);threading.Thread(target=http.serve_forever,daemon=True).start()
with socket.socket() as sock:sock.bind(('127.0.0.1',0));web_port=sock.getsockname()[1]
base='http://127.0.0.1:'+str(web_port);web=root/'public-web';web_process=subprocess.Popen([shutil.which('php'),'-S','127.0.0.1:'+str(web_port),'-t',str(web),str(web/'router.php')],env=dict(os.environ,KFX_DB_CONFIG='',KFX_DB_DSN='',KFX_DB_USER='',KFX_DB_PASSWORD='',KFX_HUB_DB=str(work/'service/community.sqlite3'),KFX_PUBLIC_ORIGIN=base,KFX_HUB_STATE=str(work/'service'),KFX_HUB_ADMIN_KEY=admin_key,KFX_RELAY_CONTROL='http://127.0.0.1:'+str(http.server_port),KFX_RELAY_CONTROL_KEY=control_key),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
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

app=Path(os.environ.get('KFX_TEST_APP','/Applications/Keeper-Enhanced.app'));launcher=app/'Contents/MacOS'/plistlib.loads((app/'Contents/Info.plist').read_bytes())['CFBundleExecutable'];engine=Path(os.environ.get('KFX_TEST_CLIENT_ENGINE',str(app/'Contents/Resources/game/keeper-enhanced'))).resolve()
def port(udp=False):
 with socket.socket(socket.AF_INET,socket.SOCK_DGRAM if udp else socket.SOCK_STREAM) as s:s.bind(('127.0.0.1',0));return s.getsockname()[1]
def profile(name):
 p=work/name;env=dict(os.environ,KFX_USER_DATA_DIR=str(p));subprocess.run([str(launcher),'--import',os.environ.get('KFX_TEST_IMPORT',str(root/'.local/keeperfx'))],env=env,check=True,stdout=subprocess.DEVNULL);subprocess.run([str(launcher),'--prepare-only'],env=env,check=True,stdout=subprocess.DEVNULL);return p
def config_api(p,apiport):
 f=p/'GameData/keeper-enhanced.cfg';s=f.read_text();s=re.sub(r'(?m)^API_ENABLED=.*$','API_ENABLED=TRUE',s);s=re.sub(r'(?m)^API_PORT=.*$',f'API_PORT={apiport}',s);f.write_text(s)
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
def launch(p,apiport,token=None,endpoint=None,client_engine=engine):
 config_api(p,apiport)
 if token:
  me=hub.handle('GET','/api/me',{},token);(p/'save/community-session.cfg').write_text(base+'\n'+token+'\n'+me['name']+'\n'+me['id']+'\n');(p/'save/community-session.cfg').chmod(0o600)
 elif not (p/'save/community-session.cfg').exists():(p/'save/community-url.cfg').write_text(base)
 env=dict(os.environ,KFX_USER_DATA_DIR=str(p),KFX_HEADLESS='1',KFX_LOCAL_NETWORK_ONLY='1',SDL_VIDEODRIVER='dummy',SDL_RENDER_DRIVER='software',SDL_AUDIODRIVER='dummy',ALSOFT_DRIVERS='null',KFX_APP_RESOURCES=str(app/'Contents/Resources'))
 log=open(p/'stdout.log','w');proc=subprocess.Popen([str(p/'GameData/keeperfx'),'-nointro','-nosound','-nick','LocalTester','-log','community.log',*(['-connect',endpoint] if endpoint else [])],executable=str(client_engine),env=env,cwd=p/'GameData',stdout=log,stderr=subprocess.STDOUT,start_new_session=True);processes.append((proc,log));until(lambda:rpc(apiport,'community_status'));return proc
coop_mode=os.environ.get('KFX_TEST_COOP')
test_coop=coop_mode in ('1','3')
coop_humans=3 if coop_mode=='3' else 2
manual_start=os.getenv('KFX_TEST_MANUAL_COOP')=='1'
test_vote=os.getenv('KFX_TEST_MAP_VOTE')=='1'
test_party=test_coop or os.environ.get('KFX_TEST_PARTY')=='1'
lobby_only=test_party or os.environ.get('KFX_TEST_LOBBY_ONLY')=='1'
try:
 hostprofile=profile('host');hostport=port(True);cfg=dict(name='Local Dedicated QA',engine_binary=os.environ.get('KFX_TEST_SERVER_ENGINE',str(engine)),user_data=str(hostprofile),hub_url=base,endpoint='127.0.0.1:'+str(hostport),visibility='public',port=hostport,rotation=([{'campaign':'original','level':53}] if test_vote else [])+[{'campaign':'original','level':52}],allow_party_map_votes=test_vote,manual_start=manual_start,humans=3 if manual_start else coop_humans if test_coop else 3 if test_party else 2 if lobby_only else 1,cooperative=test_coop,ai_style=os.getenv('KFX_TEST_AI_STYLE','defensive'),spectators=not lobby_only,app_resources=str(app/'Contents/Resources'));os.environ['KFX_HUB_ADMIN_KEY']=hub.admin_key;supervisor=ded.Host(cfg);hosterrors=[]
 def run_host():
  try:supervisor.run()
  except Exception:hosterrors.append(traceback.format_exc())
 hostthread=threading.Thread(target=run_host,daemon=True);hostthread.start();until(lambda:hub.handle('GET','/api/servers',{},'')['servers']);until(lambda:supervisor.status.get('active'))
 with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as query:
  query.settimeout(2);query.sendto(b'KFXQ1 local-probe',('127.0.0.1',hostport));assert query.recvfrom(100)[0]==b'KFXA1 local-probe'
 print('PASS: server latency query does not allocate a player',flush=True)
 user=hub.handle('POST','/api/register',{'name':'LocalKeeper','password':'local game test password'});clientprofile=profile('client');clientport=port();client=launch(clientprofile,clientport,user['token']);rpc(clientport,'community_action',command=2000);until(lambda:rpc(clientport,'community_status').get('servers')==1);rpc(clientport,'community_action',command=2010)
 if test_party:
  owner=hub.handle('GET','/api/me',{},user['token'])
  buddy=hub.handle('POST','/api/register',{'name':'PartyBuddy','password':'local game test password'})
  member=hub.handle('GET','/api/me',{},buddy['token'])
  hub.handle('POST','/api/friends',{'user':member['id']},user['token'])
  hub.handle('POST','/api/friends',{'user':owner['id']},buddy['token'])
  room=hub.handle('POST','/api/party',{'action':'invite','user':member['id']},user['token'])
  hub.handle('POST','/api/party',{'action':'accept','party':room['party']},buddy['token'])
  buddy_profile=profile('party-buddy')
  buddy_port=port()
  launch(buddy_profile,buddy_port,buddy['token'])
  until(lambda:rpc(clientport,'community_status')['party_members']==2)
  if test_vote:
   def party_call(token,action='state',**fields):
    return hub.handle('POST','/api/party',{'action':action,**fields},token)
   server_id=supervisor.status['server_id']
   opened=party_call(user['token'],'open-map-vote',server=server_id)
   assert [option['map'] for option in opened['ballot']['options']]==['original:53','original:52']
   party_call(user['token'],'map-vote',map='original:52')
   party_call(buddy['token'],'map-vote',map='original:52')
   party_call(user['token'],'confirm-map-vote',map='original:52')
   until(lambda:(party_call(user['token']).get('target') or {}).get('server',{}).get('map')=='original:52')
   assert supervisor.status.get('server_id')==server_id
   rpc(clientport,'community_action',command=2120)
  else:
   rpc(clientport,'community_action',command=2121)
  until(lambda:rpc(clientport,'community_status').get('party_ready'))
  rpc(clientport,'community_action',command=2135)
  until(lambda:rpc(buddy_port,'community_status')['party_prompt']==cfg['name'])
  rpc(buddy_port,'community_action',command=2120)
  rpc(buddy_port,'community_action',command=2128)
  until(lambda:rpc(buddy_port).get('active') and any(v.get('account')==member['id'] for v in supervisor.status.get('users',[])))
  def buddy_in_lobby():
   state=hub.handle('POST','/api/party',{'action':'state'},user['token'])
   return any(friend['id']==member['id'] and friend.get('status')=='lobby' for friend in state['friends']) and any(
    row['id']==member['id'] and row.get('join')=='In Lobby' for row in state['members'])
  until(buddy_in_lobby,seconds=20)
  print('PASS: the invited client joins through the party prompt and friends see its lobby status',flush=True)
 rpc(clientport,'community_action',command=2128 if test_party else 2040)
 if test_coop:
  ports=[clientport,buddy_port]
  if coop_humans==3:
   third=hub.handle('POST','/api/register',{'name':'ThirdKeeper','password':'local game test password'})
   third_profile=profile('third-client')
   third_port=port()
   launch(third_profile,third_port,third['token'])
   rpc(third_port,'community_action',command=2000)
   until(lambda:rpc(third_port,'community_status').get('servers')==1)
   rpc(third_port,'community_action',command=2010)
   rpc(third_port,'community_action',command=2040)
   ports.append(third_port)
  if manual_start:
   until(lambda:sum(bool(row.get('account')) for row in supervisor.status.get('users',[]))==2)
   assert all(rpc(apiport).get('turn',0)==0 for apiport in ports)
   supervisor.command('{"command":"start"}')
  until(lambda:all(rpc(apiport).get('turn',0)>20 for apiport in ports),seconds=75)
  expected_style={'defensive':0,'balanced':1,'rush':2}[cfg['ai_style']]
  assert all(rpc(apiport)['coop_human_team'] and
             rpc(apiport)['dedicated_ai_style']==expected_style for apiport in ports)
  print(f'PASS: {coop_humans} humans play on one team against the dedicated AI',flush=True)
 elif lobby_only:
  until(lambda:rpc(clientport).get('active') and any(user.get('account') for user in supervisor.status.get('users',[])))
  print('PASS: packaged client entered a public lobby with its cloud identity',flush=True)
 else:
  # The public listing must connect directly, without allocating a relay channel.
  until(lambda:rpc(clientport).get('turn',0)>=20,seconds=90);status=rpc(clientport);assert status['active'] and status['dedicated_player']==0,status;until(lambda:any(v.get('account') for v in supervisor.status.get('users',[])));assert not relay.channels,'Unexpected relay channel';print('PASS: public server joins directly; dedicated AI and cloud identity agree',flush=True)
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
 supervisor.stop=True
 hostthread.join(12)
 assert not hostthread.is_alive(), 'Public test host failed to stop'
 web_process.terminate()
 web_process.wait(timeout=5)
 private_profile=profile('private-host')
 private_port=port(True)
 supervisor=ded.Host(dict(cfg,visibility='private',user_data=str(private_profile),port=private_port,spectators=False))
 hostthread=threading.Thread(target=run_host,daemon=True)
 hostthread.start()
 until(lambda:supervisor.status.get('active'))
 old_client=os.environ.get('KFX_TEST_OLD_CLIENT_ENGINE')
 if old_client:
  old_profile=profile('old-client')
  launch(old_profile,port(),endpoint='127.0.0.1:'+str(private_port),client_engine=Path(old_client))
  until(lambda:'refused an unsupported Keeper Enhanced client version' in (private_profile/'dedicated.log').read_text())
  assert supervisor.status.get('players')==1
  print('PASS: a client without an allowlisted release is refused before lobby admission',flush=True)
 offline_profile=profile('offline-client')
 shutil.copyfile(clientprofile/'save/community-session.cfg',offline_profile/'save/community-session.cfg')
 offline_port=port()
 launch(offline_profile,offline_port,endpoint='127.0.0.1:'+str(private_port))
 if lobby_only:
  until(lambda:rpc(offline_port).get('active') and supervisor.status.get('players')==2)
  assert rpc(offline_port,'community_status')['account']=='LocalKeeper'
 else:
  until(lambda:rpc(offline_port).get('turn',0)>=20,seconds=60)
 assert not hosterrors,hosterrors
 print('PASS: remembered cloud identity can direct-connect to a private server while KeeperHub is offline',flush=True)
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
