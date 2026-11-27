#!/usr/bin/env python3
"""Run the existing full KeeperHub contract against PHP, not Python business logic."""
import importlib.util,json,os,secrets,shutil,sqlite3,subprocess,sys,tempfile,threading,time,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'services/keeperhub'))
sys.path.insert(0,str(Path(__file__).resolve().parent/'relay-fixtures'))
from relay_worker import serve_control
from relay import Relay
spec=importlib.util.spec_from_file_location('contracts',ROOT/'macos/tests/test-keeperhub.py');suite=importlib.util.module_from_spec(spec);spec.loader.exec_module(suite)
class PHPHub:
 def __init__(self,db,relay=None,admin_key=None,clock=time.time):
  self.db=str(db);self.mysql=bool(os.environ.get('KFX_TEST_MYSQL_SOCKET'));self.connection={}
  if self.mysql:
   self.socket=os.environ['KFX_TEST_MYSQL_SOCKET'];self.database='kfx_contract_'+secrets.token_hex(8)
   subprocess.run(['mariadb','--no-defaults','--socket='+self.socket,'-uroot','-e','CREATE DATABASE `'+self.database+'` CHARACTER SET utf8mb4'],check=True,capture_output=True)
   self.db='mysql:unix_socket='+self.socket+';dbname='+self.database+';charset=utf8mb4';self.connection={'user':'root','password':'','migrate':True}
  self.relay=relay;self.admin_key=admin_key or secrets.token_urlsafe(32);self.clock=clock;self.key=secrets.token_urlsafe(32);self.control=serve_control(relay,self.key) if relay else None
  if self.control:self.thread=threading.Thread(target=self.control.serve_forever,daemon=True);self.thread.start()
 def close(self):
  if self.control:self.control.shutdown();self.control.server_close();self.thread.join(3)
  if self.mysql:subprocess.run(['mariadb','--no-defaults','--socket='+self.socket,'-uroot','-e','DROP DATABASE `'+self.database+'`'],check=True,capture_output=True)
 def connect(self):
  db=sqlite3.connect(self.db);db.row_factory=sqlite3.Row;return db
 def handle(self,method,path,data,token='',peer='local',csrf='',cookie=False):
  request=dict(db=self.db,connection=self.connection,admin=self.admin_key,now=self.clock(),relay='http://127.0.0.1:'+str(self.control.server_port) if self.control else '',relay_key=self.key,method=method,path=path,data=data,token=token,peer=peer,csrf=csrf,cookie=cookie)
  process=subprocess.run([shutil.which(os.environ.get('KFX_PHP','php')),str(ROOT/'macos/tests/php-api-driver.php')],input=json.dumps(request),text=True,capture_output=True,check=True,timeout=15)
  value=json.loads(process.stdout)
  if value['status']!=200:raise suite.h.Problem(value['status'],value.get('error','PHP API error'))
  return value['data']
class NativePHPContracts(suite.HubTests):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.clock=[time.time()];self.relay=Relay();self.hub=PHPHub(Path(self.tmp.name)/'test.sqlite3',self.relay,'admin-'+'x'*30,lambda:self.clock[0]);self.a=self.post('/api/register',{'name':'KeeperOne','password':'a long password 123'});self.b=self.post('/api/register',{'name':'KeeperTwo','password':'a long password 456'});self.me=self.get('/api/me',self.a['token']);self.other=self.get('/api/me',self.b['token'])
 def server(self,**kwargs):
  if kwargs.get('kind')!='dedicated':return super().server(**kwargs)
  host=self.post('/api/dedicated-hosts',{'action':'create','name':'Local Dungeon'},self.a['token'])
  return self.register_dedicated(host['credential'],**kwargs)
 def register_dedicated(self,credential,**fields):
  return self.post('/api/servers/register',dict(name='Local Dungeon',endpoint='localhost:5556',
   engine='1.4.0.0',kind='dedicated',**{k:v for k,v in fields.items() if k!='kind'}),credential)
 def test_cloud_host_ownership_and_delegation(self):
  owner,admin=self.a['token'],self.b['token']
  with self.assertRaises(suite.h.Problem) as error:self.register_dedicated('',admin_key=self.hub.admin_key)
  self.assertEqual(error.exception.status,401)
  with self.assertRaises(suite.h.Problem) as error:self.register_dedicated(owner)
  self.assertEqual(error.exception.status,403)
  provision=self.post('/api/dedicated-hosts',{'action':'create','name':'Owned Host'},owner)
  with self.assertRaises(suite.h.Problem) as error:self.get('/api/dedicated-hosts',provision['credential'])
  self.assertEqual(error.exception.status,401)
  listing=self.register_dedicated(provision['credential'])
  self.assertEqual(self.get('/api/dedicated-hosts',owner)['hosts'][0]['server'],listing['id'])
  self.assertEqual(self.get('/api/dedicated-hosts',admin)['hosts'],[])
  def change(action,token,name=None):
   body={'action':action,'host':provision['id']}
   if name:body['name']=name
   return self.post('/api/dedicated-hosts',body,token)
  change('add-admin',owner,'KeeperTwo')
  self.assertFalse(self.get('/api/dedicated-hosts',admin)['hosts'][0]['is_owner'])
  with self.assertRaises(suite.h.Problem) as error:change('add-admin',admin,'KeeperOne')
  self.assertEqual(error.exception.status,403)
  replacement=self.register_dedicated(provision['credential'])
  self.assertNotEqual(replacement['id'],listing['id'])
  self.assertTrue(change('next',admin)['ok'])
  route='/api/servers/'+replacement['id']+'/admin-command'
  self.assertEqual(self.post(route,{},replacement['secret'])['command'],'next')
  self.assertIsNone(self.post(route,{},replacement['secret'])['command'])
  change('start',admin)
  change('remove-admin',owner,'KeeperTwo')
  self.assertIsNone(self.post(route,{},replacement['secret'])['command'])
  with self.assertRaises(suite.h.Problem) as error:change('next',admin)
  self.assertEqual(error.exception.status,403)
  change('revoke',owner)
  with self.assertRaises(suite.h.Problem) as error:self.post(route,{},replacement['secret'])
  self.assertEqual(error.exception.status,403)
  with self.assertRaises(suite.h.Problem) as error:self.register_dedicated(provision['credential'])
  self.assertEqual(error.exception.status,403)
 def test_lobby_reconnect_holds_capacity_and_reports_match_health(self):
  owner,guest=self.a['token'],self.b['token']
  provision=self.post('/api/dedicated-hosts',{'action':'create','name':'Reconnect Host'},owner)
  server=self.register_dedicated(provision['credential'],client_versions=['0.1.0'],
   map='classic:52',coop={'humans':1,'ai_style':'defensive'})
  path='/api/servers/'+server['id']; secret=server['secret']
  def admit(account):
   ticket=self.post(path+'/join',{},account)['identity_ticket']
   return self.post(path+'/admit',{'peer':1,'occupied':1,'ticket':ticket},secret)
  first=admit(guest)
  self.post(path+'/heartbeat',{'players':1,'status':'lobby','occupied':1,'lobby_epoch':first['lobby_epoch']},secret)
  self.assertIsNone(self.get('/api/rejoin-lobby',owner)['server'])
  hold=self.get('/api/rejoin-lobby',guest)
  self.assertEqual((hold['server']['id'],hold['seconds_left']),(server['id'],60))
  with self.assertRaises(suite.h.Problem) as blocked:self.post(path+'/join',{},owner)
  self.assertEqual(blocked.exception.status,409)
  second=admit(guest)
  self.assertIsNone(self.get('/api/rejoin-lobby',guest)['server'])
  self.post(path+'/heartbeat',{'players':1,'status':'lobby','occupied':1,'lobby_epoch':second['lobby_epoch']},secret)
  self.clock[0]+=61
  self.assertIsNone(self.get('/api/rejoin-lobby',guest)['server'])
  admit(owner)
  self.post(path+'/performance',{'match':server['match_id'],'turns':20,'logic_ms':73.5,
   'peak_ms':60,'slow_frames':1},secret)
  row=self.get('/api/dedicated-hosts',owner)['hosts'][0]
  self.assertEqual((row['map'],row['client_versions'],row['players']),('classic:52',['0.1.0'],2))
  self.assertEqual((row['performance'][0]['turns'],row['performance'][0]['reconnects']),(20,1))
  self.post(path+'/close',{},secret)
  self.assertEqual(self.get('/api/servers')['servers'],[])
  row=self.get('/api/dedicated-hosts',owner)['hosts'][0]
  self.assertEqual((row['status'],row['map'],row['performance'][0]['turns']),('offline','classic:52',20))
 def test_maintenance_drains_and_percentiles_survive_restart(self):
  owner=self.a['token']
  provision=self.post('/api/dedicated-hosts',{'action':'create','name':'Maintenance Host'},owner)
  server=self.register_dedicated(provision['credential'],map='classic:52')
  path='/api/servers/'+server['id'];secret=server['secret']
  host={'host':provision['id']}
  self.post(path+'/heartbeat',{'status':'playing','players':2},secret)
  with self.assertRaises(suite.h.Problem) as blocked:
   self.post('/api/dedicated-hosts',{'action':'maintenance',**host},self.b['token'])
  self.assertEqual(blocked.exception.status,403)
  self.post('/api/dedicated-hosts',{'action':'maintenance',**host},owner)
  self.assertTrue(self.post(path+'/admin-command',{},secret)['draining'])
  self.assertEqual(self.get('/api/servers')['servers'][0]['status'],'maintenance')
  self.post('/api/dedicated-hosts',{'action':'cancel-maintenance',**host},owner)
  self.assertFalse(self.post(path+'/admin-command',{},secret)['draining'])
  self.post(path+'/heartbeat',{'status':'lobby','players':1},secret)
  self.post('/api/dedicated-hosts',{'action':'maintenance',**host},owner)
  with self.assertRaises(suite.h.Problem) as blocked:self.post(path+'/join',{},self.b['token'])
  self.assertEqual(blocked.exception.status,409)
  self.post(path+'/performance',{'match':server['match_id'],'turns':20,'logic_ms':83,
   'peak_ms':12,'p95_ms':8.5,'slow_frames':0},secret)
  self.post(path+'/drained',{},secret)
  self.post(path+'/close',{},secret)
  self.assertEqual(self.get('/api/servers')['servers'][0]['status'],'maintenance')
  self.register_dedicated(provision['credential'],map='classic:52')
  row=self.get('/api/dedicated-hosts',owner)['hosts'][0]
  self.assertIsNone(row['maintenance'])
  self.assertEqual(float(row['performance'][0]['p95_ms']),8.5)
 def test_lobby_moderation_locks_kicks_and_audits(self):
  owner,guest=self.a['token'],self.b['token']
  s=self.server(kind='dedicated');path='/api/servers/'+s['id'];secret=s['secret']
  def manage(action,token=owner,**fields):
   return self.post('/api/lobby-management',{'server':s['id'],'action':action,**fields},token)
  with self.assertRaises(suite.h.Problem) as error:manage('lock',guest)
  self.assertEqual(error.exception.status,403)
  self.assertTrue(manage('lock')['locked'])
  with self.assertRaises(suite.h.Problem) as error:self.post(path+'/join',{},guest)
  self.assertEqual(error.exception.status,409)
  self.assertFalse(manage('unlock')['locked'])
  ticket=self.post(path+'/join',{},guest)['identity_ticket']
  admit=self.post(path+'/admit',{'peer':1,'occupied':1,'ticket':ticket},secret)
  self.assertEqual(manage('state')['players'][0]['user'],self.other['id'])
  kicked=manage('kick',user=self.other['id'])
  self.assertEqual(kicked['players'],[])
  self.assertTrue(any(row['action']=='kick' and row['actor']==self.me['name'] for row in kicked['audit']))
  self.assertEqual(self.post(path+'/admin-command',{},secret)['kicks'],[self.other['id']])
  self.post(path+'/heartbeat',{'players':1,'status':'lobby','occupied':1,'lobby_epoch':admit['lobby_epoch']},secret)
  self.assertIsNone(self.get('/api/rejoin-lobby',guest)['server'])
  with self.assertRaises(suite.h.Problem) as error:self.post(path+'/join',{},guest)
  self.assertEqual(error.exception.status,403)
 def test_party_rematch_restores_map_on_same_host(self):
  owner,guest=self.a['token'],self.b['token']
  def party(token,action,**fields):
   return self.post('/api/party',{'action':action,**fields},token)
  self.post('/api/friends',{'user':self.other['id']},owner)
  self.post('/api/friends',{'user':self.me['id']},guest)
  room=party(owner,'invite',user=self.other['id'])['party']
  party(guest,'accept',party=room)
  party(guest,'message',message='One more round')
  provision=self.post('/api/dedicated-hosts',{'action':'create','name':'Rematch Host'},owner)
  rotation=['classic:52','classic:53']
  first=self.register_dedicated(provision['credential'],map=rotation[0],rotation=rotation)
  path='/api/servers/'+first['id']
  revision=party(owner,'select-server',server=first['id'])['target']['revision']
  for token in (owner,guest):party(token,'readiness',revision=revision,message='Ready')
  party(owner,'prompt',server=first['id'])
  ticket=self.post(path+'/join',{},owner)['identity_ticket']
  self.post(path+'/admit',{'peer':1,'occupied':1,'ticket':ticket},first['secret'])
  self.post(path+'/heartbeat',{'status':'playing','players':2},first['secret'])
  self.post(path+'/close',{},first['secret'])
  second=self.register_dedicated(provision['credential'],map=rotation[1],rotation=rotation)
  path='/api/servers/'+second['id']
  party(owner,'prepare-rematch',server=first['id'])
  ballot=self.post(path+'/map-ballot',{},second['secret'])['ballot']
  self.assertEqual(ballot,{'party':room,'winning':rotation[0]})
  self.post(path+'/map-applied',{'party':room,'map':rotation[0]},second['secret'])
  state=party(guest,'state')
  self.assertEqual(state['party'],room)
  self.assertEqual(len(state['members']),2)
  self.assertEqual(len(state['messages']),1)
  self.assertEqual(state['target']['server']['map'],rotation[0])
  self.assertNotEqual(state['target']['revision'],revision)
 def test_no_dedicated_impersonation(self):
  server=self.server(kind='dedicated')
  self.assertFalse(server['trusted'])
  with self.assertRaises(suite.h.Problem) as error:
   self.post('/api/servers/'+server['id']+'/result',{},server['secret'])
  self.assertEqual(error.exception.status,403)
  trusted=self.server(kind='dedicated',admin_key=self.hub.admin_key)
  self.assertTrue(trusted['trusted'])
 def test_relay_auth_and_bidirectional_data(self):
  server=self.server()
  with self.assertRaises(suite.h.Problem) as error:self.post('/api/servers/'+server['id']+'/join',{'relay':True})
  self.assertEqual(error.exception.status,400)
  self.assertNotIn('relay',self.post('/api/servers/'+server['id']+'/join',{}))
 def tearDown(self):self.hub.close();super().tearDown()
 def test_existing_python_passwords_sessions_and_records_are_compatible(self):
  if self.hub.mysql:self.skipTest('SQLite legacy-file interoperability is checked in the SQLite run')
  legacy=suite.h.Hub(self.hub.db,clock=lambda:self.clock[0]);account=legacy.handle('POST','/api/register',{'name':'LegacyKeeper','password':'legacy password verified'})
  self.assertEqual(self.get('/api/me',account['token'])['name'],'LegacyKeeper')
  self.assertIn('token',self.post('/api/login',{'name':'LegacyKeeper','password':'legacy password verified'}))
  self.assertIn('token',legacy.handle('POST','/api/login',{'name':'KeeperOne','password':'a long password 123'}))
 def test_stream_binary_nul_roundtrip(self):
  import base64
  s=self.server(kind='dedicated',admin_key=self.hub.admin_key);route='/api/servers/'+s['id'];raw=b'\0\xff\x80binary\0data';stream=self.post(route+'/stream',{'offset':0,'data':base64.b64encode(raw).decode(),'finished':False},s['secret'])
  self.post(route+'/stream',{'offset':len(raw),'data':base64.b64encode(b'\0append\xff').decode(),'finished':True},s['secret']);raw+=b'\0append\xff'
  got=self.get('/api/streams/'+stream['id']);self.assertEqual(base64.b64decode(got['data']),raw);self.assertEqual(got['size'],len(raw))
 def test_failed_login_limits_survive_separate_php_processes(self):
  for i in range(10):
   with self.assertRaises(suite.h.Problem):self.hub.handle('POST','/api/login',{'name':'KeeperOne','password':'incorrect long password'},peer='rate-peer')
  with self.assertRaises(suite.h.Problem) as error:self.hub.handle('POST','/api/login',{'name':'KeeperOne','password':'a long password 123'},peer='rate-peer')
  self.assertEqual(error.exception.status,429)
if __name__=='__main__':unittest.main()
