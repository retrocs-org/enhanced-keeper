<?php
declare(strict_types=1);

// The web application account can clear its data without database DDL grants.
if (PHP_SAPI !== 'cli') { http_response_code(404); exit; }
ini_set('display_errors', '0');

$apply = $argv === [$argv[0], '--reset', 'keeperfx_e'];
if (!$apply && $argc !== 1) {
    fwrite(STDERR, "Usage: php reset.php [--reset keeperfx_e]\n");
    exit(2);
}

define('KFX_API_INTERNAL', true);
require dirname(__DIR__) . '/Hub.php';
require dirname(__DIR__) . '/config.php';

$database = 'keeperfx_e';
$db = null;
try {
    [$dsn, $connection] = database_config();
    if (!str_starts_with($dsn, 'mysql:')) {
        throw new RuntimeException('Reset requires a configured MariaDB connection.');
    }
    $connection['migrate'] = false;
    $db = (new KeeperHub($dsn, '', '', '', null, $connection))->db;
    if ($db->query('SELECT DATABASE()')->fetchColumn() !== $database) {
        throw new RuntimeException('The selected database is not keeperfx_e.');
    }

    $schema = file_get_contents(dirname(__DIR__) . '/mariadb.sql');
    if ($schema === false || !preg_match_all('/^CREATE TABLE IF NOT EXISTS ([a-z_]+)\(/m', $schema, $matches)) {
        throw new RuntimeException('Could not read the KeeperHub table list.');
    }
    $tables = $matches[1];
    $actual = $db->query(
        "SELECT TABLE_NAME, ENGINE FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE()"
    )->fetchAll(PDO::FETCH_KEY_PAIR);
    $expectedNames = $tables;
    $actualNames = array_keys($actual);
    sort($expectedNames);
    sort($actualNames);
    if ($actualNames !== $expectedNames || array_unique(array_values($actual)) !== ['InnoDB']) {
        throw new RuntimeException('The database does not match the deployed InnoDB schema. Run the migration and check for extra tables or views.');
    }

    printf("Database: %s\nTables: %s\n", $database, implode(', ', $tables));
    if (!$apply) {
        echo "Preview only. Pass --reset keeperfx_e to delete all KeeperHub records.\n";
        exit(0);
    }

    $db->beginTransaction();
    $deleted = 0;
    foreach ($tables as $table) {
        $deleted += $db->exec('DELETE FROM `' . $table . '`');
    }
    $db->commit();
    printf("Reset complete. Deleted %d records across %d tables.\n", $deleted, count($tables));
} catch (Throwable $error) {
    if ($db !== null && $db->inTransaction()) {
        $db->rollBack();
    }
    $reason = $error instanceof PDOException
        ? 'Check database connectivity, schema, and the application user’s SELECT and DELETE grants.'
        : ($error instanceof RuntimeException ? $error->getMessage() : 'Check the deployed database configuration.');
    fwrite(STDERR, "Reset stopped: $reason\n");
    exit(1);
}
