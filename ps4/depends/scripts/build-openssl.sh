#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/env-depends.sh"

VER=3.0.15
SRC="$PS4_BUILD/openssl-$VER"
TARBALL="openssl-$VER.tar.gz"
STAMP="$PS4_PREFIX/.openssl-$VER-ps4-sock-stdio"

if [ -f "$PS4_PREFIX/lib/libssl.a" ] && [ -f "$PS4_PREFIX/lib/libcrypto.a" ] && [ -f "$STAMP" ]; then
  echo "openssl already built"
  exit 0
fi

# Drop cached outputs built with the previous no-sock/no-stdio configuration.
if [ ! -f "$STAMP" ]; then
  rm -rf "$SRC" "$PS4_PREFIX/include/openssl"
  rm -f "$PS4_PREFIX/lib/libssl.a" "$PS4_PREFIX/lib/libcrypto.a" \
    "$PS4_PREFIX/lib/pkgconfig/openssl.pc" "$STAMP"
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

# OpenSSL sees the FreeBSD target triple, but OpenOrbis does not provide
# sys/sysctl.h or FreeBSD's KERN_ARND sysctl interface. Keep the standard
# /dev/urandom entropy source and omit only that unsupported FreeBSD path.
python3 - "$SRC/providers/implementations/rands/seeding/rand_unix.c" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
source = path.read_text()
replacements = (
    (
        "#if (defined(__FreeBSD__) || defined(__NetBSD__)) && !defined(OPENSSL_SYS_UEFI)",
        "#if (defined(__FreeBSD__) || defined(__NetBSD__)) && !defined(__PS4__) && !defined(OPENSSL_SYS_UEFI)",
    ),
    (
        "#  if (defined(__FreeBSD__) || defined(__NetBSD__)) && defined(KERN_ARND)",
        "#  if (defined(__FreeBSD__) || defined(__NetBSD__)) && !defined(__PS4__) && defined(KERN_ARND)",
    ),
    (
        "#  elif (defined(__FreeBSD__) || defined(__NetBSD__)) && defined(KERN_ARND)",
        "#  elif (defined(__FreeBSD__) || defined(__NetBSD__)) && !defined(__PS4__) && defined(KERN_ARND)",
    ),
)
for original, patched in replacements:
    if original in source:
        source = source.replace(original, patched, 1)
    elif patched not in source:
        raise SystemExit(f"Could not find expected OpenSSL source line: {original}")
path.write_text(source)
PY

cd "$SRC"

CROSS_CC="$CC -target x86_64-pc-freebsd12-elf --sysroot=$OO_PS4_TOOLCHAIN -nostdinc ${INC_FLAGS_STR:-}"

# Disable features unavailable in OpenOrbis or unused by Kodi:
# - sysctl is patched above; PS4 uses /dev/urandom for entropy
# - sock / dgram / dtls (bss_dgram IPv6 struct mismatch)
# - asm, shared, engines (devcrypto expects crypto/cryptodev.h), tests
./Configure BSD-generic64 \
  no-shared no-dso no-engine no-async no-tests no-ui-console no-asm \
  no-dgram no-dtls no-dtls1 no-dtls1_2 \
  no-afalgeng \
  --prefix="$PS4_PREFIX" \
  --openssldir="$PS4_PREFIX/ssl" \
  -D__PS4__ -D__ORBIS__ \
  CC="$CROSS_CC" \
  AR="$AR" \
  RANLIB="$RANLIB"

make -j"$(nproc)" build_libs
make install_dev
touch "$STAMP"
echo "openssl OK"
