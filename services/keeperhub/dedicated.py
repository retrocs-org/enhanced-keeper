#!/usr/bin/env python3
"""Headless KeeperFX host supervisor. Standard library, GPL-2.0-or-later.
One synchronized AI host + up to three human players. Does not claim a fifth slot.
"""
import argparse,getpass,stat,tempfile,base64,fcntl,json,os,re,secrets,signal,socket,subprocess,sys,threading,time,urllib.request,urllib.error,urllib.parse
from ipaddress import IPv4Address
from pathlib import Path
import struct

class NoCredentialRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, response, code, message, headers, new_url):
        return None

def owner_credential_path(profile):
    return profile / 'dedicated' / 'owner-credential.json'

def save_owner_credential(profile, base, credential):
    if not re.fullmatch(r'[A-Za-z0-9_-]{43}', credential):
        raise ValueError('Paste the dedicated server credential from your cloud profile')
    destination=owner_credential_path(profile)
    destination.parent.mkdir(mode=0o700,exist_ok=True)
    with tempfile.NamedTemporaryFile(mode='w', dir=destination.parent, delete=False) as temporary:
        try:
            json.dump({'hub':base,'credential':credential},temporary)
            temporary.flush()
            os.fsync(temporary.fileno())
            os.replace(temporary.name,destination)
        finally:
            Path(temporary.name).unlink(missing_ok=True)

def read_owner_credential(profile, base):
    try:
        with os.fdopen(os.open(owner_credential_path(profile),os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)) as stream:
            info=os.fstat(stream.fileno())
            if not stat.S_ISREG(info.st_mode) or info.st_uid!=os.geteuid() or info.st_mode&0o077:
                raise ValueError('Credential file must be owned by the server user with mode 0600')
            stored=json.loads(stream.read(4096))
        credential=stored.get('credential','')
        if stored.get('hub')!=base or not re.fullmatch(r'[A-Za-z0-9_-]{43}',credential):
            raise ValueError('Credential belongs to another hub or is invalid')
        return credential
    except (OSError,ValueError,TypeError,AttributeError) as error:
        raise RuntimeError(f'Public dedicated hosting needs a cloud owner credential. Run this supervisor with --link. {error}') from error

def request(base,route,data=None,token=''):
    headers={'Content-Type':'application/json'}
    if token:headers['Authorization']='Bearer '+token
    opener=urllib.request.build_opener(NoCredentialRedirect()).open if token else urllib.request.urlopen
    req=urllib.request.Request(base+route,data=json.dumps(data).encode() if data is not None else None,headers=headers)
    try:
        with opener(req,timeout=5) as r:return json.load(r)
    except urllib.error.HTTPError as error:
        if error.code!=404 or 'text/html' not in error.headers.get('Content-Type',''):raise
        # Same-origin fallback for PHP vhosts without rewrite rules.
        direct=base+'/index.php?route='+urllib.parse.quote(route,safe='')
        retry=urllib.request.Request(direct,data=req.data,headers=headers)
        with opener(retry,timeout=5) as response:return json.load(response)

def server_version(engine):
    version = subprocess.check_output([str(engine), '--server-build-info'], text=True, timeout=5).strip()
    if not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+', version):
        raise RuntimeError('The dedicated engine did not report a Keeper Enhanced server version')
    return version

def supported_clients(engine):
    output = subprocess.check_output([str(engine), '--supported-client-versions'], text=True, timeout=5)
    return output.splitlines()

def rpc(port,action="get_network_info",**fields):
    with socket.create_connection(('127.0.0.1',port),timeout=2) as c:
        c.sendall((json.dumps({'action':action,'ack':1,**fields})+'\n').encode());raw=b''
        while b'\n' not in raw and len(raw)<65536:
            chunk=c.recv(4096)
            if not chunk:break
            raw+=chunk
        value=json.loads(raw.split(b'\n',1)[0]);return (value.get('data') or {}) if action=='get_network_info' else bool(value.get('success'))

class NatPmpMapping:
    """Maintain a UDP forwarding lease on the default IPv4 gateway."""

    def __init__(self, port):
        self.port = port
        self.gateway = None
        self.external_port = port
        self.endpoint = None
        self.renew_at = 0
        self.expires_at = 0

    def exchange(self, opcode, payload, size):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.connect((self.gateway, 5351))
            for timeout in (0.25, 0.5, 1, 2):
                sock.settimeout(timeout)
                sock.send(bytes((0, opcode)) + payload)
                try:
                    reply = sock.recv(64)
                except socket.timeout:
                    continue
                if len(reply) < 8 or reply[:2] != bytes((0, opcode + 128)):
                    continue
                result, = struct.unpack_from('!H', reply, 2)
                if result:
                    raise OSError(f'NAT-PMP gateway rejected request (result {result})')
                if len(reply) == size:
                    return reply[8:]
        raise OSError('NAT-PMP gateway did not answer')

    def renew(self):
        routes = subprocess.run(['ip', '-j', '-4', 'route', 'show', 'default'],
                                capture_output=True, text=True, timeout=3, check=True)
        gateways = [route for route in json.loads(routes.stdout) if route.get('gateway')]
        if not gateways:
            raise OSError('No default IPv4 gateway for NAT-PMP')
        self.gateway = min(gateways, key=lambda route: route.get('metric', 0))['gateway']
        address = IPv4Address(self.exchange(0, b'', 12))
        if not address.is_global or address.is_multicast:
            raise OSError('NAT-PMP gateway has no public IPv4 address; check for upstream NAT')
        local_port, external_port, lifetime = struct.unpack(
            '!HHI', self.exchange(1, struct.pack('!HHHI', 0, self.port, self.external_port, 600), 16))
        if local_port != self.port or not external_port or not lifetime:
            raise OSError('NAT-PMP gateway did not grant the requested UDP mapping')
        now = time.monotonic()
        self.external_port = external_port
        self.renew_at = now + min(lifetime / 2, 60)
        self.expires_at = now + lifetime
        return f'{address}:{external_port}'

    def refresh(self):
        now = time.monotonic()
        if now < self.renew_at and now < self.expires_at:
            return
        try:
            endpoint = self.renew()
        except (OSError, ValueError, subprocess.SubprocessError) as error:
            if now >= self.expires_at:
                raise RuntimeError(f'Automatic endpoint failed: {error}. Configure a manual endpoint and UDP forwarding.') from error
            self.renew_at = min(now + 10, self.expires_at)
            return
        if self.endpoint and endpoint != self.endpoint:
            raise RuntimeError('The public UDP address changed; restart to register the new endpoint.')
        self.endpoint = endpoint

def validate(config):
    for field in ('engine_binary','user_data','name','rotation'):assert field in config,'Missing '+field
    engine=Path(config['engine_binary']).resolve();profile=Path(config['user_data']).resolve();assert engine.is_file() and os.access(engine,os.X_OK),'Engine is not executable';assert any((profile/'GameData'/n).is_file() for n in ('keeper-enhanced.cfg','keeperfx.cfg')),'Prepare a separate dedicated profile first';assert '.app/' not in str(profile),'User data must be outside the application';assert 1<=config.get('humans',1)<=3,'Dedicated mode supports 1–3 humans plus its AI';assert config['rotation'] and len(config['rotation'])<=32,'Choose 1–32 maps'
    for item in config['rotation']:assert re.fullmatch('[A-Za-z0-9_-]{1,63}',item['campaign']) and type(item['level'])==int and 0<item['level']<=99999,'Invalid map'
    assert config.get('ai_style','defensive') in ('defensive','balanced','rush'),'Invalid AI style'
    assert type(config.get('allow_party_map_votes',False)) is bool,'allow_party_map_votes must be true or false'
    assert type(config.get('twin_hearts_siege',False)) is bool,'twin_hearts_siege must be true or false'
    if config.get('twin_hearts_siege'):
        assert config.get('cooperative') and config.get('humans')==2,'Twin Hearts Siege needs exactly two co-op keepers'
        assert not config.get('manual_start'),'Twin Hearts Siege starts when both keepers are ready'
    assert type(config.get('ready_check',False)) is bool,'ready_check must be true or false'
    assert type(config.get('manual_start',False)) is bool,'manual_start must be true or false'
    assert type(config.get('cooperative', False)) is bool, 'cooperative must be true or false'
    assert config.get('visibility','public') in ('private','public','code'),'Visibility must be private, public, or code'
    assert type(config.get('port',5556)) is int and 1<=config.get('port',5556)<=65535,'Invalid gameplay port'
    password=os.environ.get('KFX_SERVER_PASSWORD','')
    assert len(password.encode())<=31 and not any(ord(c)<32 for c in password),'Password must be at most 31 UTF-8 bytes without control characters'
    if config.get('visibility')=='code':
        assert not password,'Code rooms must not set KFX_SERVER_PASSWORD'
    if config.get('visibility','public')=='private':return engine,profile
    endpoint=config.get('endpoint')
    assert endpoint is None or isinstance(endpoint,str),'Endpoint must be a hostname:port or auto'
    assert config.get('hub_url','').startswith(('http://127.0.0.1:','http://localhost:','https://')),'Use local HTTP or HTTPS';return engine,profile

class Host:
    def __init__(self,config):self.config=config;self.engine,self.profile=validate(config);self.stop=False;self.next=False;self.maintenance_restart=False;self.process=None;self.index=0;self.status={};self.lobby_override=None;self.api_port=None;self.lock=threading.Lock()
    def command(self,line):
        try:
            data=json.loads(line)
            command=data.get('command')
            if command=='stop':self.stop=True
            elif command=='next':self.next=True
            elif command=='setup':
                if self.config.get('twin_hearts_siege'):
                    raise ValueError('Twin Hearts Siege uses its configured map and two seats')
                if not self.config.get('cooperative'):
                    raise ValueError('Setup requires a co-op server')
                map_index, humans, style = data.get('map'), data.get('humans'), data.get('ai_style')
                if type(map_index) is not int or not 0 <= map_index < len(self.config['rotation']):
                    raise ValueError('Choose a map index from the configured rotation')
                if type(humans) is not int or not 1 <= humans <= 3 or style not in ('defensive','balanced','rush'):
                    raise ValueError('Choose 1–3 human seats and defensive, balanced or rush AI')
                with self.lock:
                    self.lobby_override=(map_index,humans,style)
                print('Co-op setup queued for the next lobby.',flush=True)
            elif command=='start':
                if not self.api_port or not rpc(self.api_port,'start_coop_lobby'):
                    raise ValueError('Manual co-op needs at least one human in the lobby before starting')
                print('Checking lobby readiness before starting.',flush=True)
            elif command=='status':print(json.dumps(self.status),flush=True)
            else:print('Unknown command; use setup, status, next or stop',file=sys.stderr)
        except (ValueError,AttributeError,OSError) as error:print('Invalid command:',error,file=sys.stderr)
    def finish_child(self):
        if self.process and self.process.poll() is None:
            self.process.terminate()
            try:self.process.wait(10)
            except subprocess.TimeoutExpired:self.process.kill();self.process.wait(5)
    def run(self):
        state=self.profile/'dedicated';state.mkdir(mode=0o700,exist_ok=True)
        with (state/'host.lock').open('a') as lock:
            os.chmod(state/'host.lock',0o600)
            try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
            except BlockingIOError:raise RuntimeError('This dedicated profile already has a running host')
            self._run_locked()
    def finish_drain(self, base, room):
        try:
            request(base,f"/api/servers/{room['id']}/drained",{},room['secret'])
        except urllib.error.HTTPError as error:
            if error.code==409:return False
            raise
        self.maintenance_restart=True
        self.stop=True
        return True

    def _run_locked(self):
        cfg=self.config;code_room=cfg.get('visibility')=='code';public=cfg.get('visibility','public')!='private';base=cfg.get('hub_url','').rstrip('/');key=os.environ.get('KFX_HUB_ADMIN_KEY','');state=self.profile/'dedicated';state.mkdir(mode=0o700,exist_ok=True)
        credential=read_owner_credential(self.profile,base) if public else ''
        startfile=state/'start.request';startfile.unlink(missing_ok=True);stopfile=state/'stop.request';nextfile=state/'next.request';stopfile.unlink(missing_ok=True);nextfile.unlink(missing_ok=True)
        code_file=state/'room-code.json'
        code_request=state/'room-code.request'
        if code_room:
            code_file.unlink(missing_ok=True)
            code_request.unlink(missing_ok=True)
        def write_code(room):
            if not room.get('code'):
                code_file.unlink(missing_ok=True)
                return
            temporary=code_file.with_suffix('.tmp')
            fd=os.open(temporary,os.O_WRONLY|os.O_CREAT|os.O_TRUNC|os.O_NOFOLLOW,0o600)
            with os.fdopen(fd,'w') as stream:
                os.fchmod(stream.fileno(),0o600)
                json.dump({'code':room['code'],'expires':int(time.time())+room['expires_in']},stream)
            temporary.replace(code_file)
        version=server_version(self.engine) if public else ''
        clients=supported_clients(self.engine) if public else []
        endpoint=cfg.get('endpoint') or ''
        mapping=NatPmpMapping(cfg.get('port',5556)) if public and endpoint=='auto' else None
        voting=public and cfg.get('cooperative') and cfg.get('allow_party_map_votes')
        retained_room=None
        next_ballot=None
        while not self.stop:
            if mapping:
                mapping.refresh()
                endpoint=mapping.endpoint
            map_index=self.index%len(cfg['rotation'])
            humans=cfg.get('humans',1)
            ai_style=cfg.get('ai_style','defensive')
            with self.lock:
                if self.lobby_override is not None:
                    map_index,humans,ai_style=self.lobby_override
                    self.lobby_override=None
            applying=next_ballot
            next_ballot=None
            if applying:
                humans,ai_style=applying['settings']
                requested=applying['winning']
                map_index=next(index for index,item in enumerate(cfg['rotation'])
                    if f"{item['campaign']}:{item['level']}"==requested)
            self.index=map_index+1
            item=cfg['rotation'][map_index]
            self.next=False
            with socket.socket() as sock:sock.bind(('127.0.0.1',0));api_port=sock.getsockname()[1]
            runtime=self.profile/'GameData';gamecfg=runtime/'keeper-enhanced.cfg';legacy=runtime/'keeperfx.cfg';text=(gamecfg if gamecfg.exists() else legacy).read_text();text=re.sub(r'(?mi)^\s*(?:API_ENABLED|API_PORT|RENDERER)\s*=.*\n?','',text);gamecfg.write_text(text+f'\nAPI_ENABLED=TRUE\nAPI_PORT={api_port}\nRENDERER=SOFTWARE\n')
            room={'id':secrets.token_hex(12),'secret':''}
            if retained_room:
                room,retained_room=retained_room,None
            elif public:
                registration = {
                    'name': cfg['name'], 'port': cfg.get('port', 5556),
                    'engine': version, 'kind': 'dedicated', 'client_versions': clients,
                    'map': f"{item['campaign']}:{item['level']}",
                    'rotation': [f"{v['campaign']}:{v['level']}" for v in cfg['rotation']],
                    'password': bool(os.environ.get('KFX_SERVER_PASSWORD')),
                    'code_room':code_room,
                    'twin_hearts_siege':bool(cfg.get('twin_hearts_siege')),
                }
                if cfg.get('cooperative'):
                    registration['coop']={'humans':humans,'ai_style':ai_style,'map_voting':bool(voting)}
                if endpoint:
                    registration['endpoint'] = endpoint
                if key:
                    registration['admin_key'] = key
                try:
                    room = request(base, '/api/servers/register', registration, credential)
                except urllib.error.HTTPError as error:
                    if error.code in (401,403):
                        raise RuntimeError('Public registration rejected. Check the owner credential and optional ranking key.') from error
                    raise
                if code_room:write_code(room)
            trusted = public and room.get('trusted', bool(key))
            spectators = trusted and not code_room and not os.environ.get('KFX_SERVER_PASSWORD') and cfg.get('spectators', False)
            env=dict(os.environ,KFX_USER_DATA_DIR=str(self.profile),KFX_HEADLESS='1',KFX_LOCAL_NETWORK_ONLY='1',SDL_VIDEODRIVER='dummy',SDL_RENDER_DRIVER='software',SDL_AUDIODRIVER='dummy',ALSOFT_DRIVERS='null',KFX_HUB_URL=base,KFX_HUB_SERVER_ID=room['id'],KFX_HUB_SERVER_SECRET=room['secret'])
            if not public:
                env['KFX_PRIVATE_SERVER']='1'
                env.pop('KFX_PUBLIC_DEDICATED',None)
                for name in ('KFX_HUB_URL','KFX_HUB_SERVER_ID','KFX_HUB_SERVER_SECRET','KFX_HUB_ADMIN_KEY'):env.pop(name,None)
            else:
                env.pop('KFX_PRIVATE_SERVER',None)
                env['KFX_PUBLIC_DEDICATED']='1'
                env.pop('KFX_HUB_ADMIN_KEY',None)
            env.pop('KFX_LOBBY_READY_CHECK',None)
            if cfg.get('ready_check'):env['KFX_LOBBY_READY_CHECK']='1'
            env.pop('KFX_DEDICATED_COOP',None)
            env.pop('KFX_DEDICATED_AI_STYLE',None)
            env.pop('KFX_COOP_MANUAL_START',None)
            env.pop('KFX_COOP_SIEGE',None)
            if cfg.get('cooperative'):
                env['KFX_DEDICATED_COOP']='1'
                env['KFX_DEDICATED_AI_STYLE']=ai_style
                if cfg.get('manual_start'):env['KFX_COOP_MANUAL_START']='1'
                if cfg.get('twin_hearts_siege'):env['KFX_COOP_SIEGE']='1'
            if cfg.get('app_resources'):env['KFX_APP_RESOURCES']=cfg['app_resources']
            replay=state/(room['id']+'.pck');port=int(cfg.get('port',5556));args=[str(runtime/'keeperfx'),'-nointro','-server',str(port),'-campaign',item['campaign'],'-level',str(item['level']),'-waitusers',str(humans+1),'-nosound','-nick',cfg.get('ai_name','ServerAI'),'-packetsave',str(replay),'-log','dedicated.log']
            log=open(state/'engine-output.log','ab');self.process=subprocess.Popen(args,executable=str(self.engine),cwd=runtime,env=env,stdout=log,stderr=subprocess.STDOUT);offset=0;had_game=False;failures=0;verified=False;stream_truncated=False
            self.api_port=api_port
            next_ballot_at=0
            departed=set()
            next_admin_at=0
            timing={}
            draining=False
            print('Headless lobby ready:',cfg['name'],'map',item['campaign'],item['level'],flush=True)
            try:
                while not self.stop and not self.next and self.process.poll() is None:
                    if mapping:
                        mapping.refresh()
                    try:
                        if stopfile.exists():self.stop=True;stopfile.unlink();break
                        if nextfile.exists():self.next=True;nextfile.unlink();break
                        if code_room and code_request.exists():
                            value=code_request.read_text().strip()
                            code_request.unlink()
                            if value in ('rotate','revoke'):
                                reply=request(base,f"/api/servers/{room['id']}/room-code",
                                    {'revoke':value=='revoke'},room['secret'])
                                write_code(reply)
                        if startfile.exists():
                            startfile.unlink()
                            self.command('{"command":"start"}')
                        status=rpc(api_port);self.status={**status,'server_id':room['id'],'map':item,'humans':humans,'ai_style':ai_style,'cooperative':bool(cfg.get('cooperative'))};(state/'status.json').write_text(json.dumps(self.status));failures=0
                        if public and time.monotonic()>=next_admin_at:
                            next_admin_at=time.monotonic()+5
                            try:
                                controls=request(base,f"/api/servers/{room['id']}/admin-command",{},room['secret'])
                                remote=controls.get('command')
                                draining=bool(controls.get('draining'))
                                if not status.get('turn',0):
                                    for account in controls.get('kicks',[]):
                                        if any(row.get('account')==account and not row.get('departed') for row in status.get('users',[])):
                                            rpc(api_port,'kick_lobby_peer',account=account)
                            except urllib.error.HTTPError as error:
                                if error.code in (401,403):
                                    raise RuntimeError('Public server access was revoked or replaced') from error
                                remote=None
                            if draining and not had_game and not status.get('turn',0):
                                if self.finish_drain(base,room):
                                    break
                                draining=False
                            if not draining and not had_game and not status.get('turn',0) and remote in ('start','next'):
                                self.command(json.dumps({'command':remote}))
                        if applying and status.get('active'):
                            request(base,f"/api/servers/{room['id']}/map-applied",
                                {'party':applying['party'],'map':applying['winning']},room['secret'])
                            applying=None
                        if not draining and public and not applying and status.get('active') and status.get('players')==1 and not had_game and time.monotonic()>=next_ballot_at:
                            next_ballot_at=time.monotonic()+3
                            vote=request(base,f"/api/servers/{room['id']}/map-ballot",{},room['secret']).get('ballot')
                            if vote:
                                next_ballot={**vote,'settings':(humans,ai_style)}
                                retained_room=room
                                break
                        departed.update(user['account'] for user in status.get('users',[]) if user.get('departed') and user.get('account'))
                        if status.get('turn',0)>0 and status.get('active'):had_game=True
                        measurements=status.get('timing',{})
                        if measurements.get('turns',0)>timing.get('turns',0):
                            timing=measurements
                        if spectators and replay.exists() and not stream_truncated:
                            with replay.open('rb') as f:f.seek(offset);raw=f.read(32768)
                            if raw:answer=request(base,'/api/servers/'+room['id']+'/stream',{'offset':offset,'data':base64.b64encode(raw).decode()},room['secret']);offset=answer['offset'];stream_truncated=bool(answer.get('truncated'))
                        human_players=[v for v in status.get('users',[]) if v.get('peer')!=0]
                        ended=had_game and (not status.get('active') or human_players and all(v.get('outcome') in (1,2) for v in human_players))
                        if ended:
                            identified=[{'user':v['account'],'result':'won' if v['outcome']==1 else 'lost'} for v in human_players if v.get('account') and v.get('outcome') in (1,2)]
                            if trusted and identified and all(v.get('verified_finish') for v in human_players):
                                try:request(base,'/api/servers/'+room['id']+'/result',{'match':room['match_id'],'results':identified,'cooperative':bool(cfg.get('cooperative',False)),'siege_finish':status.get('siege_finish',0)},room['secret']);verified=True
                                except Exception:pass # Unverified/disconnected games never become ranked wins.
                            if draining:self.finish_drain(base,room)
                            break
                    except (OSError,ValueError):
                        failures+=1
                        if failures>30:raise RuntimeError('Headless engine status unavailable for one minute')
                    time.sleep(1)
            finally:
                if self.process.poll() is None:
                    try:
                        final_timing=rpc(api_port).get('timing',{})
                        if final_timing.get('turns',0)>timing.get('turns',0):
                            timing=final_timing
                    except (OSError,ValueError):
                        pass
                self.api_port=None;self.finish_child();log.close()
                if code_room and not retained_room:code_file.unlink(missing_ok=True)
                if spectators and replay.exists() and not stream_truncated:
                    with replay.open('rb') as f:
                        f.seek(offset)
                        while True:
                            raw=f.read(32768);finished=not raw
                            try:answer=request(base,'/api/servers/'+room['id']+'/stream',{'offset':offset,'data':base64.b64encode(raw).decode(),'finished':finished},room['secret']);offset=answer['offset'];stream_truncated=bool(answer.get('truncated'))
                            except Exception:break
                            if finished:break
                if timing:
                    summary={'turns':int(timing['turns']),'logic_ms':timing['logic_ms'],
                             'peak_ms':timing['peak_ms'],'slow_frames':int(timing['slow_frames'])}
                    if 'p95_ms' in timing:summary['p95_ms']=timing['p95_ms']
                    reports=state/'performance'
                    reports.mkdir(mode=0o700,exist_ok=True)
                    (reports/(room['id']+'.json')).write_text(json.dumps({'map':item,**summary}))
                    if public:
                        try:
                            request(base,f"/api/servers/{room['id']}/performance",
                                    {'match':room['match_id'],**summary},room['secret'])
                        except (OSError,ValueError):
                            print('Could not upload match timing. The local report was kept.',file=sys.stderr)
                try:
                    if public and not retained_room:request(base,'/api/servers/'+room['id']+'/close',{'departed':sorted(departed)[:3]},room['secret'])
                except Exception:pass
            if self.process.returncode and not self.stop and not self.next and not retained_room:raise RuntimeError('Engine exited unexpectedly; inspect dedicated/engine-output.log')

def lobby_setup(config):
    print('Co-op lobby setup: Human keepers vs server AI')
    for index, item in enumerate(config['rotation'], 1):
        print(f"  {index}. {item['campaign']}:{item['level']}")
    map_number=int(input('Map number: '))
    humans=int(input('Human seats (1–3): '))
    style=input('AI style (defensive, balanced, rush): ').strip().lower()
    if not 1 <= map_number <= len(config['rotation']):
        raise ValueError('Choose a map from the rotation')
    if not 1 <= humans <= 3 or style not in ('defensive','balanced','rush'):
        raise ValueError('Choose 1–3 human seats and defensive, balanced or rush AI')
    return dict(config, cooperative=True, humans=humans, ai_style=style,
                rotation=[config['rotation'][map_number-1]])

def main():
    p=argparse.ArgumentParser();p.add_argument('config',type=Path);p.add_argument('--check',action='store_true');p.add_argument('--link',action='store_true');p.add_argument('--setup',action='store_true');a=p.parse_args();cfg=json.loads(a.config.read_text())
    if a.setup:
        try:cfg=lobby_setup(cfg)
        except (ValueError,EOFError) as error:p.error(str(error))
    _,profile=validate(cfg)
    if a.link:
        if cfg.get('visibility','public')=='private':p.error('Private servers do not need a cloud account')
        try:save_owner_credential(profile,cfg['hub_url'].rstrip('/'),getpass.getpass('Dedicated server credential: ').strip())
        except (OSError,ValueError) as error:p.error(str(error))
        print('Cloud owner credential stored. Start the supervisor normally.');return
    if a.check:print('Dedicated configuration is valid');return
    host=Host(cfg);signal.signal(signal.SIGTERM,lambda *_:setattr(host,'stop',True));signal.signal(signal.SIGINT,lambda *_:setattr(host,'stop',True))
    def commands():
        for line in sys.stdin:host.command(line)
    threading.Thread(target=commands,daemon=True).start()
    try:host.run()
    finally:host.finish_child()
    if host.maintenance_restart:raise SystemExit(75)
if __name__=='__main__':main()
