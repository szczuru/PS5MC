#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/env-depends.sh"

"$ROOT/depends/scripts/build-zlib.sh"
"$ROOT/depends/scripts/build-openssl.sh"

VER=8.9.1
SRC="$PS4_BUILD/curl-$VER"

if [ -f "$PS4_PREFIX/lib/libcurl.a" ]; then
  echo "curl already built"
  exit 0
fi

cd "$PS4_BUILD"
if [ ! -d "$SRC" ]; then
  curl -L -o "curl-$VER.tar.xz" "https://curl.se/download/curl-$VER.tar.xz"
  tar xf "curl-$VER.tar.xz"
fi

cd "$SRC"
./configure --host="$HOST" --prefix="$PS4_PREFIX" \
  --enable-static --disable-shared \
  --with-zlib="$PS4_PREFIX" \
  --with-openssl="$PS4_PREFIX" \
  --disable-ldap --disable-rtsp --disable-dict --disable-telnet \
  --disable-tftp --disable-pop3 --disable-imap --disable-smb \
  --disable-smtp --disable-gopher --disable-mqtt \
  --without-libpsl --without-brotli --without-zstd --without-libidn2 \
  CPPFLAGS="-I$PS4_PREFIX/include" \
  LDFLAGS="-L$PS4_PREFIX/lib"
make -j$(nproc)
make install
echo "curl OK"
