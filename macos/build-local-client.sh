#!/bin/sh
# Local-only app: imported originals remain outside all public release inputs.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DEFAULT_BASE="$ROOT/assets/runtime"
if [ ! -f "$DEFAULT_BASE/keeper-enhanced.cfg" ]; then
    DEFAULT_BASE="$ROOT/.local/generated-runtime"
fi
BASE=${KFX_GAME_DIR:-$DEFAULT_BASE}
OUT=${KFX_PACKAGE_DIR:-"$HOME/Development/builds/keeper-enhanced"/$(date +%Y%m%d-%H%M%S)}
GAME="$ROOT/.local/imported-client"
if [ -z "${KFX_GAME_DIR:-}" ] && [ -z "${KFX_IMPORT_IMAGE:-}" ] && [ -z "${KFX_SPRITE_ARCHIVES_DIR:-}" ] && [ -f "$GAME/keeper-enhanced.cfg" ]; then
    echo "Reusing imported client data: $GAME"
else
    if [ ! -f "$BASE/keeper-enhanced.cfg" ] && [ ! -f "$BASE/keeperfx.cfg" ]; then
        python3 "$ROOT/scripts/build-asset-runtime.py" --output "$BASE"
    fi
    set --
    if [ -n "${KFX_IMPORT_IMAGE:-}" ]; then set -- --image "$KFX_IMPORT_IMAGE"; fi
    GAME="$OUT/imported-game"
    python3 "$ROOT/macos/import-local-data.py" "$@" --base "$BASE" \
        --import-dir "$ROOT/import" --output "$GAME"
fi
python3 "$ROOT/scripts/repair-sound-banks.py" --game "$GAME"
"$ROOT/macos/build-universal.sh"
KFX_GAME_DIR="$GAME" KFX_PACKAGE_DIR="$OUT" KFX_INCLUDE_LOCAL_DATA=1 \
    KFX_SIGN_IDENTITY=- "$ROOT/macos/package-app.sh"
