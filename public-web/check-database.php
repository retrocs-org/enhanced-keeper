<?php
/** Read-only schema/connection check. Settings are loaded from this deployed directory’s config.inc.php. */
if(PHP_SAPI!=='cli')exit(1);
ini_set('display_errors','0');umask(0077);define('KFX_API_INTERNAL',true);
require __DIR__.'/lib/Hub.php';require __DIR__.'/lib/config.php';
try{[$dsn,$connection]=database_config();$connection['migrate']=false;if(!str_starts_with($dsn,'mysql:'))demand(is_file($dsn),'Database is missing',503);$hub=new KeeperHub($dsn,'','','',null,$connection);foreach(['users','sessions','servers','matches','participants','identities','streams','replays','rate_limits'] as $table)$hub->q('SELECT 1 FROM '.$table.' LIMIT 1');echo 'Database connection and schema verified ('.($hub->mysql?'MariaDB/MySQL':'SQLite').").\n";}
catch(Throwable $e){fwrite(STDERR,"Database check failed. Check the private configuration, PHP database driver, server reachability and schema grants. No credentials were printed.\n");exit(1);}
