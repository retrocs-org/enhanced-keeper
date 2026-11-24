#!/bin/zsh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GAME="${KFX_GAME_DIR:-$ROOT/.local/keeperfx}"
ENGINE="${KFX_ENGINE:-$ROOT/build-mac-universal/keeper-enhanced}"
VERSION="$(cat "$ROOT/VERSION")"
BUILD_NUMBER="$(cat "$ROOT/macos/BUILD_NUMBER")"
OUT="${KFX_PACKAGE_DIR:-$ROOT/outputs/local-$VERSION-$BUILD_NUMBER}"
IDENTITY="${KFX_SIGN_IDENTITY:?Set KFX_SIGN_IDENTITY to a Developer ID Application identity}"
LOCAL_DATA=${KFX_INCLUDE_LOCAL_DATA:-0}
[[ "$LOCAL_DATA" == 0 || "$LOCAL_DATA" == 1 ]] || { echo 'KFX_INCLUDE_LOCAL_DATA must be 0 or 1' >&2; exit 1; }
if [[ "$LOCAL_DATA" == 1 && ( "$IDENTITY" != - || -n "${KFX_NOTARY_PROFILE:-}" ) ]]; then
  echo 'Imported game data is local-only: use ad-hoc signing without notarization.' >&2; exit 1
fi
sign_flags=()
[[ "$IDENTITY" == - ]] || sign_flags=(--options runtime --timestamp)
PROFILE="${KFX_NOTARY_PROFILE:-}"
APP="$OUT/Keeper-Enhanced.app"
DMG="$OUT/Keeper-Enhanced-$VERSION-macOS-Universal.dmg"
[[ -x "$ENGINE" && -f "$GAME/keeper-enhanced.cfg" ]] || { echo "Missing engine or KeeperFX runtime data" >&2; exit 1; }
[[ ! -e "$APP" && ! -e "$DMG" ]] || { echo "Output exists; choose a new KFX_PACKAGE_DIR" >&2; exit 1; }
# Check keychain access before copying assets or compiling helpers.
[[ "$IDENTITY" == - ]] || "$ROOT/macos/signing-preflight.sh" "$IDENTITY"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
ditto "$GAME" "$APP/Contents/Resources/game"
# A distributable package must not contain files imported from the owner's CD,
# personal progress, generated caches, or previous executable signatures.
python3 - "$APP/Contents/Resources/game" "$LOCAL_DATA" <<'PY'
from pathlib import Path
import shutil,sys,json,re
root=Path(sys.argv[1])
manifest=root/'original-audio-files.json'
if sys.argv[2]=='0' and manifest.exists():
 for name in json.loads(manifest.read_text()):
  if not isinstance(name,str) or not re.fullmatch(r'track[0-9]{2}\.wav',name):raise SystemExit('Invalid imported audio manifest')
  (root/'music'/name).unlink(missing_ok=True)
 manifest.unlink()
required={'data':['bluepal.dat','bluepall.dat','dogpal.pal','hitpall.dat','lightng.pal','redpal.col','redpall.dat','slab0-0.dat','slab0-1.dat','vampal.pal','whitepal.col'],'sound':['atmos1.sbk','atmos2.sbk','bullfrog.sbk']}
for folder,names in (required.items() if sys.argv[2]=='0' else []):
 for name in names:(root/folder/name).unlink(missing_ok=True)
for p in (root/'ldata').glob('*'):
 if sys.argv[2]=='0' and p.suffix.lower()=='.smk':p.unlink()
for name in ('save','scrshots','recordings','community-session.cfg','keeperfx','keeper-enhanced','_CodeSignature'):
 p=root/name
 if p.is_dir():shutil.rmtree(p)
 elif p.exists():p.unlink()
for name in ('colours.col','tables.dat','alpha.col'):(root/'data'/name).unlink(missing_ok=True)
for p in root.rglob('.DS_Store'):p.unlink()
for p in root.glob('*.log'):p.unlink()
PY
cp "$ENGINE" "$APP/Contents/Resources/game/keeper-enhanced"
mkdir -p "$APP/Contents/Resources/controllers"
cp "$ROOT"/macos/packaging/controllers/* "$APP/Contents/Resources/controllers/"
cp "$ROOT"/macos/packaging/controllers/*.png "$APP/Contents/Resources/game/fxdata/"
clang -O2 -fobjc-arc -fblocks -Wall -Wextra -Wno-incompatible-pointer-types \
  -arch arm64 -arch x86_64 -mmacosx-version-min=13.0 -framework Cocoa \
  "$ROOT/macos/packaging/Launcher.m" -o "$APP/Contents/MacOS/KeeperFXE"
D="${KFX_MAC_DEPS_ROOT:-$ROOT/build-macos-deps}"
clang -O2 -arch arm64 -arch x86_64 -mmacosx-version-min=13.0 \
  "$ROOT/macos/packaging/connection-check.c" -I"$D/curl-prefix/include" \
  "$D/curl-prefix/lib/libcurl.a" "$D/openssl-fat/lib/libssl.a" "$D/openssl-fat/lib/libcrypto.a" \
  -lz -framework Security -framework CoreFoundation -framework SystemConfiguration \
  -o "$APP/Contents/Resources/connection-check"
# Refresh the supplemental mapping database used by both engine and diagnostic.
CONTROLLER_DB="$ROOT/config/fxdata/gamecontrollerdb.txt"
[[ -f "$CONTROLLER_DB" ]] || CONTROLLER_DB="$ROOT/third_party/keeperfx-source/config/fxdata/gamecontrollerdb.txt"
[[ -f "$CONTROLLER_DB" ]] || { echo 'Run macos/bootstrap-deps.sh to obtain the pinned controller database.' >&2; exit 1; }
cp "$CONTROLLER_DB" "$APP/Contents/Resources/game/fxdata/gamecontrollerdb.txt"
PKG_CONFIG_PATH="$D/sdl-prefix/lib/pkgconfig" pkg-config --static --libs sdl3 > "$OUT/sdl-link-flags.txt"
sdl_flags=(${(z)$(cat "$OUT/sdl-link-flags.txt")})
clang -O2 -arch arm64 -arch x86_64 -mmacosx-version-min=13.0 \
  "$ROOT/macos/packaging/controller-check.c" -I"$D/sdl-prefix/include" \
  "${sdl_flags[@]}" -o "$APP/Contents/Resources/controller-check"
clang -O2 -arch arm64 -arch x86_64 -mmacosx-version-min=13.0 \
  "$ROOT/macos/packaging/video-check.c" -I"$D/ffmpeg-fat/include" \
  "$D/ffmpeg-fat/lib/libavformat.a" "$D/ffmpeg-fat/lib/libavcodec.a" \
  "$D/ffmpeg-fat/lib/libswresample.a" "$D/ffmpeg-fat/lib/libswscale.a" "$D/ffmpeg-fat/lib/libavutil.a" \
  -lz -liconv -framework CoreFoundation -framework CoreVideo -framework CoreMedia \
  -o "$APP/Contents/Resources/video-check"
python3 - "$ROOT" "$APP/Contents/Resources" "$ENGINE" "$VERSION" "$BUILD_NUMBER" <<'META'
import json,re,sys,subprocess,hashlib
from pathlib import Path
root,out,engine=map(Path,sys.argv[1:4]);app_version,app_build=sys.argv[4:6]
actions=[]
for name,buttons in re.findall(r'\{"([^"\n]+)"[^\n]*?,\s*(CBtn_[^,]+),\s*BMV_', (root/'src/front_input.c').read_text()):
 actions.append({'action':name,'default':buttons.strip().replace('CBtn_','')})
assert len(actions)>50
(out/'controller-actions.json').write_text(json.dumps(actions,indent=2))
version={k:v for k,v in re.findall(r'(VER_\w+)=(\d+)',(root/'version.mk').read_text())}
game='.'.join(version[k] for k in ('VER_MAJOR','VER_MINOR','VER_RELEASE','VER_BUILD'))
(out/'build-info.json').write_text(json.dumps({'gameVersion':game,'appBuild':app_build,'appVersion':app_version,'product':'Keeper-Enhanced','website':'https://keeperfx-e.retrocs.org','engineSHA':hashlib.sha256(engine.read_bytes()).hexdigest(),'git':subprocess.check_output(['git','rev-parse','--short','HEAD'],cwd=root,text=True).strip()},indent=2))
META
shasum -a 256 "$ENGINE" | cut -d ' ' -f 1 > "$APP/Contents/Resources/asset-version.txt"
ICON="${KFX_ICON_PNG:-$ROOT/third_party/keeperfx-source/res/keeperfx_icon512-24bpp.png}"
[[ -f "$ICON" ]] || { echo 'Missing packaging icon. Run macos/bootstrap-deps.sh or set KFX_ICON_PNG.' >&2; exit 1; }
ICONSET="$OUT/KeeperFX.iconset";mkdir -p "$ICONSET"
for size in 16 32 128 256 512; do
  sips -z "$size" "$size" "$ICON" --out "$ICONSET/icon_${size}x${size}.png" >/dev/null
  if (( size < 512 )); then
    double=$((size * 2))
    sips -z "$double" "$double" "$ICON" --out "$ICONSET/icon_${size}x${size}@2x.png" >/dev/null
  fi
done
iconutil -c icns "$ICONSET" -o "$APP/Contents/Resources/KeeperFX.icns"
python3 - "$APP/Contents/Info.plist" "$VERSION" "$BUILD_NUMBER" <<'PY'
import plistlib,sys
info={'CFBundleIdentifier':'net.keeperfx.macos','CFBundleName':'Keeper-Enhanced','CFBundleDisplayName':'Keeper-Enhanced','KFXProjectWebsite':'https://keeperfx-e.retrocs.org','CFBundleExecutable':'KeeperFXE','CFBundleIconFile':'KeeperFX','CFBundlePackageType':'APPL','CFBundleShortVersionString':sys.argv[2],'CFBundleVersion':sys.argv[3],'LSMinimumSystemVersion':'13.0','NSHighResolutionCapable':True,'NSLocalNetworkUsageDescription':'Keeper-Enhanced uses your local network to host and join multiplayer games.','NSHumanReadableCopyright':'KeeperFX contributors. Dungeon Keeper is a trademark of Electronic Arts.'}
with open(sys.argv[1],'wb') as f:plistlib.dump(info,f)
PY
mkdir -p "$APP/Contents/Resources/Licenses"
cp "$ROOT/LICENSE" "$APP/Contents/Resources/Licenses/KeeperFX.txt"
for item in "SDL-3.4.16/LICENSE.txt" "SDL_image-3.4.6/LICENSE.txt" "SDL_mixer-3.2.4/LICENSE.txt" "curl-8.23.0/COPYING" "openssl-3.5.9/LICENSE.txt" "ffmpeg-9.0.1/COPYING.LGPLv2.1"; do
 [[ ! -f "$D/$item" ]] || cp "$D/$item" "$APP/Contents/Resources/Licenses/${item//\//-}"
done
# One process signs nested code first and the enclosing app last. This avoids
# repeatedly requesting access from separate codesign processes.
codesign --force "${sign_flags[@]}" --sign "$IDENTITY" \
 "$APP/Contents/Resources/game/keeper-enhanced" \
 "$APP/Contents/Resources/connection-check" \
 "$APP/Contents/Resources/controller-check" \
 "$APP/Contents/Resources/video-check" "$APP"
codesign --verify --deep --strict --verbose=2 "$APP"
if [[ -n "$PROFILE" ]]; then
 ditto -c -k --keepParent "$APP" "$OUT/KeeperFX-notarization.zip"
 xcrun notarytool submit "$OUT/KeeperFX-notarization.zip" --keychain-profile "$PROFILE" --wait --output-format json > "$OUT/app-notarization.json"
 python3 - "$OUT/app-notarization.json" <<'PY'
import json,sys
r=json.load(open(sys.argv[1]));assert r['status']=='Accepted',r
PY
 xcrun stapler staple "$APP"
 xcrun stapler validate "$APP"
fi
mkdir -p "$OUT/dmg-stage"
ditto "$APP" "$OUT/dmg-stage/Keeper-Enhanced.app"
ln -s /Applications "$OUT/dmg-stage/Applications"
printf '%s\n' 'Drag Keeper-Enhanced into Applications and open it.' \
  'Local builds include your imported originals. Public builds require Import Original Game on first launch.' \
  'Game data, saves, logs and settings stay in ~/Library/Application Support/keeperfx/keeperfx/.' \
  'The app opens the game directly. Options → More Options opens the integrated settings. F9 opens the controller guide.' > "$OUT/dmg-stage/Read Me.txt"
hdiutil create -volname "Keeper-Enhanced" -srcfolder "$OUT/dmg-stage" -format UDZO "$DMG"
codesign "${sign_flags[@]}" --sign "$IDENTITY" "$DMG"
if [[ -n "$PROFILE" ]]; then
 xcrun notarytool submit "$DMG" --keychain-profile "$PROFILE" --wait --output-format json > "$OUT/dmg-notarization.json"
 python3 - "$OUT/dmg-notarization.json" <<'PY'
import json,sys
r=json.load(open(sys.argv[1]));assert r['status']=='Accepted',r
PY
 xcrun stapler staple "$DMG"
 xcrun stapler validate "$DMG"
fi
codesign --verify --verbose=2 "$DMG"
hdiutil verify "$DMG"
ditto -c -k --keepParent "$APP" "$OUT/Keeper-Enhanced-$VERSION-macOS-Universal.zip"
echo "Package ready: $DMG"
