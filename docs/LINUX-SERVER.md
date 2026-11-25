# Keeper Enhanced dedicated server on Linux

Supports private direct/LAN hosting and public directory-listed hosting. Either
mode can require a password. One synchronized AI host occupies a player slot,
leaving up to three humans; this is a lockstep host, not a new authoritative
network protocol. No relay is used. Private mode makes no community HTTP calls,
publishes no server listing or spectator stream, and needs no community key.

Source: https://github.com/retrocs-org/keeper-enhanced (existing repository URL).

## Import from an original BIN/CUE disc

A fresh checkout no longer requires a manually prepared runtime:

```sh
make import
make server-linux JOBS=4
```

Place the BIN/CUE files in `import/` before running these commands. If there are
multiple CUE/ISO images, select one with `IMAGE="Dungeon Keeper.cue"`.
The missing default runtime base is generated locally from the vendored GPL
asset sources and pinned KeeperFX configuration/map/translation sources. Native
conversion tools and their PNG/zlib dependencies are built under `tools/local/`;
no prebuilt runtime bundle or Windows executable is downloaded. Your CD supplies original data,
movies and music. Dedicated-server staging excludes standalone music and movies.
All required conversion tools are built automatically under `tools/local/`.
All imported and downloaded assets remain local and Git-ignored.

## Build and import

The dedicated target excludes SDL_image/mixer, OpenAL, FFmpeg, OpenGL, desktop
window backends, controller devices, and desktop matchmaking/NAT discovery. It
uses a statically built SDL core for the existing timing/thread/filesystem and
legacy in-memory surface APIs; all video/audio/render/device drivers are disabled.
The PNG parser is built privately for shared custom-animation metadata; no system
`spng` development package is needed. Presentation frames are never rendered.

On openSUSE Leap 16.0 x86_64, install the toolchain and remaining development
libraries from your configured repositories:

```sh
zypper install --no-recommends gcc gcc-c++ cmake make git python3 patchelf pkgconf-pkg-config \
  'pkgconfig(libcurl)' 'pkgconfig(luajit)' 'pkgconfig(minizip)' 'pkgconfig(zlib)' 'pkgconfig(openssl)'
```

Python 3.9+, CMake 3.20+ and a C++20 compiler are needed. CMake fetches pinned
build-only SDL core, PNG-metadata and engine-support sources into the build
directory. No original game assets or desktop dependency packages are fetched.
Ninja is not required. Do not remove other installed packages automatically;
other applications may use them.

Build natively for the target distribution/architecture:

```sh
make package-server-linux JOBS=4
```

The tarball in `out/` contains the engine, its bundled non-system libraries,
supervisor and service files. System libraries, including glibc, remain provided
by the target OS. No original game graphics, sounds or data ship in this package.
Extract it to `/opt/keeper-enhanced-server`. Import your own **prepared matching
Keeper Enhanced game directory** (containing keeper-enhanced.cfg and campaign/data
folders), rather than an unprepared original CD:

```sh
useradd --system --home-dir /var/lib/keeper-enhanced --create-home --shell /usr/sbin/nologin keeper-enhanced
install -d -m 750 -o keeper-enhanced -g keeper-enhanced /var/lib/keeper-enhanced
sudo -u keeper-enhanced python3 /opt/keeper-enhanced-server/import-data.py /path/to/prepared/GameData /var/lib/keeper-enhanced
install -d -m 750 -o root -g keeper-enhanced /etc/keeper-enhanced
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


## Refresh an existing installation

Run `scripts/refresh-keeper-enhanced.sh` as root on the Linux host. It defaults to
`/root/keeper-enhanced` on branch `main`; override with `KFX_SERVER_SOURCE` if your
checkout has another location. A missing checkout is cloned automatically. Each run
fetches GitHub `main` directly and fast-forwards the clean local `main` branch;
local changes or unpublished commits stop the refresh without resetting them.
Source is fetched even when first-time service installation is still required. Build dependencies must already be installed, including `make`. The scripts use
CMake’s Unix Makefiles generator without Ninja. Refresh uses `out/server-linux`,
the same directory as `make server-linux`, with `KFX_DEDICATED=ON`. It additionally
enables `KFX_DECK_BUNDLE=ON` to stage the runtime libraries for installation.
Earlier build directories remain untouched. Set `KFX_JOBS=4` to control parallelism.
The staged binary must pass `--version` before the running service is stopped.

```sh
curl -fsSL https://raw.githubusercontent.com/retrocs-org/keeper-enhanced/main/scripts/refresh-keeper-enhanced.sh \
  -o /root/refresh-keeper-enhanced.sh
chmod 700 /root/refresh-keeper-enhanced.sh
/root/refresh-keeper-enhanced.sh
```

The current service runs while the updated source builds. The script stages the
engine and its bundled libraries together, validates the supervisor/configuration,
backs up the installed bundle, configuration, environment file and unit under
`/var/backups/keeper-enhanced-server`, then stops the service and swaps bundles.
A fresh engine RPC status and stable supervisor PID are required before success.
Startup failure restores the previous bundle and prior running/stopped state.

The live `server.json`, `server.env`, imported GameData, saves and existing systemd
unit/drop-ins stay in place. No database changes, asset download or data import
run during refresh. The standard `/opt/keeper-enhanced-server` and
`/var/lib/keeper-enhanced` layout is required; custom paths are rejected rather
than overwritten. An active match is interrupted during the final restart.
Linux/Leap runtime validation is still pending; swap/rollback behavior is covered
by isolated tests with simulated builds and systemd.

## Minimal server-data import

`import-data.py` now defaults to a minimal import. It retains `keeper-enhanced.cfg` and
`data`, `fxdata`, `ldata`, `creatrs`, `campgns`, `levels`, `multiplayer`, `mods`.
Standalone music, sound banks, movies, executables, saves and session credentials
are excluded. Legacy binary graphics/bootstrap files remain necessary. Complete
mod media and map companions are retained for matching multiplayer content hashes.
Use `--full` for campaigns referencing nonstandard directories. `--dry-run` reports
selected bytes without writing anything. Existing GameData is never overwritten.

```sh
python3 linux/import-data.py /path/to/prepared/GameData /path/to/server-profile --dry-run
python3 linux/import-data.py /path/to/prepared/GameData /path/to/server-profile
```

This is a conservative media reduction, not a data-free server. Minimal imports
have fixture coverage; a full dedicated multiplayer match with reduced data is
still pending validation.

Source-generated runtimes import original map layouts from the disc. They keep
the original campaign and original multiplayer maps; optional community packs
without supplied layout data are omitted from the menus. The server template
uses the original multiplayer pack, map 52. Existing configurations are preserved;
select a pack/map actually present in your imported runtime.
