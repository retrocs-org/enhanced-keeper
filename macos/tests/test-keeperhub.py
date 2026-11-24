#!/usr/bin/env python3
import importlib.util, unittest, tempfile, sys, socket, time, threading, json, urllib.request, urllib.error, http.cookiejar
from pathlib import Path
p=Path(__file__).resolve().parents[2]/'macos/tests/legacy_keeperhub.py';s=importlib.util.spec_from_file_location('keeperhub',p);h=importlib.util.module_from_spec(s);s.loader.exec_module(h)
class HubTests(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.clock=[time.time()];self.relay=h.Relay();self.hub=h.Hub(Path(self.tmp.name)/'test.sqlite3',self.relay,'admin-'+'x'*30,lambda:self.clock[0]);self.a=self.post('/api/register',{'name':'KeeperOne','password':'a long password 123'});self.b=self.post('/api/register',{'name':'KeeperTwo','password':'a long password 456'});self.me=self.get('/api/me',self.a['token']);self.other=self.get('/api/me',self.b['token'])
 def tearDown(self):self.relay.close();self.tmp.cleanup()
 def get(self,path,token=''):return self.hub.handle('GET',path,{},token)
 def post(self,path,data,token=''):return self.hub.handle('POST',path,data,token)
 def server(self,**kwargs):return self.post('/api/servers/register',dict(name='Local Dungeon',endpoint='localhost:5556',engine='1.4.0.0',**kwargs),self.a['token'])
 def test_pair_once_expiry_and_privacy(self):
  d=self.post('/api/device/start',{});self.assertTrue(self.post('/api/device/poll',{'device_code':d['device_code']})['pending']);self.post('/api/device/approve',{'user_code':d['user_code']},self.a['token']);self.clock[0]+=3;v=self.post('/api/device/poll',{'device_code':d['device_code']});self.assertEqual(self.get('/api/me',v['token'])['id'],self.me['id'])
  with self.assertRaises(h.Problem):self.post('/api/device/poll',{'device_code':d['device_code']})
  with self.assertRaises(h.Problem):self.get('/api/profiles/'+self.me['id'])
  self.post('/api/profile',{'public':True},self.a['token']);self.assertEqual(self.get('/api/profiles/'+self.me['id'])['name'],'KeeperOne')
  self.post('/api/logout',{},v['token'])
  with self.assertRaises(h.Problem):self.get('/api/me',v['token'])
 def test_directory_ownership_and_expiry(self):
  s=self.server();self.assertEqual(len(self.get('/api/servers')['servers']),1)
  with self.assertRaises(h.Problem):self.post('/api/servers/'+s['id']+'/heartbeat',{},self.b['token'])
  self.post('/api/servers/'+s['id']+'/heartbeat',{'players':3},s['secret']);self.assertEqual(self.get('/api/servers')['servers'][0]['players'],3)
  with self.assertRaises(h.Problem):self.post('/api/servers/'+s['id']+'/join',{'engine':'different'})
  self.clock[0]+=91;self.assertEqual(self.get('/api/servers')['servers'],[])
 def test_advertised_content_is_rechecked_at_join(self):
  a='a'*64;b='b'*64;s=self.server(map='classic:52',mods=a)
  self.assertEqual(self.get('/api/servers')['servers'][0]['mods'],a)
  with self.assertRaises(h.Problem):self.post('/api/servers/'+s['id']+'/join',{'content':'invalid ☃'})
  with self.assertRaises(h.Problem):self.post('/api/servers/'+s['id']+'/join',{'engine':'1.4.0.0','content':b})
  self.post('/api/servers/'+s['id']+'/join',{'engine':'1.4.0.0','content':a})
  self.post('/api/servers/'+s['id']+'/heartbeat',{'mods':b,'map':'classic:53'},s['secret'])
  with self.assertRaises(h.Problem):self.post('/api/servers/'+s['id']+'/join',{'engine':'1.4.0.0','content':a})
  self.assertEqual(self.get('/api/servers')['servers'][0]['mods'],b)
 def test_no_dedicated_impersonation(self):
  with self.assertRaises(h.Problem):self.server(kind='dedicated')
  s=self.server(kind='dedicated',admin_key=self.hub.admin_key);self.assertTrue(s['secret'])
 def test_friends_events_and_bracket(self):
  self.post('/api/friends',{'user':self.other['id']},self.a['token']);self.post('/api/friends',{'user':self.me['id']},self.b['token']);self.assertEqual(self.get('/api/friends',self.a['token'])['friends'][0]['status'],'accepted')
  e=self.post('/api/events',{'title':'Local tournament','kind':'tournament','starts':self.clock[0]+600},self.a['token']);self.post('/api/events/'+e['id']+'/join',{},self.a['token']);self.post('/api/events/'+e['id']+'/join',{},self.b['token']);self.assertEqual(len(self.get('/api/events/'+e['id']+'/bracket')['round_one']),1)
 def test_results_never_claim_client_authority(self):
  s=self.server();m=self.post('/api/servers/'+s['id']+'/match',{'participants':[self.me['id'],self.other['id']]},s['secret']);self.post('/api/matches/'+m['id']+'/result',{'result':'won','verified':True},self.a['token']);self.post('/api/matches/'+m['id']+'/result',{'result':'lost','verified':True},self.b['token']);history=self.get('/api/matches',self.a['token'])['matches'];self.assertEqual(history[0]['verification'],'confirmed');self.assertEqual(self.get('/api/profiles/'+self.me['id'],self.a['token'])['verified_stats']['matches'],0)
  with self.assertRaises(h.Problem):self.post('/api/matches/'+m['id']+'/result',{'result':'won'},self.a['token'])
  r=self.post('/api/replays',{'match':m['id'],'data':'dGVzdA=='},self.a['token']);self.assertEqual(self.get('/api/replays/'+r['id'])['data'],'dGVzdA==')
 def test_relay_auth_and_bidirectional_data(self):
  s=self.server();v=self.post('/api/servers/'+s['id']+'/join',{'relay':True})['relay'];host=self.post('/api/servers/'+s['id']+'/heartbeat',{},s['secret'])['channels'][0];a=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);b=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);evil=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
  try:
   a.settimeout(.15);b.settimeout(.15);dest=('127.0.0.1',v['port']);a.sendto(('KFXR1 host '+host['token']).encode(),dest);b.sendto(('KFXR1 client '+v['token']).encode(),dest);time.sleep(.05);evil.sendto(b'bad',dest)
   with self.assertRaises(socket.timeout):a.recvfrom(100)
   b.sendto(b'client ENet',dest);self.assertEqual(a.recvfrom(100)[0],b'client ENet');a.sendto(b'host ENet',dest);self.assertEqual(b.recvfrom(100)[0],b'host ENet')
  finally:a.close();b.close();evil.close()
 def test_trusted_engine_identity_and_verified_results(self):
  server=self.server(kind='dedicated',admin_key=self.hub.admin_key);route='/api/servers/'+server['id'];a=self.post(route+'/join',{},self.a['token']);b=self.post(route+'/join',{},self.b['token']);mid=a['match_id'];self.post(route+'/identity',{'ticket':a['identity_ticket'],'peer':1},server['secret']);self.post(route+'/identity',{'ticket':b['identity_ticket'],'peer':2},server['secret'])
  with self.assertRaises(h.Problem):self.post(route+'/identity',{'ticket':a['identity_ticket'],'peer':2},server['secret'])
  self.post(route+'/heartbeat',{'players':3,'status':'playing'},server['secret']);self.post('/api/matches/'+mid+'/result',{'result':'won'},self.a['token']);self.post('/api/matches/'+mid+'/result',{'result':'lost'},self.b['token'])
  self.assertEqual(self.get('/api/matches',self.a['token'])['matches'][0]['verification'],'casual')
  results={'match':mid,'results':[{'user':self.me['id'],'result':'won'},{'user':self.other['id'],'result':'lost'}]}
  with self.assertRaises(h.Problem):self.post(route+'/result',results,self.a['token'])
  self.assertEqual(self.post(route+'/result',results,server['secret'])['verification'],'verified');self.assertEqual(self.get('/api/profiles/'+self.me['id'],self.a['token'])['verified_stats']['wins'],1)
  e=self.post('/api/events',{'title':'Verified cup','kind':'tournament','starts':self.clock[0]+60},self.a['token']);er='/api/events/'+e['id'];self.post(er+'/join',{},self.a['token']);self.post(er+'/join',{},self.b['token']);self.post(er+'/bracket/start',{},self.a['token']);self.post(er+'/bracket/report',{'round':1,'position':0,'match':mid},self.a['token']);self.assertEqual(self.get(er+'/bracket')['champion'],self.me['id'])
  with self.assertRaises(h.Problem):self.post(er+'/bracket/report',{'round':1,'position':0,'match':mid},self.a['token'])

 def test_live_stream_offsets_and_retry(self):
  server=self.server(kind='dedicated',admin_key=self.hub.admin_key);route='/api/servers/'+server['id'];a=self.post(route+'/stream',{'offset':0,'data':'dGVzdA=='},server['secret']);self.assertEqual(a['offset'],4)
  self.assertEqual(self.post(route+'/stream',{'offset':0,'data':'dGVzdA=='},server['secret'])['offset'],4)
  with self.assertRaises(h.Problem):self.post(route+'/stream',{'offset':9,'data':'dGVzdA=='},server['secret'])
  self.post(route+'/stream',{'offset':4,'data':'','finished':True},server['secret']);v=self.get(route+'/spectate');self.assertEqual(self.get('/api/streams/'+v['stream_id']+'?offset=0')['data'],'dGVzdA==');self.assertTrue(self.get('/api/streams/'+v['stream_id']+'?offset=4')['finished'])
 def test_http_csrf_and_cookie_policy(self):
  server=h.serve(self.hub,port=0);t=threading.Thread(target=server.serve_forever,daemon=True);t.start();base=server.origin;jar=http.cookiejar.CookieJar();client=urllib.request.build_opener(urllib.request.HTTPCookieProcessor(jar))
  def req(path,data=None,headers={}):return client.open(urllib.request.Request(base+path,data=json.dumps(data).encode() if data is not None else None,headers={'Content-Type':'application/json',**headers}),timeout=3)
  try:
   result=json.load(req('/api/login',{'name':'KeeperOne','password':'a long password 123'}));self.assertNotIn('token',result);self.assertTrue(list(jar)[0].has_nonstandard_attr('HttpOnly'))
   with self.assertRaises(urllib.error.HTTPError):req('/api/profile',{'public':True})
   self.assertTrue(json.load(req('/api/profile',{'public':True},{'X-CSRF-Token':result['csrf']}))['ok'])
   with self.assertRaises(urllib.error.HTTPError):req('/api/session',headers={'Origin':'http://evil.invalid'})
   with self.assertRaises(urllib.error.HTTPError) as error:req('/')
   self.assertEqual(error.exception.code,404) # The PHP site owns public pages.
  finally:server.shutdown();server.server_close();t.join()
if __name__=='__main__':unittest.main()
