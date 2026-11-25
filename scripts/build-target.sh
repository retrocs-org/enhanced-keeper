#!/bin/sh
# Small front end; compiler configuration and dependencies remain in CMake.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TARGET=${1:?Expected server-linux, client-linux, client-macos or client-windows}
JOBS=${2:-2}
BUILD_TYPE=${3:-Release}
case "$JOBS" in ''|*[!0-9]*|0) echo 'JOBS must be a positive integer.' >&2; exit 1;; esac
case "$TARGET" in
    server-linux|client-linux)
        [ "$(uname -s)" = Linux ] || { echo "$TARGET requires Linux." >&2; exit 1; }
        BUILD="$ROOT/out/$TARGET"
        if [ "$TARGET" = server-linux ]; then DEDICATED=ON; else DEDICATED=OFF; fi
        cmake -S "$ROOT" -B "$BUILD" -G "Unix Makefiles" \
            -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DKFX_DEDICATED="$DEDICATED" \
            -DKFX_INSTALL_GAME_DATA=OFF
        ;;
    client-windows)
        command -v i686-w64-mingw32-gcc >/dev/null 2>&1 || {
            echo 'Install the i686 MinGW-w64 toolchain; this target supports native MSYS2 or cross-compilation.' >&2; exit 1;
        }
        command -v wixl >/dev/null 2>&1 || { echo 'Install wixl (msitools) to build the Windows MSI.' >&2; exit 1; }
        BUILD="$ROOT/out/$TARGET"
        sh "$ROOT/scripts/build-windows-ffmpeg.sh" "$BUILD/deps/ffmpeg-prefix" "$JOBS"
        sh "$ROOT/scripts/build-windows-openal.sh" "$BUILD/deps" "$JOBS"
        cmake -S "$ROOT" -B "$BUILD" -G "Unix Makefiles" \
            -DCMAKE_TOOLCHAIN_FILE="$ROOT/build/cmake/toolchains/mingw32.cmake" \
            -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DKFX_DEDICATED=OFF -DKFX_INSTALL_GAME_DATA=OFF
        ;;
    client-macos)
        [ "$(uname -s)" = Darwin ] || { echo 'client-macos requires macOS and Xcode command-line tools.' >&2; exit 1; }
        export KFX_JOBS="$JOBS"
        exec "$ROOT/macos/build-local-client.sh"
        ;;
    *) echo "Unknown build target: $TARGET" >&2; exit 1;;
esac
cmake --build "$BUILD" --config "$BUILD_TYPE" --target keeper-enhanced --parallel "$JOBS"
if [ "$TARGET" = client-windows ]; then
    OUT=${KFX_PACKAGE_DIR:-"$HOME/Development/builds/keeper-enhanced/$(date +%Y%m%d-%H%M%S)"}
    python3 "$ROOT/scripts/package-windows.py" "$BUILD" "$OUT"
else
    echo "Build ready in $BUILD (original game assets excluded)."
fi
