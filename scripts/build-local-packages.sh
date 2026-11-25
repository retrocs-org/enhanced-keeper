#!/bin/sh
# Build both local installers from the current checkout into one directory.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
JOBS=${1:-8}
export KFX_PACKAGE_DIR=${KFX_PACKAGE_DIR:-"$HOME/Development/builds/keeper-enhanced/$(date +%Y%m%d-%H%M%S)"}
"$ROOT/scripts/build-target.sh" client-macos "$JOBS"
"$ROOT/scripts/build-target.sh" client-windows "$JOBS"
