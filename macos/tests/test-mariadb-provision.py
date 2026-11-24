#!/usr/bin/env python3
"""Provision only against the explicitly selected disposable, networking-disabled test instance."""
import os,subprocess,secrets,stat,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
class ProvisionTests(unittest.TestCase):
 def test_private_host_scoped_crud_credentials(self):
  socket=os.environ.get('KFX_TEST_MYSQL_SOCKET')
  if not socket:self.skipTest('Set KFX_TEST_MYSQL_SOCKET to an isolated test instance')
  client=['mariadb','--no-defaults','--protocol=SOCKET','--socket='+socket,'-uroot','--batch','--skip-column-names']
  def sql(text):return subprocess.check_output([*client,'-e',text],text=True).strip()
  info=sql('SELECT @@datadir,@@skip_networking');self.assertIn('kfx-mariadb-',info);self.assertEqual(info.split('\t')[-1],'1')
  with tempfile.TemporaryDirectory() as folder:
   folder=Path(folder);defaults=folder/'admin.cnf';defaults.write_text('[client]\nuser=root\nprotocol=SOCKET\nsocket='+socket+'\n');defaults.chmod(0o600)
   name='kfx_provision_'+secrets.token_hex(4);account='kfx_app_'+secrets.token_hex(4);output=folder/'application.database.php'
   args=['bash',str(ROOT/'scripts/provision-mariadb.sh'),'--defaults-file',str(defaults),'--database',name,'--user',account,'--web-host','172.16.1.133','--output',str(output)]
   try:
    result=subprocess.run(args,text=True,capture_output=True);self.assertEqual(result.returncode,0,result.stderr)
    self.assertEqual(stat.S_IMODE(output.stat().st_mode),0o600)
    password=output.read_text().split("'password'=>'")[1].split("'")[0];self.assertNotIn(password,result.stdout+result.stderr)
    self.assertEqual(set(sql("SELECT PRIVILEGE_TYPE FROM information_schema.SCHEMA_PRIVILEGES WHERE TABLE_SCHEMA='"+name+"'").splitlines()),{'SELECT','INSERT','UPDATE','DELETE'})
    self.assertEqual(sql("SELECT Host FROM mysql.user WHERE User='"+account+"'"),'172.16.1.133')
    self.assertNotEqual(subprocess.run(args,text=True,capture_output=True).returncode,0)
   finally:sql('DROP DATABASE IF EXISTS `'+name+'`; DROP USER IF EXISTS \''+account+'\'@\'172.16.1.133\'')
if __name__=='__main__':unittest.main()
