<?php
/**
 * KeeperFX-E website + HTTP API settings (PHP 8.1+).
 * Edit the deployed copy. The vhost refresh script preserves this file.
 * Keep real passwords out of the source checkout and Git.
 */
if (!defined('KFX_SETTINGS_INTERNAL')) { http_response_code(404); exit; }
return [
    'public_origin' => 'https://keeperfx-e.retrocs.org',
    // Writable by PHP-FPM; keep database files and runtime data outside the vhost.
    'state_dir' => '/var/lib/keeperfx-e',
    'database' => [
        'dsn' => 'mysql:host=172.16.1.121;port=3306;dbname=keeperfx_e;charset=utf8mb4',
        'user' => 'keeperfx_e',
        'password' => '', // Paste the generated APPLICATION password here, never root's password.
        'ssl_ca' => '',  // Optional trusted CA file when using MariaDB TLS.
    ],
    // Optional compatibility with an existing private database connection file.
    // Leave blank when configuring the database above.
    'database_config_file' => '',
    'admin_key' => '',
    'admin_key_file' => '/var/lib/keeperfx-e/admin.key',
    // Accounts, directory and direct joining work without the optional UDP worker.
    'relay_control_url' => '', // e.g. http://127.0.0.1:8766
    'relay_control_key' => '',
    'relay_control_key_file' => '/var/lib/keeperfx-e/relay-control.key',
    'release_manifest' => '/var/lib/keeperfx-e/releases.json',
];
