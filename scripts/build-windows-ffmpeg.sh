#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
PREFIX=$1
JOBS=$2
VERSION=9.0.1
NAME="ffmpeg-$VERSION.tar.xz"
HASH=cf38e0e28c7e5605942c4a77755349b0145804a397af37eb1fb4c77cb237f635
[ ! -f "$PREFIX/.keeper-ffmpeg-$VERSION" ] || exit 0
WORK="$(dirname "$PREFIX")/ffmpeg-work"
mkdir -p "$WORK"
if [ ! -f "$WORK/$NAME" ]; then
    if [ -f "$ROOT/build-macos-deps/$NAME" ]; then
        cp "$ROOT/build-macos-deps/$NAME" "$WORK/$NAME"
    else
        curl --fail --location "https://ffmpeg.org/releases/$NAME" -o "$WORK/$NAME.download"
        mv "$WORK/$NAME.download" "$WORK/$NAME"
    fi
fi
python3 - "$WORK/$NAME" "$HASH" <<'PY'
import hashlib
import sys
from pathlib import Path
if hashlib.sha256(Path(sys.argv[1]).read_bytes()).hexdigest() != sys.argv[2]:
    raise SystemExit("FFmpeg archive checksum mismatch")
PY
tar -xf "$WORK/$NAME" -C "$WORK"
mkdir -p "$WORK/build"
cd "$WORK/build"
"$WORK/ffmpeg-$VERSION/configure" --prefix="$PREFIX" --target-os=mingw32 --arch=x86 \
    --enable-cross-compile --cross-prefix=i686-w64-mingw32- --disable-x86asm \
    --disable-programs --disable-doc --disable-debug --disable-shared --enable-static \
    --disable-autodetect --disable-everything --disable-network \
    --enable-avformat --enable-avcodec --enable-avutil --enable-swscale --enable-swresample \
    --enable-protocol=file --enable-demuxer=smacker,mov \
    --enable-decoder=smacker,smackaud,h264,aac --enable-parser=h264,aac \
    --enable-bsf=aac_adtstoasc,h264_mp4toannexb
make -j "$JOBS"
make install
touch "$PREFIX/.keeper-ffmpeg-$VERSION"
