<?php
if (!defined('KFX_API_INTERNAL')) { http_response_code(404); exit; }
trait HubServers {
    private function hostCredential(string $id,string $token): array {$row=$this->one('SELECT * FROM servers WHERE id=?',[$id]);demand($row&&hash_equals($row['secret'],hash('sha256',$token)),'Server credential rejected',403);return $row;}
    private function serverWrite(string $route,array $parts,array $data,string $token,?string $uid,string $peer): ?array {
        if($route==='/api/servers/register'){
            demand(!array_key_exists('password',$data)||is_bool($data['password']),'Password-required flag must be boolean');
            $kind=$data['kind']??'listen';
            demand(in_array($kind,['listen','dedicated'],true),'Invalid host type');
            $key=$data['admin_key']??'';
            $trusted=$kind==='dedicated'&&$key!==''&&$this->admin!==''&&is_string($key)&&hash_equals($this->admin,$key);
            if($kind==='dedicated'&&$key!=='')demand($trusted,'Dedicated administrator key rejected',403);
            if($kind==='dedicated'&&!$trusted)$kind='community';
            $name=api_text($data['name']??null,64);
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
            $sid=bin2hex(random_bytes(12));$secret=api_token();$map=mb_substr((string)($data['map']??''),0,80);$this->q('INSERT INTO servers VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)',[$sid,$name,$address,$engine,$map,mb_substr((string)($data['mods']??''),0,64),1,4,!empty($data['password'])?1:0,$region,$kind,$uid??'',hash('sha256',$secret),$this->now,'lobby',json_encode($rotation)]);$mid=bin2hex(random_bytes(16));$this->q("INSERT INTO matches(id,server,started,map,engine,status) VALUES(?,?,?,?,?,'lobby')",[$mid,$sid,$this->now,$map,$engine]);if($uid)$this->q('INSERT INTO participants(match,user) VALUES(?,?)',[$mid,$uid]);if($clients){
                $this->q('DELETE FROM server_compat WHERE server NOT IN (SELECT id FROM servers WHERE last_seen>?)',[$this->now-86400]);
                $this->q('INSERT INTO server_compat VALUES(?,?)',[$sid,json_encode(array_values(array_unique($clients)))]);
            }
            if(isset($data['coop'])){
                $coop=$data['coop'];
                demand($kind!=='listen'&&is_array($coop)&&is_int($coop['humans']??null)&&
                    $coop['humans']>=1&&$coop['humans']<=3&&in_array($coop['ai_style']??null,['defensive','balanced','rush'],true),'Invalid co-op lobby setup');
                $this->q('UPDATE servers SET capacity=? WHERE id=?',[$coop['humans']+1,$sid]);
                $this->q('DELETE FROM server_coop WHERE server NOT IN (SELECT id FROM servers WHERE last_seen>?)',[$this->now-86400]);
                $this->q('INSERT INTO server_coop VALUES(?,?)',[$sid,$coop['ai_style']]);
            }
            return ['id'=>$sid,'secret'=>$secret,'match_id'=>$mid,'endpoint'=>$address,'trusted'=>$trusted];
        }
        if(count($parts)<4||$parts[1]!=='servers')return null;$sid=$parts[2];$action=$parts[3];
        if($action==='join'){
            demand(empty($data['relay']),'Relay connections are not supported',400);
            $row=$this->one('SELECT * FROM servers WHERE id=? AND last_seen>?',[$sid,$this->now-90]);demand($row,'Server is offline',404);demand($row['status']==='lobby','Match already started. Choose an open lobby.',409);demand($row['players']<$row['capacity'],'Lobby is full. Choose another server.',409);demand(empty($data['engine'])||$data['engine']===$row['engine'],'Engine version differs',409);$fingerprint=$data['content']??'';demand(is_string($fingerprint)&&($fingerprint===''||preg_match('/^[0-9a-f]{64}$/D',$fingerprint)),'Invalid content fingerprint');demand($fingerprint===''||!preg_match('/^[0-9a-f]{64}$/D',$row['mods'])||hash_equals($row['mods'],$fingerprint),'Campaign/map or configuration/mod content differs',409);
            $requiresAccount=in_array($row['kind'],['dedicated','community'],true);
            if($requiresAccount)demand($uid,'Sign in again to join this public dedicated server',401);
            $match=$this->one('SELECT id FROM matches WHERE server=? AND ended=0 ORDER BY started DESC LIMIT 1',[$sid]);$answer=['endpoint'=>$row['endpoint'],'id'=>$sid,'match_id'=>$match['id']??''];if($requiresAccount){demand($match,'No active match',409);$answer['login_ticket_required']=true;}if($uid&&$match){$this->q('INSERT OR IGNORE INTO participants(match,user) VALUES(?,?)',[$match['id'],$uid]);$ticket=api_token();$this->q('INSERT INTO tickets(token,server,user,expires) VALUES(?,?,?,?)',[hash('sha256',$ticket),$sid,$uid,$this->now+($requiresAccount?60:600)]);$answer['identity_ticket']=$ticket;}return $answer;
        }
        $row=$this->hostCredential($sid,$token);
        if($action==='heartbeat'){
            $players=$data['players']??1;$status=$data['status']??'lobby';demand(is_int($players)&&$players>=0&&$players<=4,'Invalid player count');demand(in_array($status,['lobby','playing','offline'],true),'Invalid server status');$map=mb_substr((string)($data['map']??$row['map']),0,80);$this->q('UPDATE servers SET last_seen=?,players=?,status=?,map=?,mods=? WHERE id=?',[$this->now,$players,$status,$map,mb_substr((string)($data['mods']??$row['mods']),0,64),$sid]);if($status==='offline')$this->q("UPDATE matches SET ended=?,status=CASE WHEN status='lobby' THEN 'cancelled' ELSE 'abandoned' END WHERE server=? AND ended=0",[$this->now,$sid]);if($status==='playing')$this->q("UPDATE matches SET status='playing',map=? WHERE server=? AND status='lobby' AND ended=0",[$map,$sid]);return ['ok'=>true];
        }
        if($action==='admit'){
            demand(in_array($row['kind'],['dedicated','community'],true)&&$row['status']==='lobby','Dedicated lobby unavailable',409);
            $peer=$data['peer']??null;
            $occupied=$data['occupied']??null;
            demand(is_int($peer)&&$peer>=1&&$peer<4&&is_int($occupied)&&$occupied>=0&&$occupied<16,'Invalid lobby slots');
            $digest=hash('sha256',api_text($data['ticket']??null,100));
            $ticket=$this->one('SELECT user FROM tickets WHERE token=? AND server=? AND expires>? AND peer=-1',[$digest,$sid,$this->now]);
            demand($ticket,'Identity ticket rejected',403);
            $match=$this->one("SELECT id FROM matches WHERE server=? AND ended=0 AND status='lobby' ORDER BY started DESC LIMIT 1",[$sid]);
            demand($match,'No active lobby',409);
            foreach($this->rows('SELECT peer FROM identities WHERE match=? AND (peer=? OR user=?)',[$match['id'],$peer,$ticket['user']]) as $identity){
                demand(!($occupied&(1<<(int)$identity['peer'])),'Account already connected',409);
            }
            demand($this->q('DELETE FROM tickets WHERE token=? AND server=? AND peer=-1',[$digest,$sid])->rowCount()===1,'Identity ticket already used',403);
            $this->q('DELETE FROM identities WHERE match=? AND (peer=? OR user=?)',[$match['id'],$peer,$ticket['user']]);
            $this->q('INSERT INTO identities VALUES(?,?,?)',[$match['id'],$peer,$ticket['user']]);
            return ['user'=>$ticket['user']];
        }
        if($action==='identity'){
            $ticket=$this->one('SELECT * FROM tickets WHERE token=? AND server=? AND expires>?',[hash('sha256',api_text($data['ticket']??null,100)),$sid,$this->now]);$peer=$data['peer']??null;demand(is_int($peer)&&$peer>=1&&$peer<4,'Invalid peer');demand($ticket&&in_array($ticket['peer'],[-1,$peer],true),'Identity ticket rejected',403);$mid=$this->one('SELECT id FROM matches WHERE server=? AND ended=0 ORDER BY started DESC LIMIT 1',[$sid]);demand($mid,'No active match',409);$old=$this->one('SELECT user FROM identities WHERE match=? AND peer=?',[$mid['id'],$peer]);demand(!$old||$old['user']===$ticket['user'],'Peer identity is already bound',409);$other=$this->one('SELECT peer FROM identities WHERE match=? AND user=?',[$mid['id'],$ticket['user']]);demand(!$other||$other['peer']===$peer,'Account already occupies a peer',409);$this->q('INSERT OR IGNORE INTO identities VALUES(?,?,?)',[$mid['id'],$peer,$ticket['user']]);$this->q('UPDATE tickets SET peer=? WHERE token=?',[$peer,$ticket['token']]);return ['user'=>$ticket['user'],'peer'=>$peer];
        }
        if($action==='result'){
            demand($row['kind']==='dedicated','Only a trusted dedicated engine may verify results',403);$mid=api_text($data['match']??null,32);$match=$this->one('SELECT * FROM matches WHERE id=? AND server=?',[$mid,$sid]);demand($match&&!$match['ended'],'Match is missing or closed',409);$results=$data['results']??null;demand(is_array($results)&&array_is_list($results)&&count($results)>0&&count($results)<=3,'Invalid engine results');$ids=array_column($this->rows('SELECT user FROM identities WHERE match=?',[$mid]),'user');$users=[];$winners=[];foreach($results as $result){demand(is_array($result)&&is_string($result['user']??null)&&in_array($result['result']??null,['won','lost','draw'],true),'Unfinished or disconnected matches cannot be verified');$users[]=$result['user'];if($result['result']==='won')$winners[]=$result['user'];}sort($ids);$sorted=$users;sort($sorted);demand(count(array_unique($users))===count($users)&&$ids===$sorted,'Results must cover the authenticated game peers',409);demand(count($winners)<=1||!empty($data['cooperative']),'Multiple winners require a cooperative host result');foreach($results as $result)$this->q('UPDATE participants SET result=? WHERE match=? AND user=?',[$result['result'],$mid,$result['user']]);$this->q("UPDATE matches SET ended=?,status='finished',winner=?,verification='verified' WHERE id=?",[$this->now,$winners[0]??'',$mid]);return ['ok'=>true,'verification'=>'verified'];
        }
        if($action==='stream')return $this->streamWrite($row,$sid,$data);
        if($action==='close'){$this->q("UPDATE matches SET ended=?,status=CASE WHEN status='lobby' THEN 'cancelled' ELSE 'abandoned' END WHERE server=? AND ended=0",[$this->now,$sid]);$this->q('DELETE FROM servers WHERE id=?',[$sid]);return ['ok'=>true];}
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
