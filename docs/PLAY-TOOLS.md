# Save checks, replay controls and content profiles

All controls are inside Options → More Options. There is no external settings
launcher and no CI/hosted runner setup.

## Saved-game requirements

Choose a saved game and Check Save to see its engine/content requirements. Load
also opens this check before continuing. A different recorded engine version or
changed/missing campaign, map, configuration or mod files blocks loading with an
explanation. Restore the matching content profile or use the matching engine.

New saves record a `.requirements` sidecar with the engine version, selected
campaign/map and checksums/file lists for simulation configuration and the local
mod tree. Added files are checked too. Keep this sidecar with the save. Existing
saves without recorded requirements are explicitly unverified; they may be loaded
after confirmation. This also applies when requirements could not be recorded,
for example with linked or oversized content. Checks are bounded to 512 MiB and
16,000 content files; they do not certify compatibility with a different engine.

Before loading, a content-addressed copy is retained under
`save/CompatibilityBackups/`; existing copies are never overwritten. No automatic
binary save conversion is attempted. The original save is opened read-only, and
unsupported versions are rejected before game-state deserialization. Requirements
rotate alongside the actual three previous-save backups.

## Offline replay controls

From the main menu, More Options → Replays → Open Local Replay selects a `.pck`
file. The engine/packet headers are checked first. During playback:

- **F10** or controller **Start** opens/closes playback controls.
- **Space** pauses/resumes, **[ / ]** changes speed, and **B** adds a bookmark.
- The menu offers 0.5x, 1x, 2x and 4x playback, plus up to 16 persistent bookmarks.
- Next Bookmark processes every intervening simulation turn, then pauses there.

Bookmarks are bound to the replay's content hash and stored in the active
profile's save folder. Backward seeking/checkpoint jumping is not implemented.
Live multiplayer and live-stream spectating do not use these controls. Opening
another replay requires returning to the main menu; the picker cannot discard
an active dungeon. Replay controls do not change simulation pause packets.

## Separate content profiles

More Options → Content lists local profiles. Create Separate Profile accepts a
name of 1–32 letters, numbers, underscores or hyphens. Open Mod Folder opens that
profile's `EnhancedData/` overlay: place content using its normal relative layout,
for example `mods/my-mod/` and `mods/load_order.cfg`.

Choose Use Next Launch, then quit and reopen Keeper-Enhanced. The active game is never
hot-swapped. Each non-default profile gets a separate `GameData/`, `save/`, settings
and screenshots folder under `ContentProfiles/NAME/`. Imported original CD files
are shared read-only as preparation inputs. The default profile retains the
existing storage path and saves.

Changed overlays are applied on preparation; obsolete overlay-owned files are
removed before base assets are restored. Selecting Use Next Launch also forces
a refresh. Manual files that were not recorded as overlay-owned are preserved.
Linked profile/runtime destinations are rejected rather than followed. Keep mods
in EnhancedData instead of modifying the signed app or the default profile.

## Connection test

Network → Connection Test checks the configured community service and direct hosting
readiness. For incoming UDP verification, first open a lobby and configure its
public address under Server Browser → Host Public Game, then retry the test.

A public KeeperHub service sends a random nonce only to the requester's own public
address and accepts only the matching game reply. A successful reply verifies
that route at that moment. A timeout is not proof of a particular firewall/NAT
fault. Loopback/LAN services report Internet reachability as unverified, never as
successful. Arbitrary/internal destinations are rejected; probes are rate limited.

If direct hosting is
blocked by firewall/NAT/CGNAT, join a public server or use LAN/direct play where
appropriate. The local port is expected to be in use while the game is hosting.
No community account is required for this test.
