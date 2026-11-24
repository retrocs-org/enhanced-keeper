# Mac regression checks

Run `python3 macos/tests/test-importer-fixtures.py` after packaging. It exercises
uppercase CD layouts, missing/empty-file rejection, non-destructive imports,
versioned asset refresh, and preservation of user configuration and caches.

`test-matchmaking.c` links against the real `net_matchmaking.c.o` from a native
build plus bundled SDL/curl/OpenSSL. With no argument it checks a live list
request. Given a local port it makes two attempts against a deliberately stalled
HTTP/WebSocket server and requires each to report a connection error within
6.5 seconds. No lobby is published by this test.

`test-movies.c` links against the staged FFmpeg archives and takes a Smacker movie
path. It requires the decoder for every stream to open and produce frames.
Original game media is not included with these tests.

`python3 macos/tests/run-controller-tests.py` builds and runs the controller
regression check against the real engine input objects. It uses virtual Sony and
Microsoft gamepads and disables physical drivers only inside the test process.
Coverage includes hot-plug, face/menu/shoulder/D-pad mapping, trigger clicks,
smooth stick dead zones and full range, unrelated-controller input/removal,
failover, focus clearing and release of synthetic mouse clicks after disconnect.
Physical DualSense and transport-specific behavior still require hardware testing.

`test-save-transactions.c` compiles with `src/macos_savefile.c` and checks atomic
replacement, three-version backup rotation and preservation of the existing save
when a temporary save is missing or empty. `test-launcher-bindings.m` includes the
real native launcher writer and checks that remapping retains keyboard modifiers,
music settings and other TOML sections. Link the latter with Cocoa and ARC.

The expanded controller regression also checks menu confirm/back and transition
back into gameplay, plus cursor-speed validation. Live integration additionally
exercised manual saves, autosave rotation, loading/resaving, MP4 playback and
original-movie fallback in a separate KFX_USER_DATA_DIR profile.


`python3 macos/tests/run-ingame-options-tests.py` compiles the real in-game panel
with renderer/game boundary stubs and checks persistent settings, pause ownership,
multiplayer behavior, physical-button chord recording, old save metadata, and
nondestructive backup restoration. It renders all eight pages to `/tmp` PNGs for
visual inspection. Physical DualSense USB/Bluetooth testing remains outstanding.

Display checks also cover cursor-grab policy, persisted mode/resolution, failed-mode
fallback and keyboard/controller access to Apply Display.

`python3 macos/tests/test-display-integration.py` is the optional live display check.
It opens isolated QA game windows and switches to desktop fullscreen temporarily.
It requires imported original data in `.local/keeperfx`; use `KFX_TEST_APP` to
select a packaged app (default `/Applications/KeeperFX.app`). It verifies actual
rendered dimensions, repeated live mode changes, persistence and frontend restart.
Existing game saves and preferences are not used.


The expanded in-game check covers keep/revert/timeout/close display behavior,
unsaved provisional display preferences, all confirm/back face combinations,
controller-repeat ownership/release, grouped save search/filtering, orphaned
backups, accessibility persistence, endpoint/version validation, and nonblocking
helper output using an executable fixture. Live display integration also waits
for the real 15-second rollback. `mac.options <page> capture` writes a developer
preview to the active profile's `save/options-preview.png` using real game assets.
`test-matchmaking.c` additionally checks retained diagnostic history after the
pending error notification is consumed. It does not publish or join a lobby.

The in-game options check also covers section/parent navigation, the guide
legend, and cursor composition above the exclusive image at three window
aspect ratios. Native-art preview captures supplement the stubbed renderer.

`python3 macos/tests/test-release-tool.py` runs 22 offline release acceptance
checks adapted from Beam68. A real signed build separately verifies extracted
DMG/ZIP app identity, universal architectures, notarization and Gatekeeper.

Menu polish regressions cover the real legacy resolution callback, native
animation cadence, active-device Auto labels, remembered endpoint validation,
name edits, parent/focus navigation, save feedback, report contents, file URLs
and offscreen/oversized window fitting. Virtual-pad checks verify the device
identity exposed to the UI. `test-save-feedback-integration.py` exercises an
actual successful save/preview and a blocked temporary write in an isolated
profile; it preserves the earlier save and captures Saving Help. The optional
live display check also checks that window placement is persisted. Real monitor
unplugging and physical controller transport tests remain separate hardware QA.

Run `python3 macos/tests/run-physical-controller-check.py` explicitly for a
20-second hardware input recording. Move both sticks, both triggers and press
face buttons while it runs. The checker initialises SDL video like the game,
which is necessary for macOS GameController event delivery. A physical Bluetooth
Xbox Series X check recorded 15 button presses and all six axes for this update;
physical DualSense testing remains separate.

Camera-guide tests use the real default binding table and game key matcher to
check all four A/Cross + D-pad combinations, missing-modifier rejection and a
custom zoom remap, plus the complete three-page guide navigation cycle.

Community service checks: `python3 macos/tests/test-native-php-api.py` exercises the native PHP account
pairing/privacy, directory ownership/expiry, authenticated UDP relay, verified
results, tournament advancement, stream offsets, cookies and CSRF.
`python3 macos/tests/test-community-integration.py` starts a real headless engine,
a player joining through automatic relay fallback and a real spectator, all on
loopback with isolated game profiles. It requires imported assets and a built
engine and stops only its own processes.

## Save checks, content profiles, replay playback and connectivity (manual)

```sh
python3 macos/tests/check-play-tools.py
python3 macos/tests/run-ingame-options-tests.py
python3 macos/tests/test-connectivity.py
KFX_TEST_BOOTSTRAP=/path/to/new/KeeperFXE python3 macos/tests/test-importer-fixtures.py
python3 macos/tests/test-playback-integration.py
```

Set `KFX_MAC_DEPS_ROOT` if using a separate dependency cache. The native tests
exercise version/content mismatches, immutable copies, bookmark persistence and
network isolation. The profile importer fixture checks overlay updates/removal
and rejects linked destinations. The real playback test uses a temporary imported
profile to save/reload a dungeon, record a replay and compare world checksums at
a bookmark reached at different speeds. It also verifies that pause holds turns
and the replay controls do not alter recorded simulation state. Imported assets
and captured files remain local. These commands are not hosted CI jobs.

Manual enhancement checks (no CI/runner):

- `python3 macos/tests/check-network-resume.py`: real loopback ENet handshake,
  forged-key rejection, same-slot packet routing and reservation expiry.
- `KFX_TEST_RECONNECT=1 python3 macos/tests/test-community-integration.py`: real
  dedicated host, relay join, 35-second client suspension and live spectator.
- `test-importer-fixtures.py` also verifies private export filtering, display
  recovery and reversible profile reset using an isolated fixture directory.
- Native options checks cover all six new pages, replay labels/favourites,
  caption preferences, capture output, repeat navigation and monitor recovery.

Native PHP website/API checks use isolated private databases. test-native-php-api.py
runs the full account/directory/statistics/pairing/replay/tournament contract against
PHP; test-php-web.py exercises real public routes, cookies, CSRF and the API browser.
The MariaDB variants use only a disposable instance selected by KFX_TEST_MYSQL_SOCKET.
The legacy Python API remains a test fixture for database interoperability only.
