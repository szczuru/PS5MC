#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/env-depends.sh"

"$ROOT/depends/scripts/build-zlib.sh"
"$ROOT/depends/scripts/build-openssl.sh"

VER=8.10.1
SRC="$PS4_BUILD/curl-$VER"
TARBALL="curl-$VER.tar.xz"

if [ -f "$PS4_PREFIX/lib/libcurl.a" ]; then
  echo "curl already built"
  exit 0
fi

cd "$PS4_BUILD"
if [ ! -d "$SRC" ]; then
  echo "Downloading curl $VER ..."
  curl -fL --retry 3 -o "$TARBALL" \
    "https://curl.se/download/curl-${VER}.tar.xz"

  tar xf "$TARBALL"
fi

cd "$SRC"
if ! ./configure --host="$HOST" --prefix="$PS4_PREFIX" \
  --enable-static --disable-shared --disable-tool \\
  --with-zlib="$PS4_PREFIX" \
  --with-openssl="$PS4_PREFIX" \
  --disable-ldap --disable-rtsp --disable-dict --disable-telnet \
  --disable-tftp --disable-pop3 --disable-imap --disable-smb \
  --disable-smtp --disable-gopher --disable-mqtt \
  --without-libpsl --without-brotli --without-zstd --without-libidn2 \
  CPPFLAGS="-D_DEFAULT_SOURCE -D_BSD_SOURCE -DSOL_IP=IPPROTO_IP -I$PS4_PREFIX/include" \
  LDFLAGS="-nostdlib -fuse-ld=lld -pie -Wl,-m,elf_x86_64 -Wl,--script,$OO_PS4_TOOLCHAIN/link.x --sysroot=$OO_PS4_TOOLCHAIN -L$OO_PS4_TOOLCHAIN/lib -L$PS4_PREFIX/lib" \
  LIBS="-lc -lkernel -lSceSysmodule -lSceNet $OO_PS4_TOOLCHAIN/lib/crt1.o"; then
  echo "curl configure failed; relevant diagnostics from config.log:"
  grep -n -B 6 -A 8 -E 'gethostbyname|error:|undefined reference' config.log | tail -120 || tail -120 config.log
  exit 1
fi
make -j"$(nproc)"
make install
echo "curl OK"
