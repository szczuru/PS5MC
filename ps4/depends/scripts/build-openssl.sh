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
    exit 1
  fi
  tar xf "$TARBALL"
fi

cd "$SRC"

# Put cross flags into CC so Configure does not see them as targets
CROSS_CC="$CC -target x86_64-pc-freebsd12-elf --sysroot=$OO_PS4_TOOLCHAIN -nostdinc ${INC_FLAGS_STR:-}"

./Configure BSD-generic64 \
  no-shared no-dso no-async no-tests no-ui-console no-asm \
  --prefix="$PS4_PREFIX" \
  --openssldir="$PS4_PREFIX/ssl" \
  -D__PS4__ -D__ORBIS__ \
  CC="$CROSS_CC" \
  AR="$AR" \
  RANLIB="$RANLIB"

make -j"$(nproc)" build_libs
make install_dev
echo "openssl OK"
