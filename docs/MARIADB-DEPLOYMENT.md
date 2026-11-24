# KeeperFX-E: MariaDB and PHP vhost deployment

| Role | Host | Private address |
| --- | --- | --- |
| MariaDB | sth-ts-dbs001.kladhest.se | 172.16.1.121 |
| PHP web server | sth-ts-web003.kladhest.se | 172.16.1.133 |
| Public site | keeperfx-e.retrocs.org | Your existing HTTPS endpoint |

The website and HTTP API are entirely inside `public-web/`. Deployment refreshes
that directory into your vhost, with no build step. All database and website service
settings live in its `config.inc.php`, which refreshes always preserve. Remote
servers have not been changed during development.

## 1. Create the database on sth-ts-dbs001

Use the updated checkout as the OS user who owns the administrative `.my.cnf`:

```sh
cd /path/to/KeeperFX-E
bash scripts/provision-mariadb.sh --web-config
```

The script uses `$HOME/.my.cnf` via MariaDB's `--defaults-extra-file` as its first
option. No root database password is requested, printed or passed in argv. It creates:

- Database `keeperfx_e` with utf8mb4 and the PHP API tables.
- Account `'keeperfx_e'@'172.16.1.133'` with a randomly generated application password.
- Only SELECT, INSERT, UPDATE and DELETE on `keeperfx_e.*` (no DDL or grant option).
- `$HOME/keeperfx-e.config.inc.php`, mode 0600, containing the complete website settings.

The generated password is written only into that private config. The account is
restricted to the web server's source IP. MariaDB must listen on its private
interface and TCP 3306 must be permitted from **172.16.1.133**.

The script refuses an existing account or output file; it never silently rotates
an existing login. On failure it removes only the account it just created and its
own incomplete output, leaving database contents intact. Existing tables are not
dropped. If you already provisioned the account using the previous script, retain
that account and use its private connection file as described below.

Options: `--defaults-file`, `--database`, `--user`, `--web-host`, `--database-host`,
`--schema`, `--output`. The defaults already match your two servers. If the installed
client is named mysql, prefix the command with `MARIADB_CLIENT=mysql`.
Without `--web-config`, the older private database-only output remains supported.

## 2. Install the public-web payload on sth-ts-web003

Choose a dedicated document root. The example uses `/var/www/keeperfx-e/public-web`;
replace it with your actual vhost path. Run the first deployment from your checkout:

```sh
cd /path/to/KeeperFX-E
python3 scripts/deploy-web.py /var/www/keeperfx-e/public-web --initialize --apply
```

First initialization requires a new or empty directory. It installs the public-web
payload and a credential-free default config. The script refuses unrelated nonempty
directories and symlinks. No SQL migration or service restart happens during refresh.

Securely transfer `~/keeperfx-e.config.inc.php` from the database server to the web
server using your normal SSH/SCP account, **not** the administrator's `.my.cnf`.
Install the transferred file as `/var/www/keeperfx-e/public-web/config.inc.php`.
Give it owner/group and mode 0640 so the deployment owner and PHP-FPM can read it,
while unrelated users cannot. For example, after selecting your actual PHP group:

```sh
PHP_FPM_GROUP=www-data   # replace if your PHP-FPM pool uses a different group
sudo install -o root -g "$PHP_FPM_GROUP" -m 0640 \
  "$HOME/keeperfx-e.config.inc.php" /var/www/keeperfx-e/public-web/config.inc.php
```

The file already selects `https://keeperfx-e.retrocs.org` and MariaDB
`172.16.1.121:3306`. Review `state_dir`, `admin_key_file`, `relay_control_url`,
`relay_control_key_file` and `release_manifest` for your service layout. There is
no need to configure PHP-FPM environment variables. Existing `KFX_*` environment
overrides remain supported; remove stale overrides if you want config.inc.php to
be the sole source of settings.

Prepare `/var/lib/keeperfx-e` outside the document root with access for the actual
PHP-FPM service user/group. PHP needs pdo_mysql, mbstring and JSON. The runtime SQL
account has no CREATE, ALTER or DROP privileges, so initial schema setup stays on
the database server. Never edit the source template with production credentials.

If you have an existing `/etc/keeperfx-e/database.php` from the previous deployment,
set `database_config_file` to that path in config.inc.php instead of copying its
password. This compatibility mode also preserves your existing application user.

For MariaDB TLS, set `database.ssl_ca` to the trusted CA path and use a DSN hostname
matching the server certificate. Require SSL on that application account. Certificate
verification remains enabled.

## 3. Route requests through PHP

Set the document root to the **deployed** public-web directory, never the repository.
Apache can use the supplied `.htaccess` with mod_rewrite and AllowOverride enabled.
For Nginx/PHP-FPM, use your existing HTTPS configuration and adapt these locations:

```nginx
root /var/www/keeperfx-e/public-web;
index index.php;
# Protect config, editor backups, internal modules and hidden deployment metadata.
location ~* ^/(?:config\.inc\.php|lib(?:/|$)|\.) { return 404; }
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

Only the front controller is a public PHP entry point. Both the front controller
and private config reject direct config requests. The Nginx rule also blocks editor
backups such as config.inc.php.bak, which must never be served as static files.
The endpoint catalog, server listings and API browser are same-origin pages.

If an HTTPS proxy fronts Nginx, preserve the public Host header. Configure real
client IP handling only for that trusted proxy; rate limits and UDP reachability
checks use PHP's REMOTE_ADDR.

Verify configuration from the web server, preferably as the PHP-FPM user:

```sh
php /var/www/keeperfx-e/public-web/check-database.php
```

This reads the deployed config, checks the connection and required tables, and
prints only the result. It never prints passwords or user data.
Check `/services` and use `/api-browser` to run `GET /api/health`.

## 4. Refresh later versions

From an updated checkout on the web server:

```sh
python3 scripts/deploy-web.py /var/www/keeperfx-e/public-web
python3 scripts/deploy-web.py /var/www/keeperfx-e/public-web --apply
```

The first command is a preview. The second updates changed payload files and removes
obsolete ones. **Existing config.inc.php contents, mtime and permissions are left
untouched.** New template options must be added manually to your server config as
needed; defaults in code remain compatible. Store runtime data outside the vhost.
Database migrations are a separate administrative step; refresh never resets data.

## Optional UDP relay

The PHP accounts, directory and direct joining work without a relay. Real-time UDP
forwarding needs the separate persistent worker; it is not part of an HTTP vhost
refresh. Generate a private random control key outside the document root, readable
by PHP-FPM and the worker, and set these config.inc.php values:

```php
'relay_control_url' => 'http://127.0.0.1:8766',
'relay_control_key_file' => '/var/lib/keeperfx-e/relay-control.key',
```

A private `admin_key` or `admin_key_file` enables trusted dedicated-host registration.
Do not reuse the database password as either service key. Run the worker from its
separately deployed service code, under your normal process supervisor:

```sh
python3 services/keeperhub/relay_worker.py \
  --key-file /var/lib/keeperfx-e/relay-control.key --control-port 8766 \
  --public-host keeperfx-e.retrocs.org --udp-bind 0.0.0.0 \
  --relay-first 27700 --relay-last 27731
```

Keep the control listener on loopback. Route UDP 27700–27731 through the public
firewall/NAT to the worker (172.16.1.133 if hosted on this web server). The advertised
public hostname must actually reach it, not an unrelated CDN/proxy.

## Existing data and backups

Moving an existing SQLite service to MariaDB requires a deliberate data import,
including password salts, hashes, user IDs and sessions. Simply changing the DSN
creates a separate service; it does not migrate accounts. Keep a verified backup
before importing. Use MariaDB-native backup/restore procedures for production;
the old SQLite maintenance tool does not back up MariaDB. No imported game graphics,
sound or data are needed by this website, and none are bundled with it.
