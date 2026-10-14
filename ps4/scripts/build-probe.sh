#!/usr/bin/env bash
# Buduje program testowy PS4MC Probe -> dist/probe/*.pkg
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$here/env.sh"
"$here/00-fetch-toolchain.sh"

root="$(cd "$here/.." && pwd)"   # ps4/
out="${1:-$root/../dist/probe}"; mkdir -p "$out"; out="$(readlink -f "$out")"
tc="$OO_PS4_TOOLCHAIN"

CFLAGS=(
  --target=x86_64-pc-freebsd12-elf -std=c11 -D_GNU_SOURCE -fPIC -funwind-tables
  -isysroot "$tc" -isystem "$tc/include"
)

echo "==> kompilacja probe v3 (pure C, bez C++)..."
clang "${CFLAGS[@]}" -c "$root/probe/main.c" -o "$out/probe.o"

echo "==> linkowanie (bez libc++)..."
ld.lld -m elf_x86_64 -pie --script "$tc/link.x" --eh-frame-hdr \
  -L"$tc/lib" \
  "$out/probe.o" \
  -lc -lkernel -lSceSysmodule \
  -lSceUserService -lScePad -lSceAudioOut \
  "$tc/lib/crt1.o" -o "$out/ps4mc-probe.elf"
ls -la "$out/ps4mc-probe.elf"

echo "==> paczka PKG..."
"$here/make-pkg.sh" "$out/ps4mc-probe.elf" "$out" \
  "PS4MC Probe" PSMC00001 \
  IV0000-PSMC00001_00-KODIPROBE0000000 01.00
