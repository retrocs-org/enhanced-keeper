<?php
if (!defined('KFX_API_INTERNAL')) { http_response_code(404); exit; }
function database_config(): array {
    $state=getenv('KFX_HUB_STATE')?:dirname(__DIR__,2).'/.local/keeperhub';
    $config=[];$file=getenv('KFX_DB_CONFIG')?:'';
    if($file!==''){
        demand(is_file($file)&&!is_link($file)&&filesize($file)<=32768,'Private database configuration unavailable',503);
        $real=realpath($file);$web=realpath(dirname(__DIR__));demand($real&&$web&&!str_starts_with($real,$web.DIRECTORY_SEPARATOR),'Database configuration must be outside public-web',503);
        $config=require $file;demand(is_array($config),'Invalid database configuration',503);
    }
    $dsn=$config['dsn']??getenv('KFX_DB_DSN')?:getenv('KFX_HUB_DB')?:$state.'/community.sqlite3';
    demand(is_string($dsn),'Invalid database configuration',503);
    return [$dsn,['user'=>$config['user']??getenv('KFX_DB_USER')?:'','password'=>$config['password']??getenv('KFX_DB_PASSWORD')?:'','ssl_ca'=>$config['ssl_ca']??'','migrate'=>!str_starts_with($dsn,'mysql:')]];
}
