#!/bin/sh
# Local-only app: imported originals remain outside all public release inputs.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BASE=${KFX_GAME_DIR:-$ROOT/.local/keeperfx}
OUT=${KFX_PACKAGE_DIR:-$ROOT/outputs/local-client-$(date +%Y%m%d-%H%M%S)}
set --
if [ -n "${KFX_IMPORT_IMAGE:-}" ]; then set -- --image "$KFX_IMPORT_IMAGE"; fi
python3 "$ROOT/macos/import-local-data.py" "$@" --base "$BASE" \
    --import-dir "$ROOT/import" --output "$OUT/imported-game"
"$ROOT/macos/build-universal.sh"
KFX_GAME_DIR="$OUT/imported-game" KFX_PACKAGE_DIR="$OUT" KFX_INCLUDE_LOCAL_DATA=1 \
    KFX_SIGN_IDENTITY=- "$ROOT/macos/package-app.sh"
