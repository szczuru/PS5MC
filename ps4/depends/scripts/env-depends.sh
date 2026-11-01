#!/bin/bash
# Common environment for PS4 depends builds (OpenOrbis)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

# Load base env
if [ -f "$ROOT/scripts/env.sh" ]; then
  # shellcheck source=/dev/null
  source "$ROOT/scripts/env.sh"
fi

export PS4_PREFIX="${PS4_PREFIX:-$ROOT/depends/prefix}"
export PS4_BUILD="${PS4_BUILD:-$ROOT/depends/build}"
mkdir -p "$PS4_PREFIX" "$PS4_BUILD"

export OO_PS4_TOOLCHAIN="${OO_PS4_TOOLCHAIN:-$HOME/OpenOrbis/PS4Toolchain}"
export PATH="$OO_PS4_TOOLCHAIN/bin/linux:$OO_PS4_TOOLCHAIN/bin:$PATH"

# Use toolchain clang if available, else system clang with heavy flags
if [ -x "$OO_PS4_TOOLCHAIN/bin/linux/clang" ]; then
  export CC="$OO_PS4_TOOLCHAIN/bin/linux/clang"
  export CXX="$OO_PS4_TOOLCHAIN/bin/linux/clang++"
elif [ -x "$OO_PS4_TOOLCHAIN/bin/clang" ]; then
  export CC="$OO_PS4_TOOLCHAIN/bin/clang"
  export CXX="$OO_PS4_TOOLCHAIN/bin/clang++"
else
  export CC="clang"
  export CXX="clang++"
fi

export AR="${AR:-llvm-ar}"
export RANLIB="${RANLIB:-llvm-ranlib}"
export STRIP="${STRIP:-llvm-strip}"
export NM="${NM:-llvm-nm}"

# Discover include roots inside the toolchain
INC_FLAGS=()
for d in \
  "$OO_PS4_TOOLCHAIN/include" \
  "$OO_PS4_TOOLCHAIN/target/include" \
  "$OO_PS4_TOOLCHAIN/usr/include" \
  "$OO_PS4_TOOLCHAIN/lib/clang/18/include" \
  "$OO_PS4_TOOLCHAIN/lib/clang/17/include" \
  "$OO_PS4_TOOLCHAIN/lib/clang/16/include"
do
  if [ -d "$d" ]; then
    INC_FLAGS+=(-isystem "$d")
  fi
done

# Hard isolation from host headers
export CFLAGS="-O2 -fPIC -target x86_64-pc-freebsd12-elf -D__PS4__ -DPS4 -D__ORBIS__ --sysroot=$OO_PS4_TOOLCHAIN -nostdinc ${INC_FLAGS[*]}"
export CXXFLAGS="$CFLAGS -fno-exceptions -fno-rtti"
export LDFLAGS="-L$OO_PS4_TOOLCHAIN/lib -L$PS4_PREFIX/lib --sysroot=$OO_PS4_TOOLCHAIN"
export PKG_CONFIG_PATH="$PS4_PREFIX/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="$PS4_PREFIX/lib/pkgconfig"

export HOST="x86_64-pc-freebsd"
export PREFIX="$PS4_PREFIX"

echo "[depends] PREFIX=$PS4_PREFIX"
echo "[depends] OO_PS4_TOOLCHAIN=$OO_PS4_TOOLCHAIN"
echo "[depends] CC=$CC"
echo "[depends] include dirs found:"
for d in "${INC_FLAGS[@]}"; do echo "  $d"; done
