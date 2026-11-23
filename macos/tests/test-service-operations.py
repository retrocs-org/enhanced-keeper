#!/usr/bin/env python3
import importlib.util
import json
from pathlib import Path
import sqlite3
import stat
import subprocess
import os
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts'))
def load(name,path):
    spec=importlib.util.spec_from_file_location(name,ROOT/path);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module
hub=load('hub','services/keeperhub/hub.py')
ops=load('maintenance','services/keeperhub/maintenance.py')
web=load('web_releases','scripts/update-web-releases.py')
class ServiceOperations(unittest.TestCase):
    def setUp(self):
        self.temporary=tempfile.TemporaryDirectory();self.addCleanup(self.temporary.cleanup)
        self.root=Path(self.temporary.name);self.db=self.root/'community.sqlite3';self.hub=hub.Hub(self.db)
        self.account=self.hub.handle('POST','/api/register',{'name':'RecoveryKeeper','password':'initial secure password'})
    def post(self,path,data,token=''):
        return self.hub.handle('POST',path,data,token)
    def test_recovery_revokes_sessions_and_codes(self):
        result=self.post('/api/recovery/codes',{'password':'initial secure password'},self.account['token']);self.assertEqual(len(result['codes']),8)
        with sqlite3.connect(self.db) as database:
            self.assertNotIn(result['codes'][0],str(database.execute('SELECT * FROM recovery').fetchall()))
        with self.assertRaises(hub.Problem):self.post('/api/recovery/reset',{'name':'RecoveryKeeper','code':'wrong','password':'new secure password'})
        self.post('/api/recovery/reset',{'name':'RecoveryKeeper','code':result['codes'][0],'password':'new secure password'})
        self.assertFalse(self.hub.handle('GET','/api/session',{},self.account['token'])['signed_in'])
        with self.assertRaises(hub.Problem):self.post('/api/recovery/reset',{'name':'RecoveryKeeper','code':result['codes'][1],'password':'another secure password'})
        with self.assertRaises(hub.Problem):self.post('/api/login',{'name':'RecoveryKeeper','password':'initial secure password'})
        self.assertIn('token',self.post('/api/login',{'name':'RecoveryKeeper','password':'new secure password'}))
    def test_backup_integrity_retention_and_restore(self):
        folder=self.root/'backups'
        for i in range(3):destination=ops.backup(self.db,folder,keep=2)
        self.assertEqual(len(list(folder.glob('keeperhub-*.sqlite3'))),2)
        self.assertEqual(stat.S_IMODE(destination.stat().st_mode),0o600)
        ops.verify(destination)
        restored=hub.Hub(destination)
        self.assertIn('token',restored.handle('POST','/api/login',{'name':'RecoveryKeeper','password':'initial secure password'}))
        with self.assertRaises(ValueError):ops.backup(self.db,self.root)
    def test_recovery_requires_authentication_and_rate_limits(self):
        with self.assertRaises(hub.Problem):self.post('/api/recovery/codes',{})
        for i in range(3):self.post('/api/recovery/codes',{'password':'initial secure password'},self.account['token'])
        with self.assertRaises(hub.Problem) as error:self.post('/api/recovery/codes',{'password':'initial secure password'},self.account['token'])
        self.assertEqual(error.exception.status,429)
    def test_website_release_cache_rejects_corruption(self):
        cache=self.root/'releases.json'
        script="require 'public-web/releases.php'; echo json_encode(current_release());"
        for broken in [{'version':'1.4.0','assets':'broken'}, {'version':[], 'assets':[]}]:
            cache.write_text(json.dumps(broken))
            value=subprocess.check_output(['php','-r',script],cwd=ROOT,env=dict(os.environ,KFX_RELEASE_MANIFEST=str(cache)),text=True)
            self.assertEqual(value,'null')

    def test_release_metadata_checks_complete_pairs_and_digests(self):
        assets=[]
        for kind in ('dmg','zip'):
            name='KeeperFX-E-1.4.0-macOS-Universal.'+kind
            for suffix in ('','.sha256'):
                assets.append({'name':name+suffix,'browser_download_url':web.REPO+'/releases/download/1.4.0/'+name+suffix,'digest':'sha256:'+'a'*64})
        release={'draft':False,'prerelease':False,'tag_name':'1.4.0','assets':assets}
        fetch=lambda url,limit:('a'*64+'  '+url.split('/')[-1].removesuffix('.sha256')+'\n').encode()
        self.assertEqual(len(web.manifest(release,fetch)['assets']),2)
        release['assets'][0]['digest']='sha256:'+'b'*64
        with self.assertRaises(ValueError):web.manifest(release,fetch)
        release['draft']=True
        with self.assertRaises(ValueError):web.manifest(release,fetch)
if __name__=='__main__':unittest.main()
