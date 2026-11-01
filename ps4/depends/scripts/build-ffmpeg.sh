#!/bin/bash
# FFmpeg for PS4 – software decoders only for now
set -euo pipefail
source "$(dirname "$0")/env-depends.sh"

"$ROOT/depends/scripts/build-zlib.sh"
"$ROOT/depends/scripts/build-bzip2.sh"
"$ROOT/depends/scripts/build-openssl.sh" || true

VER=6.1.2
SRC="$PS4_BUILD/ffmpeg-$VER"
TARBALL="ffmpeg-$VER.tar.xz"

if [ -f "$PS4_PREFIX/lib/libavcodec.a" ]; then
  echo "ffmpeg already built"
  exit 0
fi

cd "$PS4_BUILD"
if [ ! -d "$SRC" ]; then
  echo "Downloading ffmpeg $VER ..."
  curl -fL --retry 3 -o "$TARBALL" \
    "https://ffmpeg.org/releases/ffmpeg-${VER}.tar.xz"

  tar xf "$TARBALL"
fi

cd "$SRC"

./configure \
  --prefix="$PS4_PREFIX" \
  --target-os=freebsd \
  --arch=x86_64 \
  --cc="$CC" \
  --cxx="$CXX" \
  --ar="$AR" \
  --ranlib="$RANLIB" \
  --strip="$STRIP" \
  --enable-static --disable-shared \
  --disable-debug --disable-doc --disable-programs \
  --enable-gpl --enable-version3 \
  --enable-zlib --enable-bzlib \
  --enable-avcodec --enable-avformat --enable-avutil \
  --enable-swscale --enable-swresample --enable-avfilter \
  --disable-x86asm --disable-inline-asm \
  --disable-network \
  --extra-cflags="$CFLAGS -I$PS4_PREFIX/include" \
  --extra-ldflags="$LDFLAGS -L$PS4_PREFIX/lib" \
  --pkg-config-flags="--static"

make -j"$(nproc)"
make install
echo "ffmpeg OK -> $PS4_PREFIX/lib/libavcodec.a"
