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

# OpenOrbis reports a FreeBSD target triple but does not implement FreeBSD's
# private umtx syscall API. Keep libc++'s portable timed-backoff implementation
# for atomic wait/notify on PS4 rather than including sys/umtx.h.
atomic_source = include.parent / "src" / "atomic.cpp"
source = atomic_source.read_text()
source, include_count = re.subn(
    r"(?m)^#elif defined\(__FreeBSD__\)$",
    "#elif defined(__FreeBSD__) && !defined(__PS4__)",
    source,
    count=1,
)
source, implementation_count = re.subn(
    r"(?m)^#elif defined\(__FreeBSD__\) && __SIZEOF_LONG__ == 8$",
    "#elif defined(__FreeBSD__) && !defined(__PS4__) && __SIZEOF_LONG__ == 8",
    source,
    count=1,
)
if include_count == 0 and "#elif defined(__FreeBSD__) && !defined(__PS4__)" in source:
    include_count = 1
if implementation_count == 0 and "#elif defined(__FreeBSD__) && !defined(__PS4__) && __SIZEOF_LONG__ == 8" in source:
    implementation_count = 1
if include_count != 1 or implementation_count != 1:
    raise SystemExit("Could not isolate libc++ FreeBSD umtx code for PS4")
atomic_source.write_text(source)

# OpenOrbis presents FreeBSD headers but its libc is musl. Do not select
# FreeBSD-only locale entry points for this target; libc++'s musl locale
# compatibility layer provides the C-locale fallbacks instead.
config_header = include / "__config"
source = config_header.read_text()
old_locale = "#  if defined(__APPLE__) || defined(__FreeBSD__) || defined(_LIBCPP_MSVCRT_LIKE) || defined(__NetBSD__)"
new_locale = "#  if defined(__APPLE__) || (defined(__FreeBSD__) && !defined(__PS4__)) || defined(_LIBCPP_MSVCRT_LIKE) || defined(__NetBSD__)"
if old_locale in source:
    source = source.replace(old_locale, new_locale, 1)
elif new_locale not in source:
    raise SystemExit("Could not configure libc++ locale extensions for PS4")
old_freebsd = "#  if defined(__APPLE__) || defined(__FreeBSD__)"
new_freebsd = "#  if defined(__APPLE__) || (defined(__FreeBSD__) && !defined(__PS4__))"
source, freebsd_locale_count = re.subn(
    rf"(?m)^{re.escape(old_freebsd)}\s*$", new_freebsd, source
)
if freebsd_locale_count == 0 and new_freebsd not in source:
    raise SystemExit("Could not disable FreeBSD rune locale assumptions for PS4")

# OpenOrbis does not provide arc4random. libc++'s /dev/urandom backend uses
# open/read/close, which the SDK supports and Kodi's std::random_device needs.
lines = source.splitlines()
random_condition = "#  if defined(__APPLE__) || (defined(__FreeBSD__) && !defined(__PS4__)) || defined(__NetBSD__) || defined(__OpenBSD__) || " + chr(92)
condition_patched = False
for index, line in enumerate(lines[:-1]):
    if line.startswith(
        "#  if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) ||"
    ) and lines[index + 1].strip() == "defined(__DragonFly__)":
        lines[index] = random_condition
        condition_patched = True
        break
    if line == random_condition and lines[index + 1].strip() == "defined(__DragonFly__)":
        condition_patched = True
        break
if not condition_patched:
    raise SystemExit("Could not select libc++ random_device backend for PS4")

ps4_random = ["#  elif defined(__PS4__)", "#    define _LIBCPP_USING_DEV_RANDOM"]
if ps4_random[0] not in lines:
    try:
        wasi_index = lines.index("#  elif defined(__wasi__) || defined(__EMSCRIPTEN__)")
    except ValueError as exc:
        raise SystemExit("Could not locate libc++ random backend selection") from exc
    lines[wasi_index:wasi_index] = ps4_random
source = "\n".join(lines) + "\n"
config_header.write_text(source)

filesystem_source = include.parent / "src" / "filesystem" / "operations.cpp"
source = filesystem_source.read_text()
old_sendfile = "#if __has_include(<sys/sendfile.h>)"
new_sendfile = "#if __has_include(<sys/sendfile.h>) && !defined(__PS4__)"
if old_sendfile in source:
    source = source.replace(old_sendfile, new_sendfile, 1)
elif new_sendfile not in source:
    raise SystemExit("Could not select libc++'s portable file-copy path for PS4")
filesystem_source.write_text(source)
PY

cmake -S "$llvm_source/runtimes" -B "$build" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$root/ps4/toolchain/libcxx-ps4.cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$prefix" \
  -DPS4_PREFIX="$prefix" \
  -DLLVM_ENABLE_RUNTIMES="libunwind;libcxxabi;libcxx" \
  -DLIBCXX_ENABLE_SHARED=OFF \
  -DLIBCXX_ENABLE_STATIC=ON \
  -DLIBCXX_ENABLE_STATIC_ABI_LIBRARY=ON \
  -DLIBCXX_ENABLE_ABI_LINKER_SCRIPT=OFF \
  -DLIBCXX_CXX_ABI=libcxxabi \
  -DLIBCXX_ABI_VERSION=1 \
  -DLIBCXX_HAS_MUSL_LIBC=ON \
  -DLIBCXX_ENABLE_EXCEPTIONS=ON \
  -DLIBCXX_ENABLE_RTTI=ON \
  -DLIBCXX_INCLUDE_TESTS=OFF \
  -DLIBCXX_INCLUDE_BENCHMARKS=OFF \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache

cmake --build "$build" --target install --parallel "${PS4_BUILD_JOBS:-2}" -- -k 0

# ExternalProject dependencies use the OpenOrbis prefix rather than PS4_PREFIX.
# Keep both prefixes on the same headers and runtime to avoid mixed libc++ ABIs.
sdk_prefix="$OO_PS4_TOOLCHAIN/ps4mc-prefix"
rm -rf "$sdk_prefix/include/c++/v1"
mkdir -p "$sdk_prefix/include/c++/v1" "$sdk_prefix/lib"
cp -a "$prefix/include/c++/v1/." "$sdk_prefix/include/c++/v1/"
install -m 0644 "$prefix/lib/libc++.a" "$sdk_prefix/lib/libc++.a"
for runtime in libc++abi libunwind; do
  if [ ! -f "$prefix/lib/$runtime.a" ]; then
    echo "Expected LLVM runtime archive is missing: $prefix/lib/$runtime.a" >&2
    exit 1
  fi
  install -m 0644 "$prefix/lib/$runtime.a" "$sdk_prefix/lib/$runtime.a"
done

# Install the two SDK compatibility headers alongside the new libc++ headers.
for target_prefix in "$prefix" "$OO_PS4_TOOLCHAIN/ps4mc-prefix"; do
  install -m 0644 "$root/ps4/toolchain/compat/xlocale.h" \
    "$target_prefix/include/xlocale.h"
  install -m 0644 "$root/ps4/toolchain/compat/pthread_np.h" \
    "$target_prefix/include/pthread_np.h"
done
echo "Built LLVM libc++ for PS4 and installed it under $prefix"
