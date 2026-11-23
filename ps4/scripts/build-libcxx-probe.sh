#!/usr/bin/env bash
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
source "$here/env.sh"

out="$(readlink -m "${1:-$root/../dist/libcxx-probe}")"
prefix="$root/ps4/depends/prefix"
tc="$OO_PS4_TOOLCHAIN"
clang_resource="$(clang++ --target=x86_64-pc-freebsd12-elf -print-resource-dir)"
compiler_rt="$(clang++ --target=x86_64-linux-gnu -rtlib=compiler-rt \
  -print-libgcc-file-name)"
mkdir -p "$out"

if [ ! -f "$prefix/lib/libc++.a" ] || [ ! -f "$prefix/lib/libc++abi.a" ] || \
   [ ! -f "$prefix/lib/libunwind.a" ]; then
  echo "ERROR: PS4 libc++ runtime has not been built" >&2
  exit 1
fi
if [ ! -f "$compiler_rt" ]; then
  echo "ERROR: compiler-rt builtins archive not found: $compiler_rt" >&2
  exit 1
fi

clang++ --target=x86_64-pc-freebsd12-elf -std=c++17 -fPIC -funwind-tables \
  -femulated-tls -fexceptions -fcxx-exceptions -nostdinc++ \
  -D_GNU_SOURCE -D__PS4__=1 -D_REENTRANT -D_FILE_OFFSET_BITS=64 \
  -D__STDC_CONSTANT_MACROS \
  -D_LIBCPP_HAS_NO_VENDOR_AVAILABILITY_ANNOTATIONS \
  -D_LIBCPP_DISABLE_AVAILABILITY -D_LIBCPP_DISABLE_EXTERN_TEMPLATE \
  -isystem "$prefix/include/c++/v1" \
  -isysroot "$tc" \
  -isystem "$root/ps4/toolchain/compat/include" \
  -isystem "$tc/include" \
  -isystem "$clang_resource/include" \
  -isystem "$prefix/include" \
  -c "$root/ps4/probe/libcxx_startup.cpp" -o "$out/libcxx-startup.o"

ld.lld -m elf_x86_64 -pie --script "$tc/link.x" --eh-frame-hdr \
  -L"$prefix/lib" -L"$tc/lib" \
  "$out/libcxx-startup.o" \
  -lc++ -lc++abi -lunwind "$compiler_rt" \
  --no-as-needed \
  -lSceVideoOut -lSceAudioOut -lScePad -lSceUserService \
  -lSceSystemService -lSceSysmodule -lScePigletv2VSH \
  -lSceNet -lkernel -lc -lm --as-needed "$tc/lib/crt1.o" \
  -o "$out/ps4-kodi-libcxx-startup-probe.elf"

nm_tool="$(command -v llvm-nm || command -v llvm-nm-18 || command -v nm || true)"
readelf_tool="$(command -v llvm-readelf || command -v llvm-readelf-18 || command -v readelf || true)"
if [ -z "$nm_tool" ] || [ -z "$readelf_tool" ]; then
  echo "ERROR: nm and readelf are required to validate the libc++ probe" >&2
  exit 1
fi
if ! "$nm_tool" -C "$out/ps4-kodi-libcxx-startup-probe.elf" | \
  grep -F 'std::__1::ios_base::Init::Init()' >/dev/null; then
  echo "ERROR: libc++ iostream initializer is missing from the probe" >&2
  exit 1
fi
if ! "$readelf_tool" -W -d "$out/ps4-kodi-libcxx-startup-probe.elf" | \
  grep -E '\(INIT_ARRAY\)' >/dev/null; then
  echo "ERROR: libc++ startup probe has no constructor array" >&2
  exit 1
fi
dynamic_info="$("$readelf_tool" -W -d "$out/ps4-kodi-libcxx-startup-probe.elf")"
for dependency in \
  libkernel.so libSceSysmodule.so libSceNet.so libScePigletv2VSH.so \
  libSceVideoOut.so libSceAudioOut.so libScePad.so libSceUserService.so \
  libSceSystemService.so libc.so libm.so; do
  if ! grep -F "Shared library: [$dependency]" <<<"$dynamic_info" >/dev/null; then
    echo "ERROR: libc++ startup probe is missing Kodi dependency $dependency" >&2
    exit 1
  fi
done

"$here/make-pkg.sh" "$out/ps4-kodi-libcxx-startup-probe.elf" "$out" \
  "Kodi libc++ startup probe" PSMC00011 \
  IV0000-PSMC00011_00-KODIPROBE0000000 01.00
