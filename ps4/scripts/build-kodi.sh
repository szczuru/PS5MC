#!/usr/bin/env bash
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
source "$here/env.sh"

prefix="$root/ps4/depends/prefix"

# Stage current libc++ headers for Kodi's C++20 code. LLVM 18 assumes a newer
# FreeBSD libm and xlocale.h than the OpenOrbis sysroot provides, so adapt the
# headers in the target prefix while leaving the host installation untouched.
host_libcxx="/usr/lib/llvm-18/include/c++/v1"
if [ -f "$host_libcxx/ranges" ]; then
  for target_prefix in "$prefix" "$OO_PS4_TOOLCHAIN/ps4mc-prefix"; do
    target_libcxx="$target_prefix/include/c++/v1"
    mkdir -p "$target_libcxx"
    cp -a "$host_libcxx/." "$target_libcxx/"
    python3 - "$target_libcxx/math.h" <<'PY'
from pathlib import Path
import re
import sys

path = Path(sys.argv[1])
source = path.read_text()
for name in ("isinf", "isnan"):
    declaration = f"using std::__math::{name};"
    marker = f"// Provided by OpenOrbis math.h: {name}."
    if declaration not in source and marker in source:
        continue
    if declaration not in source:
        raise SystemExit(f"Expected libc++ math declaration not found: {declaration}")
    source = source.replace(declaration, marker, 1)
path.write_text(source)

# OpenOrbis v0.5.4 ships a libc++abi older than LLVM 18's exception_ptr ABI.
# Keep LLVM 18's C++20 headers, but make exception_ptr use its pre-18 fallback
# (__cxa_allocate_exception + throw/catch), which the SDK runtime supports.
macro = "_LIBCPP_AVAILABILITY_HAS_INIT_PRIMARY_EXCEPTION"
patched = 0
definitions = []
availability_files = list(path.parent.rglob("availability.h"))
availability_files.extend(path.parent.rglob("__availability"))
for header in set(availability_files):
    source = header.read_text()
    pattern = rf"(?m)^(\s*#\s*define\s+{macro})\s+.*$"
    updated, count = re.subn(pattern, r"\1 0", source)
    if count:
        header.write_text(updated)
        patched += count
        definitions.append(str(header))

# LLVM 18 may keep availability settings in the extensionless __availability
# file. If the macro is absent, #if treats it as 0 and selects the ABI fallback.
if patched:
    print(f"Disabled {macro} in: {', '.join(definitions)}")
else:
    print(f"{macro} has no active definition; libc++ uses its ABI fallback")
PY
    install -m 0644 "$root/ps4/toolchain/compat/xlocale.h" "$target_prefix/include/xlocale.h"
    install -m 0644 "$root/ps4/toolchain/compat/pthread_np.h" "$target_prefix/include/pthread_np.h"
  done
fi

# The OpenOrbis SDK's libc++.a is built from an older libc++ than the LLVM 18
# headers Kodi needs. Build the matching static runtime and install it in the
# PS4 dependency prefix before rebuilding C++ dependencies and Kodi.
"$root/ps4/scripts/build-libcxx.sh"

export PKG_CONFIG_PATH="$prefix/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="$prefix/lib/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR=""

out="$(readlink -m "${1:-$root/../dist/kodi}")"
build="$root/ps4/build/kodi"
stage="$root/ps4/build/kodi-stage"

# FFmpeg writes detailed cross-link probe failures to config.log, but Ninja's
# ExternalProject wrapper otherwise hides the useful part of that file.
dump_ffmpeg_config_log_on_failure() {
  local status=$?
  if [ "$status" -ne 0 ] && [ -d "$build" ]; then
    while IFS= read -r -d '' log; do
      echo "[ps4] FFmpeg configure log: $log" >&2
      tail -n 160 "$log" >&2
    done < <(find "$build" -path '*/ffbuild/config.log' -type f -print0)
  fi
  return "$status"
}
trap dump_ffmpeg_config_log_on_failure EXIT

"$root/ps4/scripts/00-fetch-toolchain.sh"
# OpenOrbis v0.5.4's linker script only collects the unsuffixed .init_array
# input section. Clang emits priority constructors (including our pre-main
# logger) into .init_array.<priority>; without these entries the linker drops
# them even though their function symbols remain in the ELF. Patch the cached
# SDK script idempotently so priority constructors are sorted and retained.
python3 - "$OO_PS4_TOOLCHAIN/link.x" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
source = path.read_text()
old = "\t.init_array : {\n\t\t*(.init_array);\n\t}"
new = (
    "\t.init_array : {\n"
    "\t\tKEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*)))\n"
    "\t\tKEEP (*(.init_array))\n"
    "\t}"
)
if new in source:
    print("OpenOrbis linker script already retains priority constructors")
elif old in source:
    path.write_text(source.replace(old, new, 1))
    print("Patched OpenOrbis linker script to retain priority constructors")
else:
    raise SystemExit(f"Unexpected .init_array block in {path}; refusing to patch")
PY
# Test the patched SDK linker script with a tiny PS4 ELF before spending time
# rebuilding Kodi and its dependencies. This confirms both the section contents
# and the dynamic tags that the PS4 loader needs to invoke constructors.
readelf_tool="$(command -v llvm-readelf || command -v readelf || true)"
if [ -z "$readelf_tool" ]; then
  echo "ERROR: llvm-readelf/readelf is required to validate Kodi constructors" >&2
  exit 1
fi
ctor_probe="$build/constructor-array-probe"
mkdir -p "$ctor_probe"
cat > "$ctor_probe/probe.c" <<'EOF'
__attribute__((constructor(101))) static void early_constructor(void)
{
}

int main(void)
{
  return 0;
}
EOF
clang --target=x86_64-pc-freebsd12-elf -std=c11 -fPIC -funwind-tables \
  -isysroot "$OO_PS4_TOOLCHAIN" -isystem "$OO_PS4_TOOLCHAIN/include" \
  -c "$ctor_probe/probe.c" -o "$ctor_probe/probe.o"
clang --target=x86_64-pc-freebsd12-elf -nostdlib -fuse-ld=lld -pie \
  -Wl,-m,elf_x86_64 -Wl,--script,"$OO_PS4_TOOLCHAIN/link.x" \
  -Wl,--eh-frame-hdr -L"$OO_PS4_TOOLCHAIN/lib" \
  "$ctor_probe/probe.o" -lc -lkernel -lm "$OO_PS4_TOOLCHAIN/lib/crt1.o" \
  -o "$ctor_probe/probe.elf"
if ! "$readelf_tool" -W -d "$ctor_probe/probe.elf" | grep -E '\(INIT_ARRAY\)' >/dev/null || \
   ! "$readelf_tool" -W -d "$ctor_probe/probe.elf" | grep -E '\(INIT_ARRAYSZ\)[[:space:]]+(0x[1-9a-fA-F][0-9a-fA-F]*|[1-9][0-9]*)' >/dev/null; then
  echo "ERROR: patched linker script does not emit a non-empty constructor array" >&2
  "$readelf_tool" -W -S "$ctor_probe/probe.elf" | grep -E 'init_array|dynamic' || true
  "$readelf_tool" -W -d "$ctor_probe/probe.elf" || true
  exit 1
fi
echo "Verified PS4 linker script emits constructor array and dynamic tags"
"$root/ps4/scripts/build-depends.sh"
"$root/ps4/depends/scripts/build-harfbuzz.sh"
"$root/ps4/depends/scripts/build-fribidi.sh"
"$root/ps4/depends/scripts/build-fontconfig.sh"
"$root/ps4/depends/scripts/build-brotli.sh"
"$root/ps4/depends/scripts/build-nghttp2.sh"
"$root/ps4/depends/scripts/build-sqlite3.sh"
"$root/ps4/depends/scripts/build-tinyxml.sh"
"$root/ps4/depends/scripts/build-python3.sh"

# TexturePacker runs on the build host while generating Kodi skin assets.
host_tools="$root/ps4/build/host-texturepacker"
cmake -S "$root/tools/depends/native/TexturePacker/src" -B "$host_tools" \
  -DCMAKE_BUILD_TYPE=Release -DKODI_SOURCE_DIR="$root" -DARCH_DEFINES=-DTARGET_POSIX
cmake --build "$host_tools" --parallel "${PS4_BUILD_JOBS:-2}"

# Kodi also builds host executables (flatc and JsonSchemaBuilder) during the
# cross-build. Give those ExternalProjects an explicit native compiler.
native_prefix="$root/ps4/build/native-tools"
mkdir -p "$native_prefix/share"
cat > "$native_prefix/share/Toolchain-Native.cmake" <<'EOF'
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_C_COMPILER /usr/bin/cc)
set(CMAKE_CXX_COMPILER /usr/bin/c++)
set(CMAKE_AR /usr/bin/ar)
set(CMAKE_RANLIB /usr/bin/ranlib)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE NEVER)
EOF

# Install the PS4 platform sources before CMake scans the Kodi tree.
cp -a "$root/ps4/overlay/xbmc/platform/ps4" "$root/xbmc/platform/"
cp -a "$root/ps4/overlay/xbmc/windowing/ps4" "$root/xbmc/windowing/"

# OpenOrbis omits FreeBSD's DVD CSS ioctl header; use the matching FreeBSD 12 ABI shim.
cmake -S "$root" -B "$build" -U "Python3_*" -U "PYTHON_*" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$root/ps4/toolchain/ps4-kodi.cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/app0 \
  -DPS4_PREFIX="$prefix" \
  -DCMAKE_PREFIX_PATH="$prefix;$OO_PS4_TOOLCHAIN" \
  -DDEPENDS_DIR="$prefix" \
  -DDEPENDS_PATH="$prefix" \
  -DNATIVEPREFIX="$native_prefix" \
  -DENABLE_TESTING=OFF \
  -DENABLE_OPTICAL=ON \
  -DENABLE_DVDCSS=ON \
  -DENABLE_INTERNAL_LIBDVD=ON \
  -DENABLE_PYTHON=ON \
  -DENABLE_INTERNAL_SWIG=ON \
  -DPython3_USE_STATIC_LIBS=ON \
  -DPython3_INCLUDE_DIR:PATH="$prefix/include/python3.14" \
  -DPython3_LIBRARY:FILEPATH="$prefix/lib/libpython3.14.a" \
  -DENABLE_CEC=OFF \
  -DENABLE_INTERNAL_BLURAY=OFF \
  -DENABLE_AIRTUNES=OFF \
  -DENABLE_UPNP=OFF \
  -DENABLE_MYSQLCLIENT=OFF \
  -DENABLE_MICROHTTPD=OFF \
  -DOPENSSL_ROOT_DIR="$prefix" \
  -DOPENSSL_INCLUDE_DIR="$prefix/include" \
  -DOPENSSL_CRYPTO_LIBRARY="$prefix/lib/libcrypto.a" \
  -DOPENSSL_SSL_LIBRARY="$prefix/lib/libssl.a" \
  -DENABLE_INTERNAL_FFMPEG=ON \
  -DENABLE_MARIADBCLIENT=OFF \
  -DENABLE_INTERNAL_DAV1D=OFF \
  -DENABLE_INTERNAL_FSTRCMP=ON \
  -DENABLE_INTERNAL_TINYXML2=ON \
  -DWITH_TEXTUREPACKER="$host_tools/TexturePacker" \
  -DUSE_INTERNAL_LIBS=ON

# Kodi builds host flatc and the target library from the same archive. Seed
# the shared download cache first so parallel ExternalProject steps cannot
# overwrite the archive while verifying it.
flatbuffers_version_file="$root/tools/depends/native/flatbuffers/FLATBUFFERS-VERSION"
flatbuffers_version="$(sed -n 's/^VERSION=//p' "$flatbuffers_version_file")"
flatbuffers_sha512="$(sed -n 's/^SHA512=//p' "$flatbuffers_version_file")"
flatbuffers_archive="flatbuffers-${flatbuffers_version}.tar.gz"
flatbuffers_cache="$build/build/download/$flatbuffers_archive"
mkdir -p "$(dirname "$flatbuffers_cache")"
if ! printf '%s  %s\n' "$flatbuffers_sha512" "$flatbuffers_cache" | sha512sum --check --status; then
  flatbuffers_tmp="$flatbuffers_cache.tmp"
  curl -fL --retry 3 \
    "https://mirrors.kodi.tv/build-deps/sources/$flatbuffers_archive" \
    -o "$flatbuffers_tmp"
  printf '%s  %s\n' "$flatbuffers_sha512" "$flatbuffers_tmp" | sha512sum --check
  mv "$flatbuffers_tmp" "$flatbuffers_cache"
fi

# Keep building independent targets after a failure. One Actions run then
# reports multiple missing PS4 adaptations instead of stopping at the first.
cmake --build "$build" --parallel "${PS4_BUILD_JOBS:-2}" -- -k 0

# A green build is not enough if the early diagnostic entry point is dropped
# from the final ELF. Verify both the ordinary entry point and its constructor
# before producing a PKG that cannot help diagnose a pre-main crash.
nm_tool="$(command -v llvm-nm || command -v nm || true)"
if [ -z "$nm_tool" ]; then
  echo "ERROR: llvm-nm/nm is required to validate the PS4 ELF" >&2
  exit 1
fi
if ! "$nm_tool" -a "$build/kodi.elf" | grep -E '[[:space:]]main$' >/dev/null; then
  echo "ERROR: main is missing from $build/kodi.elf" >&2
  exit 1
fi
if ! "$nm_tool" -a -C "$build/kodi.elf" | grep -E '[[:space:]]PS4EarlyBootLog(\(\))?$' >/dev/null; then
  echo "ERROR: PS4EarlyBootLog is missing from $build/kodi.elf" >&2
  exit 1
fi
readelf_tool="$(command -v llvm-readelf || command -v readelf || true)"
if [ -z "$readelf_tool" ]; then
  echo "ERROR: llvm-readelf/readelf is required to validate Kodi constructors" >&2
  exit 1
fi
if ! "$readelf_tool" -W -d "$build/kodi.elf" | grep -E '\(INIT_ARRAY\)' >/dev/null || \
   ! "$readelf_tool" -W -d "$build/kodi.elf" | grep -E '\(INIT_ARRAYSZ\)[[:space:]]+(0x[1-9a-fA-F][0-9a-fA-F]*|[1-9][0-9]*)' >/dev/null; then
  echo "ERROR: kodi.elf has no non-empty constructor array" >&2
  exit 1
fi
echo "Verified main, the early boot logger, and a non-empty ELF constructor array"

rm -rf "$stage"
DESTDIR="$stage" cmake --install "$build"
mkdir -p "$stage/app0/lib/python3.14"
cp -a "$prefix/lib/python3.14"/. "$stage/app0/lib/python3.14/"
mkdir -p "$out"
"$here/make-pkg.sh" "$build/kodi.elf" "$out" \
  "Kodi PS4" PSMC00001 IV0000-PSMC00001_00-KODIPROBE0000000 01.00 "$stage/app0"
