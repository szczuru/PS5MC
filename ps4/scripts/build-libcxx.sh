#!/usr/bin/env bash
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
source "$here/env.sh"

prefix="$root/ps4/depends/prefix"
llvm_source="$root/ps4/build/llvm-project"
build="$root/ps4/build/libcxx"
llvm_ref="llvmorg-18.1.3"

if [ ! -d "$llvm_source/.git" ]; then
  mkdir -p "$(dirname "$llvm_source")"
  git clone --filter=blob:none --depth 1 --sparse --branch "$llvm_ref" \
    https://github.com/llvm/llvm-project.git "$llvm_source"
  git -C "$llvm_source" sparse-checkout set llvm cmake runtimes libcxx libcxxabi libunwind
fi

# Keep the LLVM 18 headers and runtime on the OpenOrbis-compatible exception
# ABI. The same guarded header edits are applied to the staged installation.
python3 - "$llvm_source/libcxx/include" <<'PY'
from pathlib import Path
import re
import sys

include = Path(sys.argv[1])
math_header = include / "math.h"
source = math_header.read_text()
for name in ("isinf", "isnan"):
    declaration = f"using std::__math::{name};"
    marker = f"// Provided by OpenOrbis math.h: {name}."
    if declaration in source:
        source = source.replace(declaration, marker, 1)
    elif marker not in source:
        raise SystemExit(f"Expected libc++ math declaration not found: {declaration}")
math_header.write_text(source)

macro = "_LIBCPP_AVAILABILITY_HAS_INIT_PRIMARY_EXCEPTION"
for header in (include / "__availability", include / "availability.h"):
    if not header.exists():
        continue
    source = header.read_text()
    updated, count = re.subn(
        rf"(?m)^(\s*#\s*define\s+{macro})\s+.*$", r"\1 0", source
    )
    if count:
        header.write_text(updated)
        break
else:
    raise SystemExit(f"No active definition found for {macro} in LLVM libc++ headers")
PY

cmake -S "$llvm_source/runtimes" -B "$build" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$root/ps4/toolchain/libcxx-ps4.cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$prefix" \
  -DPS4_PREFIX="$prefix" \
  -DLLVM_ENABLE_RUNTIMES=libcxx \
  -DLIBCXX_ENABLE_SHARED=OFF \
  -DLIBCXX_ENABLE_STATIC=ON \
  -DLIBCXX_ENABLE_STATIC_ABI_LIBRARY=OFF \
  -DLIBCXX_ENABLE_ABI_LINKER_SCRIPT=OFF \
  -DLIBCXX_CXX_ABI=libcxxabi \
  -DLIBCXX_CXX_ABI_INCLUDE_PATHS="$llvm_source/libcxxabi/include" \
  -DLIBCXX_ABI_VERSION=1 \
  -DLIBCXX_ENABLE_EXCEPTIONS=ON \
  -DLIBCXX_ENABLE_RTTI=ON \
  -DLIBCXX_INCLUDE_TESTS=OFF \
  -DLIBCXX_INCLUDE_BENCHMARKS=OFF \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache

cmake --build "$build" --target install-cxx --parallel "${PS4_BUILD_JOBS:-2}"

# Install the two SDK compatibility headers alongside the new libc++ headers.
for target_prefix in "$prefix" "$OO_PS4_TOOLCHAIN/ps4mc-prefix"; do
  install -m 0644 "$root/ps4/toolchain/compat/xlocale.h" \
    "$target_prefix/include/xlocale.h"
  install -m 0644 "$root/ps4/toolchain/compat/pthread_np.h" \
    "$target_prefix/include/pthread_np.h"
done
echo "Built LLVM libc++ for PS4 and installed it under $prefix"
