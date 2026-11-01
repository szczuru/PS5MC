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

# TexturePacker runs on the build host while generating Kodi skin assets.
host_tools="$root/ps4/build/host-texturepacker"
cmake -S "$root/tools/depends/native/TexturePacker/src" -B "$host_tools" \
  -DCMAKE_BUILD_TYPE=Release -DKODI_SOURCE_DIR="$root" -DARCH_DEFINES=-DTARGET_POSIX
cmake --build "$host_tools" --parallel "${PS4_BUILD_JOBS:-2}"

# Install the PS4 platform sources before CMake scans the Kodi tree.
cp -a "$root/ps4/overlay/xbmc/platform/ps4" "$root/xbmc/platform/"

cmake -S "$root" -B "$build" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$root/ps4/toolchain/ps4-kodi.cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/app0 \
  -DPS4_PREFIX="$prefix" \
  -DCMAKE_PREFIX_PATH="$prefix;$OO_PS4_TOOLCHAIN" \
  -DDEPENDS_DIR="$prefix" \
  -DDEPENDS_PATH="$prefix" \
  -DENABLE_TESTING=OFF \
  -DENABLE_OPTICAL=OFF \
  -DENABLE_PYTHON=OFF \
  -DENABLE_INTERNAL_BLURAY=OFF \
  -DENABLE_AIRTUNES=OFF \
  -DENABLE_UPNP=OFF \
  -DENABLE_INTERNAL_FFMPEG=ON \
  -DENABLE_INTERNAL_FSTRCMP=ON \
  -DWITH_TEXTUREPACKER="$host_tools/TexturePacker" \
  -DUSE_INTERNAL_LIBS=ON

cmake --build "$build" --parallel "${PS4_BUILD_JOBS:-2}"

rm -rf "$stage"
DESTDIR="$stage" cmake --install "$build"
mkdir -p "$out"
"$here/make-pkg.sh" "$build/kodi.elf" "$out" \
  "Kodi PS4" PSMC00001 IV0000-PSMC00001_00-KODIPROBE0000000 01.00 "$stage/app0"
