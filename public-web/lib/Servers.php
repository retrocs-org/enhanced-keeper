<?php
if (!defined('KFX_API_INTERNAL')) { http_response_code(404); exit; }
trait HubServers {
    private function lobbyManagement(array $data,?string $uid): array {
        demand($uid,'Sign in first',401);
        $sid=api_text($data['server']??null,24);
        $host=$this->one('SELECT id,owner FROM dedicated_hosts WHERE server=?',[$sid]);
        demand($host&&($host['owner']===$uid||$this->one('SELECT 1 FROM dedicated_admins WHERE host=? AND user=?',
            [$host['id'],$uid])),'Server administrator access required',403);
        $server=$this->lockLobby($sid);
        demand($server&&$server['status']==='lobby'&&$server['last_seen']>$this->now-90,'Lobby is closed',409);
        $match=$this->one("SELECT id FROM matches WHERE server=? AND status='lobby' AND ended=0",[$sid]);
        demand($match,'Lobby is closed',409);
        $action=$data['action']??'state';
        if($action!=='state'){
            demand(in_array($action,['lock','unlock','kick'],true),'Invalid moderation action');
            $target='';
            if($action==='kick'){
                $target=api_text($data['user']??null,32);
                demand($target!==$host['owner']&&$target!==$uid,'You cannot kick the owner or yourself',403);
                demand($this->one('SELECT 1 FROM identities WHERE match=? AND user=?',[$match['id'],$target]),
                    'That player is no longer connected',409);
                $this->q('INSERT OR IGNORE INTO lobby_kicks(server,user) VALUES(?,?)',[$sid,$target]);
                $this->q('DELETE FROM tickets WHERE server=? AND user=?',[$sid,$target]);
                $this->q('DELETE FROM lobby_reconnects WHERE server=? AND user=?',[$sid,$target]);
                $this->q('DELETE FROM party_seat_holds WHERE server=? AND user=?',[$sid,$target]);
            }else{
                $this->q('DELETE FROM lobby_locks WHERE server=?',[$sid]);
                if($action==='lock'){
                    $this->q('INSERT INTO lobby_locks(server) VALUES(?)',[$sid]);
                    $this->q('DELETE FROM tickets WHERE server=?',[$sid]);
                }
            }
            $this->q('INSERT INTO lobby_audit VALUES(?,?,?,?,?,?)',
                [bin2hex(random_bytes(12)),$host['id'],$uid,$action,$target,$this->now]);
            $older=$this->rows('SELECT id FROM lobby_audit WHERE host=? ORDER BY created DESC,id DESC LIMIT 50',[$host['id']]);
            $this->q('DELETE FROM lobby_audit WHERE host=? AND id NOT IN ('.implode(',',array_fill(0,count($older),'?')).')',
                [$host['id'],...array_column($older,'id')]);
        }
        return ['locked'=>$this->lobbyLocked($sid),'players'=>$this->rows(
            'SELECT i.peer,i.user,u.name FROM identities i JOIN users u ON u.id=i.user
                WHERE i.match=? AND NOT EXISTS(SELECT 1 FROM lobby_kicks k WHERE k.server=? AND k.user=i.user)
                ORDER BY i.peer',[$match['id'],$sid]),'audit'=>$this->rows(
            "SELECT a.action,a.created,COALESCE(u.name,'Deleted account') AS actor,
                COALESCE(t.name,'') AS target FROM lobby_audit a LEFT JOIN users u ON u.id=a.actor
                LEFT JOIN users t ON t.id=a.target WHERE a.host=? ORDER BY a.created DESC,a.id DESC LIMIT 12",[$host['id']])];
    }

    private function lobbyLocked(string $sid): bool {
        return (bool)$this->one('SELECT 1 FROM lobby_locks WHERE server=?',[$sid]);
    }

    private function lobbyKicked(string $sid,?string $uid): bool {
        return $uid&&$this->one('SELECT 1 FROM lobby_kicks WHERE server=? AND user=?',[$sid,$uid]);
    }

    private function clearLobbyModeration(string $sid): void {
        $this->q('DELETE FROM lobby_locks WHERE server=?',[$sid]);
        $this->q('DELETE FROM lobby_kicks WHERE server=?',[$sid]);
    }


    private function dedicatedHosts(?string $uid): array {
        demand($uid,'Sign in first',401);
        $hosts=$this->rows("SELECT h.id,h.name,h.owner,h.server,u.name AS owner_name,s.last_seen,s.map,s.players,s.capacity,c.versions,d.state AS maintenance,
                CASE WHEN s.last_seen>? AND s.status<>'offline' THEN s.status ELSE 'offline' END AS status
            FROM dedicated_hosts h JOIN users u ON u.id=h.owner
            LEFT JOIN servers s ON s.id=h.server
            LEFT JOIN server_compat c ON c.server=s.id
            LEFT JOIN dedicated_maintenance d ON d.host=h.id
            LEFT JOIN dedicated_admins a ON a.host=h.id AND a.user=?
            WHERE h.owner=? OR a.user=? ORDER BY h.name,h.id",[$this->now-90,$uid,$uid,$uid]);
        foreach($hosts as &$host){
            $host['is_owner']=$host['owner']===$uid;
            $host['admins']=$this->rows("SELECT u.name FROM dedicated_admins a
                JOIN users u ON u.id=a.user WHERE a.host=? ORDER BY u.name",[$host['id']]);
            $host['client_versions']=json_decode($host['versions']??'[]',true)?:[];
            $host['performance']=$this->rows('SELECT m.map,m.engine,m.started,p.turns,p.logic_ms,p.peak_ms,q.p95_ms,p.slow_frames,p.reconnects
                FROM matches m JOIN dedicated_performance p ON p.match=m.id
                JOIN dedicated_host_history h ON h.server=m.server
                LEFT JOIN dedicated_percentiles q ON q.match=m.id
                WHERE h.host=? ORDER BY m.started DESC LIMIT 50',[$host['id']]);
            unset($host['owner'],$host['versions']);
        }
        unset($host);
        return ['hosts'=>$hosts];
    }

    private function dedicatedHostWrite(array $data,?string $uid): array {
        demand($uid,'Sign in first',401);
        $action=$data['action']??'';
        if($action==='create'){
            $name=api_text($data['name']??null,64);
            $this->one('SELECT id FROM users WHERE id=?'.($this->mysql?' FOR UPDATE':''),[$uid]);
            demand(count($this->rows('SELECT id FROM dedicated_hosts WHERE owner=?',[$uid]))<16,
                'Remove an unused server before adding another',409);
            $id=bin2hex(random_bytes(12));$credential=api_token();
            $this->q('INSERT INTO dedicated_hosts(id,name,owner,credential,server) VALUES(?,?,?,?,?)',
                [$id,$name,$uid,hash('sha256',$credential),'']);
            return ['id'=>$id,'credential'=>$credential];
        }
        $id=api_text($data['host']??null,24);
        $host=$this->one('SELECT * FROM dedicated_hosts WHERE id=?'.($this->mysql?' FOR UPDATE':''),[$id]);
        demand($host,'Dedicated server not found',404);
        $owner=$host['owner']===$uid;
        if(in_array($action,['maintenance','cancel-maintenance'],true)){
            demand($owner||$this->one('SELECT 1 FROM dedicated_admins WHERE host=? AND user=?',[$id,$uid]),
                'Server administrator access required',403);
            $server=$this->lockLobby($host['server']);
            demand($server&&$server['status']!=='offline'&&$server['last_seen']>$this->now-90,
                'The server must be online',409);
            $this->q('DELETE FROM dedicated_maintenance WHERE host=?',[$id]);
            if($action==='maintenance'){
                $this->q("INSERT INTO dedicated_maintenance(host,state) VALUES(?,'draining')",[$id]);
                $this->q('DELETE FROM tickets WHERE server=?',[$host['server']]);
                $this->q('DELETE FROM party_seat_holds WHERE server=?',[$host['server']]);
                $this->q('DELETE FROM lobby_reconnects WHERE server=?',[$host['server']]);
                $this->q('DELETE FROM dedicated_commands WHERE server=?',[$host['server']]);
            }
            return ['ok'=>true];
        }
        if(in_array($action,['next','start'],true)){
            demand($owner||$this->one('SELECT 1 FROM dedicated_admins WHERE host=? AND user=?',[$id,$uid]),
                'Server administrator access required',403);
            $live=$this->one("SELECT 1 FROM servers WHERE id=? AND status='lobby' AND last_seen>?",
                [$host['server'],$this->now-90]);
            demand($live,'The server must be in an open lobby',409);
            $this->requireAvailableForJoins($host['server']);
            $this->q('DELETE FROM dedicated_commands WHERE server=?',[$host['server']]);
            $this->q('INSERT INTO dedicated_commands(server,requester,command,expires) VALUES(?,?,?,?)',
                [$host['server'],$uid,$action,$this->now+15]);
            return ['ok'=>true];
        }
        demand($owner,'Only the server owner can manage administrators',403);
        if($action==='revoke'){
            $this->removeDedicatedHost($host);
            return ['ok'=>true];
        }
        demand(in_array($action,['add-admin','remove-admin'],true),'Invalid server action');
        $account=$this->one('SELECT id FROM users WHERE name=?',[api_text($data['name']??null,24)]);
        demand($account,'Cloud account not found',404);
        demand($account['id']!==$uid,'The owner is always an administrator',409);
        if($action==='add-admin'){
            demand(count($this->rows('SELECT user FROM dedicated_admins WHERE host=?',[$id]))<32||
                $this->one('SELECT 1 FROM dedicated_admins WHERE host=? AND user=?',[$id,$account['id']]),
                'This server has 32 administrators',409);
            $this->q('INSERT OR IGNORE INTO dedicated_admins(host,user) VALUES(?,?)',[$id,$account['id']]);
        }else{
            $this->q('DELETE FROM dedicated_admins WHERE host=? AND user=?',[$id,$account['id']]);
            $this->q('DELETE FROM dedicated_commands WHERE server=? AND requester=?',[$host['server'],$account['id']]);
        }
        return ['ok'=>true];
    }

    private function removeDedicatedHost(array $host): void {
        $this->retireDedicatedLobby($host['server']);
        $this->q('DELETE FROM dedicated_maintenance WHERE host=?',[$host['id']]);
        $this->q('DELETE FROM lobby_audit WHERE host=?',[$host['id']]);
        $this->q('DELETE FROM dedicated_host_history WHERE host=?',[$host['id']]);
        $this->q('DELETE FROM dedicated_admins WHERE host=?',[$host['id']]);
        $this->q('DELETE FROM dedicated_hosts WHERE id=?',[$host['id']]);
    }

    private function retireDedicatedLobby(string $server): void {
        if($server==='')return;
        $this->lockLobby($server);
        $this->clearLobbyModeration($server);
        $this->q('DELETE FROM lobby_reconnects WHERE server=?',[$server]);
        $this->q('DELETE FROM dedicated_commands WHERE server=?',[$server]);
        $this->q('DELETE FROM tickets WHERE server=?',[$server]);
        $this->q('DELETE FROM party_seat_holds WHERE server=?',[$server]);
        $this->q("UPDATE matches SET ended=?,status=CASE WHEN status='lobby' THEN 'cancelled' ELSE 'abandoned' END
            WHERE server=? AND ended=0",[$this->now,$server]);
        $this->q("UPDATE servers SET status='offline',secret='' WHERE id=?",[$server]);
    }


    private function hostCredential(string $id,string $token): array {$row=$this->lockLobby($id);demand($row&&hash_equals($row['secret'],hash('sha256',$token)),'Server credential rejected',403);return $row;}
    private function lockLobby(string $server): array|false {
        return $this->one('SELECT * FROM servers WHERE id=?'.($this->mysql?' FOR UPDATE':''),[$server]);
    }

    private function lobbyAdmissions(string $match,bool $lock=false): array {
        return $this->rows('SELECT peer,user FROM identities WHERE match=?'.
            ($lock&&$this->mysql?' FOR UPDATE':''),[$match]);
    }

    private function maintenanceState(string $server): string {
        $row=$this->one('SELECT d.state FROM dedicated_hosts h JOIN dedicated_maintenance d ON d.host=h.id
            WHERE h.server=?',[$server]);
        return $row['state']??'';
    }

    private function requireAvailableForJoins(string $server): void {
        demand($this->maintenanceState($server)==='','Server is draining for maintenance',409);
    }

    private function lobbyReconnect(?string $uid): array {
        demand($uid,'Sign in first',401);
        $hold=$this->one("SELECT r.server,r.expires,s.endpoint FROM lobby_reconnects r JOIN servers s ON s.id=r.server
            WHERE r.user=? AND r.expires>? AND s.last_seen>? AND s.status='lobby' ORDER BY r.expires DESC LIMIT 1",
            [$uid,$this->now,$this->now-90]);
        $server=$hold?$this->partyServer($hold['server']):false;
        if($server){
            $server['endpoint']=$hold['endpoint'];
            $server['client_versions']=json_decode($server['versions']??'[]',true)?:[];
        }
        return ['server'=>$server?:null,
            'seconds_left'=>$hold?min(60,max(0,(int)ceil($hold['expires']-$this->now))):0];
    }

    private function reserveLobbyReconnect(string $server,string $user): void {
        if($this->maintenanceState($server)!==''||$this->lobbyKicked($server,$user))return;
        $this->q('DELETE FROM lobby_reconnects WHERE server=? AND (expires<=? OR user=?)',[$server,$this->now,$user]);
        $this->q('INSERT INTO lobby_reconnects(server,user,expires) VALUES(?,?,?)',[$server,$user,$this->now+60]);
    }

    private function lobbyHeldUsers(string $server,array $joined,bool $lock=false): array {
        $rows=$this->rows('SELECT user FROM party_seat_holds WHERE server=? AND expires>?'.
            ($lock&&$this->mysql?' FOR UPDATE':''),[$server,$this->now]);
        $reconnects=$this->rows('SELECT user FROM lobby_reconnects WHERE server=? AND expires>?' .
            ($lock&&$this->mysql?' FOR UPDATE':''),[$server,$this->now]);
        return array_values(array_diff(array_unique(array_merge(array_column($rows,'user'),
            array_column($reconnects,'user'))),$joined));
    }

    private function requireLobbySeat(array $server,?string $user,array $joined,int $occupied): void {
        $held=$this->lobbyHeldUsers($server['id'],$joined,true);
        demand($occupied<(int)$server['capacity'],'Lobby is full. Choose another server.',409);
        if(!in_array($user,$held,true)){
            demand($occupied+count($held)<(int)$server['capacity'],
                'The remaining lobby seats are reserved for a party.',409);
        }
    }

    private function rotateRoomCode(string $server,bool $revoke=false): array {
        $code=$revoke?'':strtoupper(bin2hex(random_bytes(4)));
        $expires=$revoke?0:$this->now+900;
        $this->q('DELETE FROM server_room_access WHERE server=?',[$server]);
        $this->q('DELETE FROM tickets WHERE server=?',[$server]);
        $this->q('UPDATE server_room_codes SET digest=?,expires=? WHERE server=?',
            [$revoke?null:hash('sha256',$code),$expires,$server]);
        return ['code'=>$code,'expires_in'=>$revoke?0:900];
    }

    private function requireRoomCode(string $server,?string $user): void {
        $room=$this->one('SELECT expires FROM server_room_codes WHERE server=?'.
            ($this->mysql?' FOR UPDATE':''),[$server]);
        if(!$room)return;
        demand($user,'Sign in before joining a code room',401);
        demand($room['expires']>$this->now&&$this->one('SELECT 1 FROM server_room_access WHERE server=? AND user=?'.
            ($this->mysql?' FOR UPDATE':''),[$server,$user]),'Enter a current room code to join',403);
    }

    private function serverWrite(string $route,array $parts,array $data,string $token,?string $uid,string $peer): ?array {
        if($route==='/api/lobby-management')return $this->lobbyManagement($data,$uid);
        if($route==='/api/dedicated-hosts')return $this->dedicatedHostWrite($data,$uid);
        if($route==='/api/servers/redeem'){
            demand($uid,'Sign in before joining a code room',401);
            $code=strtoupper(trim(api_text($data['code']??null,12)));
            demand(preg_match('/^[A-F0-9]{8}$/D',$code),'Enter an eight-character room code');
            $digest=hash('sha256',$code);
            $room=$this->one('SELECT server FROM server_room_codes WHERE digest=? AND expires>?',[$digest,$this->now]);
            demand($room,'Room code expired or invalid',404);
            $server=$this->lockLobby($room['server']);
            $this->requireAvailableForJoins($room['server']);
            demand($server&&$server['status']==='lobby'&&$server['last_seen']>$this->now-90&&
                $this->one('SELECT 1 FROM server_room_codes WHERE server=? AND digest=? AND expires>?'.
                    ($this->mysql?' FOR UPDATE':''),[$room['server'],$digest,$this->now]),'Room code expired or lobby unavailable',404);
            $this->q('INSERT OR IGNORE INTO server_room_access VALUES(?,?)',[$server['id'],$uid]);
            $listing=$this->partyServer($server['id']);
            return ['server'=>array_intersect_key($listing,array_flip(['id','name','engine','map','mods','kind','status','players','capacity']))+
                ['endpoint'=>$server['endpoint'],'client_versions'=>json_decode($listing['versions']??'[]',true)?:[]]];
        }
        if($route==='/api/servers/register'){
            demand(!array_key_exists('password',$data)||is_bool($data['password']),'Password-required flag must be boolean');
            $kind=$data['kind']??'listen';
            demand(in_array($kind,['listen','dedicated'],true),'Invalid host type');
            $host=false;
            if($kind==='dedicated'){
                demand($token!=='','Authenticate the dedicated server with an owner-issued credential',401);
                $host=$this->one('SELECT h.* FROM dedicated_hosts h JOIN users u ON u.id=h.owner
                    WHERE h.credential=?'.($this->mysql?' FOR UPDATE':''),[hash('sha256',$token)]);
                demand($host,'Dedicated server credential rejected',403);
                $this->retireDedicatedLobby($host['server']);
                $this->q('DELETE FROM servers WHERE id=?',[$host['server']]);
                $uid=$host['owner'];
            }
            $key=$data['admin_key']??'';
            $trusted=$kind==='dedicated'&&$key!==''&&$this->admin!==''&&is_string($key)&&hash_equals($this->admin,$key);
            if($kind==='dedicated'&&$key!=='')demand($trusted,'Dedicated administrator key rejected',403);
            if($kind==='dedicated'&&!$trusted)$kind='community';
            $name=$host?$host['name']:api_text($data['name']??null,64);
            $endpoint=$data['endpoint']??'';
            if($endpoint===''){
                $port=$data['port']??null;
                demand(is_int($port)&&$port>0&&$port<=65535,'A valid gameplay port is required');
                $ip=in_array($peer,$this->localPublicPeers,true)?$this->localPublicAddress:$peer;
                demand(filter_var($ip,FILTER_VALIDATE_IP,FILTER_FLAG_NO_PRIV_RANGE|FILTER_FLAG_NO_RES_RANGE)&&
                    (!defined('FILTER_FLAG_GLOBAL_RANGE')||filter_var($ip,FILTER_VALIDATE_IP,FILTER_FLAG_GLOBAL_RANGE)),
                    'The directory sees a non-public address; configure a public endpoint',422);
                $endpoint=(str_contains($ip,':')?'['.$ip.']':$ip).':'.$port;
            }
            $address=api_endpoint($endpoint);$engine=api_text($data['engine']??null,32);$region=api_text($data['region']??'Local',24);$rotation=$data['rotation']??[];demand(is_array($rotation)&&array_is_list($rotation)&&count($rotation)<=32,'Invalid map rotation');foreach($rotation as $map)demand(is_string($map)&&strlen($map)<=80,'Invalid map rotation');
            $clients=$data['client_versions']??[];
            demand(is_array($clients)&&array_is_list($clients)&&count($clients)<=16,'Invalid client versions');
            foreach($clients as $version)demand(is_string($version)&&strlen($version)<=24&&preg_match('/^[0-9]+\.[0-9]+\.[0-9]+$/D',$version),'Invalid client version');
            $this->q('DELETE FROM party_seat_holds WHERE expires<=?',[$this->now]);
            $this->q('DELETE FROM lobby_epochs WHERE server NOT IN (SELECT id FROM servers WHERE last_seen>?)',[$this->now-86400]);
            $sid=bin2hex(random_bytes(12));$secret=api_token();$map=mb_substr((string)($data['map']??''),0,80);$this->q('INSERT INTO servers VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)',[$sid,$name,$address,$engine,$map,mb_substr((string)($data['mods']??''),0,64),1,4,!empty($data['password'])?1:0,$region,$kind,$uid??'',hash('sha256',$secret),$this->now,'lobby',json_encode($rotation)]);$mid=bin2hex(random_bytes(16));$this->q("INSERT INTO matches(id,server,started,map,engine,status) VALUES(?,?,?,?,?,'lobby')",[$mid,$sid,$this->now,$map,$engine]);if($uid&&!$host)$this->q('INSERT INTO participants(match,user) VALUES(?,?)',[$mid,$uid]);if($host){
                $this->q('UPDATE dedicated_hosts SET server=? WHERE id=?',[$sid,$host['id']]);
                $this->q('INSERT INTO dedicated_host_history(host,server) VALUES(?,?)',[$host['id'],$sid]);
                $this->q("DELETE FROM dedicated_maintenance WHERE host=? AND state='restarting'",[$host['id']]);
                $this->q('DELETE FROM lobby_reconnects WHERE expires<=?',[$this->now]);
            }if($clients){
                $this->q('DELETE FROM server_compat WHERE server NOT IN (SELECT id FROM servers WHERE last_seen>?) AND server NOT IN (SELECT server FROM dedicated_hosts)',[$this->now-86400]);
                $this->q('INSERT INTO server_compat VALUES(?,?)',[$sid,json_encode(array_values(array_unique($clients)))]);
            }
            if(isset($data['coop'])){
                $coop=$data['coop'];
                demand($kind!=='listen'&&is_array($coop)&&is_int($coop['humans']??null)&&
                    $coop['humans']>=1&&$coop['humans']<=3&&in_array($coop['ai_style']??null,['defensive','balanced','rush'],true),'Invalid co-op lobby setup');
                $this->q('UPDATE servers SET capacity=? WHERE id=?',[$coop['humans']+1,$sid]);
                $this->q('DELETE FROM server_coop WHERE server NOT IN (SELECT id FROM servers WHERE last_seen>?)',[$this->now-86400]);
                $this->q('INSERT INTO server_coop VALUES(?,?)',[$sid,$coop['ai_style']]);
                if(($coop['map_voting']??false)===true){
                    $this->q('DELETE FROM server_voting WHERE server NOT IN (SELECT id FROM servers WHERE last_seen>?)',[$this->now-86400]);
                    $this->q('INSERT INTO server_voting VALUES(?)',[$sid]);
                }
            }
            if(($data['twin_hearts_siege']??false)===true){
                demand(($data['coop']['humans']??0)===2,'Twin Hearts Siege requires two human seats');
                $this->q('INSERT INTO siege_hosts VALUES(?)',[$sid]);
            }
            $result=['id'=>$sid,'secret'=>$secret,'match_id'=>$mid,'endpoint'=>$address,'trusted'=>$trusted];
            if(($data['code_room']??false)===true){
                demand($kind!=='listen'&&empty($data['password']),'Code rooms require a password-free dedicated server');
                $this->q('INSERT INTO server_room_codes VALUES(?,NULL,0)',[$sid]);
                $result+=$this->rotateRoomCode($sid);
            }
            return $result;
        }
        if(count($parts)<4||$parts[1]!=='servers')return null;$sid=$parts[2];$action=$parts[3];
        if($action==='join'){
            demand(empty($data['relay']),'Relay connections are not supported',400);
            $row=$this->lockLobby($sid);demand($row&&$row['last_seen']>$this->now-90,'Server is offline',404);demand($row['status']==='lobby','Match already started. Choose an open lobby.',409);demand($row['players']<$row['capacity'],'Lobby is full. Choose another server.',409);
            demand(!$this->one("SELECT 1 FROM party_ballots WHERE server=? AND winning<>'' AND expires>?",[$sid,$this->now]),
                'Lobby is changing maps. Try again shortly.',409);demand(empty($data['engine'])||$data['engine']===$row['engine'],'Engine version differs',409);$fingerprint=$data['content']??'';demand(is_string($fingerprint)&&($fingerprint===''||preg_match('/^[0-9a-f]{64}$/D',$fingerprint)),'Invalid content fingerprint');demand($fingerprint===''||!preg_match('/^[0-9a-f]{64}$/D',$row['mods'])||hash_equals($row['mods'],$fingerprint),'Campaign/map or configuration/mod content differs',409);
            $this->requireAvailableForJoins($sid);
            demand(!$this->lobbyLocked($sid),'The administrator locked this lobby',409);
            demand(!$this->lobbyKicked($sid,$uid),'You were removed from this lobby',403);
            $requiresAccount=in_array($row['kind'],['dedicated','community'],true);
            if($requiresAccount)demand($uid,'Sign in again to join this public dedicated server',401);
            $this->requireRoomCode($sid,$uid);
            $match=$this->one('SELECT id FROM matches WHERE server=? AND ended=0 ORDER BY started DESC LIMIT 1',[$sid]);$answer=['endpoint'=>$row['endpoint'],'id'=>$sid,'match_id'=>$match['id']??''];if($requiresAccount){
                demand($match,'No active match',409);
                $joined=array_column($this->lobbyAdmissions($match['id'],true),'user');
                $this->requireLobbySeat($row,$uid,$joined,max((int)$row['players'],count($joined)+1));
                $answer['login_ticket_required']=true;
            }if($uid&&$match){if(!$requiresAccount)$this->q('INSERT OR IGNORE INTO participants(match,user) VALUES(?,?)',[$match['id'],$uid]);$ticket=api_token();$this->q('INSERT INTO tickets(token,server,user,expires) VALUES(?,?,?,?)',[hash('sha256',$ticket),$sid,$uid,$this->now+($requiresAccount?60:600)]);$answer['identity_ticket']=$ticket;}return $answer;
        }
        $row=$this->hostCredential($sid,$token);
        if($action==='drained'){
            demand($this->maintenanceState($sid)==='draining','Maintenance was cancelled',409);
            $this->q("UPDATE dedicated_maintenance SET state='restarting'
                WHERE host IN (SELECT id FROM dedicated_hosts WHERE server=?)",[$sid]);
            return ['ok'=>true];
        }
        if($action==='admin-command'){
            $command=$this->one('SELECT command,expires FROM dedicated_commands WHERE server=?'.
                ($this->mysql?' FOR UPDATE':''),[$sid]);
            if($command)$this->q('DELETE FROM dedicated_commands WHERE server=?',[$sid]);
            return ['command'=>$command&&$command['expires']>$this->now&&$row['status']==='lobby'?
                $command['command']:null,'draining'=>$this->maintenanceState($sid)==='draining',
                'kicks'=>array_column($this->rows('SELECT user FROM lobby_kicks WHERE server=?',[$sid]),'user')];
        }
        if($action==='room-code'){
            $this->lockLobby($sid);
            demand($this->one('SELECT 1 FROM server_room_codes WHERE server=?',[$sid]),'This server is not a code room',404);
            return $this->rotateRoomCode($sid,($data['revoke']??false)===true);
        }
        if($action==='map-ballot'){
            $ballot=$this->one("SELECT party,winning FROM party_ballots WHERE server=? AND winning<>'' AND expires>?
                ORDER BY expires,party LIMIT 1",[$sid,$this->now]);
            return ['ballot'=>$ballot?:null];
        }
        if($action==='map-applied'){
            $map=api_text($data['map']??null,80);
            $party=api_text($data['party']??null,32);
            demand(in_array($map,json_decode($row['rotation'],true)?:[],true),'Map not in rotation',409);
            $ballot=$this->one('SELECT 1 FROM party_ballots WHERE party=? AND server=? AND winning=?',[$party,$sid,$map]);
            demand($ballot,'Map selection is unavailable',409);
            $this->q('DELETE FROM lobby_reconnects WHERE server=?',[$sid]);
            $this->q('DELETE FROM lobby_epochs WHERE server=?',[$sid]);
            $this->q('DELETE FROM party_seat_holds WHERE server=?',[$sid]);
            $this->q("UPDATE servers SET map=?,mods='',status='lobby',players=1,last_seen=? WHERE id=?",[$map,$this->now,$sid]);
            $this->q("UPDATE matches SET map=? WHERE server=? AND status='lobby' AND ended=0",[$map,$sid]);
            $room=$this->one('SELECT * FROM party_rooms WHERE id=?',[$party]);
            if($room&&$ballot){
                $this->resetPartyTarget($room,$this->partyServer($sid));
                $this->q('DELETE FROM party_ballots WHERE party=?',[$party]);
                $this->q('DELETE FROM party_map_votes WHERE party=?',[$party]);
            }
            return ['ok'=>true];
        }
        if($action==='heartbeat'){
            $row=$this->lockLobby($sid);
            demand($row,'Server is offline',404);
            if(($data['status']??'')==='lobby'&&isset($data['occupied'],$data['lobby_epoch'])){
                $mask=$data['occupied'];
                demand(is_int($mask)&&$mask>=0&&$mask<16,'Invalid occupied seats');
                $current=$this->one('SELECT epoch FROM lobby_epochs WHERE server=?'.($this->mysql?' FOR UPDATE':''),[$sid]);
                if($data['lobby_epoch']===($current['epoch']??'')){
                    $lobby=$this->one("SELECT id FROM matches WHERE server=? AND status='lobby' AND ended=0",[$sid]);
                    if($lobby)foreach($this->lobbyAdmissions($lobby['id'],true) as $seat){
                        if(!($mask&(1<<(int)$seat['peer']))){
                            $this->reserveLobbyReconnect($sid,$seat['user']);
                            $this->q('DELETE FROM identities WHERE match=? AND peer=?',[$lobby['id'],$seat['peer']]);
                        }
                    }
                }else return ['ok'=>true];
            }
            $players=$data['players']??1;$status=$data['status']??'lobby';demand(is_int($players)&&$players>=0&&$players<=4,'Invalid player count');demand(in_array($status,['lobby','playing','offline'],true),'Invalid server status');$map=mb_substr((string)($data['map']??$row['map']),0,80);
            $mods=mb_substr((string)($data['mods']??$row['mods']),0,64);
            if($status!=='lobby'||$map!==$row['map']||$mods!==$row['mods']){
                $this->q('DELETE FROM party_seat_holds WHERE server=?',[$sid]);
                $this->q('DELETE FROM lobby_reconnects WHERE server=?',[$sid]);
            }
            $this->q('UPDATE servers SET last_seen=?,players=?,status=?,map=?,mods=? WHERE id=?',[$this->now,$players,$status,$map,$mods,$sid]);if($status==='offline')$this->q("UPDATE matches SET ended=?,status=CASE WHEN status='lobby' THEN 'cancelled' ELSE 'abandoned' END WHERE server=? AND ended=0",[$this->now,$sid]);if($status==='playing')$this->q("UPDATE matches SET status='playing',map=? WHERE server=? AND status='lobby' AND ended=0",[$map,$sid]);return ['ok'=>true];
        }
        if($action==='admit'){
            $this->requireAvailableForJoins($sid);
            $row=$this->lockLobby($sid);
            demand($row,'Server is offline',404);
            demand(!$this->one("SELECT 1 FROM party_ballots WHERE server=? AND winning<>'' AND expires>?",[$sid,$this->now]),
                'Lobby is changing maps',409);
            demand(in_array($row['kind'],['dedicated','community'],true)&&$row['status']==='lobby','Dedicated lobby unavailable',409);
            $peer=$data['peer']??null;
            $occupied=$data['occupied']??null;
            demand(is_int($peer)&&$peer>=1&&$peer<4&&is_int($occupied)&&$occupied>=0&&$occupied<16,'Invalid lobby slots');
            $digest=hash('sha256',api_text($data['ticket']??null,100));
            $ticket=$this->one('SELECT user FROM tickets WHERE token=? AND server=? AND expires>? AND peer=-1',[$digest,$sid,$this->now]);
            demand($ticket,'Identity ticket rejected',403);
            demand(!$this->lobbyLocked($sid),'The administrator locked this lobby',409);
            demand(!$this->lobbyKicked($sid,$ticket['user']),'You were removed from this lobby',403);
            $this->requireRoomCode($sid,$ticket['user']);
            $match=$this->one("SELECT id FROM matches WHERE server=? AND ended=0 AND status='lobby' ORDER BY started DESC LIMIT 1",[$sid]);
            demand($match,'No active lobby',409);
            demand(!($occupied&(1<<$peer)),'Lobby seat is occupied',409);
            $joined=[];
            foreach($this->lobbyAdmissions($match['id'],true) as $seat){
                if($occupied&(1<<(int)$seat['peer']))$joined[]=$seat['user'];
                else{
                    $this->reserveLobbyReconnect($sid,$seat['user']);
                    $this->q('DELETE FROM identities WHERE match=? AND peer=?',[$match['id'],$seat['peer']]);
                }
            }
            demand(!in_array($ticket['user'],$joined,true),'Account already connected',409);
            $seated=1;
            for($slot=1;$slot<4;$slot++)if($occupied&(1<<$slot))$seated++;
            $this->requireLobbySeat($row,$ticket['user'],$joined,$seated);
            demand($this->q('DELETE FROM tickets WHERE token=? AND server=? AND peer=-1',[$digest,$sid])->rowCount()===1,'Identity ticket already used',403);
            $this->q('DELETE FROM identities WHERE match=? AND (peer=? OR user=?)',[$match['id'],$peer,$ticket['user']]);
            $this->q('INSERT INTO identities VALUES(?,?,?)',[$match['id'],$peer,$ticket['user']]);
            if($this->q('DELETE FROM lobby_reconnects WHERE server=? AND user=? AND expires>?',
                [$sid,$ticket['user'],$this->now])->rowCount()){
                $this->q('INSERT OR IGNORE INTO dedicated_performance(match) VALUES(?)',[$match['id']]);
                $this->q('UPDATE dedicated_performance SET reconnects=reconnects+1 WHERE match=?',[$match['id']]);
            }
            $this->q('INSERT OR IGNORE INTO participants(match,user) VALUES(?,?)',[$match['id'],$ticket['user']]);
            $membership=$this->one('SELECT m.party FROM party_members m JOIN party_rooms p ON p.id=m.party
                WHERE m.user=? AND p.server=? AND p.prompt_expires>?',[$ticket['user'],$sid,$this->now]);
            if($membership)$this->q('INSERT OR IGNORE INTO match_party_roster VALUES(?,?,?)',
                [$match['id'],$ticket['user'],$membership['party']]);
            $epoch=bin2hex(random_bytes(12));
            $this->q('DELETE FROM lobby_epochs WHERE server=?',[$sid]);
            $this->q('INSERT INTO lobby_epochs VALUES(?,?)',[$sid,$epoch]);
            $this->q('UPDATE servers SET players=? WHERE id=?',[$seated+1,$sid]);
            return ['user'=>$ticket['user'],'lobby_epoch'=>$epoch];
        }
        if($action==='identity'){
            $ticket=$this->one('SELECT * FROM tickets WHERE token=? AND server=? AND expires>?',[hash('sha256',api_text($data['ticket']??null,100)),$sid,$this->now]);$peer=$data['peer']??null;demand(is_int($peer)&&$peer>=1&&$peer<4,'Invalid peer');demand($ticket&&in_array($ticket['peer'],[-1,$peer],true),'Identity ticket rejected',403);$mid=$this->one('SELECT id FROM matches WHERE server=? AND ended=0 ORDER BY started DESC LIMIT 1',[$sid]);demand($mid,'No active match',409);$old=$this->one('SELECT user FROM identities WHERE match=? AND peer=?',[$mid['id'],$peer]);demand(!$old||$old['user']===$ticket['user'],'Peer identity is already bound',409);$other=$this->one('SELECT peer FROM identities WHERE match=? AND user=?',[$mid['id'],$ticket['user']]);demand(!$other||$other['peer']===$peer,'Account already occupies a peer',409);$this->q('INSERT OR IGNORE INTO identities VALUES(?,?,?)',[$mid['id'],$peer,$ticket['user']]);$this->q('INSERT OR IGNORE INTO participants(match,user) VALUES(?,?)',[$mid['id'],$ticket['user']]);$this->q('UPDATE tickets SET peer=? WHERE token=?',[$peer,$ticket['token']]);return ['user'=>$ticket['user'],'peer'=>$peer];
        }
        if($action==='result'){
            demand($row['kind']==='dedicated','Only a trusted dedicated engine may verify results',403);$mid=api_text($data['match']??null,32);$match=$this->one('SELECT * FROM matches WHERE id=? AND server=?',[$mid,$sid]);demand($match&&!$match['ended'],'Match is missing or closed',409);$results=$data['results']??null;demand(is_array($results)&&array_is_list($results)&&count($results)>0&&count($results)<=3,'Invalid engine results');$ids=array_column($this->rows('SELECT user FROM identities WHERE match=?',[$mid]),'user');$users=[];$winners=[];foreach($results as $result){demand(is_array($result)&&is_string($result['user']??null)&&in_array($result['result']??null,['won','lost','draw'],true),'Unfinished or disconnected matches cannot be verified');$users[]=$result['user'];if($result['result']==='won')$winners[]=$result['user'];}sort($ids);$sorted=$users;sort($sorted);demand(count(array_unique($users))===count($users)&&$ids===$sorted,'Results must cover the authenticated game peers',409);demand(count($winners)<=1||!empty($data['cooperative']),'Multiple winners require a cooperative host result');foreach($results as $result)$this->q('UPDATE participants SET result=? WHERE match=? AND user=?',[$result['result'],$mid,$result['user']]);$this->q("UPDATE matches SET ended=?,status='finished',winner=?,verification='verified' WHERE id=?",[$this->now,$winners[0]??'',$mid]);$finish=$data['siege_finish']??0;
            if(is_int($finish)&&$finish>=3000&&$finish<=2000000&&count($winners)===2&&
                $this->one('SELECT 1 FROM siege_hosts WHERE server=?',[$sid])&&
                !$this->one('SELECT 1 FROM server_room_codes WHERE server=?',[$sid])){
                $this->q('INSERT OR IGNORE INTO siege_scores VALUES(?,?)',[$mid,$finish]);
            }
            return ['ok'=>true,'verification'=>'verified'];
        }
        if($action==='stream')return $this->streamWrite($row,$sid,$data);
        if($action==='performance'){
            $match=api_text($data['match']??null,32);
            demand($this->one('SELECT 1 FROM matches WHERE id=? AND server=?',[$match,$sid]),'Match not found',404);
            $turns=$data['turns']??null;$logic=$data['logic_ms']??null;
            $peak=$data['peak_ms']??null;$slow=$data['slow_frames']??null;
            demand(is_int($turns)&&$turns>=0&&is_int($slow)&&$slow>=0&&
                (is_float($logic)||is_int($logic))&&$logic>=0&&$logic<=1e12&&
                (is_float($peak)||is_int($peak))&&$peak>=0&&$peak<=1e9,'Invalid performance report');
            $this->q('INSERT OR IGNORE INTO dedicated_performance(match) VALUES(?)',[$match]);
            $this->q('UPDATE dedicated_performance SET turns=?,logic_ms=?,peak_ms=?,slow_frames=? WHERE match=?',
                [$turns,$logic,$peak,$slow,$match]);
            if(isset($data['p95_ms'])){
                $p95=$data['p95_ms'];
                demand((is_int($p95)||is_float($p95))&&$p95>=0&&$p95<=1e9,'Invalid percentile');
                $this->q('INSERT OR IGNORE INTO dedicated_percentiles(match,p95_ms) VALUES(?,?)',[$match,$p95]);
                $this->q('UPDATE dedicated_percentiles SET p95_ms=? WHERE match=?',[$p95,$match]);
            }
            return ['ok'=>true];
        }
        if($action==='close'){
            $this->clearLobbyModeration($sid);
            $this->q('DELETE FROM lobby_reconnects WHERE server=?',[$sid]);
            $this->q('DELETE FROM dedicated_commands WHERE server=?',[$sid]);
            $departed=$data['departed']??[];
            demand(is_array($departed)&&array_is_list($departed)&&count($departed)<=3,'Invalid disconnect list');
            $active=$this->one("SELECT id FROM matches WHERE server=? AND status='playing' AND ended=0",[$sid]);
            foreach($departed as $user){
                demand(is_string($user)&&strlen($user)===32,'Invalid disconnected keeper');
                if($active)$this->q("UPDATE participants SET result='disconnected'
                    WHERE match=? AND user=? AND result='' AND EXISTS(SELECT 1 FROM identities WHERE match=? AND user=?)",
                    [$active['id'],$user,$active['id'],$user]);
            }
            $this->q('DELETE FROM party_seat_holds WHERE server=?',[$sid]);
            $this->q('DELETE FROM lobby_epochs WHERE server=?',[$sid]);
            $this->q('DELETE FROM server_room_access WHERE server=?',[$sid]);
            $this->q('DELETE FROM server_room_codes WHERE server=?',[$sid]);
            $this->q('DELETE FROM siege_hosts WHERE server=?',[$sid]);
            $this->q("UPDATE matches SET ended=?,status=CASE WHEN status='lobby' THEN 'cancelled' ELSE 'abandoned' END WHERE server=? AND ended=0",[$this->now,$sid]);if($this->one('SELECT 1 FROM dedicated_host_history WHERE server=?',[$sid]))
                $this->q("UPDATE servers SET status='offline',secret='' WHERE id=?",[$sid]);
            else $this->q('DELETE FROM servers WHERE id=?',[$sid]);
            return ['ok'=>true];
        }
        if($action==='match'){
            $participants=$data['participants']??[];demand(is_array($participants)&&array_is_list($participants)&&count($participants)<=4,'Invalid participants');foreach($participants as $id)demand(is_string($id)&&$this->one('SELECT 1 FROM users WHERE id=?',[$id]),'Unknown participant');demand(count(array_unique($participants))===count($participants),'Duplicate participants');$mid=bin2hex(random_bytes(16));$this->q('INSERT INTO matches(id,server,started,map,engine) VALUES(?,?,?,?,?)',[$mid,$sid,$this->now,$row['map'],$row['engine']]);foreach($participants as $id)$this->q('INSERT INTO participants(match,user) VALUES(?,?)',[$mid,$id]);$this->q("UPDATE servers SET status='playing' WHERE id=?",[$sid]);return ['id'=>$mid];
        }
        return null;
    }
    private function storeReplay(string $id,string $mid,string $owner,string $bytes): void {$this->blob('INSERT OR IGNORE INTO replays VALUES(?,?,?,?,?,?)',[$id,$mid,$owner,$bytes,hash('sha256',$bytes),strlen($bytes)],3);}
    private function streamReplay(array $stream,array $host): void {
        if($this->mysql){$this->q('INSERT OR IGNORE INTO replays(id,match,owner,data,sha,size) SELECT id,match,?,data,SHA2(data,256),length(data) FROM streams WHERE id=?',[$host['owner'],$stream['id']]);}
        else{$bytes=$this->one('SELECT data FROM streams WHERE id=?',[$stream['id']])['data'];$this->storeReplay($stream['id'],$stream['match'],$host['owner'],$bytes);}
    }
    private function streamWrite(array $host,string $sid,array $data): array {
        demand($host['kind']==='dedicated','Only a dedicated host may publish spectator data',403);demand(is_string($data['data']??null),'Invalid stream encoding');$raw=base64_decode($data['data'],true);demand($raw!==false,'Invalid stream encoding');$offset=$data['offset']??0;demand(is_int($offset)&&$offset>=0&&strlen($raw)<=32768,'Invalid stream chunk');
        $stream=$this->one('SELECT id,match,length(data) AS size FROM streams WHERE server=? AND finished=0 ORDER BY rowid DESC LIMIT 1',[$sid]);
        if(!$stream){demand($offset===0,'Start the stream at offset zero');$mid=$this->one('SELECT id FROM matches WHERE server=? ORDER BY started DESC LIMIT 1',[$sid]);$id=bin2hex(random_bytes(16));$this->blob('INSERT INTO streams(id,server,match,data,finished,engine) VALUES(?,?,?,?,0,?)',[$id,$sid,$mid['id']??'','',$host['engine']],3);$stream=['id'=>$id,'match'=>$mid['id']??'','size'=>0];}
        $size=(int)$stream['size'];demand($offset<=$size,'Stream chunk has a gap',409);
        if($offset<$size){$previous=$this->one('SELECT substr(data,?,?) AS chunk FROM streams WHERE id=?',[$offset+1,strlen($raw),$stream['id']]);demand($previous['chunk']===$raw,'Stream retry differs',409);}
        else{
            if($size+strlen($raw)>16*1024*1024){$this->q('UPDATE streams SET finished=1 WHERE id=?',[$stream['id']]);$this->streamReplay($stream,$host);return ['id'=>$stream['id'],'offset'=>$size,'truncated'=>true];}
            if($raw!==''){$this->blob($this->mysql?'UPDATE streams SET data=CONCAT(data,?) WHERE id=?':'UPDATE streams SET data=CAST(data || ? AS BLOB) WHERE id=?',[$raw,$stream['id']],0);$size+=strlen($raw);}
        }
        $this->q('UPDATE streams SET finished=? WHERE id=?',[!empty($data['finished'])?1:0,$stream['id']]);if(!empty($data['finished'])&&$size>0)$this->streamReplay($stream,$host);
        return ['id'=>$stream['id'],'offset'=>$size];
    }
}
