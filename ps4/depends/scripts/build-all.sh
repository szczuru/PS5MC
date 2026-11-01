#!/bin/bash
# Build the core depends set for PS4MC (M3)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SCRIPTS="$ROOT/depends/scripts"

echo "=========================================="
echo " PS4MC M3 – building depends"
echo "=========================================="

"$SCRIPTS/build-zlib.sh"
"$SCRIPTS/build-bzip2.sh"
"$SCRIPTS/build-libpng.sh"
"$SCRIPTS/build-freetype.sh"
"$SCRIPTS/build-openssl.sh"
"$SCRIPTS/build-curl.sh"
"$SCRIPTS/build-ffmpeg.sh"

echo ""
echo "=========================================="
echo " M3 depends finished"
echo " Prefix: $ROOT/depends/prefix"
echo "=========================================="
ls -la "$ROOT/depends/prefix/lib" | head -30
