#!/usr/bin/env bash
# Cross-build a static CPython for Kodi's embedded add-on engine.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
source "$root/depends/scripts/env-depends.sh"
if command -v ccache >/dev/null 2>&1; then
  export CC="ccache $CC"
fi

version_file="$root/../tools/depends/target/python3/PYTHON3-VERSION"
version="$(sed -n 's/^VERSION=//p' "$version_file")"
abi_version="${version%.*}"
sha512="$(sed -n 's/^SHA512=//p' "$version_file")"
build_root="$PS4_BUILD/python3-$version"
source_dir="$build_root/Python-$version"
archive="Python-$version.tar.xz"
archive_path="$PS4_BUILD/$archive"
host_build="$build_root/build-host"
target_build="$build_root/build-ps4"
jobs="${PS4_BUILD_JOBS:-4}"

mkdir -p "$build_root"
if ! printf '%s  %s\n' "$sha512" "$archive_path" | sha512sum --check --status; then
  curl -fL --retry 3 "https://www.python.org/ftp/python/$version/$archive" -o "$archive_path.tmp"
  printf '%s  %s\n' "$sha512" "$archive_path.tmp" | sha512sum --check
  mv "$archive_path.tmp" "$archive_path"
fi

rm -rf "$source_dir" "$host_build" "$target_build"
mkdir -p "$source_dir"
tar -xf "$archive_path" --strip-components=1 -C "$source_dir"
patch -d "$source_dir" -p1 -i "$root/depends/patches/python3/cross-freebsd.patch"
patch -d "$source_dir" -p1 -i "$root/depends/patches/python3/no-subprocess.patch"
patch -d "$source_dir" -p1 -i "$root/depends/patches/python3/static-compile.patch"
patch -d "$source_dir" -p1 -i "$root/depends/patches/python3/freebsd-no-sysctl.patch"
patch -d "$source_dir" -p1 -i "$root/depends/patches/python3/orbis-endian.patch"
patch -d "$source_dir" -p1 -i "$root/depends/patches/python3/orbis-memzero.patch"
patch -d "$source_dir" -p1 -i "$root/depends/patches/python3/orbis-getnameinfo.patch"

# Orbis dirent does not provide a usable d_ino; use CPython's unknown inode
# sentinel when constructing scandir entries.
python3 - "$source_dir/Modules/posixmodule.c" <<'PY'
from pathlib import Path
import sys

source = Path(sys.argv[1])
text = source.read_text()
old = "name_len, direntp->d_ino"
if text.count(old) != 1:
    raise SystemExit(f"expected one CPython dirent inode expression in {source}")
source.write_text(text.replace(old, "name_len, 0", 1))
PY

# CPython 3.14 needs a matching native interpreter to generate its target
# sysconfig and checked-hash stdlib bytecode.
mkdir -p "$host_build"
(
  cd "$host_build"
  env CC=/usr/bin/cc CXX=/usr/bin/c++ AR=/usr/bin/ar RANLIB=/usr/bin/ranlib \
    CFLAGS= CPPFLAGS= LDFLAGS= \
    "$source_dir/configure" --without-ensurepip --disable-test-modules
  env CC=/usr/bin/cc CXX=/usr/bin/c++ AR=/usr/bin/ar RANLIB=/usr/bin/ranlib \
    CFLAGS= CPPFLAGS= LDFLAGS= make -j"$jobs" python sharedmods
)

modules=(
  py_cv_module_grp=n/a py_cv_module_syslog=n/a py_cv_module__dbm=n/a
  py_cv_module__gdbm=n/a py_cv_module__uuid=n/a py_cv_module_readline=n/a
  py_cv_module__curses=n/a py_cv_module__curses_panel=n/a py_cv_module_xx=n/a
  py_cv_module_xxlimited=n/a py_cv_module_xxlimited_35=n/a
  py_cv_module_xxsubtype=n/a py_cv_module__xxsubinterpreters=n/a
  py_cv_module__tkinter=n/a py_cv_module__codecs_jp=n/a
  py_cv_module__codecs_kr=n/a py_cv_module__codecs_tw=n/a
  py_cv_module__decimal=n/a py_cv_module__zstd=n/a py_cv_module__lzma=n/a
  py_cv_module__ctypes=n/a py_cv_module__posixsubprocess=n/a
  py_cv_module__posixshmem=n/a py_cv_module__multiprocessing=n/a
)
cache=(
  ac_cv_func_wait3=no ac_cv_func_wait4=no ac_cv_func_waitpid=no
  ac_cv_func_execv=no ac_cv_func_fexecve=no ac_cv_func_posix_spawn=no
  ac_cv_func_posix_spawnp=no ac_cv_func_forkpty=no ac_cv_lib_util_forkpty=no
  ac_cv_func_system=no ac_cv_func_fork=no ac_cv_func_fork1=no
  ac_cv_func_vfork=no ac_cv_func_pthread_getname_np=no
  ac_cv_func_pthread_setname_np=no ac_cv_func_getentropy=yes
  ac_cv_func_chflags=no
  ac_cv_func_getaddrinfo=yes
  ac_cv_buggy_getaddrinfo=no
  ac_cv_file__dev_ptmx=no ac_cv_file__dev_ptc=no
  ac_cv_header_stropts_h=no
  ac_cv_working_openssl_ssl=yes ac_cv_working_openssl_hashlib=yes
)

mkdir -p "$target_build"
(
  cd "$target_build"
  env "${cache[@]}" \
    CPPFLAGS="-I$PS4_PREFIX/include" \
    LDFLAGS="$LDFLAGS -nostdlib -fuse-ld=lld -pie -Wl,-m,elf_x86_64 -Wl,--script,$OO_PS4_TOOLCHAIN/link.x -Wl,--eh-frame-hdr -L$OO_PS4_TOOLCHAIN/lib -L$PS4_PREFIX/lib" \
    LIBS="-lc -lkernel -lSceSysmodule -lm $OO_PS4_TOOLCHAIN/lib/crt1.o" \
    "$source_dir/configure" \
    --prefix=/app0 --host=x86_64-pc-freebsd --build=x86_64-pc-linux-gnu \
    --disable-shared --without-ensurepip --disable-framework --without-mimalloc --without-pymalloc \
    --with-build-python="$host_build/python" --with-system-expat=no \
    --with-openssl="$PS4_PREFIX" --disable-test-modules \
    MODULE_BUILDTYPE=static "${modules[@]}"

  test -f pyconfig.h || { echo "CPython configure did not produce pyconfig.h" >&2; exit 1; }
  grep -q '^#define HAVE_GETENTROPY 1' pyconfig.h || {
    echo "CPython target config lacks getentropy; Python startup would be unreliable" >&2
    exit 1
  }
  grep -q '^#define HAVE_CLOCK_GETTIME 1' pyconfig.h || {
    echo "CPython target config lacks clock_gettime; Python startup would be unreliable" >&2
    exit 1
  }
  make -j"$jobs" libpython"$abi_version".a pybuilddir.txt
)

install -d "$PS4_PREFIX/lib" "$PS4_PREFIX/lib/python$abi_version"
install -m 0644 "$target_build/libpython$abi_version.a" "$PS4_PREFIX/lib/"
install -d "$PS4_PREFIX/include/python$abi_version"
cp -a "$source_dir/Include"/. "$PS4_PREFIX/include/python$abi_version/"
install -m 0644 "$target_build/pyconfig.h" "$PS4_PREFIX/include/python$abi_version/pyconfig.h"
(cd "$source_dir/Lib" && tar --exclude=test --exclude=tests --exclude=idlelib \
  --exclude=tkinter --exclude=turtledemo --exclude=ensurepip \
  --exclude=__pycache__ -cf - .) | tar -xf - -C "$PS4_PREFIX/lib/python$abi_version/"
sysconfig_data="$(find "$target_build" -name '_sysconfigdata*.py' -type f -print -quit)"
test -n "$sysconfig_data" || { echo "CPython target sysconfig module was not generated" >&2; exit 1; }
install -m 0644 "$sysconfig_data" "$PS4_PREFIX/lib/python$abi_version/"
"$host_build/python" -m compileall -q -j"$jobs" \
  --invalidation-mode checked-hash "$PS4_PREFIX/lib/python$abi_version"

echo "CPython $version static library and standard library installed in $PS4_PREFIX"
