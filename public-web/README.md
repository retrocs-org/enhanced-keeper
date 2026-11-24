# KeeperFX-E website and native PHP API

KeeperFX-E is an independent fork of [KeeperFX](https://github.com/dkfans/keeperfx).

This directory is the **complete deployable PHP website**: homepage, original
dungeon-themed artwork, server browser, network service status, accounts and
statistics, pairing, events, challenges, replays and the interactive API browser.
Every public `/api/` HTTP endpoint is implemented here in PHP. No build step,
Composer install, Node server or Python HTTP backend is needed on the web server.
The optional real-time UDP relay remains a separate worker.

Requirements: PHP 8.1+, JSON, mbstring, PDO and pdo_mysql for MariaDB. PDO SQLite
is supported for local development. Serve this directory as the vhost document
root with the supplied Apache `.htaccess`, or the Nginx routing in the deployment
guide. Only `index.php` should be exposed as an HTTP PHP entry point.

## Configuration

Edit **the deployed `config.inc.php`**. It contains the public origin, database
DSN/user/password, private runtime directory, optional relay URL/control credentials,
administrator key and verified release manifest path. Defaults select:

- `https://keeperfx-e.retrocs.org`
- MariaDB `172.16.1.121:3306`, database/user `keeperfx_e`
- Runtime state `/var/lib/keeperfx-e`, outside the vhost

The template contains no passwords or service keys. Enable the optional relay by
setting `relay_control_url` and its key/key file. A private database file can still
be selected with `database_config_file`; environment overrides remain supported
for local tests and existing installations. The precedence is environment override,
then this config; an explicitly selected external database file supplies its own
connection credentials.

The deployed config must be readable by PHP-FPM but not publicly downloadable.
Use mode 0640 with the appropriate PHP-FPM group. Direct requests are rejected;
Nginx must also deny config backups and `/lib/` as shown in the deployment guide.
Keep databases, keys and runtime state outside the vhost. Never put production
passwords into the source checkout or Git; source-release checks reject nonempty
credential settings in the tracked config template.

## Refresh the vhost

Run on the web server from an updated checkout:

```sh
# First installation into a NEW/EMPTY dedicated document root:
python3 scripts/deploy-web.py /var/www/keeperfx-e/public-web --initialize --apply

# Later refreshes (no build or migration):
python3 scripts/deploy-web.py /var/www/keeperfx-e/public-web --apply
```

Omit `--apply` to preview. The script copies this directory only, updates changed
files and removes obsolete payload files. **An existing `config.inc.php` is never
overwritten, merged or deleted.** The ordinary refresh also preserves its mode. It installs defaults only when config
is absent. A marker identifies the dedicated vhost, and unowned nonempty directories
and symlinks are refused. Do not store uploads, logs or application data in this
managed directory. Database migrations and UDP worker deployment are separate.

Check the deployed database configuration as the PHP service user:

```sh
php /var/www/keeperfx-e/public-web/check-database.php
```

## wwwrun ownership and permissions

On your web server, use the convenience wrapper with the absolute **document root**:

```sh
sudo bash scripts/refresh-vhost.sh /path/to/keeperfx-e.retrocs.org --apply
```

It refreshes the payload, then sets the document root and its contents to
`wwwrun:wwwrun`, directories to 0755, ordinary files to 0644 and config.inc.php to
0640. The config's contents and modification time remain unchanged. This wrapper
explicitly updates ownership/mode; it never overwrites the existing config.
Omit `--apply` to preview, or add `--initialize` for a new/empty first deployment.
Use the document root only, not a parent containing other sites, logs or vhost configs.
The lower-level deploy script supports `--owner USER --group GROUP --set-permissions`
for other server accounts. Both user and group must already exist.

## Local development

From the repository root:

```sh
python3 services/keeperhub/dev.py
```

This starts PHP on loopback with an isolated SQLite configuration and an optional
private relay, using `.local/keeperhub/` state. Use `--state` and `--port` for an
isolated test instance. The source config does not need editing for this workflow.

Routes: `/`, `/servers`, `/services`, `/api-browser`, `/profile`, `/device`, `/events`,
`/challenges`, `/replays`, `/keeper/{id}` and `/api-catalog.json`. Browser changes use
HttpOnly/SameSite cookies plus CSRF; game/host clients use Bearer credentials. The
API explorer runs listed GET requests and redacts credentials from its output.

See [MariaDB deployment](../docs/MARIADB-DEPLOYMENT.md) for the two-server setup.

## Standalone refresh from GitHub main

`scripts/refresh-from-github.sh` is the standalone root-run Linux alternative: no
local checkout or Python is needed. Keep a copy outside the vhost and run it with
`sudo sh /path/to/refresh-from-github.sh`. It clones GitHub main into a temporary
directory, stages only public-web, preserves the live config.inc.php, lints the
final PHP tree, and applies wwwrun:wwwrun ownership, directories 0750 and files
0640. It swaps the directory after staging and restores the previous site if the
final rename fails. A flock lock prevents overlapping refreshes. The default vhost
is `/srv/www/vhosts/keeperfx-e.retrocs.org`; edit PARENT in the script if needed.
Dependencies are git, PHP CLI, getent, flock and standard POSIX file utilities.
An existing config must be a regular file. On first install, the credential-free
config template is installed. No database migrations or service restarts run.
