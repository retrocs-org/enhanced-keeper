<?php
/** Read-only schema/connection check. Settings are loaded from this deployed directory’s config.inc.php. */
if(PHP_SAPI!=='cli')exit(1);
ini_set('display_errors','0');umask(0077);define('KFX_API_INTERNAL',true);
require __DIR__.'/lib/Hub.php';require __DIR__.'/lib/config.php';
try {
    [$dsn, $connection] = database_config();
    $mysql = str_starts_with($dsn, 'mysql:');
    $driver = $mysql ? 'mysql' : 'sqlite';
    if (!class_exists('PDO') || !in_array($driver, PDO::getAvailableDrivers(), true)) {
        fwrite(STDERR, "Database check failed: PHP PDO driver '$driver' is not installed/enabled for this CLI. Check Apache's PHP configuration separately.\n");
        exit(1);
    }
    $connection['migrate'] = false;
    if (!$mysql) demand(is_file($dsn), 'Database is missing', 503);
    $hub = new KeeperHub($dsn, '', '', '', null, $connection);
    foreach (['users','recovery','sessions','devices','servers','friends','matches','participants','events','bracket','entrants','completions','challenges','tickets','identities','streams','replays','rate_limits'] as $table) {
        $hub->q('SELECT 1 FROM '.$table.' LIMIT 1');
    }
    echo 'Database connection and tables verified ('.($hub->mysql ? 'MariaDB/MySQL' : 'SQLite')."). No data was changed.\n";
} catch (Throwable $e) {
    // Fixed messages only: raw exception text can contain connection details or SQL.
    $native = $e instanceof PDOException ? (int)($e->errorInfo[1] ?? 0) : 0;
    $hint = match ($native) {
        1045 => 'Authentication failed. Check the deployed database user/password and the account host grant for 172.16.1.133.',
        1044, 1142, 1143 => 'Database permission denied. Check the application account grants on keeperfx_e.*.',
        1049 => 'Configured database does not exist. Create keeperfx_e and import its schema on the database server.',
        1146 => 'A required table is missing. Import public-web/lib/mariadb.sql on the database server.',
        2002, 2003, 2005, 2006, 2013 => 'Database connection failed. Check host/port, DNS, MariaDB listening address and firewall access from the webserver.',
        2026 => 'Database TLS connection failed. Check the configured CA and database TLS settings.',
        default => 'Check the private configuration, database availability and schema. Consult the private server log if needed.',
    };
    fwrite(STDERR, "Database check failed: $hint No credentials were printed.\n");
    exit(1);
}
