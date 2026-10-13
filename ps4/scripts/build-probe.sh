#!/usr/bin/env bash
# Buduje program testowy PS4MC Probe -> dist/probe/*.pkg
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$here/env.sh"
"$here/00-fetch-toolchain.sh"

root="$(cd "$here/.." && pwd)"   # ps4/
out="${1:-$root/../dist/probe}"; mkdir -p "$out"; out="$(readlink -f "$out")"
tc="$OO_PS4_TOOLCHAIN"
XCAPE="$root/libXcape"

CXXFLAGS=(
  --target=x86_64-pc-freebsd12-elf -std=c++17 -D_GNU_SOURCE -fPIC -funwind-tables
  -isysroot "$tc" -isystem "$tc/include" -isystem "$tc/include/c++/v1"
  -I"$XCAPE/include" -I"$root/src"
)

echo "==> patch libXcape relokacji..."
  python3 "$here/patch-xcape.py" "$XCAPE/lib/libXcape.a"
  echo "==> kompilacja probe..."
clang++ "${CXXFLAGS[@]}" -c "$root/probe/main.cpp"       -o "$out/probe.o"
clang++ "${CXXFLAGS[@]}" -c "$root/src/PS4Privileges.cpp" -o "$out/priv.o"

echo "==> linkowanie..."
ld.lld -m elf_x86_64 -pie --script "$tc/link.x" --eh-frame-hdr \
  -L"$tc/lib" -L"$XCAPE/lib" \
  "$out/probe.o" "$out/priv.o" \
  -lXcape -lc -lkernel -lc++ -lSceSysmodule \
  -lSceUserService -lScePad -lSceAudioOut \
  -lScePigletv2VSH -lScePrecompiledShaders \
  "$tc/lib/crt1.o" -o "$out/ps4mc-probe.elf"
ls -la "$out/ps4mc-probe.elf"

echo "==> paczka PKG..."
"$here/make-pkg.sh" "$out/ps4mc-probe.elf" "$out" \
  "PS4MC Probe" PSMC00001 \
  IV0000-PSMC00001_00-KODIPROBE0000000 01.00
