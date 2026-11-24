# KeeperFX-E: MariaDB and PHP deployment

## Target servers

| Role | Host | Private address |
| --- | --- | --- |
| MariaDB | sth-ts-dbs001.kladhest.se | 172.16.1.121 |
| PHP web server | sth-ts-web003.kladhest.se | 172.16.1.133 |
| Public site | keeperfx-e.retrocs.org | Existing public HTTPS endpoint |

Use the updated checkout containing `scripts/provision-mariadb.sh` and
`public-web/lib/mariadb.sql`. No remote server was changed during implementation;
all provisioning/API checks were run against an isolated local MariaDB instance.

## Create the database and restricted application account

Run on **sth-ts-dbs001**, as the OS user who owns the administrative `.my.cnf`:

```sh
cd /path/to/KeeperFX-E
bash scripts/provision-mariadb.sh
```

The script reads `$HOME/.my.cnf` using MariaDB's `--defaults-extra-file` option as
its first option. It does not request, print or place the root password in argv.
It creates:

- Database `keeperfx_e`, using utf8mb4.
- Account `'keeperfx_e'@'172.16.1.133'` with a fresh generated application password.
- The PHP API tables from `public-web/lib/mariadb.sql`.
- Only SELECT, INSERT, UPDATE and DELETE permissions on `keeperfx_e.*`.
- `$HOME/keeperfx-e.database.php`, a mode-0600 private PHP connection file.

The generated password is written only to the private file. The script refuses an
existing account or credential output instead of silently rotating a working login.
On failure it removes only the newly created account, leaving database contents
intact. Existing tables are never dropped. There is no global or grant-option access.

Optional arguments include `--defaults-file`, `--database`, `--user`, `--web-host`,
`--database-host`, `--schema` and `--output`. Defaults already select the servers above.
If the client is named mysql, use `MARIADB_CLIENT=mysql bash scripts/provision-mariadb.sh`.

## Configure the web server

Transfer `keeperfx-e.database.php` securely to **sth-ts-web003**. Install it outside
the website, for example `/etc/keeperfx-e/database.php`. Give the PHP-FPM service
account read access, normally with a root-owned mode-0640 file and an appropriate
PHP-FPM group. Do not copy the database administrator's `.my.cnf` onto the web server.

Set these values in the PHP-FPM pool environment (or the equivalent web-server PHP
environment):

```ini
env[KFX_PUBLIC_ORIGIN] = https://keeperfx-e.retrocs.org
env[KFX_DB_CONFIG] = /etc/keeperfx-e/database.php
env[KFX_HUB_STATE] = /var/lib/keeperfx-e
```

The generated DSN connects to 172.16.1.121:3306 with charset=utf8mb4. The SQL account
accepts connections from 172.16.1.133. MariaDB must listen on its private interface
and accept TCP 3306 from that web-server address. The public website never connects
as the database root account. PHP's pdo_mysql and mbstring extensions must be enabled.
The application does not require CREATE, ALTER or DROP permissions at runtime.

If MariaDB TLS is configured, add `ssl_ca` with the trusted CA file path to the
private PHP array; PDO verifies the server certificate. Use a DSN hostname that
matches that certificate. Require SSL on the application account when enabling TLS.

Check the connection from the web server:

```sh
cd /path/to/KeeperFX-E
KFX_DB_CONFIG=/etc/keeperfx-e/database.php php scripts/check-database.php
```

This prints only a connection/schema result. It does not print passwords or data.
Run it as the PHP service user as well when checking file permissions.

## Serve only public-web

The site's document root is `/path/to/KeeperFX-E/public-web`, never the repository
root. Apache can use the supplied `.htaccess` with mod_rewrite. With Nginx/PHP-FPM,
use the existing site's HTTPS/TLS setup and route pages through index.php. For example,
adapt the source path and PHP-FPM socket in these locations:

```nginx
root /srv/keeperfx-e/public-web;
index index.php;
location ^~ /lib/ { return 404; }
location = /index.php {
    include fastcgi_params;
    fastcgi_param SCRIPT_FILENAME $document_root/index.php;
    fastcgi_param HTTP_AUTHORIZATION $http_authorization;
    fastcgi_pass unix:/run/php/keeperfx-e.sock;
}
location ~ \.php$ { return 404; }
location / { try_files $uri @keeperfx; }
location @keeperfx {
    include fastcgi_params;
    fastcgi_param SCRIPT_FILENAME $document_root/index.php;
    fastcgi_param HTTP_AUTHORIZATION $http_authorization;
    fastcgi_pass unix:/run/php/keeperfx-e.sock;
}
```

When an existing HTTPS proxy fronts the web server, preserve the public Host
header. Configure real client IP handling only for that trusted proxy; request
rate limits and incoming UDP checks use PHP’s REMOTE_ADDR.

The front controller allowlists public assets/pages. Private PHP modules reject
standalone requests, and the Nginx rule also denies schema/module downloads.
Configure the PHP pool's request timeout and existing HTTPS settings as usual.

## Enable the UDP relay when needed

Accounts, the directory and direct joining work without a relay worker. To enable
relay connections, create a random private control key on the web server and make
it readable by PHP-FPM and the relay service user. A separate private administrator
key enables trusted dedicated-host registration. Keep both files outside public-web.
For example, with a private directory already prepared:

```sh
umask 077
openssl rand -hex 32 > /etc/keeperfx-e/relay-control.key
openssl rand -hex 32 > /etc/keeperfx-e/admin.key
```

Adjust ownership/group read access for the selected service accounts. Add:

```ini
env[KFX_RELAY_CONTROL] = http://127.0.0.1:8766
env[KFX_RELAY_CONTROL_KEY_FILE] = /etc/keeperfx-e/relay-control.key
env[KFX_HUB_ADMIN_KEY_FILE] = /etc/keeperfx-e/admin.key
```

Run the optional worker on sth-ts-web003 using the process manager you already use:

```sh
python3 services/keeperhub/relay_worker.py \
  --key-file /etc/keeperfx-e/relay-control.key \
  --control-port 8766 --udp-bind 0.0.0.0 \
  --public-host keeperfx-e.retrocs.org --relay-first 27700 --relay-last 27731
```

Its control HTTP listener is always bound to 127.0.0.1. Only UDP 27700–27731 needs the
public route for games. If the web server is behind NAT, route those UDP ports to
172.16.1.133. The HTTPS proxy alone does not forward UDP. The website's Services page
checks worker readiness; the in-game connection test checks an actual game route.
Set the game's Community Service URL to `https://keeperfx-e.retrocs.org`.

## Data and checks

SQLite remains available for local development. PHP reads existing SQLite passwords,
sessions and community records without a destructive migration. MariaDB uses its own
initialized schema; moving existing SQLite data into MariaDB requires a deliberate
backup/import rather than pointing MariaDB at an SQLite file.

Source-only tests:

```sh
python3 macos/tests/test-native-php-api.py
python3 macos/tests/test-php-web.py
python3 macos/tests/test-community-integration.py
```

To run the native API/HTTP contracts against a separate disposable MariaDB instance,
set KFX_TEST_MYSQL_SOCKET to that test instance's local Unix socket. Those tests create
and drop their own randomly named kfx_contract_/kfx_web_ databases. Never point that
variable at production. CI and hosted runners remain deferred.
