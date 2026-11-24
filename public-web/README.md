# KeeperFX-E public website and native PHP API

Set the web server document root to **this public-web directory**. It contains the
project site, live server browser, service status, accounts/statistics, game pairing,
events, challenges, replay sharing and a searchable, read-only interactive API browser.
Every public HTTP endpoint under `/api/` is implemented in PHP. There is no Python
HTTP/account API behind a proxy. The optional Python process handles UDP relay only,
with a key-protected control listener bound to loopback.

Requirements: PHP 8.1+, JSON, mbstring, PDO and pdo_mysql for MariaDB. Local development
can use pdo_sqlite. There are no Composer or Node build dependencies. Requests use
HttpOnly/SameSite cookies plus CSRF for browser changes, or Bearer tokens for game/host
clients. Passwords use the existing salted PBKDF2 format, preserving SQLite account
compatibility. Database/key/configuration files belong outside this directory.

Pages: `/`, `/servers`, `/services`, `/api-browser`, `/profile`, `/device`, `/events`,
`/challenges`, `/replays` and public profiles under `/keeper/{id}`. The endpoint catalog
is available at `/api-catalog.json`. The API browser executes only listed GET routes
and redacts sensitive response fields; mutations use the normal account/game flows.

Run locally from the repository root:

```sh
python3 services/keeperhub/dev.py
```

This starts PHP on loopback, a private relay control listener and UDP relay, with
private SQLite state under `.local/keeperhub/`. `--state` and `--port` select isolated
state and a different port. The Python account/API code is retained only under
`macos/tests/legacy_keeperhub.py` for migration and contract tests.

For the production MariaDB/web-server setup, see [MARIADB-DEPLOYMENT.md](../docs/MARIADB-DEPLOYMENT.md).
