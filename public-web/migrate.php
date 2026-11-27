<?php
/** Apply the bundled MariaDB schema using the deployed website credentials. */
if (PHP_SAPI !== 'cli') {
    http_response_code(404);
    exit;
}

ini_set('display_errors', '0');
define('KFX_API_INTERNAL', true);
require __DIR__.'/lib/Hub.php';
require __DIR__.'/lib/config.php';

try {
    [$dsn, $connection] = database_config();
    if (!str_starts_with($dsn, 'mysql:')) {
        fwrite(STDERR, "Configure the website to use MariaDB before migrating.\n");
        exit(1);
    }
    if (!class_exists('PDO') || !in_array('mysql', PDO::getAvailableDrivers(), true)) {
        fwrite(STDERR, "Enable pdo_mysql for PHP CLI.\n");
        exit(1);
    }
    if (!is_readable(__DIR__.'/lib/mariadb.sql')) {
        fwrite(STDERR, "The bundled MariaDB schema is missing or unreadable.\n");
        exit(1);
    }
    $connection['migrate'] = true;
    new KeeperHub($dsn, '', '', '', null, $connection);
    echo "MariaDB schema applied.\n";
} catch (Throwable $error) {
    $message = match ($error instanceof PDOException ? (int) ($error->errorInfo[1] ?? 0) : 0) {
        1045 => 'Authentication failed. Check the website credentials and account host.',
        1044, 1142, 1143 => 'Permission denied. The configured account needs CREATE on its database during migration.',
        1049 => 'Create the configured database before running this command.',
        default => 'Check the deployed configuration, database connection and schema.',
    };
    fwrite(STDERR, "MariaDB migration failed: $message\n");
    exit(1);
}
