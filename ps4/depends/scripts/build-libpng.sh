#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/env-depends.sh"

# requires zlib
"$ROOT/depends/scripts/build-zlib.sh"

VER=1.6.43
SRC="$PS4_BUILD/libpng-$VER"

if [ -f "$PS4_PREFIX/lib/libpng16.a" ] || [ -f "$PS4_PREFIX/lib/libpng.a" ]; then
  echo "libpng already built"
  exit 0
fi

cd "$PS4_BUILD"
if [ ! -d "$SRC" ]; then
  curl -L -o "libpng-$VER.tar.xz" "https://download.sourceforge.net/libpng/libpng-$VER.tar.xz"
  tar xf "libpng-$VER.tar.xz"
fi

cd "$SRC"
./configure --host="$HOST" --prefix="$PS4_PREFIX" \
  --enable-static --disable-shared \
  --with-zlib-prefix="$PS4_PREFIX" \
  CPPFLAGS="-I$PS4_PREFIX/include" \
  LDFLAGS="-L$PS4_PREFIX/lib"
make -j$(nproc)
make install
echo "libpng OK"
