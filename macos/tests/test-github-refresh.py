#!/usr/bin/env python3
"""Exercise the standalone shell deployer with local Git/ownership command doubles."""
import os
from pathlib import Path
import shlex
import shutil
import stat
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2]

class GithubRefreshTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);self.parent=self.root/'vhosts';self.parent.mkdir()
        self.site=self.parent/'keeperfx-e.retrocs.org';self.fixture=self.root/'fixture';web=self.fixture/'public-web';web.mkdir(parents=True)
        (web/'index.php').write_text('<?php echo "new website";\n')
        (web/'config.inc.php').write_text("<?php return ['password'=>''];\n")
        self.bin=self.root/'bin';self.bin.mkdir()
        self.stub('id','#!/bin/sh\necho 0\n')
        self.stub('getent','#!/bin/sh\nexit 0\n')
        self.stub('flock','#!/bin/sh\n[ "${TEST_LOCK_FAIL:-0}" = 0 ]\n')
        self.stub('chown','#!/bin/sh\nprintf "%s\\n" "$*" >> "$TEST_CHOWN_LOG"\n')
        self.stub('git','''#!/usr/bin/env python3
import os,shutil,sys
from pathlib import Path
Path(os.environ['TEST_GIT_LOG']).write_text(' '.join(sys.argv[1:]))
shutil.copytree(os.environ['TEST_SOURCE'],sys.argv[-1])
''')
        self.stub('mv','''#!/usr/bin/env python3
import os,sys
from pathlib import Path
if os.environ.get('TEST_SWAP_FAIL')=='1' and Path(sys.argv[1]).name=='new':sys.exit(1)
os.execv(os.environ['TEST_REAL_MV'],[os.environ['TEST_REAL_MV'],*sys.argv[1:]])
''')
        self.env={**os.environ,'PATH':str(self.bin)+os.pathsep+os.environ['PATH'],'TEST_SOURCE':str(self.fixture),'TEST_GIT_LOG':str(self.root/'git.log'),'TEST_CHOWN_LOG':str(self.root/'chown.log'),'TEST_REAL_MV':shutil.which('mv')}
        self.script=self.root/'refresh.sh'
        self.script.write_text((ROOT/'scripts/refresh-from-github.sh').read_text().replace('PARENT=/srv/www/vhosts','PARENT='+shlex.quote(str(self.parent))))
    def stub(self,name,text):
        p=self.bin/name;p.write_text(text);p.chmod(0o755)
    def old_site(self,config="<?php return ['password'=>'private-test-value'];\n"):
        self.site.mkdir();(self.site/'index.php').write_text('<?php echo "old website";\n')
        (self.site/'config.inc.php').write_text(config)
    def run_refresh(self,**env):
        return subprocess.run(['sh',str(self.script)],env={**self.env,**env},text=True,capture_output=True)
    def test_preserves_config_and_sets_modes(self):
        self.old_site();config=self.site/'config.inc.php';before=config.stat().st_mtime_ns;content=config.read_bytes()
        result=self.run_refresh();self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(config.read_bytes(),content);self.assertEqual(config.stat().st_mtime_ns,before)
        self.assertIn('new website',(self.site/'index.php').read_text())
        self.assertEqual(stat.S_IMODE(self.site.stat().st_mode),0o750)
        for p in self.site.rglob('*'):
            self.assertEqual(stat.S_IMODE(p.stat().st_mode),0o750 if p.is_dir() else 0o640)
        self.assertNotIn('private-test-value',result.stdout+result.stderr)
        self.assertIn('wwwrun:wwwrun',(self.root/'chown.log').read_text())
        self.assertIn('https://github.com/retrocs-org/KeeperFX-E.git',(self.root/'git.log').read_text())
        self.assertEqual([p for p in self.parent.glob('.refresh-keeperfx-e.*') if not p.name.endswith('.lock')],[])
    def test_invalid_live_config_leaves_site_untouched_without_leaking_values(self):
        self.old_site("<?php return ['password'=>'private-test-value' broken;\n")
        result=self.run_refresh();self.assertNotEqual(result.returncode,0)
        self.assertIn('old website',(self.site/'index.php').read_text())
        self.assertNotIn('private-test-value',result.stdout+result.stderr)
    def test_failed_swap_restores_previous_site(self):
        self.old_site();result=self.run_refresh(TEST_SWAP_FAIL='1');self.assertNotEqual(result.returncode,0)
        self.assertIn('old website',(self.site/'index.php').read_text())
        self.assertIn('private-test-value',(self.site/'config.inc.php').read_text())
    def test_first_install_uses_default_config(self):
        result=self.run_refresh();self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual((self.site/'config.inc.php').read_bytes(),(self.fixture/'public-web/config.inc.php').read_bytes())
    def test_upgrades_older_site_without_config_using_sample(self):
        self.old_site();(self.site/'config.inc.php').unlink()
        result=self.run_refresh();self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual((self.site/'config.inc.php').read_bytes(),(self.fixture/'public-web/config.inc.php').read_bytes())
        self.assertIn('Installing sample config.inc.php',result.stdout)
    def test_locked_update_and_symlink_config_are_refused(self):
        self.old_site();result=self.run_refresh(TEST_LOCK_FAIL='1');self.assertNotEqual(result.returncode,0)
        self.assertFalse((self.root/'git.log').exists())
        config=self.site/'config.inc.php';config.unlink();config.symlink_to(self.fixture/'public-web/config.inc.php')
        result=self.run_refresh();self.assertNotEqual(result.returncode,0);self.assertTrue(config.is_symlink())

if __name__=='__main__':unittest.main()
