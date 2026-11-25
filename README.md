# Keeper-Enhanced

[In-game tools](docs/POLISH-TOOLS.md) · [Website](https://keeperfx-e.retrocs.org) · [GitHub](https://github.com/retrocs-org/keeper-enhanced)

> **This repository is the native macOS fork of KeeperFX.**
> It carries the Apple Silicon + Intel macOS port, Universal build work,
> self-contained macOS dependencies, and Mac-specific runtime/FMV fixes.
> The upstream KeeperFX project is maintained at [dkfans/keeperfx](https://github.com/dkfans/keeper-enhanced).
> See [MACOS-CHANGES.md](MACOS-CHANGES.md) for the Mac port changelog, validation status and known limitations.

![macOS](https://img.shields.io/badge/macOS-13%2B-black?style=flat-square&logo=apple)
![Universal](https://img.shields.io/badge/Universal-arm64%20%2B%20x86__64-blue?style=flat-square)
![PRs welcome](https://img.shields.io/badge/PRs-welcome-brightgreen?style=flat-square)
[![Discord](https://img.shields.io/discord/480505152806191114?style=flat-square)](https://discord.gg/hE4p7vy2Hb)

[Upstream KeeperFX website](https://keeperfx.net) | [Keeper Klan Discord](https://discord.gg/hE4p7vy2Hb)

## Building

Compiled executables are named `keeper-enhanced` (`keeper-enhanced.exe` on Windows);
the heavy-log variant is `keeper-enhanced-hvlog`. Existing configuration filenames
and user-data directories remain compatible. Server refresh keeps a `keeperfx`
compatibility link for older server configurations.

CMake controls platform dependencies and compiler settings. Optional Make shortcuts:

```sh
make server-linux JOBS=4   # simulation-only server; no desktop multimedia libraries
make client-linux JOBS=4
make client-macos JOBS=4   # local universal app with imported BIN/CUE data, on macOS
make client-windows JOBS=4 # i686 MinGW-w64 toolchain required
make all JOBS=4            # native targets; Linux builds both server and client
```

`make` lists the targets. Each platform uses a separate build directory. Linux
and Windows shortcuts use GNU Make through CMake, without Ninja. The universal
macOS dependency pipeline currently requires Ninja and Xcode command-line tools.
`BUILD_TYPE=Debug` is supported by the Linux/Windows shortcuts; the universal
macOS pipeline always builds Release. Windows cross-compilation requires the
MinGW-w64 toolchain; macOS builds must run on macOS.

For a Linux server alone, use CMake directly:

```sh
cmake -S . -B out/server-linux -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release -DKFX_DEDICATED=ON -DKFX_INSTALL_GAME_DATA=OFF
cmake --build out/server-linux --target keeper-enhanced --parallel 4
out/server-linux/keeper-enhanced --version
```

`make package-server-linux` builds the portable server archive for first-time
installation. Building does not install or restart a service. See
[Linux server setup](docs/LINUX-SERVER.md) for dependencies, game-data import,
configuration and systemd installation. The original Windows Make recipes are
preserved in `Makefile.legacy` (`make -f Makefile.legacy standard`, for example).
No target includes original game assets.

## Community website

The complete PHP website and HTTP API are in `public-web/`; configure the deployed
`config.inc.php` and use `scripts/deploy-web.py` to refresh the vhost without
changing that config. See [MariaDB/vhost deployment](docs/MARIADB-DEPLOYMENT.md). Run `python3 services/keeperhub/dev.py`
to start it locally with KeeperHub. See [community setup and PHP hosting](docs/COMMUNITY.md)
for requirements and deployment configuration.

## Intro
KeeperFX (Dungeon Keeper Fan eXpansion) is an open-source project that aims to fix up, enhance and modernize
the classic dungeon management game, [Dungeon Keeper](https://en.wikipedia.org/wiki/Dungeon_Keeper).
This project is dedicated to providing an improved and customizable gaming experience while staying true to the spirit of the original game.

KeeperFX is a standalone game but requires a copy of the original game files as proof of ownership.
These files can be automatically copied from your old CDs, or from a digital edition like the ones from EA or GOG.

Originally, KeeperFX started out as a decompilation project, where we took the original game executables and reversed them back into usable code.
Currently the whole codebase of Dungeon Keeper is remade and all code has been rewritten.


## Features
- Native macOS 13+ support in this fork (Apple Silicon and Intel Universal build)
- Windows 7/10/11 support in upstream KeeperFX
- Higher screen resolutions
- Increased FPS, decoupled gfx and game logic
- Improved and modernized controls
- Many bugfixes
- Map, campaign and modding customizability
- Improved AI
- Modern multiplayer protocol
- Additional campaigns, maps, creatures and other content
- ...


## How to play

Installation instructions and a FAQ can be found on the [Github Wiki](https://github.com/dkfans/keeperfx/wiki).

You will need the original Dungeon Keeper files, either from an old CD or from the digital edition available on
[EA](https://www.ea.com/games/dungeon-keeper/dungeon-keeper),
[GOG](https://www.gog.com/game/dungeon_keeper)
or [Steam](https://store.steampowered.com/app/1996630/Dungeon_Keeper_Gold/).


## macOS build
For this fork's distributable macOS build, run:

```sh
macos/build-universal.sh
```

This builds a macOS 13+ Universal (`arm64 + x86_64`) KeeperFX executable with
the pinned third-party dependencies linked statically. The resulting release
binary has no Homebrew runtime dependency. Original Dungeon Keeper game data
is still required and is not included in this repository.

### In-game settings and save safety

The Mac app opens the game directly. Choose **Options → More Options** in the main menu or the dungeon's Options menu. **F9**, or an
unassigned right-stick click, opens the illustrated controller guide. The panel
supports mouse clicks, arrow keys, Tab, and controller D-pad/shoulder buttons.
Single-player pauses while the panel is open; multiplayer keeps running.

**Display** selects windowed or desktop fullscreen mode, window/render resolution,
and optional windowed cursor confinement. Choose **Apply Display**, then **Keep**
within 15 seconds. Esc, closing the panel or the timeout restores the previous
display; unconfirmed choices are never saved. Windowed mode leaves the cursor free by default; the settings
panel always releases it. Fullscreen uses the display's desktop resolution.

**Controls** adjusts pointer speed, stick dead zone, vibration strength, menu
confirm/back buttons and Xbox/DualSense labels. **Remap** records physical
buttons, triggers, stick directions and chords without changing keyboard
bindings. Press Enter or Start to save a recording, or Esc to cancel. Selected
controls use cream text. Hold the D-pad to scroll; shoulder buttons switch
sections. Prompts follow Xbox/DualSense and custom confirm/back choices.

**Performance** provides 30/60/120 FPS and unlimited presentation caps, a frame
timing overlay, Original/Enhanced scaling and rotating single-player autosaves.
Simulation speed stays unchanged. Options are saved in `save/macos-options.cfg`.

**Saves** lists save slots with title/campaign/level search and manual/autosave
filters. Select a slot, then its current save or one of three grouped backups.
Preview, campaign, level, date and level playtime accompany new saves; **Large
preview** expands the image. Older saves remain available without previews or
playtime. Backups restore into a free slot before loading, and failed save writes
preserve the current save. Autosaves never run in multiplayer.

**Network** runs a nonblocking lobby/UDP-port check and displays connection status,
lobby ping, the last lobby error and version comparison. **Join by IP** accepts
an IPv4 address with an optional port (default UDP 5556); the host must have a LAN
lobby open. Enter another player's three- or four-part game version to compare.
Matching versions also require compatible mods. Connection diagnostics do not
verify inbound peer reachability; a full host/join test requires another player.

**Access** adjusts settings/caption text (100–150%), cursor/hand size (100–200%),
mouse edge scrolling and its border, mentor event captions, and caption duration.
Event captions describe built-in mentor cues independently of voice volume and
appear above the play area so objective text remains visible. Custom speech and
movie dialogue require supplied subtitle text. These options persist separately
from game saves in `save/macos-options.cfg`.

The integrated menus reuse KeeperFX's own menu font, palette, button artwork and
panel texture, with a highlighted selection for keyboard and controller use.

On Mac, windowed play supports resizing with the chosen aspect ratio. Command-Q
asks before discarding single-player progress. Controller disconnects release
held clicks and open the local pause menu; multiplayer continues.

### Gamepad controls (Xbox and DualSense)

Connect a supported controller by USB or pair it in macOS Bluetooth settings.
Controllers may be connected before or during play. The in-game **Controller**
tab shows detection, and **Button guide** illustrates the default layout. Only one controller drives the game
at a time; another connected pad is selected when the active pad disconnects.

| Action | Xbox | DualSense |
| --- | --- | --- |
| Move camera | Left stick | Left stick |
| Move pointer | Right stick | Right stick |
| Primary click / drag | RT | R2 |
| Secondary click / drop / slap | LT | L2 |
| Navigate interface buttons | D-pad | D-pad |
| Rotate / zoom | Hold A + D-pad | Hold Cross + D-pad |
| Faster camera | Hold A | Hold Cross |
| Dungeon Heart | Y | Triangle |
| Jump to a fight | X | Square |
| Previous / next possessed ability | LB / RB | L1 / R1 |
| Pause menu | Menu | Options |

Keyboard and mouse remain available. Custom controller bindings in
`~/Library/Application Support/keeperfx/keeperfx/save/settings.toml` are preserved.
Bluetooth/USB availability depends on the particular controller model, firmware,
and macOS support. Adaptive triggers and gyro aiming are not implemented.

### Mac app and installer

The Mac app requests your original Dungeon Keeper files on first run.
Choose the `KEEPER` folder from your mounted CD or an existing installation.
The installer does not include the required CD data or original cinematics.

The app then opens the game directly. Use the game menus for settings and
the integrated controller, performance and save browser. The bootstrap creates
a writable game-data folder at
`~/Library/Application Support/keeperfx/keeperfx/GameData`; the signed app remains
unchanged. Existing saves remain in the adjacent `save` folder.

Build and package with a Developer ID Application identity and an existing
notarytool credential profile:

```sh
macos/build-universal.sh
KFX_SIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)" \
KFX_NOTARY_PROFILE="your-saved-profile" macos/package-app.sh
```

`KFX_GAME_DIR` selects a complete KeeperFX runtime-data directory (defaults to
`.local/keeper-enhanced`). `KFX_PACKAGE_DIR` selects a new output directory. Packaging
signs the engine, connection tester and launcher, notarizes/staples both app and
DMG, and verifies their signatures and disk-image checksum. Without
`KFX_NOTARY_PROFILE`, it produces a signed package without notarization.

The standalone build directory contains only the engine. For developer use,
`macos/play.command` starts the prepared `.local/keeper-enhanced` runtime.

See [MACOS-CHANGES.md](MACOS-CHANGES.md) for validation details and the current
Mac-specific change history.

## Development
To get started with upstream KeeperFX development, refer to the [Development Guide](https://github.com/dkfans/keeperfx/wiki/Building-KeeperFX) for
detailed instructions on setting up a development environment and building KeeperFX from source.

If you wish to discuss development, you can join the [Keeper Klan discord](https://discord.gg/hE4p7vy2Hb) and ask to
be added to the KeeperFX development channel.


## Components
| Component | Language | Info |
|---|---|---|
| [KeeperFX](https://github.com/dkfans/keeper-enhanced) | C, C++ | - |
| [Launcher](https://github.com/dkfans/keeperfx-launcherwx) | C++ | Official Launcher to edit settings and start the game with run options. |
| [FXGraphics](https://github.com/dkfans/FXGraphics) | - | Sources of KeeperFX graphics files. |
| [FXSounds](https://github.com/dkfans/FXsounds) | - | Sources of KeeperFX audio files. |
| [Masterserver](https://github.com/dkfans/keeperfx-masterserver) | PHP (CLI) | Multiplayer masterserver. Allows players to easily find public lobbies of others. |
| [Website](https://github.com/dkfans/keeperfx-website) | PHP | https://keeperfx.net |


## Tools
| Tool | Usage |
|---|---|
| sndbanker | Makes usable ingame sounds from SFX archives. |
| po2ngdat | Converts `.po` files (language) to `.dat`. |
| png2bestpal | Decides the best in-game color palette for an image and creates a `.pal` file. |
| png2ico | Converts `.png` files to `.ico`. |
| pngpal2raw | Creates a `.raw` image file that can be used by the game from a `.png` and a `.pal` (palette) file. The palette file can be created with _png2bestpal_. |
| rnctools | Handles the RNC compression of many original DK data files. |


## Further Improvements
KeeperFX could be further improved in these key areas:
- Multiplayer performance and features
- Expand and improve AI / Computer player behavior
- Improve pathfinding performance
- Expand creative freedom for modders even further
- Native cross-platform support
- Improve code readability and maintainability
- Lua support
- ...


## Contributing
We welcome contributions from the community to improve and expand KeeperFX.
- Report bugs by opening [issues](https://github.com/dkfans/keeperfx/issues).
- Submit feature requests and discuss potential improvements.
- Contribute code by creating pull requests.


## Code Signing Policy
Free code signing provided by [SignPath.io](https://about.signpath.io/), certificate by [SignPath Foundation](https://signpath.org/).


## License
This project is licensed under the [GNU General Public License v2.0](LICENSE).
Feel free to use, modify, and distribute it according to the terms of this license.

Release preparation uses `python3 scripts/release.py`; see [Releasing](docs/RELEASING.md). The app keeps the existing KeeperFX data directory and engine/save formats.

### Menu polish and diagnostics

More Options uses the original 100 ms, eight-frame button highlight for mouse
hover and keyboard/controller focus. Back returns to the parent and keeps its
selection. The old in-game Graphics resolution icon opens Display Options
directly instead of cycling identical modes.

Controller labels default to Automatic; you can choose Xbox or DualSense in
Controller Options. The illustrated guide reads the current menu and gameplay
bindings, including custom remaps. Saved Games shows the last automatic save
and has Saving Help (H on keyboard or Start on controller). Save feedback is
independent of mentor captions; the help page opens the save folder and backups.

Network Options edits the remembered player name and host address. Connection
Test has Copy Report. About & Diagnostics shows app/build/engine versions and
the active controller, opens logs/saves in Finder, and copies a support report.
Window position is remembered separately from render resolution; monitor
changes fit the window onto a usable display, and focus loss releases the cursor.

### Controller camera controls

Hold **A (Xbox)** or **Cross (DualSense)** and press **D-pad Up/Down** to zoom
in/out, or **D-pad Left/Right** to rotate the camera. These are the existing
ZoomIn/ZoomOut/RotateCW/RotateCCW bindings and can be changed in Remap.
The Controller Guide now cycles through its illustration, general controls and
Camera Controls, displaying your current bindings rather than fixed defaults.

## Optional community networking

The game includes a server browser, browser-paired accounts and access to local
community services. LAN and direct connections remain available without accounts.
Run `python3 services/keeperhub/dev.py` for the local website and directory.
See [community setup and headless hosting](docs/COMMUNITY.md) for accounts, dedicated servers
configuration, verified results, tournaments, challenges and live spectating.
Headless hosting currently uses an AI slot and supports up to three humans.

The project is now named **keeper-enhanced** (Keeper Enhanced), formerly KeeperFX-E.
Existing repository/service URLs, database names and save paths remain compatible.
See [Linux dedicated-server setup](docs/LINUX-SERVER.md) for private or public hosting
with optional passwords. Relay connections are no longer supported.

## Local macOS app with your game data

Place your own CUE and matching BIN tracks (or ISO images) in `import/`.
Both single-BIN mixed-mode and separate-track MODE1/2352 CUE layouts are supported.
When multiple discs are present, choose one explicitly (no edition is guessed):

```sh
KFX_IMPORT_IMAGE="Dungeon Keeper.cue" make client-macos JOBS=4
# Or choose the Gold Edition CUE filename instead.
```

`make client-macos JOBS=4` imports the required original data and movies, builds
the universal client, and creates an ad-hoc signed app/DMG/ZIP under
`outputs/local-client-<timestamp>/`. The app includes the imported originals and
starts without an import prompt. CD audio tracks are imported losslessly as 44.1 kHz stereo WAV files.
Both multi-BIN and single-BIN CUE layouts preserve track order and omit pregaps.
Plain ISO files contain no CD audio; use CUE/BIN to include the soundtrack.

Fresh macOS imports use the prebuilt open runtime in `assets/runtime/`.
The Git checkout supplies it, so the build skips asset downloads and conversion.
Your own disc supplies the original data and music. A plain `make client-macos`
reuses `.local/imported-client` when available. Set `KFX_IMPORT_IMAGE` to force
a fresh disc import, or `KFX_GAME_DIR=/path/to/runtime` to choose a different
prepared base. If the checked-in runtime is missing, the importer can generate
a fallback under `.local/generated-runtime`. `macos/build-universal.sh` is
compile-only.

Set `KFX_SPRITE_ARCHIVES_DIR=/path/to/fxdata` to include the nine multiplayer
sprite archives from a matching local runtime during disc import. Use the same
archives on the Mac and when running `make import-server IMAGE=...` on Linux.
The engine verifies that peers use matching sprite data.

`import/`, `.local/` and `outputs/` are Git-ignored. Source export/push policy also
rejects the entire import directory. Imported packages are for local use; public
release packaging continues to strip original data and reject data-bearing apps.
No Developer ID or notarization is used by the local build.

The main configuration is `keeper-enhanced.cfg`. The engine still reads legacy
`keeperfx.cfg` when the new file is absent. Importers write the new name; the
macOS launcher copies existing legacy user settings to the new file without
overwriting an existing new configuration.

## Import with Make

```sh
make import IMAGE="Dungeon Keeper.cue"
make import-server
# Optional original-disc overlay when preparing server data on Linux:
make import-server IMPORT_BASE=/path/to/prepared/runtime IMAGE="/home/frossmant/Dungeon Keeper.cue"
```

`make import` (also `make import-client`) includes data, movies and lossless CD
music in `.local/imported-client`. `make import-server` reduces the prepared
runtime to `.local/linux-server-import/GameData`. It skips CD audio extraction
when an IMAGE is supplied and excludes standalone music and movies.

`IMPORT_BASE` defaults to `assets/runtime/`, with local generation as a fallback.
A custom `IMPORT_BASE` must already contain a matching prepared runtime. `IMPORT_DIR` defaults to `import/`;
absolute IMAGE paths can point to discs elsewhere. Choose a fresh `IMPORT_OUTPUT`
to change the destination. Existing destinations are never overwritten. Imports
stay local and do not install services or transfer files to a server. For normal local app builds, `KFX_IMPORT_IMAGE="Dungeon Keeper.cue" make client-macos`
continues to import automatically.

### Full import, then server-only staging

```sh
make import IMAGE="Dungeon Keeper.cue"
make server-linux JOBS=4
```

The first command writes `.local/imported-client`, including soundtrack and movies.
The second regenerates minimal data at `out/server-linux-data/GameData`, then
compiles the dedicated Linux binary. Use `make server-data` to stage the data on
macOS before transferring it to Linux. `SERVER_DATA_BASE=/path/to/full/import`
selects another full import. No CD image or audio extraction is needed on rebuild.

The generated staging tree is disposable, not a live server profile. Refresh
refuses to replace a tree containing saves, recordings or dedicated-server state.
Installed data under `/var/lib/keeper-enhanced` is never changed by these targets.
The refresh script and portable binary packaging remain build-only and keep an
existing installed server's data intact.

### Cleaning

`make clean` removes standard generated build directories (`build-*`, `out`),
packages (`outputs`, `bin`, `obj`, `pkg`), generated legacy asset directories
(`sdl`, `gfx`, `sfx`) and local imported runtime copies (`.local/imported-client`,
`.local/linux-server-import`, `.local/keeperfx`, `.local/generated-runtime`,
`.local/runtime-cache`, and `tools/local/`). Use `make clean-preview` to list
paths first. No cleanup is performed merely by updating this Makefile.

Original discs in `import/`, tracked CMake source in `build/`, Git history,
third-party source checkouts and installed profiles are retained. Custom output
paths outside these standard directories are not removed. Local imports that
contain saves or recordings stop cleanup rather than lose personal data.
After cleaning, rerun `make import`; native tools and runtime data are rebuilt automatically.

### Fresh-checkout import

`make import` and `make import-server` use the prebuilt open runtime in
`assets/runtime/` and read original game data from your own BIN/CUE. The client
import also copies movies and lossless CD audio. Runtime provenance and licenses
are recorded alongside the prebuilt files. Graphics and sound source assets
are vendored in `assets/upstream/` with their GPLv3 licenses and credits.

When the checked-in runtime is unavailable, the importer generates a base under
`.local/generated-runtime` from pinned converter, PNG/zlib and KeeperFX sources. All
converter executables are built natively; no system-wide helper installation is
needed. A C/C++ compiler, CMake, Make, Git and Python 3 are prerequisites. The
first conversion can take several minutes; later runs reuse generated files.
With pinned tool/source checkouts already cached, use `OFFLINE=1` to prevent
fetching missing source inputs. Missing offline inputs produce an explicit error.
A custom `IMPORT_BASE` can still provide an already prepared matching runtime.

### Local external tools

`make tools` builds pngpal2raw, png2bestpal, po2ngdat, sndbanker, rnc and dernc
natively under `tools/local/bin/`, including local static PNG/zlib dependencies.
The importer builds these tools when generating a runtime base from source.
Tool downloads, executable, license and version metadata stay in `tools/local/`,
which is Git-ignored and excluded by source-push policy. CMake's downloaded
Windows icon helper also uses that prefix. `make clean` removes this entire
local prefix, preserving tracked `tools/CMakeLists.txt` and `tools/fxfontmaker`.
The system compiler, Python, CMake and Make are still normal build prerequisites.
The older prebuilt-bundle bootstrap remains an explicit standalone fallback script;
it is not used by the default import flow.

## Third-party asset credits

The source assets from [FXGraphics](https://github.com/dkfans/FXGraphics) and
[FXsounds](https://github.com/dkfans/FXsounds) are vendored in
`assets/upstream/fxgraphics/` and `assets/upstream/fxsounds/`. Both repositories
carry GPLv3 license files, preserved alongside their source assets. These assets
are credited to their upstream contributors, not the Keeper Enhanced team.
See [credits.md](credits.md) for revisions, licenses and attribution.

The source-only policy allows these pinned asset trees and the prebuilt open
runtime under `assets/runtime/`. Original disc images, disc-derived files,
locally generated runtime copies, and downloaded tools stay outside Git.
`make clean` preserves the repository asset trees.
