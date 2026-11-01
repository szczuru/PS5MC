#!/bin/bash
# Build zlib for PS4
set -euo pipefail
source "$(dirname "$0")/env-depends.sh"

VER=1.3.1
SRC="$PS4_BUILD/zlib-$VER"
TARBALL="zlib-$VER.tar.gz"

if [ -f "$PS4_PREFIX/lib/libz.a" ]; then
  echo "zlib already built"
  exit 0
fi

cd "$PS4_BUILD"
if [ ! -d "$SRC" ]; then
  curl -L -o "$TARBALL" "https://zlib.net/$TARBALL" || \
  curl -L -o "$TARBALL" "https://github.com/madler/zlib/releases/download/v$VER/$TARBALL"
  tar xf "$TARBALL"
fi

cd "$SRC"
CHOST=$HOST ./configure --static --prefix="$PS4_PREFIX"
make -j$(nproc)
make install
echo "zlib OK -> $PS4_PREFIX/lib/libz.a"
