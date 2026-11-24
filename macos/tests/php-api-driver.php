<?php
// Test-only CLI adapter; never part of the web document root.
if(PHP_SAPI!=='cli')exit(1);
ini_set('display_errors','0');umask(0077);define('KFX_API_INTERNAL',true);
require dirname(__DIR__,2).'/public-web/lib/Hub.php';
$request=json_decode(stream_get_contents(STDIN),true,64,JSON_THROW_ON_ERROR);
try{$hub=new KeeperHub($request['db'],$request['admin'],$request['relay']??'',$request['relay_key']??'',(float)$request['now'],$request['connection']??[]);$result=$hub->handle($request['method'],$request['path'],$request['data']??[],$request['token']??'',$request['peer']??'local',$request['csrf']??'',$request['cookie']??false);$reply=['status'=>200,'data'=>$result];}
catch(ApiProblem $e){$reply=['status'=>$e->status,'error'=>$e->getMessage()];}
catch(Throwable $e){$reply=['status'=>500,'error'=>'PHP contract error: '.get_class($e).': '.$e->getMessage()];}
echo json_encode($reply,JSON_THROW_ON_ERROR);
