# Recovery, controls and local tools

All sections are inside Options → More Options. There is no external settings
launcher, hosted CI or runner requirement.

## Display recovery

The app marks a display startup before launching the engine. After three seconds
of healthy rendering, or a clean exit, the marker is removed. An interrupted
startup restores all frontend/game modes to 800×600 windowed on the next launch;
the old configuration and Mac display preferences are copied beside keeperfx.cfg.
An unconfirmed display change also leaves the marker armed. Monitor removal
requests the safe windowed mode without reverting to the removed monitor.

## Save recovery

Saved Games → Recovery lists rotating backups, CompatibilityBackups and Trash,
with campaign/map, date and available thumbnail. Check and Restore preserves the
source and restores into a free slot after the normal compatibility confirmation.
No existing slot is overwritten. Requirements sidecars move with trashed saves.
Up to 256 extra preserved/trash copies are listed; older files remain on disk.

## Controller and readability

Controller labels automatically follow the active Xbox/DualSense unless overridden
in Connected Controllers. D-pad hold repeats navigation in both directions; the
section chooser's footer is also reachable horizontally. Accessibility → Comfort /
Text adds independent caption/tooltip sizes, reduced damage tint, cream text and
an explicit focus marker. Stick response ranges from linear (1) to cubic (3),
with the previous quadratic response (2) as the default. Keyboard movement remains
unchanged. These are local presentation/input preferences, outside save data.

## Before joining and after interruption

Host Public Game offers an optional Preview Map field (for example `classic:52`).
The final lobby selection is still checked before starting. Upgraded public hosts advertise a SHA-256 simulation-content fingerprint once a
map is known. Joining compares the engine and matching campaign/map/configuration/
mod content before opening the connection. Scans run asynchronously. Older hosts
or an unselected map remain explicitly unverified, with lobby readiness checks
before starting. Files and profiles are never downloaded or changed automatically.

Upgraded ENet endpoints exchange a random 256-bit transport resume key in memory.
After an unclean timeout, the host reserves that user's slot for 30 additional
seconds; the client retries the authenticated transport. Gameplay packet history
and the same user/account mapping are retained. A new peer cannot receive gameplay
broadcasts until its key is accepted. A clean leave, expired reservation, process
restart or host loss beyond the grace period requires returning to the lobby.
Legacy peers retain their previous disconnect behavior. This does not provide
host migration or reconnect after the application has been closed.

Multiplayer packet repair waits up to 65 seconds before falling back to resync.
Clock correction stays positive after long interruptions, and suspended wall time
does not become an unbounded simulation backlog. A local integration test suspends
a client for 35 seconds and verifies the same match/slot continues.

## Replays and performance

Replays → Replay Library lists up to 256 local `.pck` files from the active
profile's recordings folder and GameData root. It shows campaign/map, recorded turn
and player counts, date and a local title. The original format does not reliably
record wall-clock duration or player names, so those are not invented. Titles and
favourites are sidecars. Export requires a new filename and does not overwrite an
existing file. Pause/speed/bookmarks remain available in Replay Controls; backward
seeking is not implemented.

Performance → Performance Graph and Capture provides an optional FPS, frame-time,
logic-time and ping overlay. Capture records 15 seconds, at most 4,096 samples, to
save/performance-capture.csv, without names, addresses or tokens. Close the menu
and play normally during the capture. A zero ping outside multiplayer means no
network sample, rather than measured Internet latency.

## Setup and personal data

About → Setup opens display settings, controller calibration, original-data import
and the profile's music overlay folder. Original-data import still uses a native
folder chooser before the first game can run: the engine needs those assets to
render its own menus. Later setup and reimport are available in-game. Imported
changes/music are prepared after restarting, preserving the current game.

About → Data Tools exports the default and separate content profiles' saves,
settings, screenshots and EnhancedData into a ZIP under the base Exports folder.
Imported OriginalData and session/token/password/credential files are excluded.
Exported mod overlays remain personal content and are never added to Git.

Reset Profile requires two actions and applies on restart. It moves only the
active profile's runtime, saves, screenshots and overlays to base/ProfileBackups;
shared imported originals and other profiles are retained. A scheduled reset can
be cancelled before restarting. To uninstall, Show App in Finder explains quitting
and moving KeeperFX-E.app to Trash. Personal data is kept unless deliberately reset
or removed separately; the running game does not delete its signed app bundle.
