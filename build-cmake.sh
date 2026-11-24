#!/bin/sh
# Compatibility entry point. CMake settings live in scripts/build-target.sh.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ "${USE_DOCKER:-0}" != 0 ] || [ -n "${BUILD_DIR:-}" ]; then
    echo 'USE_DOCKER/BUILD_DIR are no longer supported here; use CMake directly or scripts/build-deck-bundle.sh.' >&2
    exit 2
fi
if [ "$#" -gt 0 ]; then
    echo 'Use CMake directly for custom targets; make help lists supported platform builds.' >&2
    exit 2
fi
case "${KFX_OS:-$(uname -s)}" in
    Linux|linux) TARGET=client-linux;;
    Darwin|macos) TARGET=client-macos;;
    windows|MINGW*|MSYS*|CYGWIN*) TARGET=client-windows;;
    *) echo 'Unsupported host. Set KFX_OS=linux, macos or windows.' >&2; exit 2;;
esac
exec "$ROOT/scripts/build-target.sh" "$TARGET" "${KFX_JOBS:-2}" Release
