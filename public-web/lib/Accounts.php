<?php
if (!defined('KFX_API_INTERNAL')) { http_response_code(404); exit; }
trait HubAccounts {
    private function newSession(string $uid): array {$token=api_token();$csrf=api_token(24);$this->q('INSERT INTO sessions VALUES(?,?,?,?)',[hash('sha256',$token),$uid,$csrf,$this->now+86400*30]);return [$token,$csrf];}
    private function passwordHash(string $password,string $salt): string {return hash_pbkdf2('sha256',$password,hex2bin($salt),310000,64);}
    private function sendActivation(array $signup): array {
        $ticket=api_token();
        $digest=hash('sha256',$ticket);
        $code=str_pad((string)random_int(0,999999),6,'0',STR_PAD_LEFT);
        $this->q('INSERT INTO cloud_signup VALUES(?,?,?,?,?,?,?,?)',[
            $digest,$signup['name'],$signup['email'],$signup['salt'],$signup['password'],
            $signup['avatar'],hash_hmac('sha256',$code,$ticket),$this->now+900,
        ]);
        try {
            kfx_smtp_send($signup['email'],'Verify your Keeper Enhanced account',"Your verification code is $code. It expires in 15 minutes.");
        } catch (Throwable $error) {
            $this->q('DELETE FROM cloud_signup WHERE ticket=?',[$digest]);
            throw new ApiProblem(503,'The verification email could not be sent. Try again later.');
        }
        return ['ticket'=>$ticket,'expires_in'=>900];
    }
    private function requireCurrentPassword(string $uid,array $data): array {
        $row=$this->one('SELECT name,salt,password FROM users WHERE id=?'.($this->mysql&&$this->db->inTransaction()?' FOR UPDATE':''),[$uid]);
        $password=api_text($data['current_password']??null,128);
        demand($row&&hash_equals($row['password'],$this->passwordHash($password,$row['salt'])),'Current password is incorrect',401);
        return $row;
    }

    private function profileWrite(string $route,array $data,string $token,?string $uid): ?array {
        if(!in_array($route,['/api/profile/avatar','/api/profile/email','/api/profile/email/verify','/api/profile/password','/api/profile/delete'],true))return null;
        demand($uid,'Sign in first',401);
        if($route==='/api/profile/avatar'){
            $avatar=$data['avatar']??null;
            demand(is_int($avatar)&&$avatar>=1&&$avatar<=15,'Invalid avatar');
            $this->q('INSERT OR IGNORE INTO cloud_profiles(user,email,avatar) VALUES(?,NULL,?)',[$uid,$avatar]);
            $this->q('UPDATE cloud_profiles SET avatar=? WHERE user=?',[$avatar,$uid]);
            return ['ok'=>true,'avatar'=>$avatar];
        }
        if($route==='/api/profile/email'){
            $account=$this->requireCurrentPassword($uid,$data);
            demand(function_exists('web_settings')&&kfx_smtp_ready(),'Email verification is unavailable',503);
            $email=strtolower(api_text($data['email']??null,254));
            demand(filter_var($email,FILTER_VALIDATE_EMAIL),'Enter a valid email address');
            demand(!$this->one('SELECT user FROM cloud_profiles WHERE email=?',[$email]),'Choose a different email address',409);
            $ticket=api_token();
            $digest=hash('sha256',$ticket);
            $code=str_pad((string)random_int(0,999999),6,'0',STR_PAD_LEFT);
            $this->q('DELETE FROM cloud_email_changes WHERE expires<?',[$this->now]);
            $this->q('INSERT INTO cloud_email_changes VALUES(?,?,?,?,?,?)',[$digest,$uid,$email,$account['password'],hash_hmac('sha256',$code,$ticket),$this->now+900]);
            try {
                kfx_smtp_send($email,'Confirm your Keeper Enhanced email change',"Your verification code is $code. It expires in 15 minutes.");
            } catch (Throwable $error) {
                $this->q('DELETE FROM cloud_email_changes WHERE ticket=?',[$digest]);
                throw new ApiProblem(503,'The verification email could not be sent. Try again later.');
            }
            return ['ticket'=>$ticket,'expires_in'=>900];
        }
        if($route==='/api/profile/email/verify'){
            $ticket=api_text($data['ticket']??null,64);
            $code=api_text($data['code']??null,6);
            $digest=hash('sha256',$ticket);
            $account=$this->one('SELECT password FROM users WHERE id=?'.($this->mysql?' FOR UPDATE':''),[$uid]);
            $change=$this->one('SELECT * FROM cloud_email_changes WHERE ticket=? AND user=? AND expires>?',[$digest,$uid,$this->now]);
            demand($account&&$change&&hash_equals($change['password'],$account['password'])&&
                preg_match('/^[0-9]{6}$/D',$code)&&hash_equals($change['code'],hash_hmac('sha256',$code,$ticket)),'Code is incorrect or expired');
            demand(!$this->one('SELECT user FROM cloud_profiles WHERE email=?',[$change['email']]),'Choose a different email address',409);
            $this->q('INSERT OR IGNORE INTO cloud_profiles(user,email,avatar) VALUES(?,NULL,1)',[$uid]);
            $this->q('UPDATE cloud_profiles SET email=? WHERE user=?',[$change['email'],$uid]);
            $this->q('DELETE FROM cloud_email_changes WHERE user=?',[$uid]);
            return ['ok'=>true,'email'=>$change['email']];
        }
        $account=$this->requireCurrentPassword($uid,$data);
        if($route==='/api/profile/password'){
            $password=api_text($data['password']??null,128);
            demand(mb_strlen($password)>=12,'Use at least 12 password characters');
            $salt=bin2hex(random_bytes(16));
            $this->q('UPDATE users SET salt=?,password=? WHERE id=?',[$salt,$this->passwordHash($password,$salt),$uid]);
            $this->q('DELETE FROM sessions WHERE user=? AND token<>?',[$uid,hash('sha256',$token)]);
            foreach(['devices','recovery','tickets','cloud_email_changes'] as $table)$this->q("DELETE FROM $table WHERE user=?",[$uid]);
            return ['ok'=>true];
        }
        demand(($data['confirmation']??null)==='DELETE','Type DELETE to confirm');
        $this->leaveParty($uid,$this->partyFor($uid));
        $this->q('DELETE FROM party_invites WHERE recipient=?',[$uid]);
        $this->q('DELETE FROM party_messages WHERE sender=?',[$uid]);
        $this->q('DELETE FROM friend_messages WHERE sender=? OR recipient=?',[$uid,$uid]);
        $this->q('DELETE FROM cloud_save_requirements WHERE id IN (SELECT id FROM cloud_saves WHERE user=?)',[$uid]);
        foreach(['cloud_saves','social_reads','game_activity','game_presence','sessions','devices','recovery','tickets','cloud_profiles','cloud_settings','cloud_email_changes','participants','match_party_roster','server_room_access','identities','entrants','completions'] as $table)
            $this->q("DELETE FROM $table WHERE user=?",[$uid]);
        $this->q('DELETE FROM friends WHERE a=? OR b=?',[$uid,$uid]);
        $this->q('DELETE FROM cloud_signup WHERE name=?',[$account['name']]);
        $this->q('DELETE FROM replays WHERE owner=?',[$uid]);
        $this->q("UPDATE matches SET winner='' WHERE winner=?",[$uid]);
        foreach(['a','b','winner'] as $column)$this->q("UPDATE bracket SET $column='' WHERE $column=?",[$uid]);
        foreach(['servers','events'] as $table)$this->q("UPDATE $table SET owner='' WHERE owner=?",[$uid]);
        $this->q('DELETE FROM users WHERE id=?',[$uid]);
        return ['ok'=>true];
    }

    private function accountWrite(string $route,array $data,string $token,?string $uid): ?array {
        if($route==='/api/cloud/saves'){
            demand($uid,'Sign in first',401);
            $id=bin2hex(random_bytes(16));
            $title=api_text($data['title']??null,100);
            $device=api_text($data['device']??null,50);
            $encoded=$data['data']??null;
            demand(is_string($encoded)&&strlen($encoded)<=2796204,'Save backup exceeds 2 MiB',413);
            $raw=base64_decode($encoded,true);
            demand($raw!==false&&strlen($raw)>32&&strlen($raw)<=2097152,'Invalid save data');
            $requirements=$data['requirements']??'';
            demand(is_string($requirements)&&strlen($requirements)<=131072,'Invalid save requirements');
            if($requirements!==''){
                $manifest=base64_decode($requirements,true);
                demand($manifest!==false&&str_starts_with($manifest,"KFXREQ2\n"),'Invalid save requirements');
            }
            if($this->mysql)$this->one('SELECT id FROM users WHERE id=? FOR UPDATE',[$uid]);
            $existing=$this->rows('SELECT id FROM cloud_saves WHERE user=? ORDER BY created DESC,id DESC',[$uid]);
            $this->q('INSERT INTO cloud_saves VALUES(?,?,?,?,?,?)',[$id,$uid,$title,$device,$this->now,$encoded]);
            if($requirements!=='')$this->q('INSERT INTO cloud_save_requirements VALUES(?,?)',[$id,$requirements]);
            foreach(array_slice($existing,7) as $backup){
                $this->q('DELETE FROM cloud_save_requirements WHERE id=?',[$backup['id']]);
                $this->q('DELETE FROM cloud_saves WHERE user=? AND id=?',[$uid,$backup['id']]);
            }
            return ['id'=>$id];
        }
        if($route==='/api/cloud/settings'){
            demand($uid,'Sign in first',401);
            $revision=$data['revision']??null;
            $settings=$data['settings']??null;
            demand(is_int($revision)&&$revision>=0&&$revision<2147483647,'Invalid settings revision');
            demand(is_string($settings)&&strlen($settings)<=16384&&str_starts_with($settings,'# This file gets written by the game automatically.')&&!str_contains($settings,"\0"),'Invalid game settings');
            $this->q('INSERT OR IGNORE INTO cloud_settings VALUES(?,0,?)',[$uid,'']);
            demand($this->q('UPDATE cloud_settings SET settings=?,revision=revision+1 WHERE user=? AND revision=?',[$settings,$uid,$revision])->rowCount()===1,'Cloud settings changed on another device',409);
            return ['revision'=>$revision+1];
        }
        $settings=$this->profileWrite($route,$data,$token,$uid);
        if($settings!==null)return $settings;
        if(in_array($route,['/api/cloud/signup','/api/cloud/resend'],true)){
            demand(function_exists('web_settings')&&kfx_smtp_ready(),'Email verification is unavailable',503);
            $name=api_text($data['name']??null,24);
            $email=strtolower($data['email']);
            $this->q('DELETE FROM cloud_signup WHERE expires<?',[$this->now-86400]);
            if($route==='/api/cloud/resend'){
                $signup=$this->one('SELECT * FROM cloud_signup WHERE name=? AND email=? ORDER BY expires DESC LIMIT 1',[$name,$email]);
                $password=api_text($data['password']??null,128);
                $hash=$this->passwordHash($password,$signup['salt']??str_repeat('00',16));
                if(!$signup||!hash_equals($signup['password'],$hash)||$this->one('SELECT 1 FROM users WHERE name=?',[$name]))
                    return ['ticket'=>api_token(),'expires_in'=>900];
                return $this->sendActivation($signup);
            }
            $password=api_text($data['password']??null,128);
            demand(preg_match('/^[A-Za-z0-9_-]{3,24}$/D',$name)&&mb_strlen($password)>=12,'Use a 3–24 character name and a password of at least 12 characters');
            $avatar=$data['avatar']??1;
            demand(is_int($avatar)&&$avatar>=1&&$avatar<=15,'Invalid avatar');
            demand(!$this->one('SELECT 1 FROM users WHERE name=?',[$name]),'Name is already registered',409);
            $salt=bin2hex(random_bytes(16));
            return $this->sendActivation([
                'name'=>$name,'email'=>$email,'salt'=>$salt,
                'password'=>$this->passwordHash($password,$salt),'avatar'=>$avatar,
            ]);
        }
        if($route==='/api/cloud/verify'){
            $ticket=api_text($data['ticket']??null,64);
            $code=api_text($data['code']??null,6);
            $digest=hash('sha256',$ticket);
            $row=$this->one('SELECT * FROM cloud_signup WHERE ticket=? AND expires>?',[$digest,$this->now]);
            demand($row&&preg_match('/^[0-9]{6}$/D',$code)&&hash_equals($row['code'],hash_hmac('sha256',$code,$ticket)),'Code is incorrect or expired',400);
            demand($this->q('DELETE FROM cloud_signup WHERE ticket=?',[$digest])->rowCount()===1,'Code already used',409);
            demand(!$this->one('SELECT 1 FROM users WHERE name=?',[$row['name']])&&!$this->one('SELECT 1 FROM cloud_profiles WHERE email=?',[$row['email']]),'Account is already registered',409);
            $uid=bin2hex(random_bytes(16));
            $this->q('INSERT INTO users(id,name,salt,password,created) VALUES(?,?,?,?,?)',[$uid,$row['name'],$row['salt'],$row['password'],$this->now]);
            $this->q('INSERT INTO cloud_profiles VALUES(?,?,?)',[$uid,$row['email'],$row['avatar']]);
            [$session,$csrf]=$this->newSession($uid);
            return ['token'=>$session,'csrf'=>$csrf,'name'=>$row['name'],'id'=>$uid,'avatar'=>(int)$row['avatar']];
        }
        if(in_array($route,['/api/register','/api/login','/api/cloud/login'],true)){
            $cloudLogin=$route==='/api/cloud/login';
            $name=api_text($data['name']??null,$cloudLogin?254:24);
            $byEmail=$cloudLogin&&str_contains($name,'@');
            $password=api_text($data['password']??null,128);
            if($byEmail)demand(filter_var($name,FILTER_VALIDATE_EMAIL),'Enter a valid email address');
            else demand(preg_match('/^[A-Za-z0-9_-]{3,24}$/D',$name),'Name must be 3–24 letters, numbers, _ or -');
            demand(mb_strlen($password)>=12,'Use at least 12 password characters');
            if($route==='/api/register'){demand(!function_exists('web_settings')||!kfx_smtp_ready(),'Create your account with email verification',410);demand(!$this->one('SELECT 1 FROM users WHERE name=?',[$name]),'Name is already registered',409);$uid=bin2hex(random_bytes(16));$salt=bin2hex(random_bytes(16));$this->q('INSERT INTO users(id,name,salt,password,created) VALUES(?,?,?,?,?)',[$uid,$name,$salt,$this->passwordHash($password,$salt),$this->now]);}
            else{
                $row=$byEmail
                    ?$this->one('SELECT u.* FROM users u JOIN cloud_profiles p ON p.user=u.id WHERE p.email=?',[strtolower($name)])
                    :$this->one('SELECT * FROM users WHERE name=?',[$name]);
                if(!$row&&$cloudLogin&&function_exists('web_settings')&&kfx_smtp_ready()){
                    $field=$byEmail?'email':'name';
                    $pending=$this->one("SELECT * FROM cloud_signup WHERE $field=? AND expires>? ORDER BY expires DESC LIMIT 1",
                        [$byEmail?strtolower($name):$name,$this->now-86400]);
                    if($pending&&hash_equals($pending['password'],$this->passwordHash($password,$pending['salt']))){
                        return ['pending_cloud_activation'=>$pending];
                    }
                }
                $hash=$this->passwordHash($password,$row['salt']??str_repeat('00',16));
                demand($row&&hash_equals($row['password'],$hash),$cloudLogin?'Username, email address or password is incorrect':'Name or password is incorrect',401);
                $uid=$row['id'];
                $name=$row['name'];
            }
            [$value,$csrf]=$this->newSession($uid);
            if($route==='/api/cloud/login'){
                $profile=$this->one('SELECT avatar FROM cloud_profiles WHERE user=?',[$uid]);
                return ['token'=>$value,'name'=>$name,'id'=>$uid,'avatar'=>(int)($profile['avatar']??1)];
            }
            return ['token'=>$value,'csrf'=>$csrf,'name'=>$name];
        }
        if($route==='/api/recovery/codes'){
            demand($uid,'Sign in first',401);$password=api_text($data['password']??null,128);$row=$this->one('SELECT salt,password FROM users WHERE id=?',[$uid]);demand(hash_equals($row['password'],$this->passwordHash($password,$row['salt'])),'Current password is incorrect',401);$this->q('DELETE FROM recovery WHERE user=?',[$uid]);$codes=[];for($i=0;$i<8;$i++){$code=bin2hex(random_bytes(12));$codes[]=$code;$this->q('INSERT INTO recovery VALUES(?,?)',[$uid,hash('sha256',$code)]);}return ['codes'=>$codes,'message'=>'Store these codes offline. Each code can be used once. Previous codes are invalid.'];
        }
        if($route==='/api/recovery/reset'){
            $name=api_text($data['name']??null,24);$code=api_text($data['code']??null,64);$password=api_text($data['password']??null,128);demand(mb_strlen($password)>=12,'Use at least 12 password characters');$row=$this->one('SELECT u.id FROM users u JOIN recovery r ON r.user=u.id WHERE u.name=? AND r.code=?',[$name,hash('sha256',$code)]);demand($row,'Account or recovery code is incorrect',401);$salt=bin2hex(random_bytes(16));$this->q('UPDATE users SET salt=?,password=? WHERE id=?',[$salt,$this->passwordHash($password,$salt),$row['id']]);foreach(['recovery','sessions','devices'] as $table)$this->q("DELETE FROM $table WHERE user=?",[$row['id']]);return ['ok'=>true,'message'=>'Password changed. All sessions and recovery codes were revoked. Sign in and generate new codes.'];
        }
        if($route==='/api/logout'){demand($uid,'Sign in first',401);$this->q('DELETE FROM sessions WHERE token=?',[hash('sha256',$token)]);return ['ok'=>true];}
        if($route==='/api/device/start'){$code=api_token();$userCode=strtoupper(bin2hex(random_bytes(4)));$this->q('INSERT INTO devices(code,user_code,expires) VALUES(?,?,?)',[hash('sha256',$code),$userCode,$this->now+600]);return ['device_code'=>$code,'user_code'=>$userCode,'verification_path'=>'/index.php?route=%2Fdevice','expires_in'=>600,'interval'=>3];}
        if($route==='/api/device/poll'){$key=hash('sha256',api_text($data['device_code']??null,100));$row=$this->one('SELECT * FROM devices WHERE code=?',[$key]);demand($row&&$row['expires']>$this->now,'Pairing expired; start again',410);demand($this->now-$row['last_poll']>=2,'Poll more slowly',429);$this->q('UPDATE devices SET last_poll=? WHERE code=?',[$this->now,$key]);if(!$row['user'])return ['pending'=>true];[$value]=$this->newSession($row['user']);$name=$this->one('SELECT name FROM users WHERE id=?',[$row['user']])['name'];$this->q('DELETE FROM devices WHERE code=?',[$key]);return ['token'=>$value,'name'=>$name];}
        if($route==='/api/device/approve'){demand($uid,'Sign in first',401);$code=strtoupper(api_text($data['user_code']??null,8));$row=$this->one('SELECT * FROM devices WHERE user_code=? AND expires>?',[$code,$this->now]);demand($row&&!$row['user'],'Code is invalid or already approved',404);$this->q('UPDATE devices SET user=? WHERE user_code=?',[$uid,$code]);return ['ok'=>true];}
        if($route==='/api/profile'){demand($uid,'Sign in first',401);$this->q('UPDATE users SET public=? WHERE id=?',[!empty($data['public'])?1:0,$uid]);return ['ok'=>true];}
        if($route==='/api/friends'){
            demand($uid,'Sign in first',401);$other=api_text($data['user']??null,32);demand($other!==$uid&&$this->one('SELECT 1 FROM users WHERE id=?',[$other]),'Choose an existing other player');$row=$this->one('SELECT * FROM friends WHERE (a=? AND b=?) OR (a=? AND b=?)',[$uid,$other,$other,$uid]);if(($data['action']??'')==='remove')$this->q('DELETE FROM friends WHERE (a=? AND b=?) OR (a=? AND b=?)',[$uid,$other,$other,$uid]);elseif($row&&$row['b']===$uid)$this->q("UPDATE friends SET status='accepted' WHERE a=? AND b=?",[$other,$uid]);elseif(!$row)$this->q("INSERT INTO friends VALUES(?,?,'pending')",[$uid,$other]);return ['ok'=>true];
        }
        return null;
    }
}
