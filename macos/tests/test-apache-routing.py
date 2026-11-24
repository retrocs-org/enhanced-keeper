#!/usr/bin/env python3
"""Verify Apache's real front-controller fallback separately from PHP business tests."""
from pathlib import Path
import grp,os,pwd,shutil,socket,subprocess,tempfile,time,unittest,urllib.request,urllib.error
ROOT=Path(__file__).resolve().parents[2]
class ApacheRoutingTests(unittest.TestCase):
 def test_pages_api_and_private_files(self):
  binary=shutil.which('httpd') or shutil.which('apache2')
  modules=next((Path(p) for p in ['/usr/libexec/apache2','/usr/lib/apache2/modules','/usr/lib64/httpd/modules'] if (Path(p)/'mod_dir.so').is_file()),None)
  if not binary or modules is None:self.skipTest('Apache and its module directory are required')
  with tempfile.TemporaryDirectory(prefix='kfx-apache-') as folder:
   state=Path(folder);web=state/'web';web.mkdir();(web/'lib').mkdir()
   (web/'index.php').write_text('KFX_FRONT_CONTROLLER')
   (web/'site.css').write_text('CSS_STATIC_FILE')
   (web/'config.inc.php').write_text('PRIVATE_CONFIG')
   (web/'config.inc.php.bak').write_text('PRIVATE_BACKUP')
   (web/'lib/mariadb.sql').write_text('PRIVATE_SCHEMA')
   with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
   origin='http://127.0.0.1:'+str(port)
   common=f'ServerRoot "{state}"\nServerName 127.0.0.1\nListen 127.0.0.1:{port}\nPidFile "{state}/pid"\nErrorLog "{state}/errors.log"\n'
   for name in ['mpm_event','unixd','authz_core','authz_host','dir']:
    common+=f'LoadModule {name}_module "{modules}/mod_{name}.so"\n'
   common+=f'User {pwd.getpwuid(os.geteuid()).pw_name}\nGroup {grp.getgrgid(os.getegid()).gr_name}\nDocumentRoot "{web}"\n<Directory "{web}">\nRequire all granted\nDirectoryIndex index.php\n</Directory>\n'
   overlay=(ROOT/'docs/apache-keeperfx-e.conf').read_text().replace('/srv/www/vhosts/keeperfx-e.retrocs.org',str(web))
   def fetch(path):
    with urllib.request.urlopen(origin+path,timeout=2) as response:return response.read().decode()
   for enabled in (False,True,'htaccess'):
    config=state/'httpd.conf'
    if enabled=='htaccess':
     (web/'.htaccess').write_text((ROOT/'public-web/.htaccess').read_text())
     (web/'lib/.htaccess').write_text((ROOT/'public-web/lib/.htaccess').read_text())
     config.write_text(common+f'<Directory "{web}">\nAllowOverride All\n</Directory>\n')
    else:config.write_text(common+(overlay if enabled else ''))
    lint=subprocess.run([binary,'-t','-f',str(config)],capture_output=True,text=True)
    self.assertEqual(lint.returncode,0,lint.stderr)
    process=subprocess.Popen([binary,'-X','-f',str(config)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    try:
     for _ in range(100):
      try:fetch('/');break
      except urllib.error.URLError:
       self.assertIsNone(process.poll(),(state/'errors.log').read_text() if (state/'errors.log').exists() else 'Apache stopped');time.sleep(.03)
     else:self.fail('Apache did not start')
     if not enabled:
      with self.assertRaises(urllib.error.HTTPError) as error:fetch('/servers')
      self.assertEqual(error.exception.code,404)
     else:
      for path in ['/servers','/services','/api-browser','/events','/replays','/profile','/api/session','/api/servers/abc/heartbeat']:
       self.assertEqual(fetch(path),'KFX_FRONT_CONTROLLER')
      self.assertEqual(fetch('/site.css'),'CSS_STATIC_FILE')
      for path in ['/config.inc.php','/config.inc.php.bak','/lib/mariadb.sql']:
       with self.assertRaises(urllib.error.HTTPError) as error:fetch(path)
       self.assertEqual(error.exception.code,403)
    finally:process.terminate();process.wait(timeout=5)
if __name__=='__main__':unittest.main()
