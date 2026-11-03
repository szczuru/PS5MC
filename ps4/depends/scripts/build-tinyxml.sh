#!/usr/bin/env bash
# Build target TinyXML required by Kodi's XML parsing.
set -euo pipefail
source "$(dirname "$0")/env-depends.sh"

VER=2.6.2_2
SRC="$PS4_BUILD/tinyxml-$VER"
TARBALL="tinyxml-$VER.tar.gz"

if [ -f "$PS4_PREFIX/lib/libtinyxml.a" ] &&
   [ -f "$PS4_PREFIX/include/tinyxml.h" ]; then
  echo "tinyxml already built"
  exit 0
fi

mkdir -p "$PS4_BUILD"
if [ ! -d "$SRC" ]; then
  cd "$PS4_BUILD"
  curl -fL --retry 3 -o "$TARBALL" \
    "https://mirrors.kodi.tv/build-deps/sources/$TARBALL"
  tar xf "$TARBALL"
fi

cd "$SRC"
autoreconf -vif
export CC CXX AR RANLIB
export CFLAGS="$CFLAGS -fPIC"
# Kodi uses TiXmlDocument RTTI, so TinyXML must emit typeinfo even though most
# PS4 third-party dependencies are built without it.
export CXXFLAGS="$CXXFLAGS -fPIC -frtti -isystem $OO_PS4_TOOLCHAIN/include/c++/v1 -include stdlib.h"
export LDFLAGS="$LDFLAGS -nostdlib -fuse-ld=lld -pie -Wl,-m,elf_x86_64 -Wl,--script,$OO_PS4_TOOLCHAIN/link.x -Wl,--eh-frame-hdr -lc++ -lc -lkernel -lSceSysmodule $OO_PS4_TOOLCHAIN/lib/crt1.o"

if ! ./configure --prefix="$PS4_PREFIX" --host="$HOST" --disable-shared; then
  echo "TinyXML configure failed; diagnostics from config.log:"
  tail -120 config.log
  exit 1
fi
make -j1 -C src
make -C src install

test -f "$PS4_PREFIX/lib/libtinyxml.a"
test -f "$PS4_PREFIX/include/tinyxml.h"
echo "tinyxml OK -> $PS4_PREFIX/lib/libtinyxml.a"
