#!/usr/bin/env bash
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
source "$here/env.sh"

prefix="$root/ps4/depends/prefix"
export PKG_CONFIG_PATH="$prefix/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="$prefix/lib/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR=""

out="$(readlink -m "${1:-$root/../dist/kodi}")"
build="$root/ps4/build/kodi"
stage="$root/ps4/build/kodi-stage"

"$root/ps4/scripts/00-fetch-toolchain.sh"
"$root/ps4/scripts/build-depends.sh"
"$root/ps4/depends/scripts/build-harfbuzz.sh"
"$root/ps4/depends/scripts/build-fribidi.sh"
"$root/ps4/depends/scripts/build-fontconfig.sh"
"$root/ps4/depends/scripts/build-brotli.sh"
"$root/ps4/depends/scripts/build-nghttp2.sh"
"$root/ps4/depends/scripts/build-sqlite3.sh"
"$root/ps4/depends/scripts/build-tinyxml.sh"

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

cmake -S "$root" -B "$build" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$root/ps4/toolchain/ps4-kodi.cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/app0 \
  -DPS4_PREFIX="$prefix" \
  -DCMAKE_PREFIX_PATH="$prefix;$OO_PS4_TOOLCHAIN" \
  -DDEPENDS_DIR="$prefix" \
  -DDEPENDS_PATH="$prefix" \
  -DNATIVEPREFIX="$native_prefix" \
  -DENABLE_TESTING=OFF \
  -DENABLE_OPTICAL=OFF \
  -DENABLE_PYTHON=OFF \
  -DENABLE_CEC=OFF \
  -DENABLE_INTERNAL_BLURAY=OFF \
  -DENABLE_AIRTUNES=OFF \
  -DENABLE_UPNP=OFF \
  -DENABLE_INTERNAL_FFMPEG=ON \
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

rm -rf "$stage"
DESTDIR="$stage" cmake --install "$build"
mkdir -p "$out"
"$here/make-pkg.sh" "$build/kodi.elf" "$out" \
  "Kodi PS4" PSMC00001 IV0000-PSMC00001_00-KODIPROBE0000000 01.00 "$stage/app0"
