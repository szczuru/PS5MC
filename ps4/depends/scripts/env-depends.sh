#!/bin/bash
# Common environment for PS4 depends builds (OpenOrbis)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

if [ -f "$ROOT/scripts/env.sh" ]; then
  # shellcheck source=/dev/null
  source "$ROOT/scripts/env.sh"
fi

export PS4_PREFIX="${PS4_PREFIX:-$ROOT/depends/prefix}"
export PS4_BUILD="${PS4_BUILD:-$ROOT/depends/build}"
mkdir -p "$PS4_PREFIX" "$PS4_BUILD"

export OO_PS4_TOOLCHAIN="${OO_PS4_TOOLCHAIN:-$HOME/OpenOrbis/PS4Toolchain}"
export PATH="$OO_PS4_TOOLCHAIN/bin/linux:$OO_PS4_TOOLCHAIN/bin:$PATH"

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

if command -v llvm-ar >/dev/null 2>&1; then
  export AR="$(command -v llvm-ar)"
elif [ -x "$OO_PS4_TOOLCHAIN/bin/linux/llvm-ar" ]; then
  export AR="$OO_PS4_TOOLCHAIN/bin/linux/llvm-ar"
elif command -v ar >/dev/null 2>&1; then
  export AR="$(command -v ar)"
else
  export AR="ar"
fi

if command -v llvm-ranlib >/dev/null 2>&1; then
  export RANLIB="$(command -v llvm-ranlib)"
elif command -v ranlib >/dev/null 2>&1; then
  export RANLIB="$(command -v ranlib)"
else
  export RANLIB="ranlib"
fi

export STRIP="${STRIP:-strip}"
export NM="${NM:-nm}"

# Include roots: toolchain + clang builtins (emmintrin.h etc.)
INC_FLAGS=()
for d in \
  "$OO_PS4_TOOLCHAIN/include" \
  "$OO_PS4_TOOLCHAIN/target/include" \
  "$OO_PS4_TOOLCHAIN/usr/include"
do
  if [ -d "$d" ]; then
    INC_FLAGS+=(-isystem "$d")
  fi
done

# Clang resource dir (SSE/AVX intrinsics headers)
CLANG_RES="$($CC -print-resource-dir 2>/dev/null || true)"
if [ -n "$CLANG_RES" ] && [ -d "$CLANG_RES/include" ]; then
  INC_FLAGS+=(-isystem "$CLANG_RES/include")
fi
# Fallback common locations
for d in \
  /usr/lib/llvm-18/lib/clang/18/include \
  /usr/lib/llvm-17/lib/clang/17/include \
  /usr/lib/clang/18/include
do
  if [ -d "$d" ]; then
    INC_FLAGS+=(-isystem "$d")
    break
  fi
done

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
echo "[depends] AR=$AR"
echo "[depends] include dirs:"
printf '  %s\n' "${INC_FLAGS[@]}"
