# Keeper-Enhanced community services

The optional service adds an in-game server browser, browser-paired accounts,
website profiles/statistics, friends, events, verified tournament brackets,
cooperative challenge completions, replay sharing and buffered live spectating.
LAN, hostname/IP direct connections and the original public lobby service remain
independent. No account or community service is required for LAN/direct play.

## Local development

The website and complete HTTP API are native PHP 8.1+ under `public-web/`, with
MariaDB for production and SQLite for local development. Python 3.9+ is needed only
for the dedicated-host supervisor. Install PHP
CLI with PDO SQLite and mbstring to run the local integrated website:
From the repository root:

```sh
python3 services/keeperhub/dev.py
```

The game defaults to `https://keeperfx-e.retrocs.org`. To use your own service,
open More Options → Network → Server Browser → Account → Community Service,
enter your HTTPS URL and choose Save Service Address. The choice persists in your
player profile; existing custom service addresses are preserved. Changing services
clears the current service's account credentials before connecting to the new one.
LAN and direct play remain independent.

For local development, enter `http://127.0.0.1:8765` in that same setting. If the
port is already in use, pass `--port 8766` and use that port in the game and
in the dedicated host configuration.
Open the printed localhost address and create an account if wanted. The service stores its
SQLite database and private administrator key in `.local/keeperhub/`. Neither
belongs in a release or source control. Stop the service with Ctrl-C, or use
`python3 services/keeperhub/dev.py --stop` from another terminal.

In the game choose More Options → Network → Server Browser / Accounts.

- **Internet** lists the selected community service. **Public** uses
  the configured public server directory.
- **LAN** discovers local games. **Favourites** and **Recent** retain addresses
  across restarts.
- Left/right changes browser tabs. Up/down and confirm reach server rows,
  paging, refresh, hosting, accounts, search, sorting and compatibility filters.
- Server details provide direct joining, password entry, favourites, address
  copying and spectating. There is no relay option or automatic relay fallback.
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
New Keeper-Enhanced peers advertise the `+hub1` readiness capability; older peers do not
automatically pass this headless-host preflight. Normal LAN/direct and original
lobby paths remain available.

Prepare a separate writable profile using an installed app and your own imported
Dungeon Keeper files, for example:

```sh
KFX_USER_DATA_DIR="$PWD/.local/dedicated-user" \
  "/Applications/Keeper-Enhanced.app/Contents/MacOS/KeeperFXE" \
  --import "$PWD/.local/keeperfx"
KFX_USER_DATA_DIR="$PWD/.local/dedicated-user" \
  "/Applications/Keeper-Enhanced.app/Contents/MacOS/KeeperFXE" --prepare-only
```

Set `visibility` to `private` (no directory or key) or `public` (listed, with
an administrator key). Either mode accepts an optional `KFX_SERVER_PASSWORD`.
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

The directory is HTTP/JSON; gameplay remains direct ENet/UDP. The service uses
password hashing, hashed session tokens, HttpOnly/SameSite cookies, CSRF and
origin/Host checks, bounded bodies and rate limits. It never distributes original
Dungeon Keeper assets. Public servers require a reachable gameplay UDP port.
Private servers do not publish their address or contact the directory.

See [Linux dedicated servers](LINUX-SERVER.md) for native packaging, data import,
private/public configuration, optional passwords and systemd operation.

## PHP website deployment

The website, accounts, HTTP API and API browser are all under `public-web/`. The
front controller handles `/api/` directly in PHP rather than proxying a Python API.
Use this directory as the document root. See [MariaDB deployment](MARIADB-DEPLOYMENT.md)
for the database server sth-ts-dbs001 (172.16.1.121), web server sth-ts-web003
(172.16.1.133), restricted application account and private PHP-FPM configuration.

The old Python API implementation is a compatibility fixture under macos/tests only.


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

Monitor `/api/health`: database failures return an error. Expired sessions, pairing requests and identity
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
