# KeeperFX-E community services

The optional service adds an in-game server browser, browser-paired accounts,
website profiles/statistics, friends, events, verified tournament brackets,
cooperative challenge completions, replay sharing and buffered live spectating.
LAN, hostname/IP direct connections and the original public lobby service remain
independent. No account or community service is required for LAN/direct play.

## Local development

The website is PHP 8.1+ under `public-web/`. KeeperHub remains the Python 3.9+
SQLite account/directory API and UDP relay, with no third-party Python packages.
Install PHP CLI as well as Python to run the local website and API together.
From the repository root:

```sh
python3 services/keeperhub/dev.py
```

If that port is already in use, pass `--port 8766` and set the same address in
the game’s Community Service settings and dedicated host configuration.
Open the printed localhost address and create an account if wanted. The service stores its
SQLite database and private administrator key in `.local/keeperhub/`. Neither
belongs in a release or source control. Stop the service with Ctrl-C, or use
`python3 services/keeperhub/dev.py --stop` from another terminal.

In the game choose More Options → Network → Server Browser / Accounts.

- **Internet** lists the selected community service. **Original Lobbies** opens
  the upstream KeeperFX lobby service separately.
- **LAN** discovers local games. **Favourites** and **Recent** retain addresses
  across restarts and resolve refreshed relay identities from the directory.
- Left/right changes browser tabs. Up/down and confirm reach server rows,
  paging, refresh, hosting, accounts, search, sorting and compatibility filters.
- Server details provide direct joining, relay joining, password entry,
  favourites, address copying and spectating. A failed public direct join tries
  the configured service relay once. The relay needs the host's outbound
  registrations; there is no service dependency for ordinary direct joining.
- Sign In opens the local website and displays an eight-character pairing code.
  Approve only the code on your own game. Game session credentials are stored
  separately with mode 0600; changing service addresses clears them.
- Community pages show events, friends, match history, challenges and replay
  listings. Account creation, friend requests, event management, replay upload
  and tournament result entry are available on the website.
- Service settings allow loopback HTTP for development and require HTTPS for
  remote services. TLS certificate verification remains enabled.

Latency queries echo a bounded nonce without creating an ENet player. Numeric
IPv4/IPv6 addresses can be probed; unresolved hostnames show unknown ping.
Engine filtering is an initial check. The existing preflight checks selected
map/campaign bytes and mods before a normal host starts.

## Headless hosting

The current lockstep engine makes the host a player. This implementation uses
**one synchronized AI host and up to three humans**, with no visible window or
sound. It does not claim four human slots or a separate authoritative simulation
protocol. Spectators replay a buffered input stream and consume no player slot.
New KeeperFX-E peers advertise the `+hub1` readiness capability; older peers do not
automatically pass this headless-host preflight. Normal LAN/direct and original
lobby paths remain available.

Prepare a separate writable profile using an installed app and your own imported
Dungeon Keeper files, for example:

```sh
KFX_USER_DATA_DIR="$PWD/.local/dedicated-user" \
  "/Applications/KeeperFX-E.app/Contents/MacOS/KeeperFXE" \
  --import "$PWD/.local/keeperfx"
KFX_USER_DATA_DIR="$PWD/.local/dedicated-user" \
  "/Applications/KeeperFX-E.app/Contents/MacOS/KeeperFXE" --prepare-only
```

Copy `examples/dedicated-local.json` to an ignored local file and adjust paths.
Choose maps with enough player positions. The example runs Classic map 52 with
one human and the AI host. The engine binary and GameData must have matching
assets. The supervisor writes only into the dedicated profile, never the app.

Start the supervisor without printing or copying its administrator key:

```sh
python3 services/keeperhub/run-local-host.py .local/dedicated.json
```

`run-local-host.py` reads `.local/keeperhub/admin.key` into the child environment.
For a different service, set `KFX_HUB_ADMIN_KEY` through your own secret manager
and invoke `dedicated.py CONFIG` directly. An optional `KFX_SERVER_PASSWORD`
sets the game lobby password. Administrator and account tokens are never passed
in command-line arguments.

The supervisor supports JSON commands on stdin: `{"command":"status"}`,
`{"command":"next"}` and `{"command":"stop"}`. It rotates maps after a match,
archives replay files under the dedicated profile, and stops its own child on
shutdown. From another terminal use `run-local-host.py CONFIG --stop`, `--next`
or `--status` to control only that configured host. Spectator publishing is explicitly enabled in its configuration.

## Results and community features

Guest and player-reported games remain **casual**. Consistent player reports can
be **confirmed**, which is still not a verified statistic. Only a dedicated host
registered with the service administrator key may report **verified** results.
Account join tickets are scoped to a server, expire, and bind to actual ENet
peers; players do not send their account bearer tokens through game chat.

The engine records terminal outcomes separately from early departures. The
supervisor cannot count an early disconnect as a verified victory. The trust
boundary is the dedicated-server operator: this is not a claim of cheat-proof
networking. Accounts are private by default; public profile URLs are
`/keeper/ACCOUNT_ID` and show verified statistics and earned achievements.

Tournament organizers can start single-elimination brackets with 2–32 entrants.
Byes advance automatically. Later rounds advance only using a service-verified
match with the correct pair of authenticated players; a match cannot be reused.
Challenge completions require a verified win on the specified map. Cooperative
hosts may report multiple allied winners when explicitly configured.

Opted-in headless hosts publish bounded append-only replay streams. The game
checks the engine version, buffers data, and waits when a stream has not caught
up. Spectators cannot issue live game actions. Finished streams become replay
downloads with SHA-256 checksums; account holders may also upload completed-match
replays up to 2 MiB. Live streams are limited to 16 MiB in this first local
implementation. This is buffered replay spectating, not free-camera rollback or
mid-match player joining.

## Service boundaries

The directory is HTTP/JSON; gameplay remains ENet/UDP. Relay channels require
separate host/client tokens, accept traffic only from authenticated endpoints,
expire when idle and have capacity/bandwidth limits. The relay is transport,
not a game simulator. The service uses password hashing, hashed session tokens,
HttpOnly/SameSite cookies, CSRF and origin/Host checks, bounded bodies and rate
limits. It never distributes original Dungeon Keeper assets.

This is a local-tested implementation. Before public deployment it still needs
an operated HTTPS endpoint, public relay UDP port range, backups, monitoring,
account recovery/email policy and testing across real routers/CGNAT. No public
infrastructure, accounts or ranking data are created by the build.

## Verification

```sh
python3 macos/tests/test-keeperhub.py
python3 macos/tests/test-php-web.py
python3 macos/tests/run-ingame-options-tests.py
python3 macos/tests/run-readiness-tests.py
python3 macos/tests/test-community-integration.py
```

The integration test uses isolated imported profiles and loopback services. It
starts a real headless engine, deliberately advertises an unreachable direct
port, joins through automatic relay fallback, binds a cloud identity and starts
a separate real spectator without increasing the player count. Only its own
processes are stopped. Its temporary profiles contain private test credentials
and must not be published.

## PHP website deployment

Set the web server document root to `public-web/`, never the repository root.
The site requires PHP 8.1+ with HTTP stream support (`allow_url_fopen=On`); it
uses no Composer dependencies. PHP serves a new project landing page, a separate `/community` section, assets
and a bounded same-origin `/api/` gateway. JavaScript remains responsible for
interactive forms and listings. The API, SQLite database and UDP relay remain
in KeeperHub; PHP hosting alone does not replace the game service.

The public origin defaults to `https://keeperfx-e.retrocs.org` and the private
API address to `http://127.0.0.1:8766`. Override with `KFX_PUBLIC_ORIGIN` (no trailing
slash) and `KFX_HUB_UPSTREAM` in PHP-FPM's environment or Apache's `SetEnv`.
The upstream must be loopback HTTP. It is never selected from visitor input.
The Python service's `--origin` must match the PHP public origin exactly:

```sh
# Supply KFX_HUB_ADMIN_KEY from the service manager/secret store.
python3 services/keeperhub/hub.py --bind 127.0.0.1 --port 8766 \
  --origin https://keeperfx-e.retrocs.org --trust-loopback-proxy \
  --db /var/lib/keeperfx-e/community.sqlite3
```

Keep the database, administrator key and private configuration outside the
web document root. `--trust-loopback-proxy` preserves per-client rate limits
using the address set by PHP; only enable it for a private API behind this local
gateway. Configure relay public host/UDP ports separately as described above.
Do not expose the private HTTP API directly to the Internet.

Apache needs `mod_rewrite` and permission to use the supplied `.htaccess`.
For Nginx with PHP-FPM, adapt the following inside the site's HTTPS server block:

```nginx
root /srv/keeperfx-e/public-web;
index index.php;
location / { rewrite ^ /index.php last; }
location = /index.php {
    include fastcgi_params;
    fastcgi_param SCRIPT_FILENAME $document_root/index.php;
    fastcgi_param HTTP_AUTHORIZATION $http_authorization;
    fastcgi_pass unix:/run/php/php-fpm.sock;
}
location ~ \.php$ { return 404; }
```

Use your server's actual FPM socket path. All routes, including CSS/JavaScript,
pass through the front controller. It rejects unknown routes, cross-origin
requests and unexpected hosts; forwards account cookies/CSRF and game bearer
authentication; refuses redirects from the upstream; and returns generic errors
when the service is offline. Both the browser and game can use the same HTTPS
origin. PHP's built-in server is used only for local development.

## Account recovery and operations

Signed-in players can generate eight offline recovery codes after confirming
their current password. Store the codes privately. Only hashes are stored on the
service. Password recovery revokes every browser/game session, approved pairing
and remaining recovery code; sign in again and issue a fresh set. The service
uses offline codes rather than claiming to send recovery email.

Use the online SQLite backup command instead of copying a live database file:

```sh
python3 services/keeperhub/maintenance.py backup --db /var/lib/keeperfx-e/community.sqlite3 --output /var/backups/keeperfx-e --keep 7
python3 services/keeperhub/maintenance.py check --db /var/backups/keeperfx-e/SELECTED-BACKUP.sqlite3
```

Backups are verified, standalone SQLite files with private permissions. Retention
prunes only this tool's own named backups. To restore, stop KeeperHub, keep a copy
of the current database and its WAL/SHM files, replace the stopped database with a
verified backup, then restart. Backups contain private accounts and sessions;
keep them outside Git and the website, with an independently protected offsite
copy. Arrange the backup command through your server's scheduler before launch.

Monitor `/api/health`: database failures return an error, and an expected relay
that has stopped returns 503. Expired sessions, pairing requests and identity
tickets are cleaned up; recovery generation/reset is rate limited and the
in-memory abuse table has a hard bound. These are local-tested service features;
public HTTPS, backups/alerts and mail policy still need an operated server.

Run `python3 scripts/update-web-releases.py` after publishing a stable release,
and periodically from the website host's scheduler. It atomically refreshes
`.local/web/releases.json` (or `--output` outside the document root), verifies
both DMG/ZIP checksum files and host digests when present, and ignores draft or
prerelease releases. PHP can use `KFX_RELEASE_MANIFEST` to select that private
cache. Missing releases retain the normal release-page link; no invented version
or download is advertised. An update failure leaves the last valid cache intact.

On a desync or peer departure, the game writes one bounded
`save/network-diagnostics.json` report and includes it in Copy Diagnostics.
Existing automatic state resynchronization remains in use. Upgraded ENet peers
can resume a reserved player slot after an unclean timeout using an in-memory
session key. Clean departures, expired reservations and application restarts
still require a lobby join. See [recovery and local tools](POLISH-TOOLS.md) for
the bounded grace period and tested limits.
