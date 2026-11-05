#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/env-depends.sh"

VER=2.3.0
SRC="$PS4_BUILD/libcdio-$VER"
TARBALL="libcdio-$VER.tar.bz2"
SHA512="42e1659df57b558f5bfc81badf137682793070d1301fdf4926d221eca1fffc8d1b912d23ebe0a2e84a9bbd832f257033ef854b5349cf80fc8184bce759800d6c"

if [ -f "$PS4_PREFIX/lib/libcdio.a" ] && [ -f "$PS4_PREFIX/include/cdio/cdio.h" ]; then
  echo "libcdio already built"
  exit 0
fi

cd "$PS4_BUILD"
if [ ! -d "$SRC" ]; then
  echo "Downloading libcdio $VER ..."
  curl -fL --retry 3 -o "$TARBALL" \
    "https://mirrors.kodi.tv/build-deps/sources/$TARBALL"
  printf '%s  %s\n' "$SHA512" "$TARBALL" | sha512sum --check
  tar xf "$TARBALL"
fi

cd "$SRC"
autoreconf -vif
CC="$CC -target x86_64-pc-freebsd12-elf --sysroot=$OO_PS4_TOOLCHAIN -nostdinc ${INC_FLAGS_STR:-}" \
  CFLAGS="$CFLAGS -I$PS4_PREFIX/include" \
  LDFLAGS="$LDFLAGS -nostdlib -fuse-ld=lld -pie -Wl,-m,elf_x86_64 -Wl,--script,$OO_PS4_TOOLCHAIN/link.x -Wl,--eh-frame-hdr -L$OO_PS4_TOOLCHAIN/lib -L$PS4_PREFIX/lib" \
  LIBS="-lkernel -lc -lm $OO_PS4_TOOLCHAIN/lib/crt1.o" \
  ./configure --build="$(gcc -dumpmachine)" --host="$HOST" --prefix="$PS4_PREFIX" \
    --disable-shared --enable-static \
    --with-cd-drive=no --with-cd-info=no --with-cdda-player=no --with-cd-read=no \
    --with-iso-info=no --with-iso-read=no --disable-example-progs \
    --disable-cpp-progs --disable-cxx || {
      echo "libcdio configure failed; config.log follows:" >&2
      tail -n 160 config.log >&2
      exit 1
    }

make -j"$(nproc)"
make install
echo "libcdio OK -> $PS4_PREFIX/lib/libcdio.a"
