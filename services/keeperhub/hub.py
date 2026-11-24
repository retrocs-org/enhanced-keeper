#!/usr/bin/env python3
"""KeeperFX community directory and relay. Standard library, Python 3.9+. GPL-2.0-or-later."""
import argparse, base64, hashlib, hmac, http.cookies, ipaddress, json, os, re, secrets, selectors, socket, sqlite3, threading, time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit, parse_qs

class Problem(Exception):
    def __init__(self, status, message): self.status, self.message = status, message

def require(ok, text, status=400):
    if not ok: raise Problem(status, text)
def digest(s): return hashlib.sha256(s.encode()).hexdigest()
def clean(value, limit=80):
    require(isinstance(value,str) and 0<len(value)<=limit and all(ord(c)>=32 for c in value), 'Invalid text')
    return value

def endpoint(value):
    clean(value,150)
    match=re.fullmatch(r'(\[[0-9a-fA-F:]+\]|[A-Za-z0-9][A-Za-z0-9.-]*):([0-9]{1,5})',value)
    require(match and 0<int(match[2])<65536,'Use hostname:port or [IPv6]:port')
    return value

def udp_peer_probe(address, peer):
    """Probe only the requester's public address; never arbitrary/internal targets."""
    value=endpoint(address)
    host,port=value.rsplit(':',1);host=host.strip('[]');port=int(port)
    try:
        owner=ipaddress.ip_address(peer)
        if not owner.is_global or owner.is_multicast or owner.is_reserved:
            return {'reachable':None,'reason':'A loopback/LAN service cannot verify Internet reachability. Use a public service or a second machine.'}
        destinations=socket.getaddrinfo(host,port,type=socket.SOCK_DGRAM)
        require(destinations and all(ipaddress.ip_address(row[4][0])==owner for row in destinations),'The endpoint must resolve only to this connection’s public address. Check DNS/NAT settings.',400)
        family,kind,protocol,_,destination=destinations[0]
        nonce=secrets.token_hex(16).encode();packet=b'KFXQ1 '+nonce
        with socket.socket(family,kind,protocol) as sock:
            sock.settimeout(.8);sock.connect(destination);sock.send(packet)
            try:reachable=sock.recv(128)==b'KFXA1 '+nonce
            except (socket.timeout,ConnectionError,OSError):reachable=False
        return {'reachable':reachable,'reason':'Incoming UDP challenge answered by the game.' if reachable else 'No valid UDP reply. Open a lobby, then retry. Firewall, NAT or CGNAT may block direct hosting; try relay.'}
    except socket.gaierror:
        raise Problem(400,'The host name could not be resolved') from None
    except ValueError:
        raise Problem(400,'Invalid peer address') from None
    except OSError:
        return {'reachable':False,'reason':'The UDP probe could not reach the host. Open a lobby or try relay.'}

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

class Hub:
    def __init__(self,db,relay=None,admin_key=None,clock=time.time):
        self.db=str(db);self.relay=relay;self.clock=clock;self.admin_key=admin_key or secrets.token_urlsafe(32);self.rates={};self.last_cleanup=0;self.lock=threading.RLock()
        with self.connect() as c:c.executescript('''
        PRAGMA journal_mode=WAL;
        CREATE TABLE IF NOT EXISTS users(id TEXT PRIMARY KEY,name TEXT UNIQUE COLLATE NOCASE,salt TEXT,password TEXT,public INTEGER DEFAULT 0,created REAL);
        CREATE TABLE IF NOT EXISTS recovery(user TEXT,code TEXT UNIQUE,PRIMARY KEY(user,code));
        CREATE TABLE IF NOT EXISTS sessions(token TEXT PRIMARY KEY,user TEXT,csrf TEXT,expires REAL);
        CREATE INDEX IF NOT EXISTS session_expiry ON sessions(expires);
        CREATE TABLE IF NOT EXISTS devices(code TEXT PRIMARY KEY,user_code TEXT UNIQUE,user TEXT,expires REAL,last_poll REAL DEFAULT 0);
        CREATE TABLE IF NOT EXISTS servers(id TEXT PRIMARY KEY,name TEXT,endpoint TEXT,engine TEXT,map TEXT,mods TEXT,players INTEGER,capacity INTEGER,password INTEGER,region TEXT,kind TEXT,owner TEXT,secret TEXT,last_seen REAL,status TEXT,rotation TEXT);
        CREATE TABLE IF NOT EXISTS friends(a TEXT,b TEXT,status TEXT,PRIMARY KEY(a,b));
        CREATE TABLE IF NOT EXISTS matches(id TEXT PRIMARY KEY,server TEXT,started REAL,ended REAL DEFAULT 0,map TEXT,engine TEXT,status TEXT DEFAULT 'playing',winner TEXT DEFAULT '',verification TEXT DEFAULT 'casual');
        CREATE TABLE IF NOT EXISTS participants(match TEXT,user TEXT,result TEXT DEFAULT '',PRIMARY KEY(match,user));
        CREATE TABLE IF NOT EXISTS events(id TEXT PRIMARY KEY,owner TEXT,title TEXT,kind TEXT,starts REAL,description TEXT);
        CREATE TABLE IF NOT EXISTS bracket(event TEXT,round INTEGER,position INTEGER,a TEXT,b TEXT,winner TEXT,match TEXT UNIQUE,status TEXT,PRIMARY KEY(event,round,position));
        CREATE TABLE IF NOT EXISTS entrants(event TEXT,user TEXT,PRIMARY KEY(event,user));
        CREATE TABLE IF NOT EXISTS completions(user TEXT,challenge TEXT,match TEXT,PRIMARY KEY(user,challenge,match));
        CREATE TABLE IF NOT EXISTS challenges(id TEXT PRIMARY KEY,title TEXT,map TEXT,description TEXT);
        CREATE TABLE IF NOT EXISTS tickets(token TEXT PRIMARY KEY,server TEXT,user TEXT,expires REAL,peer INTEGER DEFAULT -1);
        CREATE TABLE IF NOT EXISTS identities(match TEXT,peer INTEGER,user TEXT,PRIMARY KEY(match,peer),UNIQUE(match,user));
        CREATE TABLE IF NOT EXISTS streams(id TEXT PRIMARY KEY,server TEXT,match TEXT,data BLOB,finished INTEGER,engine TEXT);
        CREATE TABLE IF NOT EXISTS replays(id TEXT PRIMARY KEY,match TEXT,owner TEXT,data BLOB,sha TEXT,size INTEGER);
        ''')
    def connect(self):
        c=sqlite3.connect(self.db,timeout=5);c.row_factory=sqlite3.Row;return c
    def limit(self,peer,bucket,maximum=20,seconds=60):
        now=self.clock();key=(peer,bucket)
        with self.lock:
            if len(self.rates)>4096:self.rates={k:v for k,v in self.rates.items() if v[0]>now-3600}
            require(key in self.rates or len(self.rates)<8192,'Service is busy; retry later',503)
            start,count=self.rates.get(key,(now,0));count=count+1 if now-start<seconds else 1;start=start if now-start<seconds else now;self.rates[key]=(start,count)
        require(count<=maximum,'Too many requests; try again later',429)
    def session(self,c,token):return c.execute('SELECT s.*,u.name FROM sessions s JOIN users u ON u.id=s.user WHERE token=? AND expires>?',(digest(token),self.clock())).fetchone() if token else None
    def new_session(self,c,user):
        token=secrets.token_urlsafe(32);csrf=secrets.token_urlsafe(24);c.execute('INSERT INTO sessions VALUES(?,?,?,?)',(digest(token),user,csrf,self.clock()+86400*30));return token,csrf
    def server(self,c,sid,token):
        row=c.execute('SELECT * FROM servers WHERE id=?',(sid,)).fetchone();require(row and hmac.compare_digest(row['secret'],digest(token)),'Server credential rejected',403);return row
    def handle(self,method,path,data,token='',peer='local',csrf='',cookie=False):
        now=self.clock();url=urlsplit(path);route=url.path;parts=route.strip('/').split('/');query=parse_qs(url.query)
        require(method in ('GET','POST'),'Unsupported method',405)
        if method=='POST' and route=='/api/connectivity':
            self.limit(peer,'connectivity',5,60)
            result=udp_peer_probe(data.get('endpoint',''),peer)
            return {**result,'relay_available':bool(self.relay and self.relay.alive and self.relay.thread.is_alive()),'relay_path_verified':False}

        with self.lock,self.connect() as c:
            if now-self.last_cleanup>=60:
                c.execute('DELETE FROM sessions WHERE expires<?',(now,));c.execute('DELETE FROM devices WHERE expires<?',(now,));c.execute('DELETE FROM tickets WHERE expires<?',(now,));self.last_cleanup=now
            session=self.session(c,token);uid=session['user'] if session else None
            if cookie and method=='POST':require(session and hmac.compare_digest(csrf,session['csrf']),'Reload the page before submitting',403)
            def auth():require(uid,'Sign in first',401);return uid
            if method=='GET':
                if route=='/api/health':
                    require(not self.relay or self.relay.alive and self.relay.thread.is_alive(),'Relay service unavailable',503)
                    return {'ok':True,'protocol':1,'relay':bool(self.relay and self.relay.alive and self.relay.thread.is_alive()),'authority':'client-simulated'}
                if route=='/api/session':return {'signed_in':bool(session),'name':session['name'] if session else '', 'csrf':session['csrf'] if session else ''}
                if route=='/api/me':
                    auth();row=c.execute('SELECT id,name,public FROM users WHERE id=?',(uid,)).fetchone();return dict(row)
                if route=='/api/servers':
                    c.execute("UPDATE matches SET ended=?,status=CASE WHEN status='lobby' THEN 'cancelled' ELSE 'abandoned' END WHERE ended=0 AND server IN (SELECT id FROM servers WHERE last_seen<?)",(now,now-90))
                    rows=c.execute("SELECT id,name,endpoint,engine,map,mods,players,capacity,password,region,kind,status FROM servers WHERE last_seen>? ORDER BY name LIMIT 128",(now-90,));return {'servers':[dict(r) for r in rows]}
                if route=='/api/friends':
                    auth();return {'friends':[dict(r) for r in c.execute('SELECT u.id,u.name,f.status,f.a AS requester FROM friends f JOIN users u ON u.id=CASE WHEN f.a=? THEN f.b ELSE f.a END WHERE f.a=? OR f.b=?',(uid,uid,uid))]}
                if route=='/api/matches':
                    auth()
                    rows=c.execute("SELECT m.* FROM matches m JOIN participants p ON p.match=m.id WHERE p.user=? AND m.status NOT IN ('lobby','cancelled') ORDER BY started DESC LIMIT 50",(uid,))
                    return {'matches':[dict(r) for r in rows]}
                if route=='/api/events':return {'events':[dict(r) for r in c.execute('SELECT e.*,COUNT(n.user) AS entrants FROM events e LEFT JOIN entrants n ON n.event=e.id GROUP BY e.id ORDER BY starts LIMIT 100')]}
                if route=='/api/challenges':return {'challenges':[dict(r) for r in c.execute('SELECT * FROM challenges ORDER BY title LIMIT 100')]}
                if len(parts)==4 and parts[:2]==['api','events'] and parts[3]=='bracket':
                    games=[dict(v) for v in c.execute('SELECT * FROM bracket WHERE event=? ORDER BY round,position',(parts[2],))]
                    if games:
                        for game in games:
                            for role in ('a','b','winner'):
                                player=c.execute('SELECT name FROM users WHERE id=?',(game[role],)).fetchone();game[role+'_name']=player['name'] if player else 'Bye'
                        return {'games':games,'champion':games[-1]['winner'] if games[-1]['status'] in ('played','bye') else ''}
                if len(parts)==4 and parts[:2]==['api','events'] and parts[3]=='bracket':
                    rows=[dict(r) for r in c.execute('SELECT u.id,u.name FROM entrants e JOIN users u ON u.id=e.user WHERE event=? ORDER BY u.id',(parts[2],))];return {'round_one':[{'a':rows[i],'b':rows[i+1] if i+1<len(rows) else None} for i in range(0,len(rows),2)]}
                if len(parts)==3 and parts[:2]==['api','profiles']:
                    row=c.execute('SELECT id,name,public FROM users WHERE id=?',(parts[2],)).fetchone();require(row,'Profile not found',404);require(row['public'] or uid==row['id'],'Profile is private',403)
                    stats=c.execute("SELECT COUNT(*) AS matches,SUM(CASE WHEN p.result='won' THEN 1 ELSE 0 END) AS wins FROM matches m JOIN participants p ON p.match=m.id WHERE p.user=? AND verification='verified' AND ended>0",(row['id'],)).fetchone();return {**dict(row),'verified_stats':dict(stats),'achievements':(['First verified match'] if stats['matches'] else [])+(['First victory'] if stats['wins'] else [])+(['Veteran Keeper'] if stats['matches']>=10 else [])+(['Challenge Keeper'] if c.execute('SELECT 1 FROM completions WHERE user=?',(row['id'],)).fetchone() else [])}
                if len(parts)==4 and parts[:2]==['api','servers'] and parts[3]=='spectate':
                    row=c.execute('SELECT id AS stream_id,engine,length(data) AS size,finished FROM streams WHERE server=? ORDER BY rowid DESC LIMIT 1',(parts[2],)).fetchone();require(row and row['size']>0,'No spectator stream yet; wait for the match to start',404);return dict(row)
                if len(parts)==3 and parts[:2]==['api','streams']:
                    row=c.execute('SELECT * FROM streams WHERE id=?',(parts[2],)).fetchone();require(row,'Stream not found',404);offset=int(query.get('offset',['0'])[0]);require(0<=offset<=len(row['data']),'Invalid stream offset');raw=row['data'][offset:offset+32768];return {'offset':offset,'data':base64.b64encode(raw).decode(),'size':len(row['data']),'finished':bool(row['finished']),'engine':row['engine']}
                if route=='/api/replays':return {'replays':[dict(r) for r in c.execute('SELECT r.id,r.match,r.sha,r.size FROM replays r JOIN matches m ON m.id=r.match ORDER BY m.started DESC LIMIT 100')]}
                if len(parts)==3 and parts[:2]==['api','replays']:
                    row=c.execute('SELECT * FROM replays WHERE id=?',(parts[2],)).fetchone();require(row,'Replay not found',404);return {'sha256':row['sha'],'data':base64.b64encode(row['data']).decode(),'engine':c.execute('SELECT engine FROM matches WHERE id=?',(row['match'],)).fetchone()[0]}
                raise Problem(404,'Not found')
            self.limit(peer,'requests',360)
            if route in ('/api/register','/api/login'):
                self.limit(peer,'login',10);name=clean(data.get('name'),24);password=clean(data.get('password'),128);require(re.fullmatch(r'[A-Za-z0-9_-]{3,24}',name),'Name must be 3–24 letters, numbers, _ or -');require(len(password)>=12,'Use at least 12 password characters')
                if route=='/api/register':
                    require(not c.execute('SELECT 1 FROM users WHERE name=?',(name,)).fetchone(),'Name is already registered',409);uid=secrets.token_hex(16);salt=secrets.token_hex(16);hashed=hashlib.pbkdf2_hmac('sha256',password.encode(),bytes.fromhex(salt),310000).hex();c.execute('INSERT INTO users(id,name,salt,password,created) VALUES(?,?,?,?,?)',(uid,name,salt,hashed,now))
                else:
                    row=c.execute('SELECT * FROM users WHERE name=?',(name,)).fetchone();salt=row['salt'] if row else '00'*16;hashed=hashlib.pbkdf2_hmac('sha256',password.encode(),bytes.fromhex(salt),310000).hex();require(row and hmac.compare_digest(hashed,row['password']),'Name or password is incorrect',401);uid=row['id']
                value,csrf=self.new_session(c,uid);return {'token':value,'csrf':csrf,'name':name}
            if route=='/api/recovery/codes':
                auth();self.limit(uid,'recovery-issue',3,3600)
                password=clean(data.get('password'),128);owner=c.execute('SELECT salt,password FROM users WHERE id=?',(uid,)).fetchone()
                require(hmac.compare_digest(hashlib.pbkdf2_hmac('sha256',password.encode(),bytes.fromhex(owner['salt']),310000).hex(),owner['password']),'Current password is incorrect',401)
                c.execute('DELETE FROM recovery WHERE user=?',(uid,))
                codes=[secrets.token_hex(12) for _ in range(8)]
                c.executemany('INSERT INTO recovery VALUES(?,?)',[(uid,digest(code)) for code in codes])
                return {'codes':codes,'message':'Store these codes offline. Each code can be used once. Previous codes are invalid.'}
            if route=='/api/recovery/reset':
                self.limit(peer,'recovery',5,3600)
                name=clean(data.get('name'),24);code=clean(data.get('code'),64);password=clean(data.get('password'),128)
                require(len(password)>=12,'Use at least 12 password characters')
                row=c.execute('SELECT u.id FROM users u JOIN recovery r ON r.user=u.id WHERE u.name=? AND r.code=?',(name,digest(code))).fetchone()
                require(row,'Account or recovery code is incorrect',401)
                salt=secrets.token_hex(16);hashed=hashlib.pbkdf2_hmac('sha256',password.encode(),bytes.fromhex(salt),310000).hex()
                c.execute('UPDATE users SET salt=?,password=? WHERE id=?',(salt,hashed,row['id']))
                c.execute('DELETE FROM recovery WHERE user=?',(row['id'],))
                c.execute('DELETE FROM sessions WHERE user=?',(row['id'],))
                c.execute('DELETE FROM devices WHERE user=?',(row['id'],))
                return {'ok':True,'message':'Password changed. All sessions and recovery codes were revoked. Sign in and generate new codes.'}
            if route=='/api/logout':auth();c.execute('DELETE FROM sessions WHERE token=?',(digest(token),));return {'ok':True}
            if route=='/api/device/start':
                self.limit(peer,'device',10);code=secrets.token_urlsafe(32);user_code=secrets.token_hex(4).upper();c.execute('DELETE FROM devices WHERE expires<?',(now,));c.execute('INSERT INTO devices(code,user_code,expires) VALUES(?,?,?)',(digest(code),user_code,now+600));return {'device_code':code,'user_code':user_code,'verification_path':'/device','expires_in':600,'interval':3}
            if route=='/api/device/poll':
                key=digest(clean(data.get('device_code'),100));row=c.execute('SELECT * FROM devices WHERE code=?',(key,)).fetchone();require(row and row['expires']>now,'Pairing expired; start again',410);require(now-row['last_poll']>=2,'Poll more slowly',429);c.execute('UPDATE devices SET last_poll=? WHERE code=?',(now,key))
                if not row['user']:return {'pending':True}
                value,_=self.new_session(c,row['user']);name=c.execute('SELECT name FROM users WHERE id=?',(row['user'],)).fetchone()[0];c.execute('DELETE FROM devices WHERE code=?',(key,));return {'token':value,'name':name}
            if route=='/api/device/approve':
                auth();code=clean(data.get('user_code'),8).upper();row=c.execute('SELECT * FROM devices WHERE user_code=? AND expires>?',(code,now)).fetchone();require(row and not row['user'],'Code is invalid or already approved',404);c.execute('UPDATE devices SET user=? WHERE user_code=?',(uid,code));return {'ok':True}
            if route=='/api/profile':auth();c.execute('UPDATE users SET public=? WHERE id=?',(bool(data.get('public')),uid));return {'ok':True}
            if route=='/api/friends':
                auth();other=clean(data.get('user'),32);require(other!=uid and c.execute('SELECT 1 FROM users WHERE id=?',(other,)).fetchone(),'Choose an existing other player');row=c.execute('SELECT * FROM friends WHERE (a=? AND b=?) OR (a=? AND b=?)',(uid,other,other,uid)).fetchone()
                if data.get('action')=='remove':c.execute('DELETE FROM friends WHERE (a=? AND b=?) OR (a=? AND b=?)',(uid,other,other,uid))
                elif row and row['b']==uid:c.execute("UPDATE friends SET status='accepted' WHERE a=? AND b=?",(other,uid))
                elif not row:c.execute("INSERT INTO friends VALUES(?,?,'pending')",(uid,other))
                return {'ok':True}
            if route=='/api/servers/register':
                self.limit(peer,'host',10);sid=secrets.token_hex(12);secret=secrets.token_urlsafe(32);kind=data.get('kind','listen');require(kind in ('listen','dedicated'),'Invalid host type');require(kind!='dedicated' or hmac.compare_digest(data.get('admin_key',''),self.admin_key),'Dedicated registration needs the local administrator key',403)
                name=clean(data.get('name'),64);address=endpoint(data.get('endpoint',''));engine=clean(data.get('engine'),32);region=clean(data.get('region','Local'),24);rotation=data.get('rotation',[]);require(isinstance(rotation,list) and len(rotation)<=32 and all(isinstance(x,str) and len(x)<=80 for x in rotation),'Invalid map rotation')
                c.execute('INSERT INTO servers VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)',(sid,name,address,engine,str(data.get('map',''))[:80],str(data.get('mods',''))[:64],1,4,bool(data.get('password')),region,kind,uid or '',digest(secret),now,'lobby',json.dumps(rotation)));mid=secrets.token_hex(16);c.execute("INSERT INTO matches(id,server,started,map,engine,status) VALUES(?,?,?,?,?,'lobby')",(mid,sid,now,str(data.get('map',''))[:80],engine));
                if uid:c.execute('INSERT INTO participants(match,user) VALUES(?,?)',(mid,uid))
                return {'id':sid,'secret':secret,'match_id':mid}
            if len(parts)>=3 and parts[:2]==['api','servers']:
                sid=parts[2];action=parts[3] if len(parts)>3 else ''
                if action=='join':
                    row=c.execute('SELECT * FROM servers WHERE id=? AND last_seen>?',(sid,now-90)).fetchone();require(row,'Server is offline',404);require(row['status']=='lobby' and row['players']<row['capacity'],'Server is full or playing',409);require(not data.get('engine') or data['engine']==row['engine'],'Engine version differs',409)
                    match=c.execute("SELECT id FROM matches WHERE server=? AND ended=0 ORDER BY started DESC LIMIT 1",(sid,)).fetchone();answer={'endpoint':row['endpoint'],'id':sid,'match_id':match['id'] if match else ''}
                    if uid and match:
                        c.execute('INSERT OR IGNORE INTO participants(match,user) VALUES(?,?)',(match['id'],uid));ticket=secrets.token_urlsafe(32);c.execute('INSERT INTO tickets(token,server,user,expires) VALUES(?,?,?,?)',(digest(ticket),sid,uid,now+600));answer['identity_ticket']=ticket
                    if data.get('relay'):require(self.relay,'Relay is unavailable',503);answer['relay']=self.relay.allocate(sid)
                    return answer
                row=self.server(c,sid,token)
                if action=='heartbeat':
                    players=data.get('players',1);require(type(players)==int and 0<=players<=4,'Invalid player count');status=data.get('status','lobby');require(status in ('lobby','playing','offline'),'Invalid server status');c.execute('UPDATE servers SET last_seen=?,players=?,status=?,map=? WHERE id=?',(now,players,status,str(data.get('map',row['map']))[:80],sid));
                    if status=='offline':c.execute("UPDATE matches SET ended=?,status=CASE WHEN status='lobby' THEN 'cancelled' ELSE 'abandoned' END WHERE server=? AND ended=0",(now,sid))
                    if status=='playing':c.execute("UPDATE matches SET status='playing',map=? WHERE server=? AND status='lobby' AND ended=0",(str(data.get('map',row['map']))[:80],sid))
                    return {'ok':True,'channels':self.relay.hosting(sid) if self.relay else []}
                if action=='identity':
                    ticket=c.execute('SELECT * FROM tickets WHERE token=? AND server=? AND expires>?',(digest(clean(data.get('ticket'),100)),sid,now)).fetchone();peer_id=data.get('peer');require(type(peer_id)==int and 1<=peer_id<4,'Invalid peer');require(ticket and ticket['peer'] in (-1,peer_id),'Identity ticket rejected',403);mid=c.execute('SELECT id FROM matches WHERE server=? AND ended=0 ORDER BY started DESC LIMIT 1',(sid,)).fetchone();require(mid,'No active match',409)
                    existing=c.execute('SELECT user FROM identities WHERE match=? AND peer=?',(mid['id'],peer_id)).fetchone();require(not existing or existing['user']==ticket['user'],'Peer identity is already bound',409);other=c.execute('SELECT peer FROM identities WHERE match=? AND user=?',(mid['id'],ticket['user'])).fetchone();require(not other or other['peer']==peer_id,'Account already occupies a peer',409)
                    c.execute('INSERT OR IGNORE INTO identities VALUES(?,?,?)',(mid['id'],peer_id,ticket['user']));c.execute('UPDATE tickets SET peer=? WHERE token=?',(peer_id,ticket['token']));return {'user':ticket['user'],'peer':peer_id}
                if action=='result':
                    require(row['kind']=='dedicated','Only a trusted dedicated engine may verify results',403);mid=clean(data.get('match'),32);match=c.execute('SELECT * FROM matches WHERE id=? AND server=?',(mid,sid)).fetchone();require(match and not match['ended'],'Match is missing or closed',409);results=data.get('results');require(isinstance(results,list) and 0<len(results)<=3,'Invalid engine results');identities={v['user'] for v in c.execute('SELECT user FROM identities WHERE match=?',(mid,))};require(len({v.get('user') for v in results})==len(results) and {v.get('user') for v in results}==identities,'Results must cover the authenticated game peers',409);require(all(v.get('result') in ('won','lost','draw') for v in results),'Unfinished or disconnected matches cannot be verified');winners=[v['user'] for v in results if v['result']=='won'];require(len(winners)<=1 or bool(data.get('cooperative')),'Multiple winners require a cooperative host result');c.executemany('UPDATE participants SET result=? WHERE match=? AND user=?',[(v['result'],mid,v['user']) for v in results]);c.execute("UPDATE matches SET ended=?,status='finished',winner=?,verification='verified' WHERE id=?",(now,winners[0] if winners else '',mid));return {'ok':True,'verification':'verified'}
                if action=='stream':
                    require(row['kind']=='dedicated','Only a dedicated host may publish spectator data',403)
                    try:raw=base64.b64decode(data.get('data',''),validate=True)
                    except (ValueError,TypeError):raise Problem(400,'Invalid stream encoding')
                    offset=data.get('offset',0);require(type(offset)==int and offset>=0 and len(raw)<=32768,'Invalid stream chunk')
                    stream=c.execute('SELECT * FROM streams WHERE server=? AND finished=0 ORDER BY rowid DESC LIMIT 1',(sid,)).fetchone()
                    if not stream:
                        require(offset==0,'Start the stream at offset zero');mid=c.execute('SELECT id FROM matches WHERE server=? ORDER BY started DESC LIMIT 1',(sid,)).fetchone();streamid=secrets.token_hex(16);c.execute('INSERT INTO streams VALUES(?,?,?,?,0,?)',(streamid,sid,mid['id'] if mid else '',b'',row['engine']));stream=c.execute('SELECT * FROM streams WHERE id=?',(streamid,)).fetchone()
                    old=stream['data'];require(offset<=len(old),'Stream chunk has a gap',409)
                    if offset<len(old):require(old[offset:offset+len(raw)]==raw,'Stream retry differs',409)
                    else:
                        if len(old)+len(raw)>16*1024*1024:
                            c.execute('UPDATE streams SET finished=1 WHERE id=?',(stream['id'],));c.execute('INSERT OR IGNORE INTO replays VALUES(?,?,?,?,?,?)',(stream['id'],stream['match'],row['owner'],sqlite3.Binary(old),hashlib.sha256(old).hexdigest(),len(old)));return {'id':stream['id'],'offset':len(old),'truncated':True}
                        old+=raw
                    c.execute('UPDATE streams SET data=?,finished=? WHERE id=?',(sqlite3.Binary(old),bool(data.get('finished')),stream['id']));
                    if data.get('finished') and old:c.execute('INSERT OR IGNORE INTO replays VALUES(?,?,?,?,?,?)',(stream['id'],stream['match'],row['owner'],sqlite3.Binary(old),hashlib.sha256(old).hexdigest(),len(old)))
                    return {'id':stream['id'],'offset':len(old)}
                if action=='close':c.execute("UPDATE matches SET ended=?,status=CASE WHEN status='lobby' THEN 'cancelled' ELSE 'abandoned' END WHERE server=? AND ended=0",(now,sid));c.execute('DELETE FROM servers WHERE id=?',(sid,));return {'ok':True}
                if action=='match':
                    participants=data.get('participants',[]);require(isinstance(participants,list) and len(participants)<=4,'Invalid participants');require(len(set(participants))==len(participants),'Duplicate participants');require(all(c.execute('SELECT 1 FROM users WHERE id=?',(x,)).fetchone() for x in participants),'Unknown participant');mid=secrets.token_hex(16);c.execute('INSERT INTO matches(id,server,started,map,engine) VALUES(?,?,?,?,?)',(mid,sid,now,row['map'],row['engine']));c.executemany('INSERT INTO participants(match,user) VALUES(?,?)',[(mid,x) for x in participants]);c.execute("UPDATE servers SET status='playing' WHERE id=?",(sid,));return {'id':mid}
            if len(parts)==4 and parts[:2]==['api','matches'] and parts[3]=='result':
                mid=parts[2];row=c.execute('SELECT * FROM matches WHERE id=?',(mid,)).fetchone();require(row,'Match not found',404);result=data.get('result');require(result in ('won','lost','draw','disconnected'),'Invalid result');auth();require(c.execute('SELECT 1 FROM participants WHERE match=? AND user=?',(mid,uid)).fetchone(),'Not a match participant',403);require(not row['ended'],'Match is already closed',409);c.execute('UPDATE participants SET result=? WHERE match=? AND user=?',(result,mid,uid));results=c.execute('SELECT * FROM participants WHERE match=?',(mid,)).fetchall()
                if results and all(x['result'] for x in results) and c.execute('SELECT kind FROM servers WHERE id=?',(row['server'],)).fetchone()[0]!='dedicated':
                    winners=[x['user'] for x in results if x['result']=='won'];consistent=len(winners)==1 and all(x['result'] in ('won','lost') for x in results);c.execute('UPDATE matches SET ended=?,winner=?,verification=?,status=? WHERE id=?',(now,winners[0] if len(winners)==1 else '', 'confirmed' if consistent else 'casual','finished',mid))
                return {'ok':True,'verification':'client-reported'}
            if route=='/api/events':
                auth();title=clean(data.get('title'),80);kind=data.get('kind','match');require(kind in ('match','tournament','cooperative'),'Invalid event type');starts=data.get('starts');require(type(starts) in (int,float) and now<=starts<=now+366*86400,'Invalid event time');eid=secrets.token_hex(12);c.execute('INSERT INTO events VALUES(?,?,?,?,?,?)',(eid,uid,title,kind,starts,str(data.get('description',''))[:1000]));return {'id':eid}
            if len(parts)==4 and parts[:2]==['api','events'] and parts[3]=='join':auth();require(c.execute('SELECT 1 FROM events WHERE id=?',(parts[2],)).fetchone(),'Event not found',404);require(not c.execute('SELECT 1 FROM bracket WHERE event=?',(parts[2],)).fetchone(),'Tournament registration has closed',409);c.execute('INSERT OR IGNORE INTO entrants VALUES(?,?)',(parts[2],uid));return {'ok':True}
            if len(parts)==5 and parts[:2]==['api','events'] and parts[3]=='bracket':
                auth();event=c.execute('SELECT * FROM events WHERE id=?',(parts[2],)).fetchone();require(event and event['owner']==uid and event['kind']=='tournament','Tournament organizer required',403)
                if parts[4]=='start':
                    require(not c.execute('SELECT 1 FROM bracket WHERE event=?',(parts[2],)).fetchone(),'Bracket already started',409);entrants=[v['user'] for v in c.execute('SELECT user FROM entrants WHERE event=?',(parts[2],))];require(2<=len(entrants)<=32,'A bracket needs 2–32 entrants');secrets.SystemRandom().shuffle(entrants);width=1
                    while width<len(entrants):width*=2
                    entrants+=['']*(width-len(entrants));round_no=1;count=width//2
                    while count:
                        for position in range(count):
                            a,b=(entrants[position],entrants[width-position-1]) if round_no==1 else ('','');bye=round_no==1 and (not a or not b);c.execute('INSERT INTO bracket VALUES(?,?,?,?,?,?,NULL,?)',(parts[2],round_no,position,a,b,(a or b) if bye else '', 'bye' if bye else 'pending' if round_no==1 else 'waiting'))
                        round_no+=1;count//=2
                elif parts[4]=='report':
                    round_no,position=data.get('round'),data.get('position');require(type(round_no)==int and type(position)==int,'Choose a bracket game');game=c.execute('SELECT * FROM bracket WHERE event=? AND round=? AND position=?',(parts[2],round_no,position)).fetchone();require(game and game['status']=='pending','Bracket game is not ready',409);match=c.execute("SELECT * FROM matches WHERE id=? AND verification='verified'",(data.get('match'),)).fetchone();require(match and match['winner'] in (game['a'],game['b']),'Choose a verified match won by this pairing',409);participants={v['user'] for v in c.execute('SELECT user FROM identities WHERE match=?',(match['id'],))};require(participants=={game['a'],game['b']},'Verified players differ from the bracket',409);require(not c.execute('SELECT 1 FROM bracket WHERE match=?',(match['id'],)).fetchone(),'Match already used in a bracket',409);c.execute("UPDATE bracket SET winner=?,match=?,status='played' WHERE event=? AND round=? AND position=?",(match['winner'],match['id'],parts[2],round_no,position))
                else:raise Problem(404,'Not found')
                for game in c.execute("SELECT * FROM bracket WHERE event=? AND status='waiting' ORDER BY round,position",(parts[2],)).fetchall():
                    parents=c.execute('SELECT * FROM bracket WHERE event=? AND round=? AND position IN (?,?) ORDER BY position',(parts[2],game['round']-1,game['position']*2,game['position']*2+1)).fetchall()
                    if len(parents)==2 and all(v['status'] in ('played','bye') for v in parents):
                        a,b=[v['winner'] for v in parents];bye=not a or not b;c.execute('UPDATE bracket SET a=?,b=?,winner=?,status=? WHERE event=? AND round=? AND position=?',(a,b,(a or b) if bye else '', 'bye' if bye else 'pending',parts[2],game['round'],game['position']))
                return {'ok':True}
            if len(parts)==4 and parts[:2]==['api','challenges'] and parts[3]=='complete':
                auth();challenge=c.execute('SELECT * FROM challenges WHERE id=?',(parts[2],)).fetchone();require(challenge,'Challenge not found',404);match=c.execute("SELECT m.* FROM matches m JOIN participants p ON p.match=m.id WHERE m.id=? AND p.user=? AND p.result='won' AND m.verification='verified'",(data.get('match'),uid)).fetchone();require(match and match['map']==challenge['map'],'Use a verified win on this challenge map',409);c.execute('INSERT OR IGNORE INTO completions VALUES(?,?,?)',(uid,parts[2],match['id']));return {'ok':True}
            if route=='/api/challenges':
                require(hmac.compare_digest(token,self.admin_key),'Administrator credential required',403);cid=secrets.token_hex(12);c.execute('INSERT INTO challenges VALUES(?,?,?,?)',(cid,clean(data.get('title'),80),clean(data.get('map'),80),clean(data.get('description'),1000)));return {'id':cid}
            if route=='/api/replays':
                auth();mid=clean(data.get('match'),32);row=c.execute('SELECT m.* FROM matches m JOIN participants p ON p.match=m.id WHERE m.id=? AND p.user=?',(mid,uid)).fetchone();require(row and row['ended'],'Only participants can upload completed-match replays',403)
                try:raw=base64.b64decode(data.get('data',''),validate=True)
                except (ValueError,TypeError):raise Problem(400,'Invalid replay encoding')
                require(0<len(raw)<=2*1024*1024,'Replay limit is 2 MiB');rid=secrets.token_hex(16);sha=hashlib.sha256(raw).hexdigest();c.execute('INSERT INTO replays VALUES(?,?,?,?,?,?)',(rid,mid,uid,raw,sha,len(raw)));return {'id':rid,'sha256':sha}
            raise Problem(404,'Not found')

class Handler(BaseHTTPRequestHandler):
    server_version='KeeperHub/1'
    def setup(self):
        super().setup();self.connection.settimeout(5)
    def log_message(self,*args):pass # Never log pairing codes, tokens or request bodies.
    def respond(self,status,body,kind='application/json',cookie=None):
        raw=json.dumps(body,separators=(',',':')).encode() if kind=='application/json' else body
        self.send_response(status);self.send_header('Content-Type',kind);self.send_header('Content-Length',str(len(raw)));self.send_header('Cache-Control','no-store');self.send_header('X-Content-Type-Options','nosniff');self.send_header('Referrer-Policy','no-referrer');self.send_header('Content-Security-Policy',"default-src 'self'; script-src 'self'; style-src 'self'; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'")
        if cookie:self.send_header('Set-Cookie',cookie)
        self.end_headers();self.wfile.write(raw)
    def do_GET(self):self.dispatch('GET')
    def do_POST(self):self.dispatch('POST')
    def dispatch(self,method):
        try:
            origin=self.headers.get('Origin');expected=self.server.origin
            require(not origin or origin==expected,'Cross-origin request rejected',403)
            require(self.headers.get('Host')==urlsplit(expected).netloc,'Unexpected service host',403)
            require(len(self.headers.get_all('Content-Length',[]))<=1,'Duplicate content length',400)
            route=urlsplit(self.path).path
            require(route.startswith('/api/'),'Not found',404)
            require(self.headers.get('Transfer-Encoding') is None,'Chunked requests are unsupported',400);length=int(self.headers.get('Content-Length','0'));require(0<=length<=3*1024*1024,'Request too large',413)
            data=json.loads(self.rfile.read(length) or b'{}') if method=='POST' else {};require(isinstance(data,dict),'Expected an object')
            jar=http.cookies.SimpleCookie();jar.load(self.headers.get('Cookie',''));cookie=jar.get('kfx_session');bearer=self.headers.get('Authorization','');token=bearer[7:] if bearer.startswith('Bearer ') else cookie.value if cookie else ''
            peer=self.client_address[0]
            if self.server.trust_loopback_proxy and ipaddress.ip_address(peer).is_loopback and self.headers.get('X-KFX-Client-IP'):
                peer=str(ipaddress.ip_address(self.headers['X-KFX-Client-IP']))
            result=self.server.hub.handle(method,self.path,data,token,peer,self.headers.get('X-CSRF-Token',''),bool(cookie and not bearer));set_cookie=None
            if route in ('/api/login','/api/register') and 'token' in result:
                value=result.pop('token');set_cookie='kfx_session='+value+'; HttpOnly; SameSite=Strict; Path=/; Max-Age=2592000'+('; Secure' if expected.startswith('https://') else '')
            if route=='/api/logout':set_cookie='kfx_session=; HttpOnly; SameSite=Strict; Path=/; Max-Age=0'
            self.respond(200,result,cookie=set_cookie)
        except Problem as e:self.respond(e.status,{'error':e.message})
        except (ValueError,TypeError,KeyError,OverflowError):self.respond(400,{'error':'Malformed request'})
        except (BrokenPipeError,ConnectionResetError):pass
        except Exception:self.respond(500,{'error':'Service error'})

def serve(hub,host='127.0.0.1',port=8765,origin=None,trust_loopback_proxy=False):
    server=ThreadingHTTPServer((host,port),Handler);server.hub=hub;server.trust_loopback_proxy=trust_loopback_proxy;server.origin=origin or 'http://'+host+':'+str(server.server_port);return server

def main():
    p=argparse.ArgumentParser();p.add_argument('--db',default='keeperhub.sqlite3');p.add_argument('--bind',default='127.0.0.1');p.add_argument('--port',type=int,default=8765);p.add_argument('--public-host',default='127.0.0.1');p.add_argument('--relay-first',type=int,default=27700);p.add_argument('--relay-last',type=int,default=27731);p.add_argument('--origin');p.add_argument('--trust-loopback-proxy',action='store_true',help='Trust client IP supplied by the local PHP gateway');a=p.parse_args()
    require(a.bind in ('127.0.0.1','::1') or a.origin and a.origin.startswith('https://'),'Non-loopback HTTP requires an HTTPS reverse-proxy origin')
    os.umask(0o077);key=os.environ.get('KFX_HUB_ADMIN_KEY');require(key and len(key)>=24,'Set KFX_HUB_ADMIN_KEY to a random value of at least 24 characters')
    relay=Relay(a.bind,a.public_host,(a.relay_first,a.relay_last));hub=Hub(a.db,relay,key);server=serve(hub,a.bind,a.port,a.origin,a.trust_loopback_proxy);print('KeeperHub local service:',server.origin,flush=True)
    try:server.serve_forever()
    except KeyboardInterrupt:pass
    finally:server.server_close();relay.close()
if __name__=='__main__':main()
