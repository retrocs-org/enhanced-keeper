#!/usr/bin/env python3
"""Manual service/helper connectivity checks; no external hosts are contacted."""
import importlib.util
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import threading
import unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('hub',ROOT/'macos/tests/legacy_keeperhub.py');hub=importlib.util.module_from_spec(spec);spec.loader.exec_module(hub)
class UDP:
    def __enter__(self):return self
    def __exit__(self,*args):pass
    def settimeout(self,value):pass
    def connect(self,address):self.address=address
    def send(self,value):self.packet=value
    def recv(self,size):return self.packet.replace(b'KFXQ1 ',b'KFXA1 ',1)
class ConnectivityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary=tempfile.TemporaryDirectory();cls.executable=Path(os.environ['KFX_TEST_CONNECTION_CHECK']) if os.environ.get('KFX_TEST_CONNECTION_CHECK') else Path(cls.temporary.name)/'connection-check'
        if not os.environ.get('KFX_TEST_CONNECTION_CHECK'):
            deps=Path(os.environ.get('KFX_MAC_DEPS_ROOT',ROOT/'build-macos-deps'))
            subprocess.run(['clang','-O2',str(ROOT/'macos/packaging/connection-check.c'),'-I'+str(deps/'curl-prefix/include'),str(deps/'curl-prefix/lib/libcurl.a'),str(deps/'openssl-fat/lib/libssl.a'),str(deps/'openssl-fat/lib/libcrypto.a'),'-lz','-framework','Security','-framework','CoreFoundation','-framework','SystemConfiguration','-o',str(cls.executable)],check=True)
    @classmethod
    def tearDownClass(cls):cls.temporary.cleanup()

    def test_nonce_probe_is_pinned_to_requester(self):
        udp=UDP()
        with patch.object(hub.socket,'getaddrinfo',return_value=[(socket.AF_INET,socket.SOCK_DGRAM,17,'',('8.8.8.8',5556))]),patch.object(hub.socket,'socket',return_value=udp):
            self.assertTrue(hub.udp_peer_probe('myhost.example:5556','8.8.8.8')['reachable'])
        self.assertEqual(udp.address,('8.8.8.8',5556));self.assertEqual(len(udp.packet),38)
    def test_rejects_arbitrary_and_private_destinations(self):
        with patch.object(hub.socket,'getaddrinfo',return_value=[(socket.AF_INET,socket.SOCK_DGRAM,17,'',('1.1.1.1',5556))]),patch.object(hub.socket,'socket') as make:
            with self.assertRaises(hub.Problem):hub.udp_peer_probe('other.example:5556','8.8.8.8')
            make.assert_not_called()
        with patch.object(hub.socket,'socket') as make:
            self.assertIsNone(hub.udp_peer_probe('localhost:5556','127.0.0.1')['reachable']);make.assert_not_called()
    def test_real_helper_reports_local_limits_and_expected_bound_port(self):
        executable=self.executable
        self.assertTrue(executable.is_file(),'Build the local connection-check helper first')
        with tempfile.TemporaryDirectory() as temporary, socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as bound:
            service=hub.Hub(Path(temporary)/'service.sqlite3');server=hub.serve(service,port=0);thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
            try:
                bound.bind(('127.0.0.1',0));port=bound.getsockname()[1]
                result=subprocess.run([str(executable),'--service',server.origin,'--endpoint',f'127.0.0.1:{port}','--port',str(port),'--hosting','1'],capture_output=True,text=True,timeout=8)
                self.assertEqual(result.returncode,0,result.stdout+result.stderr)
                self.assertIn('Community service: ready.',result.stdout)
                self.assertIn('Direct hosting: not verified',result.stdout)
                self.assertIn('expected while hosting',result.stdout)
                self.assertNotIn('Direct hosting: verified',result.stdout)
            finally:server.shutdown();server.server_close();thread.join()
if __name__=='__main__':unittest.main()
