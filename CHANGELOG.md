# Keeper Enhanced changelog

This changelog covers changes made by the Keeper Enhanced fork. Upstream KeeperFX
changes remain credited to the KeeperFX contributors. Version **0.1** uses
**0.1.0** in package metadata; the underlying engine/network version remains
**1.4.0.0** for compatibility. This is the initial release line, not a claim that
packages have already been published.

## 0.1.0

### Features
- Vendor credited GPLv3 FXGraphics/FXsounds source assets and preserve their licenses.
- Generate runtime assets locally using native tools and import original BIN/CUE
  game data, movies and lossless CD music.
- Native macOS application and Universal build workflow for Apple Silicon and Intel.
- In-game sections for display, controller, performance, save and network settings.
- Xbox and DualSense remapping, stick calibration, illustrated controller guides,
  and camera zoom/rotation controls.
- Content profiles with separate runtime files, settings and saves.
- Replay organization, playback controls, bookmarks and export tools.
- Text/cursor sizing, captions, reduced damage flashes and performance diagnostics.

### Fixes
- Start at the main menu with soundtrack playback instead of forcing the Setup Guide.
- Keep intro playback in the selected display mode; migrate the main configuration
  to keeper-enhanced.cfg while preserving legacy settings.
- Improve save locations, backup browsing, compatibility checks and nondestructive
  restoration into a free save slot.
- Restore readable menu layouts, original-style highlights and pointer visibility.
- Correct in-game options navigation and add resolution/fullscreen configuration.
- Make windowed cursor confinement optional and recover unconfirmed display changes.
- Retry community requests through the PHP front controller when clean routes
  return an Apache HTML 404, retaining request bodies and authentication.
- Hide private account actions until a session is verified, including logout and
  unavailable-service handling.

### Multiplayer and dedicated servers
- Support LAN, private direct connections and publicly listed servers. Remove
  relay joining and automatic relay fallback.
- Add private/public dedicated-host modes, each with optional password protection.
  Private hosts do not contact the community directory or publish spectator streams.
- Add a simulation-only build mode excluding desktop window/render backends,
  OpenGL, OpenAL, SDL_image/mixer, FFmpeg and controller devices.
- Retain a static SDL core for legacy timing/thread/filesystem/in-memory surface
  APIs, and privately build the parser needed for custom-animation PNG metadata.
  No system SDL multimedia, FFmpeg, OpenAL or spng development packages are required.
- Keep directory heartbeats, account identity and readiness checks running without
  UI frames; enable the shared content-check implementation in Linux server builds.
- Add native Linux packaging, prepared-data import and a systemd service template.
- Add a GNU Make refresh script with staged engine/library replacement, preserved
  configuration/secrets/data, fresh engine readiness checks and rollback.

### Website and services
- Keep the complete PHP website, HTTP API and configuration template in public-web.
- Add accounts, pairing, public server browsing, match records, events, challenges,
  replays and an unlisted developer API browser.
- Rank only opted-in public profiles with completed, server-verified matches.
- Add MariaDB configuration, CLI schema initialization and safe connection checks.
- Preserve deployed config.inc.php during website refreshes.
- Add a compact Dungeon Keeper → KeeperFX → KeeperFX Enhanced features timeline,
  short feature sections and a Steam link for the original game.

### Packaging and source management
- Add local source-push and macOS release/signing/notarization tooling.
- Exclude imported original game graphics, sounds and data from source publication;
  use the import tools with the user's own game files.
- Rename package/project branding to keeper-enhanced while preserving existing
  repository/service URLs, database names, saves and application identity.
- Keep release notes generated from this changelog. CI/runners remain deferred.

### Verification and current limits
- The simulation-only target has been compiled and tested on macOS with a normal
  game client: direct joining, dedicated AI, content readiness, account identity
  and buffered replay spectating. Live openSUSE Leap validation remains pending.
- Dedicated hosting uses the existing lockstep protocol: one synchronized AI host
  leaves up to three human slots. It is not a new authoritative simulation protocol.
- Original Dungeon Keeper data is required and is not distributed by this project.

Earlier working notes are preserved in [development history](docs/DEVELOPMENT-HISTORY.md).
