#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/env-depends.sh"

VER=3.0.15
SRC="$PS4_BUILD/openssl-$VER"
TARBALL="openssl-$VER.tar.gz"

if [ -f "$PS4_PREFIX/lib/libssl.a" ] && [ -f "$PS4_PREFIX/lib/libcrypto.a" ]; then
  echo "openssl already built"
  exit 0
fi

cd "$PS4_BUILD"
if [ ! -d "$SRC" ]; then
  echo "Downloading openssl $VER ..."
  curl -fL --retry 3 -o "$TARBALL" \
    "https://github.com/openssl/openssl/releases/download/openssl-${VER}/openssl-${VER}.tar.gz" \
    || curl -fL --retry 3 -o "$TARBALL" \
    "https://www.openssl.org/source/openssl-${VER}.tar.gz"

  if ! gzip -t "$TARBALL" 2>/dev/null; then
    echo "ERROR: openssl tarball invalid"
    file "$TARBALL" || true
    exit 1
  fi
  tar xf "$TARBALL"
fi

cd "$SRC"
./Configure BSD-generic64 no-shared no-dso no-async no-tests no-ui-console \
  --prefix="$PS4_PREFIX" --openssldir="$PS4_PREFIX/ssl" \
  -D__PS4__ $CFLAGS

make -j"$(nproc)" build_libs
make install_dev
echo "openssl OK"
