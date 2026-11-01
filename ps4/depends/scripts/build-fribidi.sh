#!/usr/bin/env bash
# Build target FriBidi required by libass and Kodi subtitle layout.
set -euo pipefail
source "$(dirname "$0")/env-depends.sh"

VER=1.0.16
SRC="$PS4_BUILD/fribidi-$VER"
TARBALL="fribidi-$VER.tar.xz"
BUILD="$SRC/build-ps4"
MESON_CROSS="$PS4_BUILD/ps4-fribidi-cross.ini"
CLANG_RESOURCE="$("$CC" -print-resource-dir)"

if [ -f "$PS4_PREFIX/lib/libfribidi.a" ] && [ -f "$PS4_PREFIX/lib/pkgconfig/fribidi.pc" ]; then
  echo "fribidi already built"
  exit 0
fi

mkdir -p "$PS4_BUILD"
if [ ! -d "$SRC" ]; then
  cd "$PS4_BUILD"
  curl -fL --retry 3 -o "$TARBALL" \
    "https://github.com/fribidi/fribidi/releases/download/v$VER/$TARBALL"
  tar xf "$TARBALL"
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
c_args = ['-target', 'x86_64-pc-freebsd12-elf', '--sysroot=$OO_PS4_TOOLCHAIN', '-nostdinc', '-isystem', '$OO_PS4_TOOLCHAIN/include', '-isystem', '$CLANG_RESOURCE/include', '-isystem', '$PS4_PREFIX/include']
c_link_args = ['--sysroot=$OO_PS4_TOOLCHAIN', '-nostdlib', '-fuse-ld=lld', '-pie', '-Wl,-m,elf_x86_64', '-Wl,--script,$OO_PS4_TOOLCHAIN/link.x', '-Wl,--eh-frame-hdr', '-L$OO_PS4_TOOLCHAIN/lib', '-L$PS4_PREFIX/lib', '-lc', '-lkernel', '-lSceSysmodule', '$OO_PS4_TOOLCHAIN/lib/crt1.o']
EOF

export PKG_CONFIG_PATH="$PS4_PREFIX/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="$PS4_PREFIX/lib/pkgconfig"

rm -rf "$BUILD"
meson setup "$BUILD" "$SRC" \
  --cross-file "$MESON_CROSS" \
  --prefix "$PS4_PREFIX" \
  --libdir lib \
  --buildtype release \
  -Ddocs=false \
  -Dtests=false \
  -Dbin=false \
  -Ddefault_library=static
ninja -C "$BUILD"
ninja -C "$BUILD" install
echo "fribidi OK -> $PS4_PREFIX/lib/libfribidi.a"
