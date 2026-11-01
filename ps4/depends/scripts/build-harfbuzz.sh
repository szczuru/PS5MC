#!/usr/bin/env bash
# Build the target HarfBuzz library required by libass for Kodi subtitles.
set -euo pipefail
source "$(dirname "$0")/env-depends.sh"

VER=14.2.0
SRC="$PS4_BUILD/harfbuzz-$VER"
TARBALL="harfbuzz-$VER.tar.xz"
BUILD="$SRC/build-ps4"
MESON_CROSS="$PS4_BUILD/ps4-meson-cross.ini"

if [ -f "$PS4_PREFIX/lib/libharfbuzz.a" ] && [ -f "$PS4_PREFIX/lib/pkgconfig/harfbuzz.pc" ]; then
  echo "harfbuzz already built"
  exit 0
fi

mkdir -p "$PS4_BUILD"
if [ ! -d "$SRC" ]; then
  cd "$PS4_BUILD"
  curl -fL --retry 3 -o "$TARBALL" \
    "https://github.com/harfbuzz/harfbuzz/releases/download/$VER/$TARBALL"
  tar xf "$TARBALL"
fi

# HarfBuzz 14.2.0 mistakenly asks for FreeType >= 12.0.6; the intended
# minimum is 2.12.0. The PS4 dependency build provides FreeType 2.13.2.
python3 - "$SRC/meson.build" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
source = path.read_text()
wrong = "freetype_min_version = '>= 12.0.6'"
fixed = "freetype_min_version = '>= 2.12.0'"
if wrong in source:
    path.write_text(source.replace(wrong, fixed, 1))
elif fixed not in source:
    raise SystemExit("Unexpected HarfBuzz FreeType version requirement")
PY

cat > "$MESON_CROSS" <<EOF
[binaries]
c = ['$CC']
cpp = ['$CXX']
ar = ['$AR']
strip = ['$STRIP']
pkg-config = ['pkg-config']
cmake = ['cmake']

[properties]
needs_exe_wrapper = true

[host_machine]
system = 'freebsd'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'

[built-in options]
c_args = ['-target', 'x86_64-pc-freebsd12-elf', '--sysroot=$OO_PS4_TOOLCHAIN', '-nostdinc', '-isystem', '$OO_PS4_TOOLCHAIN/include', '-isystem', '$PS4_PREFIX/include']
cpp_args = ['-target', 'x86_64-pc-freebsd12-elf', '--sysroot=$OO_PS4_TOOLCHAIN', '-nostdinc', '-isystem', '$OO_PS4_TOOLCHAIN/include', '-isystem', '$OO_PS4_TOOLCHAIN/include/c++/v1', '-isystem', '$PS4_PREFIX/include', '-include', 'stdlib.h']
c_link_args = ['--sysroot=$OO_PS4_TOOLCHAIN', '-nostdlib', '-fuse-ld=lld', '-pie', '-Wl,-m,elf_x86_64', '-Wl,--script,$OO_PS4_TOOLCHAIN/link.x', '-Wl,--eh-frame-hdr', '-L$OO_PS4_TOOLCHAIN/lib', '-L$PS4_PREFIX/lib', '-lc', '-lkernel', '-lSceSysmodule', '$OO_PS4_TOOLCHAIN/lib/crt1.o']
cpp_link_args = ['--sysroot=$OO_PS4_TOOLCHAIN', '-nostdlib', '-fuse-ld=lld', '-pie', '-Wl,-m,elf_x86_64', '-Wl,--script,$OO_PS4_TOOLCHAIN/link.x', '-Wl,--eh-frame-hdr', '-L$OO_PS4_TOOLCHAIN/lib', '-L$PS4_PREFIX/lib', '-lc++', '-lc', '-lkernel', '-lSceSysmodule', '$OO_PS4_TOOLCHAIN/lib/crt1.o']
EOF

export PKG_CONFIG_PATH="$PS4_PREFIX/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="$PS4_PREFIX/lib/pkgconfig"

# The PS4 FreeType CMake build installs the archive and headers but no .pc file.
# Meson uses pkg-config to locate this target dependency.
cat > "$PS4_PREFIX/lib/pkgconfig/freetype2.pc" <<EOF
prefix=$PS4_PREFIX
exec_prefix=\${prefix}
libdir=\${prefix}/lib
includedir=\${prefix}/include/freetype2

Name: FreeType
Description: FreeType font rendering library
Version: 2.13.2
Libs: -L\${libdir} -lfreetype -lz
Cflags: -I\${includedir} -I\${prefix}/include
EOF

echo "=== target FreeType pkg-config ==="
cat "$PS4_PREFIX/lib/pkgconfig/freetype2.pc"
pkg-config --modversion freetype2
pkg-config --cflags --libs freetype2

rm -rf "$BUILD"
if ! meson setup "$BUILD" "$SRC" \
  --cross-file "$MESON_CROSS" \
  --prefix "$PS4_PREFIX" \
  --libdir lib \
  --buildtype release \
  -Ddefault_library=static \
  -Db_staticpic=true \
  -Ddocs=disabled \
  -Dtests=disabled \
  -Dicu=disabled \
  -Dicu_builtin=false \
  -Dfreetype=enabled \
  -Dglib=disabled \
  -Dgobject=disabled \
  -Dintrospection=disabled \
  -Dutilities=disabled \
  -Dgpu=disabled \
  -Dgraphite2=disabled \
  -Dpkg_config_path="$PS4_PREFIX/lib/pkgconfig"; then
  echo "=== Meson configuration log ==="
  cat "$BUILD/meson-logs/meson-log.txt" || true
  exit 1
fi
ninja -C "$BUILD"
ninja -C "$BUILD" install
echo "harfbuzz OK -> $PS4_PREFIX/lib/libharfbuzz.a"
