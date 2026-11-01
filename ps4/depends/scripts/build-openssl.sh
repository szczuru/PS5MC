#!/bin/bash
# Build a minimal OpenSSL (or rather a very limited static one) for PS4.
# Full OpenSSL is painful; we use a stripped configure.
set -euo pipefail
source "$(dirname "$0")/env-depends.sh"

VER=3.0.14
SRC="$PS4_BUILD/openssl-$VER"

if [ -f "$PS4_PREFIX/lib/libssl.a" ] && [ -f "$PS4_PREFIX/lib/libcrypto.a" ]; then
  echo "openssl already built"
  exit 0
fi

cd "$PS4_BUILD"
if [ ! -d "$SRC" ]; then
  curl -L -o "openssl-$VER.tar.gz" "https://www.openssl.org/source/openssl-$VER.tar.gz"
  tar xf "openssl-$VER.tar.gz"
fi

cd "$SRC"
# BSD-generic64 is the closest target we can use with clang
./Configure BSD-generic64 no-shared no-dso no-async no-tests no-ui-console \
  --prefix="$PS4_PREFIX" --openssldir="$PS4_PREFIX/ssl" \
  -D__PS4__ $CFLAGS

make -j$(nproc) build_libs
make install_dev
echo "openssl OK"
