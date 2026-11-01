#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/env-depends.sh"

VER=1.0.8
SRC="$PS4_BUILD/bzip2-$VER"
TARBALL="bzip2-$VER.tar.gz"

if [ -f "$PS4_PREFIX/lib/libbz2.a" ]; then
  echo "bzip2 already built"
  exit 0
fi

cd "$PS4_BUILD"
if [ ! -d "$SRC" ]; then
  echo "Downloading bzip2 $VER ..."
  curl -fL --retry 3 -o "$TARBALL" \
    "https://sourceware.org/pub/bzip2/bzip2-$VER.tar.gz"

  if ! gzip -t "$TARBALL" 2>/dev/null; then
    echo "ERROR: bzip2 tarball invalid"
    file "$TARBALL" || true
    exit 1
  fi
  tar xf "$TARBALL"
fi

cd "$SRC"
make -j"$(nproc)" libbz2.a \
  CC="$CC" CFLAGS="$CFLAGS" AR="$AR" RANLIB="$RANLIB"
install -d "$PS4_PREFIX/lib" "$PS4_PREFIX/include"
install -m 644 libbz2.a "$PS4_PREFIX/lib/"
install -m 644 bzlib.h "$PS4_PREFIX/include/"
echo "bzip2 OK"
