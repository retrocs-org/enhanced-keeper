#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DEPS=$1
JOBS=$2
REVISION=b2c48f7718ef3fcf67921a8b6534c4914e328970
PREFIX="$DEPS/openal-prefix"
[ ! -f "$PREFIX/.$REVISION" ] || exit 0
SOURCE="$ROOT/build-macos-deps/openal-soft-src"
if [ "$(git -C "$SOURCE" rev-parse HEAD 2>/dev/null || true)" != "$REVISION" ]; then
    SOURCE="$DEPS/openal-source"
    if [ ! -d "$SOURCE/.git" ]; then git init "$SOURCE"; fi
    git -C "$SOURCE" fetch --depth 1 https://github.com/kcat/openal-soft.git "$REVISION"
    git -C "$SOURCE" checkout --detach FETCH_HEAD
fi
cmake -S "$SOURCE" -B "$DEPS/openal-build" -G "Unix Makefiles" \
    -DCMAKE_TOOLCHAIN_FILE="$ROOT/build/cmake/toolchains/mingw32.cmake" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" -DLIBTYPE=STATIC \
    -DALSOFT_UTILS=OFF -DALSOFT_EXAMPLES=OFF -DALSOFT_TESTS=OFF -DALSOFT_ENABLE_MODULES=OFF \
    -DALSOFT_BACKEND_SDL2=OFF -DALSOFT_BACKEND_SDL3=OFF -DALSOFT_BACKEND_DSOUND=OFF \
    -DALSOFT_REQUIRE_WASAPI=ON -DALSOFT_UPDATE_BUILD_VERSION=OFF
cmake --build "$DEPS/openal-build" --parallel "$JOBS"
cmake --install "$DEPS/openal-build"
cp "$SOURCE/COPYING" "$PREFIX/COPYING"
touch "$PREFIX/.$REVISION"
