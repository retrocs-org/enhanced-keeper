<?php
/** Explicit CLI-only schema initialization/repair. Never runs during HTTP requests. */
if (PHP_SAPI !== 'cli') { http_response_code(404); exit; }
ini_set('display_errors', '0');
umask(0077);
define('KFX_API_INTERNAL', true);
require dirname(__DIR__).'/Hub.php';
require dirname(__DIR__).'/config.php';
try {
    [$dsn, $connection] = database_config();
    $connection['migrate'] = true;
    new KeeperHub($dsn, '', '', '', null, $connection);
    echo "Schema initialization completed. Existing tables and data were preserved.\n";
    // The checker defines the same guard; run it as a separate command after migration.
} catch (Throwable $e) {
    $native = $e instanceof PDOException ? (int)($e->errorInfo[1] ?? 0) : 0;
    $hint = match ($native) {
        1044, 1142, 1143 => 'The configured database account needs CREATE on its database for this command. Grant it temporarily, then revoke it after migration.',
        1045 => 'Database authentication failed. Check the deployed configuration and host grant.',
        1049 => 'Create the configured database first on the database server.',
        default => 'Run check-database.php to diagnose the connection; check PHP PDO drivers and the private database configuration.',
    };
    fwrite(STDERR, "Migration failed: $hint A partial run can safely be retried. No credentials were printed.\n");
    exit(1);
}
