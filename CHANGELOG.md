# KeeperFX-E changes

## 1.4.0

### Features
- Check saved-game engine versions and content before loading; preserve a
  content-addressed copy and record campaign/map/configuration/mod requirements.
- Add offline replay pause, 0.5x–4x speed, persistent bookmarks and a local file picker.
- Add restart-safe content profiles with separate runtimes, saves and settings.
- Add service/relay readiness and requester-pinned incoming UDP connection tests.
- Add an optional community service and original-style in-game server browser with
  Internet, LAN, favourites, recent connections, search, sorting and engine filters.
- Add hostname/IPv6 direct joining, password entry, no-slot latency queries and
  authenticated ENet relay fallback for community-listed hosts.
- Add headless hosting with map rotation, one synchronized AI and up to three
  human players, plus buffered live spectators that consume no player slots.
- Add browser-paired accounts, private/public website profiles, friends, match
  history, verified statistics, achievements, events, tournament brackets,
  cooperative challenge completions and checksum-verified replay downloads.
- Preserve guest LAN/direct play and access to the original public lobby service.
- Native universal macOS app with integrated display, controller, performance,
  saves, remapping, guide, network and accessibility sections.
- DualSense and Xbox controller support, configurable physical button bindings,
  illustrated controls and adjustable stick, pointer and vibration settings.
- Atomic saves, rotating backups, previews, search and local automatic saves.
- Connection diagnostics, version comparison and direct IPv4 joining.

### Enhancements
- Remove all five hosted GitHub Actions workflows; CI/runners are deferred.
- Make source-only macOS builds fetch pinned vendor source and packaging art locally.
- Add Developer ID signing preflight before package assembly.
- Add bounded frame-time/CPU/memory capture and sleeping macOS frame waits.
- Limit interpolated menus to 60 FPS and reduce background/minimized dungeon drawing.
- Add live controller testing and resting-stick drift calibration with cancellation.
- Handle Retina scale changes and window restoration through existing display recovery.
- Record bounded desync/disconnect diagnostics without player credentials.
- Add password-confirmed offline recovery codes, session revocation and verified SQLite backups.
- Show checksum-verified published release downloads from a private website cache.
- Add a new KeeperFX-E landing page and PHP-hosted community website, with a
  same-origin API gateway and documented Apache/Nginx deployment.
- Rename the application, website branding and release packages to KeeperFX-E.
- Configure the retrocs-org GitHub/GitLab repositories and project website.
- Select and remember an active controller, with automatic Xbox/DualSense labels.
- Rename saves, protect favourites, preview backups and move save groups to Trash.
- Check selected multiplayer map, engine and mod content before a host starts.
- Hold an unassigned R3 during dungeon play for the controller quick-action wheel.
- Add large-text, comfort and reduced-camera-movement accessibility presets.
- Add a controller camera guide with current zoom and rotation bindings.
- Match original animated menu borders for mouse, keyboard and controller focus,
  with consistent large text and parent navigation.
- Detect controller button labels automatically and show current custom bindings.
- Add independent save feedback, last-autosave status and recovery help.
- Remember host addresses and expose persisted player names, with copyable reports.
- Add in-game About & Diagnostics, version/controller details and log/save access.
- Remember window position and fit windows after monitor changes.
- Restore original menu artwork, large gold buttons and readable native text.
- Keep the existing KeeperFX save/settings location when upgrading to Enhanced.
- Add Beam68-style build/check/prepare/publish stages, source provenance,
  checksum-verified DMG/ZIP packages and GitHub/GitLab publication receipts.
- Sign, notarize and staple the universal application and DMG.

### Bugs
- Keep distinct campaign/map-pack identities in save and replay metadata.
- Fix overlapping replay filename copies and preserve dedicated-AI replay state.
- Keep simultaneous tab/row highlight animations independent.
- Prevent early departures from becoming verified victories.
- Open Display Options directly from the in-game Graphics resolution icon.
- Release the pointer when the settings window loses focus or crosses a letterbox.
- Render the original cursor above the settings panel and release windowed
  cursor confinement by default.
- Confirm display changes with Keep/Revert and a 15-second rollback timeout.
- Bound stalled lobby requests and preserve existing saves during recovery.
