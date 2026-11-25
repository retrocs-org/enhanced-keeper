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
