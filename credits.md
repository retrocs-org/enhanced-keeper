# Credits and asset licenses

Keeper Enhanced is a fork of [KeeperFX](https://github.com/dkfans/keeperfx).
Credits remain with the original authors and contributors; vendoring assets does
not make them original work by the Keeper Enhanced team.

## Dungeon Keeper

Dungeon Keeper was created by Bullfrog Productions. Dungeon Keeper and its
original artwork, movies, sound effects and CD soundtrack belong to their
respective rights holders, including Electronic Arts. Original disc images and
files extracted from them are local user inputs and are not included in this
repository or public release packages.

## KeeperFX

Thanks to the KeeperFX contributors for the engine, gameplay improvements,
configuration, tools, translations and campaigns. The engine is distributed under
the GPL; see the repository's root LICENSE and source-file notices. Individual
third-party components retain their own licenses.

## FXGraphics — graphics source assets

- Authors: the FXGraphics contributors; retain per-file credits and notices.
- Upstream: https://github.com/dkfans/FXGraphics
- Pinned revision: `bc1b78e81cc5cade97d9c262c175a4bea0ed3a0f`.
- Vendored source: `assets/upstream/fxgraphics/`.
- Declared repository license: GNU GPL version 3; see
  [the preserved license](assets/upstream/fxgraphics/LICENSE).

## FXsounds — audio source assets

- Authors: the FXsounds contributors; retain per-file credits and notices.
- Upstream: https://github.com/dkfans/FXsounds
- Pinned revision: `5258a75829f205aa01d2e7889bf6481ef468a7a0`.
- Vendored source: `assets/upstream/fxsounds/`.
- Declared repository license: GNU GPL version 3; see
  [the preserved license](assets/upstream/fxsounds/LICENSE).

`assets/upstream/asset-sources.json` records the revisions and original Git blob
hashes for the vendored files. The upstream files and license texts are preserved
without modification. Repository-level license declarations are recorded here;
they do not relicense original Dungeon Keeper disc content.

## Prebuilt open runtime

`assets/runtime/` contains the output of `scripts/build-asset-runtime.py`.
Its `asset-build.json` records the exact source revisions. The generated
graphics and sounds use the source trees and licenses above. The configuration,
maps, campaign scripts, translation sources, and packaging recipes come from
the [pinned KeeperFX checkout](https://github.com/dkfans/keeperfx/tree/392c7ebdbbd6da6e9596ac769edb4353a01dca5f).
The font inputs and generators are in `tools/fxfontmaker/`. Original game data
is supplied separately by the local disc import.

## Keeper Enhanced

Thanks to contributors to the macOS port, native builds, dedicated server,
controller support, import tools and community website. Build-time third-party
tools and libraries keep their own notices and are fetched outside the tracked
asset source directories.
