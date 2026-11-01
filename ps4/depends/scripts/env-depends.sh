#!/bin/bash
# Common environment for PS4 depends builds (OpenOrbis)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT/scripts/env.sh"

# Where we install the cross-compiled libraries
export PS4_PREFIX="${PS4_PREFIX:-$ROOT/depends/prefix}"
export PS4_BUILD="${PS4_BUILD:-$ROOT/depends/build}"
mkdir -p "$PS4_PREFIX" "$PS4_BUILD"

# OpenOrbis toolchain paths (set by env.sh / 00-fetch-toolchain.sh)
export OO_PS4_TOOLCHAIN="${OO_PS4_TOOLCHAIN:-$ROOT/toolchain/OpenOrbis/PS4Toolchain}"
export PATH="$OO_PS4_TOOLCHAIN/bin:$PATH"

export CC="clang"
export CXX="clang++"
export AR="llvm-ar"
export RANLIB="llvm-ranlib"
export STRIP="llvm-strip"
export NM="llvm-nm"

# Flags that work with OpenOrbis clang
export CFLAGS="-O2 -fPIC -target x86_64-pc-freebsd12-elf -isysroot $OO_PS4_TOOLCHAIN -D__PS4__ -DPS4"
export CXXFLAGS="$CFLAGS -fno-exceptions -fno-rtti"
export LDFLAGS="-L$OO_PS4_TOOLCHAIN/lib -L$PS4_PREFIX/lib"
export PKG_CONFIG_PATH="$PS4_PREFIX/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="$PS4_PREFIX/lib/pkgconfig"

export HOST="x86_64-pc-freebsd"
export PREFIX="$PS4_PREFIX"

echo "[depends] PREFIX=$PS4_PREFIX"
echo "[depends] OO_PS4_TOOLCHAIN=$OO_PS4_TOOLCHAIN"
