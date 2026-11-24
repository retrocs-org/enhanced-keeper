#!/usr/bin/env python3
"""Deployment preservation and standalone config-only PHP acceptance checks."""
import contextlib
import importlib.util
import json
import os
from pathlib import Path
import socket
import stat
import subprocess
import tempfile
import time
import unittest
import urllib.error
import urllib.request
import urllib.parse

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('deploy_web',ROOT/'scripts/deploy-web.py')
deploy=importlib.util.module_from_spec(spec);spec.loader.exec_module(deploy)

class WebDeployTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.base=Path(self.temp.name);self.target=self.base/'vhost'
    def refresh(self,**args):
        with contextlib.redirect_stdout(__import__('io').StringIO()):deploy.refresh(self.target,**args)
    def test_refresh_keeps_configuration_bytes_and_permissions_and_cleans_stale_files(self):
        self.refresh(apply=True,initialize=True)
        config=self.target/'config.inc.php'
        # Distinctive bytes ensure no template merge or silent credential replacement.
        expected=b'<?php\n/* server-only settings */\nreturn ["password"=>"test-only-value"];\n'
        config.write_bytes(expected);config.chmod(0o640)
        before=config.stat()
        (self.target/'site.css').write_text('outdated')
        (self.target/'obsolete.php').write_text('old')
        self.refresh(apply=True)
        self.assertEqual(config.read_bytes(),expected)
        self.assertEqual(config.stat().st_mtime_ns,before.st_mtime_ns)
        self.assertEqual(stat.S_IMODE(config.stat().st_mode),0o640)
        self.assertFalse((self.target/'obsolete.php').exists())
        self.assertEqual((self.target/'site.css').read_bytes(),(ROOT/'public-web/site.css').read_bytes())
    def test_preview_does_not_change_existing_files(self):
        self.refresh(apply=True,initialize=True)
        (self.target/'site.css').write_text('old')
        self.refresh()
        self.assertEqual((self.target/'site.css').read_text(),'old')
    def test_refuses_unowned_nonempty_directory_and_symlinks(self):
        self.target.mkdir();(self.target/'other-site.php').write_text('keep')
        with self.assertRaises(ValueError):self.refresh(apply=True,initialize=True)
        self.assertEqual((self.target/'other-site.php').read_text(),'keep')
        (self.target/'other-site.php').unlink();self.refresh(apply=True,initialize=True)
        (self.target/'escaped').symlink_to(self.base,target_is_directory=True)
        with self.assertRaises(ValueError):self.refresh(apply=True)
    def test_source_release_rejects_server_credentials(self):
        import sys
        sys.path.insert(0,str(ROOT/'scripts'))
        spec=importlib.util.spec_from_file_location('check_sensitive',ROOT/'scripts/check-sensitive.py')
        scanner=importlib.util.module_from_spec(spec);spec.loader.exec_module(scanner)
        from release_support import ReleaseError
        source=self.base/'source';(source/'public-web').mkdir(parents=True)
        config=source/'public-web/config.inc.php'
        text=(ROOT/'public-web/config.inc.php').read_text();config.write_text(text)
        scanner.scan(source,{'public-web/config.inc.php':{}})
        for key in ['password','admin_key','relay_control_key']:
            config.write_text(text.replace("'"+key+"' => ''", "'"+key+"' => 'test-only-secret'"))
            with self.assertRaises(ReleaseError):scanner.scan(source,{'public-web/config.inc.php':{}})
    def test_explicit_permission_refresh_preserves_config_and_sets_ownership(self):
        import pwd,grp
        from unittest.mock import patch
        self.refresh(apply=True,initialize=True)
        config=self.target/'config.inc.php';config.write_bytes(b'<?php /* private server settings */');config.chmod(0o600)
        before=config.stat().st_mtime_ns
        owner=pwd.getpwuid(os.geteuid()).pw_name;group=grp.getgrgid(os.getegid()).gr_name
        with patch.object(deploy.os,'chown') as chown:
            self.refresh(apply=True,owner=owner,group=group,set_permissions=True)
        self.assertEqual(config.read_bytes(),b'<?php /* private server settings */')
        self.assertEqual(config.stat().st_mtime_ns,before)
        for path in [self.target,*self.target.rglob('*')]:
            expected=0o755 if path.is_dir() else 0o640 if path==config else 0o644
            self.assertEqual(stat.S_IMODE(path.stat().st_mode),expected)
        self.assertEqual(chown.call_count,1+len(list(self.target.rglob('*'))))
        chown.assert_any_call(config.resolve(),os.geteuid(),os.getegid())
    def test_permission_preview_does_not_chown_or_chmod(self):
        import pwd,grp
        from unittest.mock import patch
        self.refresh(apply=True,initialize=True)
        config=self.target/'config.inc.php';config.chmod(0o600)
        with patch.object(deploy.os,'chown') as chown:
            self.refresh(owner=pwd.getpwuid(os.geteuid()).pw_name,group=grp.getgrgid(os.getegid()).gr_name,set_permissions=True)
        chown.assert_not_called();self.assertEqual(stat.S_IMODE(config.stat().st_mode),0o600)
    def test_public_web_runs_alone_using_only_its_config(self):
        self.refresh(apply=True,initialize=True)
        with socket.socket() as sock:
            sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
        origin='http://127.0.0.1:'+str(port)
        config=self.target/'config.inc.php';text=config.read_text()
        text=text.replace('https://keeperfx-e.retrocs.org',origin).replace('/var/lib/keeperfx-e',str(self.base/'state'))
        text=text.replace('mysql:host=172.16.1.121;port=3306;dbname=keeperfx_e;charset=utf8mb4',str(self.base/'state/community.sqlite3'))
        config.write_text(text)
        env={key:value for key,value in os.environ.items() if not key.startswith('KFX_')}
        php=subprocess.Popen(['php','-S','127.0.0.1:'+str(port),'-t',str(self.target)],env=env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        try:
            for _ in range(100):
                try:
                    with urllib.request.urlopen(origin,timeout=2) as response:self.assertEqual(response.status,200)
                    break
                except urllib.error.URLError:time.sleep(.03)
            else:self.fail('Deployed PHP site did not start')
            direct=lambda path:origin+'/index.php?route='+urllib.parse.quote(path,safe='')
            for path in ['/servers','/services','/api-browser','/events','/replays','/profile','/device']:
                with urllib.request.urlopen(direct(path),timeout=2) as response:
                    self.assertEqual(response.status,200);self.assertIn('routes.js',response.read().decode())
            with urllib.request.urlopen(origin,timeout=2) as response:self.assertIn('/index.php?route=%2Fservers',response.read().decode())
            request=urllib.request.Request(direct('/api/register'),data=json.dumps({'name':'ConfigKeeper','password':'a long config password'}).encode(),headers={'Content-Type':'application/json'})
            with urllib.request.urlopen(request,timeout=4) as response:
                account=json.load(response);self.assertIn('csrf',account);self.assertNotIn('token',account)
            with urllib.request.urlopen(direct('/api/health'),timeout=4) as response:self.assertEqual(json.load(response)['http_backend'],'php')
            for path in ['/config.inc.php','/config.inc.php~','/lib/settings.php','/lib/mariadb.sql']:
                with self.assertRaises(urllib.error.HTTPError) as error:urllib.request.urlopen(direct(path),timeout=2)
                self.assertEqual(error.exception.code,404)
            with urllib.request.urlopen(origin+'/assets/dungeon-heart.svg',timeout=2) as response:self.assertEqual(response.headers.get_content_type(),'image/svg+xml')
        finally:php.terminate();php.wait(timeout=5)

if __name__=='__main__':unittest.main()
