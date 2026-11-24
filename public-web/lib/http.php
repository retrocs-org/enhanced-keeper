<?php
if (!defined('KFX_API_INTERNAL')) { http_response_code(404); exit; }
require_once __DIR__.'/Hub.php';
require_once __DIR__.'/config.php';
function serve_php_api(string $origin,string $uri,string $method): never {
    try{
        demand((int)($_SERVER['CONTENT_LENGTH']??0)<=3*1024*1024,'Request too large',413);
        $raw=file_get_contents('php://input',false,null,0,3*1024*1024+1);demand(is_string($raw)&&strlen($raw)<=3*1024*1024,'Request too large',413);
        $object=json_decode($raw?:'{}',false,64,JSON_THROW_ON_ERROR);demand(is_object($object),'Expected an object');$data=json_decode($raw?:'{}',true,64,JSON_THROW_ON_ERROR);
        foreach(['name','code','user_code','device_code','user','ticket','map','mods','engine','region','title','description','kind','status','match','result','data','endpoint','content','admin_key','action'] as $key)if(array_key_exists($key,$data))demand(is_string($data[$key]),'Invalid '.$key);
        $authorization=$_SERVER['HTTP_AUTHORIZATION']??'';$cookie=$_COOKIE['kfx_session']??'';demand(is_string($cookie),'Invalid session');$bearer='';if($authorization!==''){demand(preg_match('/^Bearer ([A-Za-z0-9_-]{1,150})$/D',$authorization,$m),'Invalid authorization',401);$bearer=$m[1];}
        $state=getenv('KFX_HUB_STATE')?:dirname(__DIR__,2).'/.local/keeperhub';[$db,$connection]=database_config();
        $key=getenv('KFX_HUB_ADMIN_KEY')?:'';$keyFile=getenv('KFX_HUB_ADMIN_KEY_FILE')?:$state.'/admin.key';if($key===''&&is_file($keyFile)&&!is_link($keyFile))$key=trim((string)file_get_contents($keyFile));
        $relayKey=getenv('KFX_RELAY_CONTROL_KEY')?:'';$relayFile=getenv('KFX_RELAY_CONTROL_KEY_FILE')?:$state.'/relay-control.key';if($relayKey===''&&is_file($relayFile)&&!is_link($relayFile))$relayKey=trim((string)file_get_contents($relayFile));
        $hub=new KeeperHub($db,$key,getenv('KFX_RELAY_CONTROL')?:'',$relayKey,null,$connection);
        $value=$hub->handle($method,$uri,$data,$bearer?:$cookie,$_SERVER['REMOTE_ADDR']??'local',$_SERVER['HTTP_X_CSRF_TOKEN']??'',$cookie!==''&&$bearer==='');
        $route=parse_url($uri,PHP_URL_PATH);
        if(in_array($route,['/api/login','/api/register'],true)&&isset($value['token'])){setcookie('kfx_session',$value['token'],['expires'=>time()+2592000,'path'=>'/','secure'=>str_starts_with($origin,'https://'),'httponly'=>true,'samesite'=>'Strict']);unset($value['token']);}
        if($route==='/api/logout')setcookie('kfx_session','',['expires'=>1,'path'=>'/','secure'=>str_starts_with($origin,'https://'),'httponly'=>true,'samesite'=>'Strict']);
        header('Content-Type: application/json; charset=utf-8');echo json_encode($value,JSON_THROW_ON_ERROR|JSON_UNESCAPED_SLASHES);exit;
    }catch(ApiProblem $e){problem($e->status,$e->getMessage());}catch(JsonException|TypeError|ValueError $e){problem(400,'Malformed request');}catch(Throwable $e){problem(503,'Service is unavailable. Please try again.');}
}
