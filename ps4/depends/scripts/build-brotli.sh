#!/usr/bin/env bash
# Build target Brotli libraries required by the PS4 static curl build.
set -euo pipefail
source "$(dirname "$0")/env-depends.sh"

VER=1.2.0
SRC="$PS4_BUILD/brotli-$VER"
BUILD="$SRC/build-ps4"
TARBALL="brotli-$VER.tar.gz"
TOOLCHAIN_FILE="$ROOT/toolchain/ps4-kodi.cmake"
PATCH="$ROOT/../tools/depends/target/brotli/01-all-cmake-install-config.patch"

if [ -f "$PS4_PREFIX/lib/libbrotlicommon.a" ] &&
   [ -f "$PS4_PREFIX/lib/libbrotlidec.a" ] &&
   [ -f "$PS4_PREFIX/lib/libbrotlienc.a" ] &&
   [ -f "$PS4_PREFIX/lib/cmake/brotli/brotli-config.cmake" ]; then
  echo "brotli already built"
  exit 0
fi

mkdir -p "$PS4_BUILD"
if [ ! -d "$SRC" ]; then
  cd "$PS4_BUILD"
  curl -fL --retry 3 -o "$TARBALL" \
    "https://github.com/google/brotli/archive/refs/tags/v$VER.tar.gz"
  tar xf "$TARBALL"
fi

cd "$SRC"
if ! grep -q "BROTLI_GENERATED_DIR" CMakeLists.txt; then
  patch -p1 -i "$PATCH"
fi

rm -rf "$BUILD"
cmake -S "$SRC" -B "$BUILD" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
  -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
  -DOO_PS4_TOOLCHAIN="$OO_PS4_TOOLCHAIN" -DPS4_PREFIX="$PS4_PREFIX" \
  -DCMAKE_INSTALL_PREFIX="$PS4_PREFIX" -DCMAKE_INSTALL_LIBDIR=lib \
  -DCMAKE_INSTALL_INCLUDEDIR=include \
  -DBUILD_SHARED_LIBS=OFF \
  -DBROTLI_DISABLE_TESTS=ON \
  -DBROTLI_BUILD_TOOLS=OFF \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" --parallel 2
cmake --install "$BUILD"

test -f "$PS4_PREFIX/lib/libbrotlicommon.a"
test -f "$PS4_PREFIX/lib/libbrotlidec.a"
test -f "$PS4_PREFIX/lib/libbrotlienc.a"
echo "brotli OK -> $PS4_PREFIX/lib/libbrotlidec.a"
