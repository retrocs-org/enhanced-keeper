"""UDP relay only. All public HTTP API and account storage are implemented in PHP."""
import hmac,secrets,selectors,socket,threading,time
class Problem(Exception):
    def __init__(self,status,message):self.status,self.message=status,message

def require(ok,text,status=400):
    if not ok:raise Problem(status,text)

class Relay:
    """One UDP socket per client. Both endpoints authenticate from their ENet socket.
    Forwarding only begins after two registrations; this is not an open UDP proxy.
    """
    def __init__(self, bind='127.0.0.1', public='127.0.0.1', ports=(0,0), capacity=24):
        self.bind,self.public,self.ports,self.capacity=bind,public,ports,capacity
        self.lock=threading.RLock();self.channels={};self.selector=selectors.DefaultSelector();self.alive=True
        self.thread=threading.Thread(target=self.run,daemon=True);self.thread.start()
    def allocate(self,room):
        with self.lock:
            require(len(self.channels)<self.capacity,'Relay capacity reached',503)
            require(sum(c['room']==room for c in self.channels.values())<8,'This lobby has too many pending relay joins; retry shortly',503)
            sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.setblocking(False)
            try:
                if self.ports[0]:
                    for port in range(self.ports[0],self.ports[1]+1):
                        try:sock.bind((self.bind,port));break
                        except OSError:continue
                    else:raise Problem(503,'Relay port range is full')
                else:sock.bind((self.bind,0))
                channel={'id':secrets.token_hex(12),'room':room,'host_key':secrets.token_urlsafe(24),'client_key':secrets.token_urlsafe(24),'host':None,'client':None,'socket':sock,'expires':time.monotonic()+45,'window':time.monotonic(),'bytes':0}
                self.channels[channel['id']]=channel;self.selector.register(sock,selectors.EVENT_READ,channel)
                return self.public_channel(channel,'client')
            except BaseException:sock.close();raise
    def public_channel(self,c,role):return {'id':c['id'],'host':self.public,'port':c['socket'].getsockname()[1],'token':c[role+'_key']}
    def hosting(self,room):
        with self.lock:return [self.public_channel(c,'host') for c in self.channels.values() if c['room']==room]
    def drop(self,c):
        self.selector.unregister(c['socket']);c['socket'].close();self.channels.pop(c['id'],None)
    def run(self):
        while self.alive:
            for key,_ in self.selector.select(.05):
                with self.lock:
                    c=key.data
                    try:data,addr=key.fileobj.recvfrom(65536)
                    except OSError:continue
                    if data.startswith(b'KFXR1 '):
                        try:_,role,token=data.decode('ascii').split(' ')
                        except (UnicodeError,ValueError):continue
                        if role in ('host','client') and hmac.compare_digest(token,c[role+'_key']):
                            c[role]=addr
                            if role=='client':c['expires']=time.monotonic()+45
                        continue
                    role='host' if addr==c['host'] else 'client' if addr==c['client'] else None
                    if not role or not c['host'] or not c['client']:continue
                    now=time.monotonic()
                    if now-c['window']>=1:c['window']=now;c['bytes']=0
                    c['bytes']+=len(data)
                    if c['bytes']>512*1024:continue
                    try:key.fileobj.sendto(data,c['client' if role=='host' else 'host']);c['expires']=now+45
                    except OSError:pass
            with self.lock:
                for c in list(self.channels.values()):
                    if c['expires']<time.monotonic():self.drop(c)
    def close(self):
        self.alive=False;self.thread.join(2)
        with self.lock:
            for c in list(self.channels.values()):self.drop(c)
        self.selector.close()

