#!/usr/bin/env bash
# Build target SQLite required by Kodi's database and add-on subsystems.
set -euo pipefail
source "$(dirname "$0")/env-depends.sh"

VER=3390400
YEAR=2022
SRC="$PS4_BUILD/sqlite-autoconf-$VER"
TARBALL="sqlite-autoconf-$VER.tar.gz"
PATCH_DIR="$ROOT/../tools/depends/target/sqlite3"

if [ -f "$PS4_PREFIX/lib/libsqlite3.a" ] &&
   [ -f "$PS4_PREFIX/include/sqlite3.h" ]; then
  echo "sqlite3 already built"
  exit 0
fi

mkdir -p "$PS4_BUILD"
if [ ! -d "$SRC" ]; then
  cd "$PS4_BUILD"
  curl -fL --retry 3 -o "$TARBALL" "https://www.sqlite.org/$YEAR/$TARBALL"
  tar xf "$TARBALL"
fi

cd "$SRC"
if grep -q "bin_PROGRAMS = sqlite3" Makefile.am; then
  patch -p1 -i "$PATCH_DIR/001-Disable-sqlite3bin.patch"
fi
if ! grep -q "MAP_POPULATE" sqlite3.c; then
  patch -p1 -i "$PATCH_DIR/sqlite3.c.patch"
fi

autoreconf -vif
export CC CFLAGS="$CFLAGS -DSQLITE_TEMP_STORE=3 -DSQLITE_DEFAULT_MMAP_SIZE=0x10000000"
export LDFLAGS="$LDFLAGS -nostdlib -fuse-ld=lld -pie -Wl,-m,elf_x86_64 -Wl,--script,$OO_PS4_TOOLCHAIN/link.x -Wl,--eh-frame-hdr -lc -lkernel -lSceSysmodule $OO_PS4_TOOLCHAIN/lib/crt1.o"
if ! ./configure \
  --prefix="$PS4_PREFIX" \
  --host="$HOST" \
  --disable-shared --enable-static \
  --enable-threadsafe --disable-readline; then
  echo "SQLite configure failed; diagnostics from config.log:"
  tail -120 config.log
  exit 1
fi

make -j2 libsqlite3.la
make install
test -f "$PS4_PREFIX/lib/libsqlite3.a"
test -f "$PS4_PREFIX/include/sqlite3.h"
echo "sqlite3 OK -> $PS4_PREFIX/lib/libsqlite3.a"
