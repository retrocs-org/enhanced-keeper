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
        if($party['owner']!==$user){
            $this->q('DELETE FROM party_members WHERE party=? AND user=?',[$id,$user]);
            return;
        }
        foreach(['party_messages','party_invites','party_members'] as $table){
            $this->q("DELETE FROM $table WHERE party=?",[$id]);
        }
        $this->q('DELETE FROM party_rooms WHERE id=?',[$id]);
    }

    private function acceptedFriend(string $user,string $friend): bool {
        return (bool)$this->one("SELECT 1 FROM friends WHERE status='accepted' AND ((a=? AND b=?) OR (a=? AND b=?))",
            [$user,$friend,$friend,$user]);
    }

    private function partyState(string $user,string $friend=''): array {
        $party=$this->partyFor($user);
        $friends=$this->rows("SELECT u.id,u.name,COALESCE(g.seen,0) AS seen FROM friends f
            JOIN users u ON u.id=CASE WHEN f.a=? THEN f.b ELSE f.a END
            LEFT JOIN game_presence g ON g.user=u.id
            WHERE (f.a=? OR f.b=?) AND f.status='accepted' ORDER BY u.name LIMIT 128",[$user,$user,$user]);
        foreach($friends as &$row){
            $row['online']=(float)$row['seen']>$this->now-35;
            unset($row['seen']);
        }
        unset($row);
        $invites=$this->rows('SELECT p.id,u.name FROM party_invites i
            JOIN party_rooms p ON p.id=i.party JOIN users u ON u.id=p.owner
            WHERE i.recipient=? AND i.expires>? ORDER BY i.expires DESC LIMIT 16',[$user,$this->now]);
        $members=$messages=[];
        $prompt=null;
        if($party){
            $members=$this->rows('SELECT u.id,u.name FROM party_members m JOIN users u ON u.id=m.user
                WHERE m.party=? ORDER BY u.name',[$party['id']]);
            $messages=array_reverse($this->rows('SELECT u.name,m.body FROM party_messages m JOIN users u ON u.id=m.sender
                WHERE m.party=? ORDER BY m.created DESC,m.id DESC LIMIT 30',[$party['id']]));
            if((float)$party['prompt_expires']>$this->now){
                $server=$this->one("SELECT id,name FROM servers WHERE id=? AND last_seen>? AND status='lobby'",
                    [$party['server'],$this->now-90]);
                if($server)$prompt=$server+['idempotency_key'=>$party['prompt']];
            }
        }
        $direct=[];
        if($friend!==''&&$this->acceptedFriend($user,$friend)){
            $direct=array_reverse($this->rows('SELECT u.name,m.body FROM friend_messages m JOIN users u ON u.id=m.sender
                WHERE (m.sender=? AND m.recipient=?) OR (m.sender=? AND m.recipient=?)
                ORDER BY m.created DESC,m.id DESC LIMIT 30',[$user,$friend,$friend,$user]));
        }
        return ['conversation'=>$friend, 'friend_messages'=>$direct, 'party'=>$party['id']??'', 'owner'=>$party['owner']??'', 'friends'=>$friends,
            'invites'=>$invites, 'members'=>$members, 'messages'=>$messages, 'prompt'=>$prompt];
    }

    private function partyWrite(string $route,array $data,?string $user): ?array {
        if($route!=='/api/party')return null;
        demand($user,'Sign in first',401);
        $action=$data['action']??'state';
        if($action==='state'){
            $sql=$this->mysql?'INSERT INTO game_presence VALUES(?,?) ON DUPLICATE KEY UPDATE seen=VALUES(seen)'
                :'INSERT INTO game_presence VALUES(?,?) ON CONFLICT(user) DO UPDATE SET seen=excluded.seen';
            $this->q($sql,[$user,$this->now]);
            return $this->partyState($user,isset($data['user'])?api_text($data['user'],32):'');
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
            }else{
                demand($party['owner']===$user,'Only the party owner can do that',403);
                if($action==='kick'){
                    $member=api_text($data['user']??null,32);
                    demand($member!==$user,'Use Leave Party to disband');
                    demand($this->q('DELETE FROM party_members WHERE party=? AND user=?',[$id,$member])->rowCount()===1,'Player is not in this party',404);
                }elseif($action==='prompt'){
                    $server=api_text($data['server']??null,32);
                    $row=$this->one("SELECT players,capacity,password FROM servers WHERE id=? AND last_seen>? AND status='lobby'",[$server,$this->now-90]);
                    $count=(int)$this->one('SELECT COUNT(*) AS size FROM party_members WHERE party=?',[$id])['size'];
                    demand($row&&$row['capacity']-$row['players']>=$count,'Choose an open public server with room for the party',409);
                    demand(!$row['password'],'Choose a public server without a lobby password',409);
                    $this->q('UPDATE party_rooms SET server=?,prompt=?,prompt_expires=? WHERE id=?',
                        [$server,bin2hex(random_bytes(12)),$this->now+90,$id]);
                }else throw new ApiProblem(404,'Unknown party action');
            }
        }
        return $this->partyState($user);
    }
}
