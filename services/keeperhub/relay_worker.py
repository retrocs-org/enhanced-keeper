#!/usr/bin/env python3
"""Private loopback control for the UDP relay; public HTTP belongs to PHP."""
import argparse,hmac,json,os,signal,threading
from pathlib import Path
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from relay import Relay,Problem

class ControlHandler(BaseHTTPRequestHandler):
    def log_message(self,*args):pass
    def do_POST(self):
        status=200
        try:
            if self.path!='/control':raise Problem(404,'Not found')
            if not hmac.compare_digest(self.headers.get('Authorization',''),'Bearer '+self.server.key):raise Problem(403,'Control credential rejected')
            length=int(self.headers.get('Content-Length','0'))
            if not 0<length<=4096:raise Problem(400,'Invalid control request')
            value=json.loads(self.rfile.read(length));action=value.get('action');room=value.get('room','')
            if not isinstance(room,str) or len(room)>64:raise Problem(400,'Invalid room')
            if action=='health':answer={'ok':self.server.relay.alive and self.server.relay.thread.is_alive()}
            elif action=='allocate':answer=self.server.relay.allocate(room)
            elif action=='hosting':answer={'channels':self.server.relay.hosting(room)}
            else:raise Problem(404,'Not found')
        except Problem as e:status=e.status;answer={'error':e.message}
        except (ValueError,TypeError,AttributeError):status=400;answer={'error':'Invalid control request'}
        body=json.dumps(answer,separators=(',',':')).encode();self.send_response(status);self.send_header('Content-Type','application/json');self.send_header('Content-Length',str(len(body)));self.send_header('Cache-Control','no-store');self.end_headers()
        try:self.wfile.write(body)
        except (BrokenPipeError,ConnectionResetError):pass

def serve_control(relay,key,port=0):
    server=ThreadingHTTPServer(('127.0.0.1',port),ControlHandler);server.relay=relay;server.key=key;return server

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--control-port',type=int,default=8766);parser.add_argument('--key-file',required=True,type=Path);parser.add_argument('--udp-bind',default='0.0.0.0');parser.add_argument('--public-host',default='127.0.0.1');parser.add_argument('--relay-first',type=int,default=27700);parser.add_argument('--relay-last',type=int,default=27731);args=parser.parse_args()
    key=args.key_file.read_text().strip()
    if args.key_file.is_symlink() or len(key)<32:parser.error('Use a private regular control key file with at least 32 characters')
    relay=Relay(args.udp_bind,args.public_host,(args.relay_first,args.relay_last));server=serve_control(relay,key,args.control_port)
    def stop(*_):threading.Thread(target=server.shutdown,daemon=True).start()
    signal.signal(signal.SIGTERM,stop);signal.signal(signal.SIGINT,stop)
    print('Private UDP relay control on 127.0.0.1:'+str(server.server_port),flush=True)
    try:server.serve_forever()
    finally:server.server_close();relay.close()
if __name__=='__main__':main()
