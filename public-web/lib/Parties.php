<?php
if (!defined('KFX_API_INTERNAL')) { http_response_code(404); exit; }

trait HubParties {
    private function partyFor(string $user): array|false {
        return $this->one('SELECT p.* FROM party_rooms p JOIN party_members m ON m.party=p.id WHERE m.user=?'
            .($this->mysql?' FOR UPDATE':''),[$user]);
    }

    private function leaveParty(string $user, array|false $party): void {
        if(!$party)return;
        $id=$party['id'];
        if($party['owner']===$user)$this->q('DELETE FROM party_joins WHERE prompt=?',[$party['prompt']]);
        if($party['owner']!==$user){
            $this->q('DELETE FROM party_seat_holds WHERE party=? AND user=?',[$id,$user]);
            $this->q('DELETE FROM party_members WHERE party=? AND user=?',[$id,$user]);
            $this->q('DELETE FROM party_checks WHERE party=? AND user=?',[$id,$user]);
            $this->q('DELETE FROM party_map_votes WHERE party=? AND user=?',[$id,$user]);
            return;
        }
        foreach(['party_messages','party_invites','party_members','party_checks','party_targets','party_ballots','party_map_votes','party_seat_holds'] as $table){
            $this->q("DELETE FROM $table WHERE party=?",[$id]);
        }
        $this->q('DELETE FROM party_rooms WHERE id=?',[$id]);
    }

    private function acceptedFriend(string $user,string $friend): bool {
        return (bool)$this->one("SELECT 1 FROM friends WHERE status='accepted' AND ((a=? AND b=?) OR (a=? AND b=?))",
            [$user,$friend,$friend,$user]);
    }

    private function markSocialRead(string $user,string $thread,array $messages): void {
        if(!$messages)return;
        $seen=(float)end($messages)['created'];
        $sql=$this->mysql?'INSERT INTO social_reads VALUES(?,?,?) ON DUPLICATE KEY UPDATE seen=GREATEST(seen,VALUES(seen))'
            :'INSERT INTO social_reads VALUES(?,?,?) ON CONFLICT(user,thread) DO UPDATE SET seen=MAX(seen,excluded.seen)';
        $this->q($sql,[$user,$thread,$seen]);
    }

    private function partyServer(string $id): array|false {
        return $this->one('SELECT s.id,s.name,s.engine,s.map,s.mods,s.kind,s.status,s.players,s.capacity,s.password,s.rotation,c.versions,CASE WHEN v.server IS NULL THEN 0 ELSE 1 END AS voting
            FROM servers s LEFT JOIN server_compat c ON c.server=s.id LEFT JOIN server_voting v ON v.server=s.id WHERE s.id=? AND s.last_seen>?',
            [$id,$this->now-90]);
    }

    private function partyServerSignature(array $server): string {
        return hash('sha256',serialize(array_intersect_key($server,array_flip(['engine','map','mods','kind','versions']))));
    }

    private function partyServerBlocker(array|false $server,int $count): string {
        if(!$server||$server['status']!=='lobby')return 'Server lobby unavailable';
        if($this->maintenanceState($server['id'])!=='')return 'Server is draining for maintenance';
        if($this->lobbyLocked($server['id']))return 'The administrator locked this lobby';
        if($server['password'])return 'Server requires a password';
        if($server['capacity']-$server['players']<$count)return 'Not enough room for the party';
        return '';
    }

    private function resetPartyTarget(array $party,array $server): void {
        $id=$party['id'];
        $this->q('DELETE FROM party_seat_holds WHERE party=?',[$id]);
        $this->q('DELETE FROM party_targets WHERE party=?',[$id]);
        $this->q('DELETE FROM party_checks WHERE party=?',[$id]);
        $this->q('INSERT INTO party_targets VALUES(?,?,?,?)',
            [$id,$server['id'],bin2hex(random_bytes(12)),$this->partyServerSignature($server)]);
        $this->q('DELETE FROM party_joins WHERE prompt=?',[$party['prompt']]);
        $this->q("UPDATE party_rooms SET server='',prompt='',prompt_expires=0 WHERE id=?",[$id]);
    }

    private function partyBallot(array $party,string $user): ?array {
        $ballot=$this->one('SELECT * FROM party_ballots WHERE party=? AND expires>?',[$party['id'],$this->now]);
        if(!$ballot)return null;
        $server=$this->partyServer($ballot['server']);
        if(!$server)return null;
        $maps=json_decode($server['rotation'],true)?:[];
        $votes=$this->rows('SELECT v.user,v.map FROM party_map_votes v JOIN party_members m
            ON m.party=v.party AND m.user=v.user WHERE v.party=?',[$party['id']]);
        $counts=array_count_values(array_column($votes,'map'));
        return ['winning'=>$ballot['winning'],'server'=>$server['name'],
            'options'=>array_map(static fn($map)=>['map'=>$map,'votes'=>(int)($counts[$map]??0)],$maps),
            'choice'=>array_column($votes,'map','user')[$user]??''];
    }

    private function partyState(string $user,string $friend='',string $reading=''): array {
        $party=$this->partyFor($user);
        $friends=$this->rows("SELECT u.id,u.name,COALESCE(g.seen,0) AS seen,a.activity FROM friends f
            JOIN users u ON u.id=CASE WHEN f.a=? THEN f.b ELSE f.a END
            LEFT JOIN game_presence g ON g.user=u.id
            LEFT JOIN game_activity a ON a.user=u.id
            WHERE (f.a=? OR f.b=?) AND f.status='accepted' ORDER BY u.name LIMIT 128",[$user,$user,$user]);
        foreach($friends as &$row){
            $row['online']=(float)$row['seen']>$this->now-35;
            $row['status']=$row['online']?($row['activity']??'online'):'offline';
            unset($row['seen'],$row['activity']);
        }
        unset($row);
        $invites=$this->rows('SELECT p.id,u.name FROM party_invites i
            JOIN party_rooms p ON p.id=i.party JOIN users u ON u.id=p.owner
            WHERE i.recipient=? AND i.expires>? ORDER BY i.expires DESC LIMIT 16',[$user,$this->now]);
        $members=$messages=[];
        $prompt=$target=null;
        if($party){
            $members=$this->rows('SELECT u.id,u.name,c.result,c.seen FROM party_members m JOIN users u ON u.id=m.user
                LEFT JOIN party_checks c ON c.party=m.party AND c.user=m.user
                WHERE m.party=? ORDER BY u.name',[$party['id']]);
            $selection=$this->one('SELECT * FROM party_targets WHERE party=?',[$party['id']]);
            if($selection){
                $server=$this->partyServer($selection['server']);
                $blocker=$this->partyServerBlocker($server,count($members));
                if($server&&$this->partyServerSignature($server)!==$selection['signature'])$blocker='Server changed. Select it again.';
                $listing=$server;
                if($listing){
                    $listing['client_versions']=json_decode($listing['versions']??'[]',true)?:[];
                    unset($listing['versions']);
                }
                $target=['id'=>$selection['server'],'revision'=>$selection['revision'],'blocker'=>$blocker,
                    'server'=>$listing?:null];
            }
            foreach($members as &$member){
                $member['readiness']=$target ? ($target['blocker'] ?: ((float)$member['seen']>$this->now-20?$member['result']:'Checking')) : '';
                unset($member['seen'],$member['result']);
            }
            unset($member);
            $messages=array_reverse($this->rows('SELECT u.name,m.body,m.created FROM party_messages m JOIN users u ON u.id=m.sender
                WHERE m.party=? ORDER BY m.created DESC,m.id DESC LIMIT 30',[$party['id']]));
            if((float)$party['prompt_expires']>$this->now&&$target&&$server&&
                $server['status']==='lobby'&&$this->partyServerSignature($server)===$selection['signature']){
                $server=$this->one("SELECT id,name FROM servers WHERE id=? AND last_seen>? AND status='lobby'",
                    [$party['server'],$this->now-90]);
                if($server){
                    $prompt=$server+['idempotency_key'=>$party['prompt'],
                        'seconds_remaining'=>(int)ceil($party['prompt_expires']-$this->now)];
                    $held=$this->rows('SELECT user FROM party_seat_holds WHERE party=? AND server=? AND expires>?',
                        [$party['id'],$server['id'],$this->now]);
                    $prompt['reserved']=(bool)$held;
                    $joined=$held?array_column($this->rows("SELECT i.user FROM identities i JOIN matches m ON m.id=i.match
                        WHERE m.server=? AND m.status='lobby' AND m.ended=0",[$server['id']]),'user'):[];
                    $results=array_column($this->rows('SELECT user,result FROM party_joins WHERE prompt=?',[$party['prompt']]),'result','user');
                    foreach($members as &$member){
                        $reported=$results[$member['id']]??($held?'Reserved':'Invited');
                        $member['join']=in_array($member['id'],$joined,true)?'In Lobby':
                            ($held&&$reported==='In Lobby'?'Reserved':$reported);
                    }
                    unset($member);
                }
            }
        }
        $direct=[];
        if($friend!==''&&$this->acceptedFriend($user,$friend)){
            $direct=array_reverse($this->rows('SELECT u.name,m.body,m.created FROM friend_messages m JOIN users u ON u.id=m.sender
                WHERE (m.sender=? AND m.recipient=?) OR (m.sender=? AND m.recipient=?)
                ORDER BY m.created DESC,m.id DESC LIMIT 30',[$user,$friend,$friend,$user]));
        }
        if($reading==='friend'&&$friend!=='')$this->markSocialRead($user,$friend,$direct);
        if($reading==='party'&&$party)$this->markSocialRead($user,'p-'.$party['id'],$messages);
        $unread=$this->rows("SELECT m.sender,COUNT(*) AS total FROM friend_messages m
            LEFT JOIN social_reads r ON r.user=m.recipient AND r.thread=m.sender
            WHERE m.recipient=? AND m.created>COALESCE(r.seen,0)
            AND EXISTS(SELECT 1 FROM friends f WHERE f.status='accepted' AND
              ((f.a=m.sender AND f.b=m.recipient) OR (f.b=m.sender AND f.a=m.recipient))) GROUP BY m.sender",[$user]);
        $counts=array_column($unread,'total','sender');
        foreach($friends as &$row)$row['unread']=(int)($counts[$row['id']]??0);
        unset($row);
        $partyUnread=$party?(int)$this->one('SELECT COUNT(*) AS total FROM party_messages m
            LEFT JOIN social_reads r ON r.user=? AND r.thread=?
            WHERE m.party=? AND m.sender<>? AND m.created>COALESCE(r.seen,0)',
            [$user,'p-'.$party['id'],$party['id'],$user])['total']:0;
        return ['unread'=>(int)array_sum($counts)+$partyUnread,'party_unread'=>$partyUnread,'conversation'=>$friend, 'friend_messages'=>$direct, 'party'=>$party['id']??'', 'owner'=>$party['owner']??'', 'friends'=>$friends,
            'invites'=>$invites, 'members'=>$members, 'messages'=>$messages, 'prompt'=>$prompt, 'target'=>$target,'ballot'=>$party?$this->partyBallot($party,$user):null];
    }

    private function partyWrite(string $route,array $data,?string $user): ?array {
        if($route!=='/api/party')return null;
        demand($user,'Sign in first',401);
        $action=$data['action']??'state';
        if($action==='state'){
            $activity=$data['activity']??'online';
            demand(in_array($activity,['online','lobby','playing','idle'],true),'Invalid player activity');
            $updateActivity=$this->mysql?'INSERT INTO game_activity VALUES(?,?) ON DUPLICATE KEY UPDATE activity=VALUES(activity)'
                :'INSERT INTO game_activity VALUES(?,?) ON CONFLICT(user) DO UPDATE SET activity=excluded.activity';
            $this->q($updateActivity,[$user,$activity]);
            $sql=$this->mysql?'INSERT INTO game_presence VALUES(?,?) ON DUPLICATE KEY UPDATE seen=VALUES(seen)'
                :'INSERT INTO game_presence VALUES(?,?) ON CONFLICT(user) DO UPDATE SET seen=excluded.seen';
            $this->q($sql,[$user,$this->now]);
            return $this->partyState($user,isset($data['user'])?api_text($data['user'],32):'',is_string($data['reading']??null)?$data['reading']:'');
        }

        if($action==='friend-message'){
            $friend=api_text($data['user']??null,32);
            demand($this->acceptedFriend($user,$friend),'Only accepted friends can message each other',403);
            $body=trim(api_text($data['message']??null,220));
            demand($body!=='','Enter a message');
            $this->q('INSERT INTO friend_messages VALUES(?,?,?,?,?)',[bin2hex(random_bytes(16)),$user,$friend,$body,$this->now]);
            $thread=[$user,$friend,$friend,$user];
            $pair='(sender=? AND recipient=?) OR (sender=? AND recipient=?)';
            $recent=$this->rows("SELECT id FROM friend_messages WHERE $pair ORDER BY created DESC,id DESC LIMIT 80",$thread);
            if(count($recent)===80)$this->q("DELETE FROM friend_messages WHERE ($pair) AND id NOT IN (".implode(',',array_fill(0,80,'?')).')',[...$thread,...array_column($recent,'id')]);
            return $this->partyState($user,$friend);
        }
        $party=$this->partyFor($user);
        if($action==='invite'){
            $friend=api_text($data['user']??null,32);
            demand($this->acceptedFriend($user,$friend),'Only accepted friends can be invited',403);
            if(!$party){
                $id=bin2hex(random_bytes(16));
                $this->q('INSERT INTO party_rooms(id,owner) VALUES(?,?)',[$id,$user]);
                demand($this->q('INSERT OR IGNORE INTO party_members VALUES(?,?)',[$user,$id])->rowCount()===1,'Already in a party',409);
                $party=['id'=>$id,'owner'=>$user];
            }
            demand($party['owner']===$user,'Only the party owner can invite',403);
            demand(!$this->one('SELECT 1 FROM party_members WHERE user=?',[$friend]),'This friend is already in a party',409);
            $this->q('DELETE FROM party_invites WHERE party=? AND (recipient=? OR expires<?)',[$party['id'],$friend,$this->now]);
            demand(!$this->one('SELECT 1 FROM party_invites WHERE party=? GROUP BY party HAVING COUNT(*) >= 16',[$party['id']]),'Too many pending invitations',409);
            $this->q('INSERT INTO party_invites VALUES(?,?,?)',[$party['id'],$friend,$this->now+120]);
        }elseif($action==='accept'||$action==='decline'){
            $id=api_text($data['party']??null,32);
            $room=$this->one('SELECT id,owner FROM party_rooms WHERE id=?'.($this->mysql?' FOR UPDATE':''),[$id]);
            demand($room&&$this->q('DELETE FROM party_invites WHERE party=? AND recipient=? AND expires>?',
                [$id,$user,$this->now])->rowCount()===1,'Party invitation expired',404);
            if($action==='accept'){
                demand(!$party,'Leave your current party first',409);
                demand((int)$this->one('SELECT COUNT(*) AS size FROM party_members WHERE party=?',[$id])['size']<4,'Party is full',409);
                demand($this->q('INSERT OR IGNORE INTO party_members VALUES(?,?)',[$user,$id])->rowCount()===1,'Already in a party',409);
                $this->q('DELETE FROM party_invites WHERE recipient=?',[$user]);
                $this->q('DELETE FROM party_seat_holds WHERE party=?',[$id]);
                $this->q('DELETE FROM party_joins WHERE prompt=(SELECT prompt FROM party_rooms WHERE id=?)',[$id]);
                $this->q("UPDATE party_rooms SET server='',prompt='',prompt_expires=0 WHERE id=?",[$id]);
            }
        }elseif($action==='leave'){
            $this->leaveParty($user,$party);
        }else{
            demand($party,'Join a party first',403);
            $id=$party['id'];
            if($action==='message'){
                $body=trim(api_text($data['message']??null,220));
                demand($body!=='','Enter a message');
                $this->q('INSERT INTO party_messages VALUES(?,?,?,?,?)',[bin2hex(random_bytes(16)),$id,$user,$body,$this->now]);
                $recent=$this->rows('SELECT id FROM party_messages WHERE party=? ORDER BY created DESC,id DESC LIMIT 80',[$id]);
                if(count($recent)===80)$this->q('DELETE FROM party_messages WHERE party=? AND id NOT IN ('.implode(',',array_fill(0,80,'?')).')',[$id,...array_column($recent,'id')]);
            }elseif($action==='join-progress'){
                $prompt=api_text($data['prompt']??null,24);
                demand($prompt===$party['prompt']&&(float)$party['prompt_expires']>$this->now,'Join prompt expired',409);
                $result=trim(api_text($data['message']??null,160));
                $this->q('DELETE FROM party_joins WHERE prompt=? AND user=?',[$prompt,$user]);
                $this->q('INSERT INTO party_joins VALUES(?,?,?)',[$prompt,$user,$result]);
            }elseif($action==='map-vote'){
                $map=api_text($data['map']??null,80);
                $ballot=$this->partyBallot($party,$user);
                demand($ballot&&$ballot['winning']===''&&in_array($map,array_column($ballot['options'],'map'),true),
                    'Choose a map from the open ballot',409);
                $this->q('DELETE FROM party_map_votes WHERE party=? AND user=?',[$id,$user]);
                $this->q('INSERT INTO party_map_votes VALUES(?,?,?)',[$id,$user,$map]);
            }elseif($action==='readiness'){
                $revision=api_text($data['revision']??null,24);
                $target=$this->one('SELECT revision FROM party_targets WHERE party=?',[$id]);
                demand($target&&$revision===$target['revision'],'Party server selection changed',409);
                $result=trim(api_text($data['message']??null,160));
                demand($result!=='','Readiness result required');
                $this->q('DELETE FROM party_checks WHERE party=? AND user=?',[$id,$user]);
                $this->q('INSERT INTO party_checks VALUES(?,?,?,?)',[$id,$user,$result,$this->now]);
            }else{
                demand($party['owner']===$user,'Only the party owner can do that',403);
                if($action==='kick'){
                    $member=api_text($data['user']??null,32);
                    demand($member!==$user,'Use Leave Party to disband');
                    demand($this->q('DELETE FROM party_members WHERE party=? AND user=?',[$id,$member])->rowCount()===1,'Player is not in this party',404);
                    $this->q('DELETE FROM party_checks WHERE party=? AND user=?',[$id,$member]);
                    $this->q('DELETE FROM party_map_votes WHERE party=? AND user=?',[$id,$member]);
                    $this->q('DELETE FROM party_seat_holds WHERE party=? AND user=?',[$id,$member]);
                }elseif($action==='prepare-rematch'){
                    $previous=api_text($data['server']??null,24);
                    $prior=$this->one("SELECT m.map,h.server FROM matches m
                        JOIN match_party_roster r ON r.match=m.id
                        JOIN dedicated_host_history hh ON hh.server=m.server
                        JOIN dedicated_hosts h ON h.id=hh.host
                        WHERE m.server=? AND r.party=? AND r.user=? AND m.ended>0
                          AND m.status IN ('finished','abandoned') ORDER BY m.started DESC LIMIT 1",[$previous,$id,$user]);
                    demand($prior,'No finished match found for this party',404);
                    $this->lockLobby($prior['server']);
                    $listing=$this->partyServer($prior['server']);
                    $members=(int)$this->one('SELECT COUNT(*) AS size FROM party_members WHERE party=?',[$id])['size'];
                    $blocker=$this->partyServerBlocker($listing,$members);
                    demand($blocker==='',$blocker,409);
                    if($listing['map']===$prior['map']){
                        $this->resetPartyTarget($party,$listing);
                    }else{
                        demand((int)$listing['players']===1&&in_array($prior['map'],json_decode($listing['rotation'],true)?:[],true),
                            'The server needs an empty lobby and the previous map in its rotation',409);
                        demand(!$this->lobbyHeldUsers($listing['id'],[],true),'Wait for existing seat holds to expire',409);
                        demand(!$this->one("SELECT 1 FROM party_ballots WHERE server=? AND winning<>'' AND expires>?",
                            [$listing['id'],$this->now]),'The server is already changing maps',409);
                        $this->q('DELETE FROM party_ballots WHERE party=?',[$id]);
                        $this->q('DELETE FROM party_map_votes WHERE party=?',[$id]);
                        $this->q('INSERT INTO party_ballots VALUES(?,?,?,?)',[$id,$listing['id'],$prior['map'],$this->now+45]);
                    }
                }elseif($action==='open-map-vote'){
                    $server=api_text($data['server']??null,32);
                    $listing=$this->partyServer($server);
                    demand($listing&&$listing['voting']&&$listing['status']==='lobby','This lobby does not allow map voting',409);
                    demand(!$this->one("SELECT 1 FROM party_ballots WHERE party=? AND winning<>'' AND expires>?",[$id,$this->now]),
                        'The previous selection is still loading',409);
                    demand(count(json_decode($listing['rotation'],true)?:[])>1,'This lobby has one map',409);
                    $this->q('DELETE FROM party_ballots WHERE party=?',[$id]);
                    $this->q('DELETE FROM party_map_votes WHERE party=?',[$id]);
                    $this->q('INSERT INTO party_ballots VALUES(?,?,?,?)',[$id,$server,'',$this->now+180]);
                }elseif($action==='confirm-map-vote'){
                    $map=api_text($data['map']??null,80);
                    $ballot=$this->partyBallot($party,$user);
                    $selection=$this->one('SELECT server FROM party_ballots WHERE party=?',[$id]);
                    demand($ballot&&$ballot['winning']===''&&$selection,'Map vote expired',409);
                    $counts=array_column($ballot['options'],'votes','map');
                    demand(($counts[$map]??0)>0&&$counts[$map]===max($counts),'Confirm a map tied for the most votes',409);
                    if($this->mysql)$this->one('SELECT id FROM servers WHERE id=? FOR UPDATE',[$selection['server']]);
                    $listing=$this->partyServer($selection['server']);
                    demand($listing&&$listing['voting']&&$listing['status']==='lobby','The lobby is unavailable',409);
                    if($map===$listing['map']){
                        $this->resetPartyTarget($party,$listing);
                        $this->q('DELETE FROM party_ballots WHERE party=?',[$id]);
                    }else{
                        demand((int)$listing['players']===1,'The server must be empty before changing maps',409);
                        demand(!$this->lobbyHeldUsers($listing['id'],[],true),'Wait for lobby seat holds to expire before changing maps',409);
                        demand(!$this->one("SELECT 1 FROM party_ballots WHERE server=? AND winning<>'' AND expires>?".($this->mysql?" FOR UPDATE":""),
                            [$listing['id'],$this->now]),'The server is already changing maps',409);
                        $this->q('UPDATE party_ballots SET winning=?,expires=? WHERE party=?',[$map,$this->now+45,$id]);
                    }

                }elseif($action==='select-server'||$action==='prompt'){
                    $server=api_text($data['server']??null,32);
                    $locked=$action==='prompt'?$this->lockLobby($server):null;
                    $row=$this->partyServer($server);
                    $count=(int)$this->one('SELECT COUNT(*) AS size FROM party_members WHERE party=?',[$id])['size'];
                    $blocker=$this->partyServerBlocker($row,$count);
                    demand($blocker==='',$blocker,409);
                    $signature=$this->partyServerSignature($row);
                    if($action==='select-server'){
                        $this->resetPartyTarget($party,$row);
                    }else{
                        demand(!$this->partyBallot($party,$user),'Finish the map vote first',409);
                        $target=$this->one('SELECT * FROM party_targets WHERE party=?',[$id]);
                        demand($target&&$target['server']===$server&&$target['signature']===$signature,'Select the server to check readiness',409);
                        demand((float)$party['prompt_expires']<=$this->now,'Join prompt already sent',409);
                        $ready=(int)$this->one("SELECT COUNT(*) AS size FROM party_members m JOIN party_checks c
                            ON m.party=c.party AND m.user=c.user WHERE m.party=? AND c.result='Ready' AND c.seen>?",
                            [$id,$this->now-20])['size'];
                        demand($ready===$count,'Wait until everyone is ready',409);
                        demand($locked&&$locked['status']==='lobby'&&$locked['last_seen']>$this->now-90&&
                            $locked['map']===$row['map']&&$locked['mods']===$row['mods'],'Server lobby changed. Select it again.',409);
                        demand(!$this->one("SELECT 1 FROM party_ballots WHERE server=? AND winning<>'' AND expires>?".
                            ($this->mysql?' FOR UPDATE':''),[$server,$this->now]),'Lobby is changing maps',409);
                        $reserved=in_array($locked['kind'],['dedicated','community'],true);
                        $expires=$this->now+($reserved?60:90);
                        $this->q('DELETE FROM party_seat_holds WHERE party=?',[$id]);
                        if($reserved){
                            $match=$this->one("SELECT id FROM matches WHERE server=? AND ended=0 AND status='lobby'",[$server]);
                            demand($match,'Server lobby unavailable',409);
                            $joined=array_column($this->lobbyAdmissions($match['id'],true),'user');
                            $held=$this->lobbyHeldUsers($server,$joined,true);
                            $members=array_column($this->rows('SELECT user FROM party_members WHERE party=?',[$id]),'user');
                            $unused=count(array_diff($members,$joined,$held));
                            demand(max((int)$locked['players'],count($joined)+1)+count($held)+$unused<=(int)$locked['capacity'],
                                'The lobby has too few unreserved seats for your party.',409);
                            foreach($members as $member)$this->q('INSERT INTO party_seat_holds VALUES(?,?,?,?)',
                                [$server,$id,$member,$expires]);
                        }
                        $this->q('DELETE FROM party_joins WHERE prompt=?',[$party['prompt']]);
                        $this->q('UPDATE party_rooms SET server=?,prompt=?,prompt_expires=? WHERE id=?',
                            [$server,bin2hex(random_bytes(12)),$expires,$id]);
                    }
                }else throw new ApiProblem(404,'Unknown party action');
            }
        }
        return $this->partyState($user);
    }
}
