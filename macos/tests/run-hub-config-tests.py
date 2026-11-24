from pathlib import Path
import subprocess,shlex
r=Path(__file__).resolve().parents[2];b=r/'build-mac-universal'
commands=subprocess.check_output(['ninja','-t','commands','keeperfx'],cwd=b,text=True).splitlines()
a=next(shlex.split(x) for x in commands if '/net_hub.cpp.o ' in x);flags=[];i=1
while i<len(a):
 x=a[i]
 if x in ('-o','-MF','-MT'):i+=2;continue
 if x in ('-c','-MD','-DNDEBUG') or x.endswith('/net_hub.cpp'):i+=1;continue
 flags.append(x);i+=1
link=shlex.split(commands[-1]);libs=[];i=1
while i<len(link):
 x=link[i]
 if x=='-o':i+=2;continue
 if x.endswith('.o') or x in ('&&',':','-DNDEBUG','/usr/bin/c++'):i+=1;continue
 libs.append(x);i+=1
subprocess.run(['clang++',*flags,str(r/'macos/tests/test-hub-config.cpp'),'-Wl,-dead_strip',*libs,'-o','/tmp/test-hub-config'],cwd=b,check=True)
subprocess.run(['/tmp/test-hub-config'],check=True)

# A web server with rewriting disabled must be retried through the PHP entry point.
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
import threading,urllib.parse
requests=[]
class Routes(BaseHTTPRequestHandler):
 def log_message(self,*args):pass
 def do_GET(self):self.answer()
 def do_POST(self):self.answer()
 def answer(self):
  body=self.rfile.read(int(self.headers.get('Content-Length','0')))
  requests.append((self.path,self.command,self.headers.get('Authorization'),body))
  if self.path.startswith('/index.php?route='):
   self.send_response(200);self.send_header('Content-Type','application/json');self.end_headers();self.wfile.write(b'{"ok":true}')
  else:
   self.send_response(404);self.send_header('Content-Type','application/json' if self.path=='/api/not-found' else 'text/html');self.end_headers();self.wfile.write(b'not found')
server=ThreadingHTTPServer(('127.0.0.1',0),Routes);thread=threading.Thread(target=server.serve_forever);thread.start()
try:
 subprocess.run(['/tmp/test-hub-config','http://127.0.0.1:'+str(server.server_port)],check=True)
 assert len(requests)==5,requests
 assert urllib.parse.parse_qs(urllib.parse.urlsplit(requests[1][0]).query)['route']==['/api/streams/test?offset=4']
 assert requests[3][1:] == ('POST','Bearer test-token',b'{}'),requests
 print('Service URL persistence and HTML-404 direct-route fallback passed.')
finally:server.shutdown();server.server_close();thread.join()
