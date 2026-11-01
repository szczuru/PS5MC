#!/usr/bin/env bash
# Build target Expat and Fontconfig required by libass/Kodi subtitle rendering.
set -euo pipefail
source "$(dirname "$0")/env-depends.sh"

EXPAT_VER=2.8.1
FONTCONFIG_VER=2.15.0
EXPAT_SRC="$PS4_BUILD/expat-$EXPAT_VER"
EXPAT_BUILD="$EXPAT_SRC/build-ps4"
FONTCONFIG_SRC="$PS4_BUILD/fontconfig-$FONTCONFIG_VER"
FONTCONFIG_BUILD="$FONTCONFIG_SRC/build-ps4"
MESON_CROSS="$PS4_BUILD/ps4-fontconfig-cross.ini"
CLANG_RESOURCE="$("$CC" -print-resource-dir)"
TOOLCHAIN_FILE="$ROOT/toolchain/ps4-kodi.cmake"

mkdir -p "$PS4_BUILD" "$PS4_PREFIX/lib/pkgconfig"

if [ ! -f "$PS4_PREFIX/lib/libexpat.a" ] || [ ! -f "$PS4_PREFIX/lib/pkgconfig/expat.pc" ]; then
  if [ ! -d "$EXPAT_SRC" ]; then
    cd "$PS4_BUILD"
    curl -fL --retry 3 -o "expat-$EXPAT_VER.tar.xz" \
      "https://github.com/libexpat/libexpat/releases/download/R_2_8_1/expat-$EXPAT_VER.tar.xz"
    tar xf "expat-$EXPAT_VER.tar.xz"
  fi
  rm -rf "$EXPAT_BUILD"
  cmake -S "$EXPAT_SRC" -B "$EXPAT_BUILD" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
    -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
    -DOO_PS4_TOOLCHAIN="$OO_PS4_TOOLCHAIN" -DPS4_PREFIX="$PS4_PREFIX" \
    -DCMAKE_INSTALL_PREFIX="$PS4_PREFIX" -DCMAKE_INSTALL_LIBDIR=lib \
    -DCMAKE_INSTALL_INCLUDEDIR=include \
    -DEXPAT_BUILD_TOOLS=OFF -DEXPAT_BUILD_EXAMPLES=OFF \
    -DEXPAT_BUILD_TESTS=OFF -DEXPAT_SHARED_LIBS=OFF \
    -DEXPAT_BUILD_DOCS=OFF -DEXPAT_BUILD_PKGCONFIG=ON \
    -DCMAKE_BUILD_TYPE=Release
  cmake --build "$EXPAT_BUILD" --parallel "${PS4_BUILD_JOBS:-2}"
  cmake --install "$EXPAT_BUILD"
fi

cat > "$MESON_CROSS" <<EOF
[binaries]
c = ['$CC']
ar = ['$AR']
strip = ['$STRIP']
pkg-config = ['pkg-config']

[properties]
needs_exe_wrapper = true

[host_machine]
system = 'freebsd'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'

[built-in options]
c_args = ['-fPIC', '-target', 'x86_64-pc-freebsd12-elf', '--sysroot=$OO_PS4_TOOLCHAIN', '-nostdinc', '-isystem', '$OO_PS4_TOOLCHAIN/include', '-isystem', '$CLANG_RESOURCE/include', '-isystem', '$PS4_PREFIX/include']
c_link_args = ['--sysroot=$OO_PS4_TOOLCHAIN', '-nostdlib', '-fuse-ld=lld', '-pie', '-Wl,-m,elf_x86_64', '-Wl,--script,$OO_PS4_TOOLCHAIN/link.x', '-Wl,--eh-frame-hdr', '-L$OO_PS4_TOOLCHAIN/lib', '-L$PS4_PREFIX/lib', '-lc', '-lkernel', '-lSceSysmodule', '$OO_PS4_TOOLCHAIN/lib/crt1.o']
EOF

export PKG_CONFIG_PATH="$PS4_PREFIX/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="$PS4_PREFIX/lib/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR=""

if [ ! -f "$PS4_PREFIX/lib/libfontconfig.a" ] || [ ! -f "$PS4_PREFIX/include/fontconfig/fontconfig.h" ]; then
  if [ ! -d "$FONTCONFIG_SRC" ]; then
    cd "$PS4_BUILD"
    curl -fL --retry 3 -o "fontconfig-$FONTCONFIG_VER.tar.xz" \
      "https://www.freedesktop.org/software/fontconfig/release/fontconfig-$FONTCONFIG_VER.tar.xz"
    tar xf "fontconfig-$FONTCONFIG_VER.tar.xz"
  fi
  rm -rf "$FONTCONFIG_BUILD"
  if ! meson setup "$FONTCONFIG_BUILD" "$FONTCONFIG_SRC" \
    --cross-file "$MESON_CROSS" --prefix "$PS4_PREFIX" --libdir lib \
    --buildtype release -Ddoc=disabled -Dtests=disabled -Dtools=disabled \
    -Dcache-build=disabled -Ddefault_library=static; then
    echo "=== Fontconfig Meson configuration log ==="
    cat "$FONTCONFIG_BUILD/meson-logs/meson-log.txt" || true
    exit 1
  fi
  ninja -C "$FONTCONFIG_BUILD"
  ninja -C "$FONTCONFIG_BUILD" install
fi

test -f "$PS4_PREFIX/lib/libexpat.a"
test -f "$PS4_PREFIX/lib/libfontconfig.a"
test -f "$PS4_PREFIX/include/fontconfig/fontconfig.h"
echo "fontconfig OK -> $PS4_PREFIX/lib/libfontconfig.a"
