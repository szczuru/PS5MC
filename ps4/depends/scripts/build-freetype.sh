#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/env-depends.sh"

"$ROOT/depends/scripts/build-zlib.sh"
"$ROOT/depends/scripts/build-libpng.sh"

VER=2.13.2
SRC="$PS4_BUILD/freetype-$VER"

if [ -f "$PS4_PREFIX/lib/libfreetype.a" ]; then
  echo "freetype already built"
  exit 0
fi

cd "$PS4_BUILD"
if [ ! -d "$SRC" ]; then
  curl -L -o "freetype-$VER.tar.xz" "https://download.savannah.gnu.org/releases/freetype/freetype-$VER.tar.xz"
  tar xf "freetype-$VER.tar.xz"
fi

cd "$SRC"
./configure --host="$HOST" --prefix="$PS4_PREFIX" \
  --enable-static --disable-shared \
  --without-harfbuzz --without-brotli --without-bzip2 \
  CPPFLAGS="-I$PS4_PREFIX/include" \
  LDFLAGS="-L$PS4_PREFIX/lib"
make -j$(nproc)
make install
echo "freetype OK"
