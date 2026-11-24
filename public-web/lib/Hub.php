<?php
declare(strict_types=1);
if (!defined('KFX_API_INTERNAL')) { http_response_code(404); exit; }
require_once __DIR__.'/Accounts.php';
require_once __DIR__.'/Servers.php';
require_once __DIR__.'/Community.php';
final class ApiProblem extends RuntimeException { public function __construct(public int $status,string $message){parent::__construct($message);} }
function demand(mixed $ok,string $message,int $status=400): void {if(!$ok)throw new ApiProblem($status,$message);}
function api_text(mixed $value,int $limit=80): string {demand(is_string($value)&&mb_strlen($value)>0&&mb_strlen($value)<=$limit&&!preg_match('/[\x00-\x1f\x7f]/u',$value),'Invalid text');return $value;}
function api_token(int $bytes=32): string {return rtrim(strtr(base64_encode(random_bytes($bytes)),'+/','-_'),'=');}
function api_endpoint(mixed $value): string {$value=api_text($value,150);demand(preg_match('/^(\[[0-9a-fA-F:]+\]|[A-Za-z0-9][A-Za-z0-9.-]*):([0-9]{1,5})$/D',$value,$m)&&((int)$m[2])>0&&((int)$m[2])<65536,'Use hostname:port or [IPv6]:port');return $value;}
final class KeeperHub {
    use HubAccounts,HubServers,HubCommunity;
    public PDO $db; public float $now; public bool $mysql=false;
    public function __construct(string $file,private string $admin='',private string $relayUrl='',private string $relayKey='',?float $now=null,array $connection=[]){
        $this->now=$now??microtime(true);
        if(str_starts_with($file,'mysql:')){
            $this->mysql=true;$options=[PDO::ATTR_ERRMODE=>PDO::ERRMODE_EXCEPTION,PDO::ATTR_DEFAULT_FETCH_MODE=>PDO::FETCH_ASSOC,PDO::ATTR_EMULATE_PREPARES=>false,PDO::ATTR_STRINGIFY_FETCHES=>false,PDO::ATTR_TIMEOUT=>5];
            if(!empty($connection['ssl_ca'])){$options[PDO::MYSQL_ATTR_SSL_CA]=$connection['ssl_ca'];$options[PDO::MYSQL_ATTR_SSL_VERIFY_SERVER_CERT]=true;}
            $this->db=new PDO($file,$connection['user']??'',$connection['password']??'',$options);
            if(!empty($connection['migrate']))foreach(explode(';',file_get_contents(__DIR__.'/mariadb.sql')) as $sql)if(trim($sql)!=='')$this->db->exec($sql);
            return;
        }
        $folder=dirname($file);demand(!is_link($file)&&!is_link($folder),'Invalid private database path',503);
        if(!is_dir($folder))demand(mkdir($folder,0700,true),'Private database folder is unavailable',503);
        $web=realpath(dirname(__DIR__));$parent=realpath($folder);demand($parent&&$web&&$parent!==$web&&!str_starts_with($parent,$web.DIRECTORY_SEPARATOR),'Database must be outside public-web',503);
        $this->db=new PDO('sqlite:'.$file,null,null,[PDO::ATTR_ERRMODE=>PDO::ERRMODE_EXCEPTION,PDO::ATTR_DEFAULT_FETCH_MODE=>PDO::FETCH_ASSOC]);
        $this->db->exec('PRAGMA busy_timeout=5000');if($connection['migrate']??true){$this->db->exec(require __DIR__.'/schema.php');chmod($file,0600);}
    }
    private function sql(string $sql): string {
        if(!$this->mysql)return $sql;
        $sql=str_replace('INSERT OR IGNORE','INSERT IGNORE',$sql);
        $sql=str_replace('ORDER BY rowid','ORDER BY sequence_id',$sql);
        return preg_replace('/\b(match|round|position|public)\b/i','`$1`',$sql);
    }
    public function q(string $sql,array $params=[]): PDOStatement {$s=$this->db->prepare($this->sql($sql));$s->execute($params);return $s;}
    public function one(string $sql,array $params=[]): array|false{return $this->q($sql,$params)->fetch();}
    public function rows(string $sql,array $params=[]): array{return $this->q($sql,$params)->fetchAll();}
    public function blob(string $sql,array $params,int $binary): void {$s=$this->db->prepare($this->sql($sql));foreach($params as $i=>$value)$s->bindValue($i+1,$value,$i===$binary?PDO::PARAM_LOB:(is_int($value)?PDO::PARAM_INT:PDO::PARAM_STR));$s->execute();}
    public function limit(string $peer,string $bucket,int $max=20,int $seconds=60): void {
        $this->db->exec($this->mysql?'START TRANSACTION':'BEGIN IMMEDIATE');try{
            $this->q('DELETE FROM rate_limits WHERE start<?',[$this->now-86400]);
            $r=$this->one('SELECT * FROM rate_limits WHERE peer=? AND bucket=?',[$peer,$bucket]);
            if(!$r)demand((int)$this->q('SELECT COUNT(*) FROM rate_limits')->fetchColumn()<8192,'Service is busy; retry later',503);
            $same=$r&&$this->now-$r['start']<$seconds;$count=$same?$r['count']+1:1;$start=$same?$r['start']:$this->now;
            if($this->mysql){$this->q('INSERT INTO rate_limits VALUES(?,?,?,1) ON DUPLICATE KEY UPDATE count=IF(start>?,count+1,1),start=IF(start>?,start,VALUES(start))',[$peer,$bucket,$this->now,$this->now-$seconds,$this->now-$seconds]);$count=(int)$this->one('SELECT count FROM rate_limits WHERE peer=? AND bucket=?',[$peer,$bucket])['count'];}else $this->q('INSERT INTO rate_limits VALUES(?,?,?,?) ON CONFLICT(peer,bucket) DO UPDATE SET start=excluded.start,count=excluded.count',[$peer,$bucket,$start,$count]);$this->db->exec('COMMIT');
        }catch(Throwable $e){$this->db->exec('ROLLBACK');throw $e;}
        demand($count<=$max,'Too many requests; try again later',429);
    }
    public function relayAvailable(): bool {return false;}
    public function handle(string $method,string $uri,array $data=[],string $token='',string $peer='local',string $csrf='',bool $cookie=false): array {
        $route=parse_url($uri,PHP_URL_PATH);$parts=explode('/',trim((string)$route,'/'));parse_str((string)(parse_url($uri,PHP_URL_QUERY)??''),$query);
        demand(in_array($method,['GET','POST'],true),'Unsupported method',405);
        if($method==='POST')$this->limit($peer,$route==='/api/connectivity'?'connectivity':'requests',$route==='/api/connectivity'?5:360);
        if(in_array($route,['/api/login','/api/register'],true)&&$method==='POST')$this->limit($peer,'login',10);
        if($route==='/api/recovery/reset'&&$method==='POST')$this->limit($peer,'recovery',5,3600);
        $session=$token!==''?$this->one('SELECT s.*,u.name FROM sessions s JOIN users u ON u.id=s.user WHERE token=? AND expires>?',[hash('sha256',$token),$this->now]):false;$uid=$session['user']??null;
        if($cookie&&$method==='POST')demand($session&&hash_equals($session['csrf'],$csrf),'Reload the page before submitting',403);
        if($route==='/api/recovery/codes'&&$method==='POST'){demand($uid,'Sign in first',401);$this->limit($uid,'recovery-issue',3,3600);}
        if($route==='/api/device/start'&&$method==='POST')$this->limit($peer,'device',10);
        if($route==='/api/connectivity'&&$method==='POST')return $this->probe($data,$peer)+['relay_available'=>$this->relayAvailable(),'relay_path_verified'=>false];
        $this->db->beginTransaction();try{
            foreach(['sessions','devices','tickets'] as $table)$this->q("DELETE FROM $table WHERE expires<?",[$this->now]);
            if($method==='GET')$answer=$this->read($route,$parts,$query,$session,$uid);
            else $answer=$this->accountWrite($route,$data,$token,$uid)??$this->serverWrite($route,$parts,$data,$token,$uid)??$this->communityWrite($route,$parts,$data,$token,$uid);
            demand(is_array($answer),'Not found',404);$this->db->commit();return $answer;
        }catch(Throwable $e){if($this->db->inTransaction())$this->db->rollBack();throw $e;}
    }
    private function read(string $route,array $parts,array $query,array|false $session,?string $uid): array {
        if($route==='/api/health'){ return ['ok'=>true,'protocol'=>1,'relay'=>false,'authority'=>'client-simulated','http_backend'=>'php'];}
        if($route==='/api/session')return ['signed_in'=>(bool)$session,'name'=>$session['name']??'','csrf'=>$session['csrf']??''];
        if($route==='/api/me'){demand($uid,'Sign in first',401);return $this->one('SELECT id,name,public FROM users WHERE id=?',[$uid]);}
        if($route==='/api/servers'){$this->q("UPDATE matches SET ended=?,status=CASE WHEN status='lobby' THEN 'cancelled' ELSE 'abandoned' END WHERE ended=0 AND server IN (SELECT id FROM servers WHERE last_seen<?)",[$this->now,$this->now-90]);return ['servers'=>$this->rows('SELECT id,name,endpoint,engine,map,mods,players,capacity,password,region,kind,status FROM servers WHERE last_seen>? ORDER BY name LIMIT 128',[$this->now-90])];}
        if($route==='/api/friends'){demand($uid,'Sign in first',401);return ['friends'=>$this->rows('SELECT u.id,u.name,f.status,f.a AS requester FROM friends f JOIN users u ON u.id=CASE WHEN f.a=? THEN f.b ELSE f.a END WHERE f.a=? OR f.b=?',[$uid,$uid,$uid])];}
        if($route==='/api/matches'){demand($uid,'Sign in first',401);return ['matches'=>$this->rows("SELECT m.* FROM matches m JOIN participants p ON p.match=m.id WHERE p.user=? AND m.status NOT IN ('lobby','cancelled') ORDER BY started DESC LIMIT 50",[$uid])];}
        if($route==='/api/leaderboard'){
            $rows=$this->rows("SELECT u.id,u.name,COUNT(*) AS matches,SUM(CASE WHEN p.result='won' THEN 1 ELSE 0 END) AS wins FROM users u JOIN participants p ON p.user=u.id JOIN matches m ON m.id=p.match WHERE u.public=1 AND m.verification='verified' AND m.ended>0 GROUP BY u.id,u.name ORDER BY wins DESC,matches DESC,u.name ASC LIMIT 100");
            foreach($rows as &$row){$row['matches']=(int)$row['matches'];$row['wins']=(int)$row['wins'];}unset($row);
            return ['players'=>$rows];
        }
        if($route==='/api/events')return ['events'=>$this->rows('SELECT e.*,COUNT(n.user) AS entrants FROM events e LEFT JOIN entrants n ON n.event=e.id GROUP BY e.id ORDER BY starts LIMIT 100')];
        if($route==='/api/challenges')return ['challenges'=>$this->rows('SELECT * FROM challenges ORDER BY title LIMIT 100')];
        if(count($parts)===4&&$parts[1]==='events'&&$parts[3]==='bracket')return $this->bracketRead($parts[2]);
        if(count($parts)===3&&$parts[1]==='profiles'){
            $row=$this->one('SELECT id,name,public FROM users WHERE id=?',[$parts[2]]);demand($row,'Profile not found',404);demand($row['public']||$uid===$row['id'],'Profile is private',403);
            $stats=$this->one("SELECT COUNT(*) AS matches,SUM(CASE WHEN p.result='won' THEN 1 ELSE 0 END) AS wins FROM matches m JOIN participants p ON p.match=m.id WHERE p.user=? AND verification='verified' AND ended>0",[$row['id']]);$stats['matches']=(int)$stats['matches'];if($stats['wins']!==null)$stats['wins']=(int)$stats['wins'];$achievements=[];if($stats['matches'])$achievements[]='First verified match';if($stats['wins'])$achievements[]='First victory';if($stats['matches']>=10)$achievements[]='Veteran Keeper';if($this->one('SELECT 1 FROM completions WHERE user=?',[$row['id']]))$achievements[]='Challenge Keeper';return $row+['verified_stats'=>$stats,'achievements'=>$achievements];
        }
        if(count($parts)===4&&$parts[1]==='servers'&&$parts[3]==='spectate'){$row=$this->one('SELECT id AS stream_id,engine,length(data) AS size,finished FROM streams WHERE server=? ORDER BY rowid DESC LIMIT 1',[$parts[2]]);demand($row&&$row['size']>0,'No spectator stream yet; wait for the match to start',404);return $row;}
        if(count($parts)===3&&$parts[1]==='streams'){$row=$this->one('SELECT * FROM streams WHERE id=?',[$parts[2]]);demand($row,'Stream not found',404);$offset=filter_var($query['offset']??'0',FILTER_VALIDATE_INT);demand($offset!==false&&$offset>=0&&$offset<=strlen($row['data']),'Invalid stream offset');return ['offset'=>$offset,'data'=>base64_encode(substr($row['data'],$offset,32768)),'size'=>strlen($row['data']),'finished'=>(bool)$row['finished'],'engine'=>$row['engine']];}
        if($route==='/api/replays')return ['replays'=>$this->rows('SELECT r.id,r.match,r.sha,r.size FROM replays r JOIN matches m ON m.id=r.match ORDER BY m.started DESC LIMIT 100')];
        if(count($parts)===3&&$parts[1]==='replays'){$row=$this->one('SELECT * FROM replays WHERE id=?',[$parts[2]]);demand($row,'Replay not found',404);return ['sha256'=>$row['sha'],'data'=>base64_encode($row['data']),'engine'=>$this->one('SELECT engine FROM matches WHERE id=?',[$row['match']])['engine']];}
        throw new ApiProblem(404,'Not found');
    }
    private function probe(array $data,string $peer): array {
        $value=api_endpoint($data['endpoint']??'');$global=filter_var($peer,FILTER_VALIDATE_IP,FILTER_FLAG_NO_PRIV_RANGE|FILTER_FLAG_NO_RES_RANGE);
        if(!$global||str_starts_with(strtolower($peer),'::ffff:')||str_starts_with(strtolower($peer),'ff')||preg_match('/^(?:22[4-9]|23[0-9])\./',$peer)||preg_match('/^100\.(?:6[4-9]|[7-9][0-9]|1[01][0-9]|12[0-7])\./',$peer))return ['reachable'=>null,'reason'=>'A loopback/LAN service cannot verify Internet reachability. Use a public service or a second machine.'];
        $colon=strrpos($value,':');$host=trim(substr($value,0,$colon),'[]');$port=(int)substr($value,$colon+1);$addresses=filter_var($host,FILTER_VALIDATE_IP)?[$host]:array_merge(array_column(dns_get_record($host,DNS_A)?:[],'ip'),array_column(dns_get_record($host,DNS_AAAA)?:[],'ipv6'));
        demand($addresses,'The host name could not be resolved');foreach($addresses as $address)demand(inet_pton($address)===inet_pton($peer),'The endpoint must resolve only to this connection’s public address.');
        $socket=@stream_socket_client('udp://'.(str_contains($peer,':')?'['.$peer.']':$peer).':'.$port,$errno,$error,.8);$reachable=false;if($socket){stream_set_timeout($socket,0,800000);$nonce=bin2hex(random_bytes(16));fwrite($socket,'KFXQ1 '.$nonce);$reachable=fread($socket,128)==='KFXA1 '.$nonce;fclose($socket);}return ['reachable'=>$reachable,'reason'=>$reachable?'Incoming UDP challenge answered by the game.':'No valid UDP reply. Open a lobby, then retry. Firewall, NAT or CGNAT may block direct hosting; join a public server instead.'];
    }
}
