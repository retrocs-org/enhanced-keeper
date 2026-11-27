#!/usr/bin/env python3
"""Exercise dedicated visibility/password combinations without starting a game."""
import importlib.util,json,os,tempfile,unittest
from pathlib import Path
from unittest.mock import patch,MagicMock
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('dedicated',ROOT/'services/keeperhub/dedicated.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
class Modes(unittest.TestCase):
 def test_modes(self):
  for visibility in ('private','public'):
   for password in ('','test-secret'):
    with self.subTest(visibility=visibility,password=bool(password)),tempfile.TemporaryDirectory() as folder:
     profile=Path(folder);(profile/'GameData').mkdir();(profile/'GameData/keeper-enhanced.cfg').write_text('OTHER=TRUE\n')
     config={'visibility':visibility,'engine_binary':'/usr/bin/true','user_data':folder,'name':'Test','rotation':[{'campaign':'classic','level':52}]}
     if visibility=='public':
      config.update(hub_url='https://example.invalid')
      mod.save_owner_credential(profile,config['hub_url'],'a'*43)
     if password:config['endpoint']='example.invalid:5556'
     process=MagicMock();process.poll.side_effect=[None,0,0];process.returncode=0
     sock=MagicMock();sock.__enter__.return_value.getsockname.return_value=('127.0.0.1',12345)
     with patch.dict(os.environ,{'KFX_SERVER_PASSWORD':password,'KFX_HUB_ADMIN_KEY':('x'*32 if password else ''),'KFX_HUB_SERVER_ID':'old'},clear=True),patch.object(mod,'server_version',return_value='0.1.0'),patch.object(mod,'supported_clients',return_value=['0.1.0']),patch.object(mod,'request',return_value={'id':'abc','secret':'secret','match_id':'match'}) as request,patch.object(mod.subprocess,'Popen',return_value=process) as popen,patch.object(mod.socket,'socket',return_value=sock),patch.object(mod,'rpc',return_value={}),patch.object(mod.time,'sleep') as sleep:
      host=mod.Host(config);sleep.side_effect=lambda _:setattr(host,'stop',True);host.run()
      env=popen.call_args.kwargs['env'];self.assertEqual(env['KFX_SERVER_PASSWORD'],password)
      self.assertNotIn('KFX_HUB_ADMIN_KEY',env)
      if visibility=='private':
       request.assert_not_called();self.assertEqual(env['KFX_PRIVATE_SERVER'],'1');self.assertNotIn('KFX_HUB_SERVER_ID',env)
      else:
       registration=request.call_args_list[0].args[2]
       self.assertEqual(registration['password'],bool(password));self.assertEqual(request.call_count,3)
       self.assertEqual(request.call_args_list[0].args[3],'a'*43)
       self.assertNotIn('a'*43,env.values())
       self.assertEqual(registration['port'],5556)
       self.assertEqual(registration['engine'],'0.1.0')
       self.assertEqual(registration['client_versions'],['0.1.0'])
       self.assertEqual('admin_key' in registration,bool(password))
       self.assertEqual('endpoint' in registration,bool(password))
      self.assertIn('API_ENABLED=TRUE',(profile/'GameData/keeper-enhanced.cfg').read_text())
 def test_maintenance_finishes_live_match_first(self):
  with tempfile.TemporaryDirectory() as folder,patch.dict(os.environ,{},clear=True):
   profile=Path(folder);(profile/'GameData').mkdir();(profile/'GameData/keeper-enhanced.cfg').touch()
   config={'visibility':'public','hub_url':'https://example.invalid','engine_binary':'/usr/bin/true',
           'user_data':folder,'name':'Drain','rotation':[{'campaign':'classic','level':52}]}
   mod.save_owner_credential(profile,config['hub_url'],'a'*43)
   process=MagicMock();process.poll.return_value=None;process.returncode=0
   host=mod.Host(config)
   calls=[]
   def request(base,route,data=None,token=''):
    calls.append(route.rsplit('/',1)[-1])
    if route.endswith('/register'):return {'id':'abc','secret':'secret','match_id':'match'}
    if route.endswith('/admin-command'):return {'draining':True}
    return {}
   live={'active':True,'turn':30,'players':2,'users':[{'peer':1,'account':'keeper','outcome':0}]}
   done={**live,'turn':31,'users':[{'peer':1,'account':'keeper','outcome':1}],
         'timing':{'turns':31,'logic_ms':72,'peak_ms':9,'p95_ms':7,'slow_frames':0}}
   def tick():
    self.assertNotIn('drained',calls)
    self.assertFalse(host.stop)
   with patch.object(mod,'server_version',return_value='0.1.0'),patch.object(mod,'supported_clients',return_value=['0.1.0']),patch.object(mod,'request',side_effect=request),patch.object(mod.subprocess,'Popen',return_value=process),patch.object(mod,'rpc',side_effect=[live,done,done]),patch.object(mod.time,'sleep',side_effect=lambda _:tick()):
    host.run()
   self.assertTrue(host.maintenance_restart)
   self.assertLess(calls.index('drained'),calls.index('performance'))
   self.assertLess(calls.index('performance'),calls.index('close'))
   self.assertEqual(json.loads((profile/'dedicated/performance/abc.json').read_text())['p95_ms'],7)
 def test_password_limit(self):
  with tempfile.TemporaryDirectory() as folder,patch.dict(os.environ,{'KFX_SERVER_PASSWORD':'x'*32}):
   profile=Path(folder);(profile/'GameData').mkdir();(profile/'GameData/keeper-enhanced.cfg').touch()
   with self.assertRaises(AssertionError):mod.validate({'visibility':'private','engine_binary':'/usr/bin/true','user_data':folder,'name':'Test','rotation':[{'campaign':'classic','level':52}]})
if __name__=='__main__':unittest.main()
