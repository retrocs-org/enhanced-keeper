<?php
if (!defined('KFX_API_INTERNAL')) { http_response_code(404); exit; }
require_once __DIR__.'/settings.php';
function database_config(): array {
    $state=web_setting('state_dir','KFX_HUB_STATE');
    $config=web_settings()['database']??[];
    demand(is_array($config),'Invalid database configuration',503);
    $file=web_setting('database_config_file','KFX_DB_CONFIG');
    if($file!==''){
        demand(is_file($file)&&!is_link($file)&&filesize($file)<=32768,'Private database configuration unavailable',503);
        $real=realpath($file);$web=realpath(dirname(__DIR__));demand($real&&$web&&!str_starts_with($real,$web.DIRECTORY_SEPARATOR),'External database configuration must be outside public-web',503);
        $config=require $file;demand(is_array($config),'Invalid database configuration',503);
        $dsn=$config['dsn']??'';
    } else {
        $dsn=getenv('KFX_DB_DSN')?:getenv('KFX_HUB_DB')?:($config['dsn']??$state.'/community.sqlite3');
        foreach(['user'=>'KFX_DB_USER','password'=>'KFX_DB_PASSWORD'] as $key=>$env) {
            $value=getenv($env);if($value!==false)$config[$key]=$value;
        }
    }
    demand(is_string($dsn)&&$dsn!=='','Invalid database configuration',503);
    foreach(['user','password','ssl_ca'] as $key)demand(is_string($config[$key]??''),'Invalid database configuration',503);
    return [$dsn,['user'=>$config['user']??'','password'=>$config['password']??'','ssl_ca'=>$config['ssl_ca']??'','migrate'=>!str_starts_with($dsn,'mysql:')]];
}
