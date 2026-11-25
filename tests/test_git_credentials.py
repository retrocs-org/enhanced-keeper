import importlib.util
from pathlib import Path
import subprocess
import unittest
from unittest.mock import patch

spec=importlib.util.spec_from_file_location('credential',Path(__file__).resolve().parents[1]/'scripts/git-credential-project.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class CredentialTests(unittest.TestCase):
    def test_scoped_hosts_and_paths(self):
        for host,user in {'github.com':'x-access-token','git.example.test':'oauth2'}.items():
            with patch.object(m,'destinations',return_value={host:{'repository':m.REPOSITORY,'username':user,'vault':'Private','item':host,'field':m.REPOSITORY+'-rw'}}), patch.object(m.subprocess,'run',return_value=subprocess.CompletedProcess([],0,'fixture-token\n','')) as run:
                value=m.credentials({'protocol':'https','host':host,'path':m.REPOSITORY+'.git'})
                self.assertEqual(value,{'username':user,'password':'fixture-token'})
                self.assertIn('label='+m.REPOSITORY+'-rw',run.call_args.args[0])
    def test_unrelated_destinations_get_no_credential(self):
        for request in ({'protocol':'http','host':'github.com','path':m.REPOSITORY},{'protocol':'https','host':'other.example','path':m.REPOSITORY},{'protocol':'https','host':'github.com','path':'other/repository'}):
            with patch.object(m.subprocess,'run') as run:
                self.assertEqual(m.credentials(request),{});run.assert_not_called()
    def test_failure_does_not_return_secret(self):
        with patch.object(m.subprocess,'run',return_value=subprocess.CompletedProcess([],1,'unexpected-output','private-error')):
            self.assertEqual(m.credentials({'protocol':'https','host':'github.com','path':m.REPOSITORY}),{'quit':'1'})
if __name__=='__main__':unittest.main()
