#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/env-depends.sh"

VER=1.3.1
SRC="$PS4_BUILD/zlib-$VER"
TARBALL="zlib-$VER.tar.gz"

if [ -f "$PS4_PREFIX/lib/libz.a" ]; then
  echo "zlib already built"
  exit 0
fi

cd "$PS4_BUILD"

if [ ! -d "$SRC" ]; then
  echo "Downloading zlib $VER ..."
  curl -fL --retry 3 -o "$TARBALL" \
    "https://github.com/madler/zlib/releases/download/v${VER}/zlib-${VER}.tar.gz"

  if ! gzip -t "$TARBALL" 2>/dev/null; then
    echo "ERROR: not a valid gzip"
    file "$TARBALL" || true
    exit 1
  fi
  tar xf "$TARBALL"
fi

cd "$SRC"

# Hand-write a minimal Makefile to avoid broken configure detection under cross
cat > Makefile.ps4 << MAKE
CC = $CC
CFLAGS = $CFLAGS -DNO_FSEEKO -DNO_STRERROR -DHAVE_HIDDEN
AR = $AR
RANLIB = $RANLIB

OBJS = adler32.o crc32.o deflate.o infback.o inffast.o inflate.o inftrees.o trees.o zutil.o compress.o uncompr.o gzclose.o gzlib.o gzread.o gzwrite.o

all: libz.a

libz.a: \$(OBJS)
	\$(AR) rcs \$@ \$(OBJS)
	\$(RANLIB) \$@

%.o: %.c
	\$(CC) \$(CFLAGS) -I. -c -o \$@ \$<

clean:
	rm -f \$(OBJS) libz.a
MAKE

make -f Makefile.ps4 -j"$(nproc)"

install -d "$PS4_PREFIX/lib" "$PS4_PREFIX/include"
install -m 644 libz.a "$PS4_PREFIX/lib/"
install -m 644 zlib.h zconf.h "$PS4_PREFIX/include/"
echo "zlib OK -> $PS4_PREFIX/lib/libz.a"
