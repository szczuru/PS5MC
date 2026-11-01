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

if ! ./configure \
  --prefix="$PS4_PREFIX" \
  --target-os=freebsd \
  --arch=x86_64 \
  --enable-cross-compile \
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
  --extra-ldflags="$LDFLAGS -nostdlib -fuse-ld=lld -pie -Wl,-m,elf_x86_64 -Wl,--script,$OO_PS4_TOOLCHAIN/link.x --sysroot=$OO_PS4_TOOLCHAIN -L$OO_PS4_TOOLCHAIN/lib -L$PS4_PREFIX/lib" \
  --extra-libs="-lc -lkernel -lSceSysmodule $OO_PS4_TOOLCHAIN/lib/crt1.o" \
  --pkg-config-flags="--static"; then
  echo "FFmpeg configure failed; diagnostics from ffbuild/config.log:"
  tail -160 ffbuild/config.log
  exit 1
fi

make -j"$(nproc)"
make install
echo "ffmpeg OK -> $PS4_PREFIX/lib/libavcodec.a"
