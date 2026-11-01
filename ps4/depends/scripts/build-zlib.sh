#!/bin/bash
# Build zlib for PS4
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
  # Prefer GitHub release (more reliable)
  curl -fL --retry 3 -o "$TARBALL" \
    "https://github.com/madler/zlib/releases/download/v${VER}/zlib-${VER}.tar.gz" \
    || curl -fL --retry 3 -o "$TARBALL" \
    "https://www.zlib.net/zlib-${VER}.tar.gz"

  # Sanity check – must be a real gzip
  if ! gzip -t "$TARBALL" 2>/dev/null; then
    echo "ERROR: downloaded file is not a valid gzip:"
    file "$TARBALL" || true
    head -c 200 "$TARBALL" || true
    exit 1
  fi

  tar xf "$TARBALL"
fi

cd "$SRC"
CHOST=$HOST ./configure --static --prefix="$PS4_PREFIX"
make -j"$(nproc)"
make install
echo "zlib OK -> $PS4_PREFIX/lib/libz.a"
