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

# Use cmake if available (more reliable for cross), else minimal autotools with cache
if command -v cmake >/dev/null 2>&1; then
  rm -rf build-ps4
  mkdir build-ps4 && cd build-ps4
  cmake .. \
    -DCMAKE_C_COMPILER="$CC" \
    -DCMAKE_C_FLAGS="$CFLAGS" \
    -DCMAKE_AR="$AR" \
    -DCMAKE_RANLIB="$RANLIB" \
    -DCMAKE_INSTALL_PREFIX="$PS4_PREFIX" \
    -DBUILD_SHARED_LIBS=OFF \
    -DFT_DISABLE_HARFBUZZ=ON \
    -DFT_DISABLE_BROTLI=ON \
    -DFT_DISABLE_BZIP2=ON \
    -DFT_REQUIRE_ZLIB=ON \
    -DZLIB_LIBRARY="$PS4_PREFIX/lib/libz.a" \
    -DZLIB_INCLUDE_DIR="$PS4_PREFIX/include" \
    -DPNG_LIBRARY="$PS4_PREFIX/lib/libpng16.a" \
    -DPNG_PNG_INCLUDE_DIR="$PS4_PREFIX/include" \
    -DCMAKE_SYSTEM_NAME=FreeBSD \
    -DCMAKE_SYSTEM_PROCESSOR=x86_64
  cmake --build . -j"$(nproc)"
  cmake --install .
else
  echo "cmake not found, skipping freetype for now"
  exit 1
fi

echo "freetype OK"
