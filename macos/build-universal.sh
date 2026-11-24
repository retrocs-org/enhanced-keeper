#!/bin/zsh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEPS="${KFX_MAC_DEPS_ROOT:-$ROOT/build-macos-deps}"
BUILD="${KFX_MAC_BUILD_DIR:-$ROOT/build-mac-universal}"
TARGET="${MACOSX_DEPLOYMENT_TARGET:-13.0}"
# A previous dependency build may have stopped after installing only SDL3.
required_deps=(
  sdl-prefix/lib/libSDL3.a
  sdl-prefix/lib/cmake/SDL3/SDL3Config.cmake
  sdlimg-prefix/lib/libSDL3_image.a
  sdlimg-prefix/lib/cmake/SDL3_image/SDL3_imageConfig.cmake
  sdlmix-prefix/lib/libSDL3_mixer.a
  sdlmix-prefix/lib/cmake/SDL3_mixer/SDL3_mixerConfig.cmake
  openal-prefix/lib/libopenal.a
  openal-prefix/lib/cmake/OpenAL/OpenALConfig.cmake
  lua-fat/lib/liblua.a
  spng-prefix/lib/libspng_static.a
  minizip-fat/lib/libminizip.a
  miniupnpc-prefix/lib/libminiupnpc.a
  libnatpmp-fat/lib/libnatpmp.a
  ffmpeg-fat/lib/libavformat.a
  ffmpeg-fat/lib/libavcodec.a
  ffmpeg-fat/lib/libswresample.a
  ffmpeg-fat/lib/libswscale.a
  ffmpeg-fat/lib/libavutil.a
)
for dependency in "${required_deps[@]}"; do
  if [[ ! -f "$DEPS/$dependency" ]]; then
    echo "Missing macOS dependency: $dependency; rebuilding dependencies"
    "$ROOT/macos/build-universal-deps.sh"
    break
  fi
done
[[ -f "$DEPS/curl-prefix/lib/libcurl.a" ]] || "$ROOT/macos/build-network-deps.sh"
"$ROOT/macos/bootstrap-deps.sh"
rm -rf "$BUILD"
cmake -S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET="$TARGET" \
  -DKFX_MAC_SELF_CONTAINED=ON -DKFX_MAC_DEPS_ROOT="$DEPS"
cmake --build "$BUILD" --target keeper-enhanced -j "${KFX_JOBS:-$(sysctl -n hw.logicalcpu)}"
file "$BUILD/keeper-enhanced"
lipo -info "$BUILD/keeper-enhanced"
if otool -L "$BUILD/keeper-enhanced" | grep -q '/opt/homebrew'; then
  echo 'ERROR: Homebrew runtime dependency remains' >&2; exit 1
fi
echo "Universal self-contained KeeperFX ready: $BUILD/keeper-enhanced"
