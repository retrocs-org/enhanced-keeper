<?php
if (!defined('KFX_API_INTERNAL')) { http_response_code(404); exit; }
trait HubAccounts {
    private function newSession(string $uid): array {$token=api_token();$csrf=api_token(24);$this->q('INSERT INTO sessions VALUES(?,?,?,?)',[hash('sha256',$token),$uid,$csrf,$this->now+86400*30]);return [$token,$csrf];}
    private function passwordHash(string $password,string $salt): string {return hash_pbkdf2('sha256',$password,hex2bin($salt),310000,64);}
    private function accountWrite(string $route,array $data,string $token,?string $uid): ?array {
        if($route==='/api/cloud/signup'){
            demand(function_exists('web_settings')&&kfx_smtp_ready(),'Email verification is unavailable',503);
            $name=api_text($data['name']??null,24);
            $password=api_text($data['password']??null,128);
            demand(preg_match('/^[A-Za-z0-9_-]{3,24}$/D',$name)&&mb_strlen($password)>=12,'Use a 3–24 character name and a password of at least 12 characters');
            $email=strtolower($data['email']);
            $avatar=$data['avatar']??1;
            demand(is_int($avatar)&&$avatar>=1&&$avatar<=15,'Invalid avatar');
            demand(!$this->one('SELECT 1 FROM users WHERE name=?',[$name]),'Name is already registered',409);
            $ticket=api_token();
            $code=str_pad((string)random_int(0,999999),6,'0',STR_PAD_LEFT);
            $salt=bin2hex(random_bytes(16));
            $this->q('DELETE FROM cloud_signup WHERE expires<?',[$this->now]);
            $this->q('INSERT INTO cloud_signup VALUES(?,?,?,?,?,?,?,?)',[hash('sha256',$ticket),$name,$email,$salt,$this->passwordHash($password,$salt),$avatar,hash_hmac('sha256',$code,$ticket),$this->now+900]);
            try {
                kfx_smtp_send($email,'Verify your Keeper Enhanced account',"Your verification code is $code. It expires in 15 minutes.");
            } catch (Throwable $error) {
                $this->q('DELETE FROM cloud_signup WHERE ticket=?',[hash('sha256',$ticket)]);
                throw new ApiProblem(503,'The verification email could not be sent. Try again later.');
            }
            return ['ticket'=>$ticket,'expires_in'=>900];
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
            $name=api_text($data['name']??null,24);$password=api_text($data['password']??null,128);demand(preg_match('/^[A-Za-z0-9_-]{3,24}$/D',$name),'Name must be 3–24 letters, numbers, _ or -');demand(mb_strlen($password)>=12,'Use at least 12 password characters');
            if($route==='/api/register'){demand(!function_exists('web_settings')||!kfx_smtp_ready(),'Create your account with email verification',410);demand(!$this->one('SELECT 1 FROM users WHERE name=?',[$name]),'Name is already registered',409);$uid=bin2hex(random_bytes(16));$salt=bin2hex(random_bytes(16));$this->q('INSERT INTO users(id,name,salt,password,created) VALUES(?,?,?,?,?)',[$uid,$name,$salt,$this->passwordHash($password,$salt),$this->now]);}
            else{$row=$this->one('SELECT * FROM users WHERE name=?',[$name]);$hash=$this->passwordHash($password,$row['salt']??str_repeat('00',16));demand($row&&hash_equals($row['password'],$hash),'Name or password is incorrect',401);$uid=$row['id'];$name=$row['name'];}
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
