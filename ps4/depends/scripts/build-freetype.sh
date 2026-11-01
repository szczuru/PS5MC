#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/env-depends.sh"

"$ROOT/depends/scripts/build-zlib.sh"
"$ROOT/depends/scripts/build-libpng.sh"

VER=2.13.2
SRC="$PS4_BUILD/freetype-$VER"
TARBALL="freetype-$VER.tar.xz"

if [ -f "$PS4_PREFIX/lib/libfreetype.a" ]; then
  echo "freetype already built"
  exit 0
fi

cd "$PS4_BUILD"
if [ ! -d "$SRC" ]; then
  echo "Downloading freetype $VER ..."
  curl -fL --retry 3 -o "$TARBALL" \
    "https://download.savannah.gnu.org/releases/freetype/freetype-${VER}.tar.xz"
  tar xf "$TARBALL"
fi

cd "$SRC"
rm -rf build-ps4
mkdir build-ps4 && cd build-ps4

# Avoid executable link tests – we only need a static library
cmake .. \
  -DCMAKE_C_COMPILER="$CC" \
  -DCMAKE_C_FLAGS="$CFLAGS" \
  -DCMAKE_AR="$AR" \
  -DCMAKE_RANLIB="$RANLIB" \
  -DCMAKE_INSTALL_PREFIX="$PS4_PREFIX" \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
  -DCMAKE_SYSTEM_NAME=Generic \
  -DCMAKE_SYSTEM_PROCESSOR=x86_64 \
  -DBUILD_SHARED_LIBS=OFF \
  -DFT_DISABLE_HARFBUZZ=ON \
  -DFT_DISABLE_BROTLI=ON \
  -DFT_DISABLE_BZIP2=ON \
  -DFT_DISABLE_PNG=ON \
  -DZLIB_LIBRARY="$PS4_PREFIX/lib/libz.a" \
  -DZLIB_INCLUDE_DIR="$PS4_PREFIX/include" \
  -DCMAKE_BUILD_TYPE=Release

cmake --build . -j"$(nproc)"
cmake --install .

echo "freetype OK"
