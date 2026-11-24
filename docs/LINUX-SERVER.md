# Keeper Enhanced dedicated server on Linux

Supports private direct/LAN hosting and public directory-listed hosting. Either
mode can require a password. One synchronized AI host occupies a player slot,
leaving up to three humans; this is a lockstep host, not a new authoritative
network protocol. No relay is used. Private mode makes no community HTTP calls,
publishes no server listing or spectator stream, and needs no community key.

Source: https://github.com/retrocs-org/KeeperFX-E (existing repository URL).

## Build and import

On Linux install the dependencies documented in `build/docker/deck.Dockerfile`
(and the SDL3 dependencies resolved by CMake), plus Python 3, Ninja and patchelf.
Build natively for the target distribution/architecture:

```sh
scripts/build-linux-server.sh
```

The tarball in `out/` contains the engine, its bundled non-system libraries,
supervisor and service files. System libraries, including glibc, remain provided
by the target OS. No original game graphics, sounds or data ship in this package.
Extract it to `/opt/keeper-enhanced-server`. Import your own **prepared matching
Keeper Enhanced game directory** (containing keeperfx.cfg and campaign/data
folders), rather than an unprepared original CD:

```sh
useradd --system --home-dir /var/lib/keeper-enhanced --create-home --shell /usr/sbin/nologin keeper-enhanced
install -d -m 750 -o keeper-enhanced -g keeper-enhanced /var/lib/keeper-enhanced
sudo -u keeper-enhanced python3 /opt/keeper-enhanced-server/import-data.py /path/to/prepared/GameData /var/lib/keeper-enhanced
install -d -m 750 /etc/keeper-enhanced
install -m 640 /opt/keeper-enhanced-server/server.json /etc/keeper-enhanced/server.json
chown root:keeper-enhanced /etc/keeper-enhanced/server.json
```

The importer refuses an existing GameData tree and symlink sources. Update
`engine_binary`, maps, player count, port and paths in server.json as needed.
The chosen map must have enough player positions. Linux builds and runtime must
use compatible dependencies; a macOS binary cannot be used on Linux.

## Visibility and optional password

Set `"visibility": "private"` for LAN or direct-address play. Set
`"visibility": "public"` to register in the directory; configure `endpoint` with
the reachable hostname and UDP port and `hub_url` with your PHP service URL.
The existing default URL is retained across the project rename.

Create `/etc/keeper-enhanced/server.env` with mode 600, root-owned. For a
password-protected server add:

```ini
KFX_SERVER_PASSWORD=replace-with-your-game-password
```

Omit it for an open server. Maximum password length: 31 UTF-8 bytes. For public
hosting also set `KFX_HUB_ADMIN_KEY` to the directory administrator key. Private
hosting needs no key. Secrets are passed through the service environment, not
command-line arguments, logs or server-directory fields. The listing only
indicates whether a password is required. Never commit this environment file.

Allow incoming **UDP 5556** (or the configured port) and forward it to the host
if behind NAT. LAN broadcast discovery uses UDP 5557 on the same subnet. HTTP
HAProxy configuration does not forward gameplay UDP. Public registration requires
outbound HTTPS to the directory. Direct players need no website account.

## Run

```sh
sudo -u keeper-enhanced python3 /opt/keeper-enhanced-server/dedicated.py /etc/keeper-enhanced/server.json --check
install -m 644 /opt/keeper-enhanced-server/keeper-enhanced-server.service /etc/systemd/system/
systemctl daemon-reload
systemctl enable --now keeper-enhanced-server
journalctl -u keeper-enhanced-server -f
```

The supervisor restarts per map, records local replays and supports graceful
systemd shutdown. Public servers optionally publish spectator streams with
`"spectators": true` on unprotected public servers; password-protected and
private servers always keep recordings local. One process
per writable profile/UDP port. Check status in the profile's dedicated/status.json.
The --check command validates configuration only; it does not establish external
UDP reachability or test a multiplayer match.
