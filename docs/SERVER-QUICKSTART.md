# Existing Apache vhost and new MariaDB account

The website now uses direct `/index.php?route=...` page and API URLs. Refreshing
public-web is sufficient for those links; the Apache change below enables the
older clean URL aliases too. All website and HTTP API code ships in public-web.

## Web server: sth-ts-web003 (172.16.1.133), as root

The existing document root is `/srv/www/vhosts/keeperfx-e.retrocs.org`.
If the homepage works but every clean URL returns Apache's 404, load the routing
overlay into Apache's existing conf.d includes. No new VirtualHost is needed:

```sh
curl -fsSL https://raw.githubusercontent.com/retrocs-org/KeeperFX-E/main/docs/apache-keeperfx-e.conf \
  -o /etc/apache2/conf.d/keeperfx-e-routing.conf
APACHECTL=$(command -v apache2ctl || command -v apachectl)
"$APACHECTL" configtest && systemctl reload apache2
cd /srv/www/vhosts
./refresh-keeperfx-e.sh
```

The overlay uses `FallbackResource /index.php`; it works with .htaccess disabled.
It blocks config/backups and private modules. The existing TLS/vhost is retained.
If conf.d is not included by your Apache setup, load this file from its actual
include directory or inside your existing vhost configuration instead.

## Database server: sth-ts-dbs001 (172.16.1.121)

Run as the OS user who owns the administrative `.my.cnf`. Launch the SQL client
without a command-line password:

```sh
mariadb --defaults-extra-file="$HOME/.my.cnf"
```

Run this SQL, replacing the application password placeholder:

```sql
CREATE DATABASE IF NOT EXISTS keeperfx_e
  CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER 'keeperfx_e'@'172.16.1.133'
  IDENTIFIED BY 'PUT_A_NEW_STRONG_PASSWORD_HERE';
GRANT SELECT, INSERT, UPDATE, DELETE ON keeperfx_e.*
  TO 'keeperfx_e'@'172.16.1.133';
EXIT;
```

This creates a new account. If it already exists, retain its working credentials
instead of rerunning CREATE USER or changing its password accidentally.
Install the application tables on that same database server:

```sh
curl -fsSL https://raw.githubusercontent.com/retrocs-org/KeeperFX-E/main/public-web/lib/mariadb.sql \
  -o /tmp/keeperfx-e-schema.sql
mariadb --defaults-extra-file="$HOME/.my.cnf" keeperfx_e < /tmp/keeperfx-e-schema.sql
```

MariaDB must listen on its private interface and allow TCP 3306 from 172.16.1.133.
The host-scoped application account has CRUD access to this database only.

## Configure PHP on sth-ts-web003

Edit `/srv/www/vhosts/keeperfx-e.retrocs.org/config.inc.php`. Leave
`database_config_file` empty when configuring the connection directly:

```php
'database' => [
    'dsn' => 'mysql:host=172.16.1.121;port=3306;dbname=keeperfx_e;charset=utf8mb4',
    'user' => 'keeperfx_e',
    'password' => 'THE_SAME_APPLICATION_PASSWORD',
    'ssl_ca' => '',
],
```

PHP needs pdo_mysql and mbstring. Keep the server config out of Git; refreshes
preserve it. Verify as the web service user:

```sh
sudo -u wwwrun php /srv/www/vhosts/keeperfx-e.retrocs.org/check-database.php
curl -fsS https://keeperfx-e.retrocs.org/api/session
```

The CLI check prints only connection/schema status. The URL should return JSON.
If it still returns Apache's HTML 404, routing is not yet active. If it returns
PHP's JSON 503, check the private config, PHP extensions, schema and DB reachability.
For generated credentials and complete provisioning instead of manual SQL, use
[scripts/provision-mariadb.sh](../scripts/provision-mariadb.sh) as described in
[MARIADB-DEPLOYMENT.md](MARIADB-DEPLOYMENT.md).
