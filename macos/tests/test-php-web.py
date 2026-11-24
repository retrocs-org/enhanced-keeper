#!/usr/bin/env python3
"""Exercise the real PHP front controller against an isolated KeeperHub API."""
import http.cookiejar
import importlib.util
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile
import threading
import time
import unittest
import urllib.error
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('keeperhub', ROOT/'macos/tests/legacy_keeperhub.py')
hub = importlib.util.module_from_spec(spec)
spec.loader.exec_module(hub)
import sys
sys.path.insert(0,str(ROOT/'services/keeperhub'))
from relay_worker import serve_control
from relay import Relay


class PHPWebsiteTests(unittest.TestCase):
    def setUp(self):
        php = shutil.which(os.environ.get('KFX_PHP', 'php'))
        self.assertIsNotNone(php, 'PHP 8.1+ is required for website acceptance tests')
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.relay = Relay()
        self.addCleanup(self.relay.close)
        with socket.socket() as sock:
            sock.bind(('127.0.0.1', 0))
            port = sock.getsockname()[1]
        self.origin = 'http://127.0.0.1:'+str(port)
        service = hub.Hub(Path(self.temp.name)/'test.sqlite3', self.relay, 'test-admin-'+'x'*30)
        self.backend = serve_control(self.relay,'test-relay-control-'+'r'*32)
        self.thread = threading.Thread(target=self.backend.serve_forever, daemon=True)
        self.thread.start()
        self.addCleanup(self.stop_backend)
        environment = dict(os.environ, KFX_DB_CONFIG='', KFX_DB_DSN='', KFX_DB_USER='', KFX_DB_PASSWORD='', KFX_PUBLIC_ORIGIN=self.origin,
                           KFX_HUB_DB=str(Path(self.temp.name)/'test.sqlite3'), KFX_HUB_ADMIN_KEY='test-admin-'+'x'*30, KFX_RELAY_CONTROL='http://127.0.0.1:'+str(self.backend.server_port), KFX_RELAY_CONTROL_KEY='test-relay-control-'+'r'*32)
        mysql_socket=os.environ.get('KFX_TEST_MYSQL_SOCKET')
        if mysql_socket:
            self.mysql_database='kfx_web_'+__import__('secrets').token_hex(8)
            command=['mariadb','--no-defaults','--socket='+mysql_socket,'-uroot']
            subprocess.run([*command,'-e','CREATE DATABASE `'+self.mysql_database+'` CHARACTER SET utf8mb4'],check=True,capture_output=True)
            with (ROOT/'public-web/lib/mariadb.sql').open() as schema:subprocess.run([*command,self.mysql_database],stdin=schema,check=True,capture_output=True)
            self.addCleanup(lambda:subprocess.run([*command,'-e','DROP DATABASE `'+self.mysql_database+'`'],check=True,capture_output=True))
            environment.update(KFX_DB_DSN='mysql:unix_socket='+mysql_socket+';dbname='+self.mysql_database+';charset=utf8mb4',KFX_DB_USER='root',KFX_DB_PASSWORD='')
        web = ROOT/'public-web'
        self.php = subprocess.Popen([php, '-S', '127.0.0.1:'+str(port), '-t', str(web), str(web/'router.php')],
                                    env=environment, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.addCleanup(self.stop_php)
        self.jar = http.cookiejar.CookieJar()
        self.client = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(self.jar))
        for attempt in range(100):
            try:
                self.request('/').close()
                break
            except (urllib.error.URLError, ConnectionError):
                self.assertIsNone(self.php.poll(), 'PHP server stopped')
                time.sleep(.02)
        else:
            self.fail('PHP server did not start')

    def stop_backend(self):
        self.backend.shutdown()
        self.backend.server_close()
        self.thread.join(timeout=3)

    def stop_php(self):
        self.php.terminate()
        self.php.wait(timeout=5)

    def request(self, path, data=None, headers=None, method=None):
        return self.client.open(urllib.request.Request(self.origin+path,
            data=json.dumps(data).encode() if data is not None else None,
            headers={'Content-Type':'application/json', **(headers or {})}, method=method), timeout=4)

    def test_pages_assets_and_unknown_routes(self):
        for path in ['/', '/index.php', '/community', '/servers', '/events', '/profile', '/device', '/services', '/api-browser', '/replays', '/challenges', '/keeper/'+'a'*32]:
            with self.request(path) as response:
                page=response.read().decode()
                self.assertIn('KeeperFX-E',page)
                self.assertNotIn('gitlab',page.lower())
                self.assertIn("frame-ancestors 'none'", response.headers['Content-Security-Policy'])
        for path, content_type in [('/app.js', 'text/javascript'), ('/portal.js', 'text/javascript'), ('/api-browser.js', 'text/javascript'), ('/style.css', 'text/css'), ('/site.css', 'text/css'), ('/assets/keeper-sigil.svg', 'image/svg+xml'), ('/assets/dungeon-heart.svg', 'image/svg+xml')]:
            with self.request(path) as response:
                self.assertTrue(response.headers['Content-Type'].startswith(content_type))
        for path in ['/community.php', '/router.php', '/lib/Hub.php', '/config.inc.php', '/config.inc.php.bak', '/lib/settings.php', '/check-database.php', '/layout.php', '/api-catalog.php', '/hub.py', '/.git/config', '/api/../hub.py', '/missing']:
            with self.assertRaises(urllib.error.HTTPError) as error:
                self.request(path)
            self.assertEqual(error.exception.code, 404)

    def test_api_catalog_and_service_pages_use_native_php(self):
        health=json.load(self.request('/api/health'));self.assertEqual(health['http_backend'],'php')
        catalog=json.load(self.request('/api-catalog.json'));self.assertGreater(len(catalog),30)
        for entry in catalog:
            if entry['interactive']:self.assertEqual(entry['method'],'GET');self.assertNotIn('{',entry['path'])
        with self.request('/api-browser') as response:
            page=response.read().decode();self.assertIn('api-explorer',page);self.assertIn('Endpoint reference',page)
        with self.request('/services') as response:self.assertIn('relay-status',response.read().decode())
        with self.request('/profile') as response:self.assertIn('<form method="post" id="login"',response.read().decode())

    def test_original_landing_is_separate_from_community(self):
        with self.request('/') as response:
            page = response.read().decode()
        self.assertIn('Your dungeon.', page)
        self.assertIn('KeeperFX-E is a fork of',page)
        self.assertIn('https://github.com/dkfans/keeperfx',page)
        self.assertIn('Browse downloads', page)
        self.assertNotIn('id="login"', page)
        with self.request('/community') as response:
            self.assertIn('id="servers"', response.read().decode())

    def test_accounts_csrf_logout_and_directory(self):
        with self.request('/api/register', {'name':'PHPKeeper', 'password':'a long password 123'}) as response:
            account = json.load(response)
        self.assertNotIn('token', account)
        self.assertTrue(list(self.jar)[0].has_nonstandard_attr('HttpOnly'))
        self.assertEqual(list(self.jar)[0].get_nonstandard_attr('SameSite'), 'Strict')
        self.assertTrue(json.load(self.request('/api/session'))['signed_in'])
        self.assertEqual(json.load(self.request('/api/servers'))['servers'], [])
        with self.assertRaises(urllib.error.HTTPError) as error:
            self.request('/api/profile', {'public':True})
        self.assertEqual(error.exception.code, 403)
        csrf = {'X-CSRF-Token':account['csrf']}
        self.assertTrue(json.load(self.request('/api/profile', {'public':True}, csrf))['ok'])
        self.request('/api/logout', {}, csrf).close()
        self.assertFalse(json.load(self.request('/api/session'))['signed_in'])

    def test_cross_origin_host_and_method_rejected(self):
        for headers in [{'Origin':'https://evil.invalid'}, {'Host':'evil.invalid'}]:
            with self.assertRaises(urllib.error.HTTPError) as error:
                self.request('/api/session', headers=headers)
            self.assertEqual(error.exception.code, 403)
        with self.assertRaises(urllib.error.HTTPError) as error:
            self.request('/api/session', method='DELETE')
        self.assertEqual(error.exception.code, 405)

    def test_game_bearer_auth_and_pairing_still_work(self):
        device = json.load(self.request('/api/device/start', {}))
        account = json.load(self.request('/api/register', {'name':'GameKeeper', 'password':'a long password 456'}))
        self.request('/api/device/approve', {'user_code':device['user_code']}, {'X-CSRF-Token':account['csrf']}).close()
        game = json.load(urllib.request.urlopen(urllib.request.Request(self.origin+'/api/device/poll',
            data=json.dumps({'device_code':device['device_code']}).encode(), headers={'Content-Type':'application/json'}), timeout=4))
        self.assertIn('token', game)
        self.assertTrue(json.load(self.request('/api/me', headers={'Authorization':'Bearer '+game['token']}))['id'])
        host=json.load(self.request('/api/servers/register',{'name':'PHP Game Host','endpoint':'127.0.0.1:5556','engine':'1.4.0.0','password':False},{'Authorization':'Bearer '+game['token']}))
        self.assertIn('secret',host)

    def test_relay_failure_is_bounded_and_accounts_remain_available(self):
        self.backend.shutdown()
        self.backend.server_close()
        with self.assertRaises(urllib.error.HTTPError) as error:
            self.request('/api/health')
        self.assertEqual(error.exception.code, 503)
        body = json.load(error.exception)
        self.assertNotIn('127.0.0.1', body['error'])
        self.assertFalse(json.load(self.request('/api/session'))['signed_in'])


if __name__ == '__main__':
    unittest.main()
