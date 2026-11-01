#!/usr/bin/env bash
# Build target nghttp2 required by Kodi's static curl dependency.
set -euo pipefail
source "$(dirname "$0")/env-depends.sh"

VER=1.69.0
SRC="$PS4_BUILD/nghttp2-$VER"
BUILD="$SRC/build-ps4"
TARBALL="nghttp2-$VER.tar.xz"
TOOLCHAIN_FILE="$ROOT/toolchain/ps4-kodi.cmake"

if [ -f "$PS4_PREFIX/lib/libnghttp2.a" ] &&
   [ -f "$PS4_PREFIX/lib/pkgconfig/libnghttp2.pc" ]; then
  echo "nghttp2 already built"
  exit 0
fi

mkdir -p "$PS4_BUILD"
if [ ! -d "$SRC" ]; then
  cd "$PS4_BUILD"
  curl -fL --retry 3 -o "$TARBALL" \
    "https://github.com/nghttp2/nghttp2/releases/download/v$VER/$TARBALL"
  tar xf "$TARBALL"
fi

rm -rf "$BUILD"
cmake -S "$SRC" -B "$BUILD" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
  -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
  -DOO_PS4_TOOLCHAIN="$OO_PS4_TOOLCHAIN" -DPS4_PREFIX="$PS4_PREFIX" \
  -DCMAKE_INSTALL_PREFIX="$PS4_PREFIX" -DCMAKE_INSTALL_LIBDIR=lib \
  -DCMAKE_INSTALL_INCLUDEDIR=include \
  -DENABLE_DEBUG=OFF -DENABLE_FAILMALLOC=OFF \
  -DENABLE_LIB_ONLY=ON -DENABLE_DOC=OFF \
  -DBUILD_STATIC_LIBS=ON -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=OFF -DWITH_LIBXML2=OFF \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" --parallel 2
cmake --install "$BUILD"

test -f "$PS4_PREFIX/lib/libnghttp2.a"
test -f "$PS4_PREFIX/lib/pkgconfig/libnghttp2.pc"
echo "nghttp2 OK -> $PS4_PREFIX/lib/libnghttp2.a"
