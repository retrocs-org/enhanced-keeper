# Releasing KeeperFX-E

This macOS workflow adapts the supplied Beam68 release system. Python 3.9+,
Xcode command-line tools, CMake, Ninja and PHP 8.1+ CLI are required. The runtime requires
macOS 13+. Public releases use a clean committed checkout; local review builds
can use `--allow-dirty` and are deliberately ineligible for publication.

`VERSION` is the KeeperFX-E app/release version. `macos/BUILD_NUMBER` is the
monotonic Apple bundle build number. The upstream engine's version stays in
`version.mk` for game/network compatibility. The product is KeeperFX-E;
the engine executable, bundle identifier and Application Support directory keep
their existing names so saves and settings carry over. The repository directory
and remote repositories are not renamed by packaging.

## Configure once

Copy `examples/release-config.json` outside the checkout to
`~/Development/tools/git-release/projects/keeperfx-e.json`, or select a
file with `--config` / `KFX_RELEASE_CONFIG`. Set the Developer ID identity and
saved `xcrun notarytool` profile under `env`. The profile name is a keychain
selector, never a password. `KFX_GAME_DIR` optionally selects the assembled
KeeperFX runtime asset directory (default `.local/keeperfx`). Original CD files,
movies and personal saves are stripped from distributable packages.

Set GitHub/GitLab web and matching SSH/HTTPS Git URLs explicitly before publishing.
No upstream destination is assumed. Tokens come from the environment or exact
1Password selectors (`provider: 1password`, `reference: op://Vault/Item/Field`),
resolved only for selected publication targets. Do not place secrets in JSON.
Build subprocesses do not inherit release API tokens. Never check local config in.

## Build, check, prepare

```sh
python3 scripts/release.py plan
python3 scripts/release.py build --output outputs/release-1.4.0-new
python3 scripts/release.py check --output outputs/release-1.4.0-new
python3 scripts/release.py prepare --output outputs/release-1.4.0-new --tag 1.4.0
```

For a local test of uncommitted changes, add `--allow-dirty` to build. It still
runs the release, menu and virtual-controller tests, builds a fresh universal
engine, signs/notarizes the app and DMG, verifies signatures and Gatekeeper,
compares both extracted packages with the same signed app, and hashes every
selected source input and package. A dirty build is review-only. Commit all
inputs and rebuild in a fresh output directory before publication.

The four public assets are exactly:

- `KeeperFX-E-1.4.0-macOS-Universal.dmg`
- `KeeperFX-E-1.4.0-macOS-Universal.dmg.sha256`
- `KeeperFX-E-1.4.0-macOS-Universal.zip`
- `KeeperFX-E-1.4.0-macOS-Universal.zip.sha256`

Each checksum is SHA-256, two spaces, exact filename and newline. Build logs,
full source/toolchain/app provenance (`release/build.json`), accepted notary
responses, reviewed manifest and publication copies stay under `release/` and
are not uploaded as assets. Matching source fingerprints establish matching
inputs, not reproducible Apple signatures or universal runtime certification.

`CHANGELOG.md` keeps history; `docs/RELEASE-NOTES.md` must contain the complete
matching version entry alone. Tags default to the exact unprefixed VERSION;
release titles are `KeeperFX-E <version>`. Updating any selected source,
script, artwork, documentation or package invalidates the prepared build.
The underlying `macos/package-app.sh` remains available for standalone local
packaging, but public uploads always use the checked release workflow.

## Publish reviewed bytes

Source branch pushing is separate and never forced:

```sh
python3 scripts/release.py source --to github,gitlab --dry-run
python3 scripts/release.py source --to github,gitlab --confirm-push
python3 scripts/release.py publish --output outputs/release-1.4.0-new \
  --tag 1.4.0 --to github,gitlab --confirm-publish
```

Choose only configured services. No repositories are created, remote repositories
renamed, website deployed or source pushed by build/check/prepare. Publishing
requires explicit confirmation and a clean committed checkout. All selected
services are preflighted before mutations. The same annotated tag object must
point to the prepared commit everywhere; tags are never moved or forced.

GitHub receives a draft, verifies size and server SHA-256 for every asset, then
publishes it as a normal latest release. GitLab receives versioned generic
packages, downloads/hashes them, then creates matching release links. Redirected
package downloads never forward credentials across origins. Existing matching
assets may be reused; unknown digests, different notes, unexpected assets or
conflicting tags stop publication. The hosts supply tagged source archives.

Keep `release/manifest.json`, the prepared copies and `release/receipt.json`
after interruptions. Resume with the same output/tag/config. Completed services
remain recorded; a later failure never rolls them back. Use a new patch version
and fresh output directory for corrections. Never edit receipts/checksums to
force a retry. Hosted adapters have offline mocked tests; validate against
separate test repositories before the first production publication. No live
publication is performed by the tests.

## Upgrade

Drag KeeperFX-E.app to Applications. When replacing the former KeeperFX.app,
quit the old game first. Existing data remains under
`~/Library/Application Support/keeperfx/keeperfx/`. The Enhanced name does not
change the engine/save format. Original Dungeon Keeper data is still required.

See `scripts/BEAM68-LICENSE.txt` for the reused release infrastructure licence.

Canonical project destinations: https://keeperfx-e.retrocs.org, https://github.com/retrocs-org/KeeperFX-E and https://gitlab.kladhest.se/retrocs-org/keeperfx-e. The website domain is reserved for deployment; existing local community-service preferences are retained.

### Source initialization and release notes

The release CLI supports the NovaDock workflow while keeping macOS packaging.
For an existing checkout, commit the public source before running the source
preflight and confirmed push commands above. Both destinations receive the same
reviewed commit, without force pushes or automatic tag uploads. Tracked private
configuration and generated local files are refused.

For a new source folder with a reviewed `.gitignore` and empty remote repositories:

```sh
python3 scripts/release.py source --init --to github,gitlab --dry-run
python3 scripts/release.py source --init --to github,gitlab --confirm-push --commit-message "Initial KeeperFX-E source"
```

The initialization dry run leaves the folder and Git metadata unchanged. A
confirmed initialization stages only the selected public inputs and creates the
first commit; it never makes additional commits in an established checkout.
Git transport uses your configured SSH agent or Git credential helper. Release
API credentials use environment variables or the configured 1Password references;
GitHub App installation tokens can be supplied through `GITHUB_TOKEN`.

Generate and verify the current version's release description before building:

```sh
python3 scripts/release_notes.py --write
python3 scripts/release_notes.py --check
```

Builds, source pushes and release publication remain separate operations.
Publication requires `--confirm-publish` and verified prepared artifacts.

### Source-only repository policy

Imported graphics, audio, fonts, maps, campaigns, translations and runtime
configuration stay local and are populated using the existing import tools.
Vendor downloads and compiled binaries are excluded too. The source push checks
all reachable history, so removing a file from the current revision cannot
accidentally publish an older copy. The existing upstream checkout contains
content in its history and cannot be pushed by this tool.

Create a fresh source-only copy before the first repository push:

```sh
python3 scripts/export_source.py --output /tmp/KeeperFX-E-source
cd /tmp/KeeperFX-E-source
python3 scripts/release.py source --init --to github,gitlab --dry-run
python3 scripts/release.py source --init --to github,gitlab --confirm-push
```

The export has no `.git` history and excludes runtime content and vendor trees.
It retains engine source and its original license/attribution, our enhancements,
build/import/release tools, website/service code, and the supplied controller UI
illustrations. Local imports and dependency bootstrapping are required before
building/packaging from this source-only copy. The app icon under `res/` stays a
local packaging input rather than repository content.

## Source-only build verification and signing

`macos/bootstrap-deps.sh` retrieves build-only upstream vendor files at a pinned
commit into ignored `third_party/keeperfx-source/`. CMake uses that copy when the
vendored `deps/` tree is absent. The MP3 decoder include and embedded window icon
also use the pinned vendor source. Controller mapping data and app icon are
obtained there during packaging; no imported game assets are committed.

A source-only checkout was configured and built for arm64 and x86_64 using an
existing universal binary dependency cache. To build dependencies on a new Mac,
run `macos/build-universal.sh`; it builds missing dependency prefixes first.
The assembled runtime directory is a separate local input: set `KFX_GAME_DIR`
for packaging and use the app's import tools for your original Dungeon Keeper
files. Set `KFX_MAC_DEPS_ROOT` only if reusing another dependency cache.

Run `macos/signing-preflight.sh IDENTITY` to test the private key before a long
build. Packaging runs this check before copying game data or compiling helpers.
If it fails, unlock the keychain containing the Developer ID private key and
allow codesign access through Keychain Access. The scripts do not change ACLs,
unlock keychains with stored passwords or disable signing verification.

## Local checks only

GitHub Actions workflows have been removed. No GitLab CI configuration or hosted
runner configuration is supplied. Build, test and release commands remain manual;
CI and runner provisioning are deferred. The local release command invokes the
native and service checks directly, without registering or contacting a runner.
