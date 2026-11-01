#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/env-depends.sh"

VER=1.0.8
SRC="$PS4_BUILD/bzip2-$VER"

if [ -f "$PS4_PREFIX/lib/libbz2.a" ]; then
  echo "bzip2 already built"
  exit 0
fi

cd "$PS4_BUILD"
if [ ! -d "$SRC" ]; then
  curl -L -o "bzip2-$VER.tar.gz" "https://sourceware.org/pub/bzip2/bzip2-$VER.tar.gz"
  tar xf "bzip2-$VER.tar.gz"
fi

cd "$SRC"
make -j$(nproc) libbz2.a \
  CC="$CC" CFLAGS="$CFLAGS" AR="$AR" RANLIB="$RANLIB"
install -d "$PS4_PREFIX/lib" "$PS4_PREFIX/include"
install -m 644 libbz2.a "$PS4_PREFIX/lib/"
install -m 644 bzlib.h "$PS4_PREFIX/include/"
echo "bzip2 OK"
