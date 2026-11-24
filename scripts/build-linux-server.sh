#!/bin/sh
# Native Linux package, using the existing portable-library CMake installation.
set -eu
[ "$(uname -s)" = Linux ] || { echo 'Run this build on Linux.' >&2; exit 1; }
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD="$ROOT/out/linux-server-build"
PACKAGE="$ROOT/out/keeper-enhanced-server"
[ ! -e "$PACKAGE" ] || { echo 'Output exists; move out/keeper-enhanced-server aside first.' >&2; exit 1; }
cmake -S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DKFX_DECK_BUNDLE=ON -DKFX_INSTALL_GAME_DATA=OFF
cmake --build "$BUILD" --target keeperfx --parallel "${KFX_JOBS:-2}"
cmake --install "$BUILD" --prefix "$PACKAGE/engine"
cp "$ROOT/services/keeperhub/dedicated.py" "$PACKAGE/dedicated.py"
cp "$ROOT/linux/import-data.py" "$ROOT/linux/server.json" "$ROOT/linux/keeper-enhanced-server.service" "$PACKAGE/"
cp "$ROOT/docs/LINUX-SERVER.md" "$PACKAGE/README.md"
cp "$ROOT/LICENSE" "$PACKAGE/"
(cd "$ROOT/out" && tar -czf "keeper-enhanced-server-$(uname -m).tar.gz" keeper-enhanced-server)
echo "Built $ROOT/out/keeper-enhanced-server-$(uname -m).tar.gz (game assets excluded)."
