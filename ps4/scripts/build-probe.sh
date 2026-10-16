#!/usr/bin/env bash
# Buduje PS4MC Probe -> dist/probe/*.pkg
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$here/env.sh"
"$here/00-fetch-toolchain.sh"

root="$(cd "$here/.." && pwd)"
out="${1:-$root/../dist/probe}"; mkdir -p "$out"; out="$(readlink -f "$out")"
tc="$OO_PS4_TOOLCHAIN"
XCAPE="$root/libXcape"

# Patch relokacji libXcape (R_PLT32_BND -> R_PLT32)
python3 "$here/patch-xcape.py" "$XCAPE/lib/libXcape.a"

CFLAGS=(
  --target=x86_64-pc-freebsd12-elf -std=c11 -D_GNU_SOURCE -fPIC -funwind-tables
  -isysroot "$tc" -isystem "$tc/include"
  -I"$XCAPE/include"
)

echo "==> kompilacja probe v4..."
clang "${CFLAGS[@]}" -c "$root/probe/main.c" -o "$out/probe.o"

echo "==> linkowanie..."
ld.lld -m elf_x86_64 -pie --script "$tc/link.x" --eh-frame-hdr \
  -L"$tc/lib" -L"$XCAPE/lib" \
  "$out/probe.o" \
  --whole-archive "$XCAPE/lib/libXcape.a" --no-whole-archive \
  -lc -lkernel -lm \
  -lSceSysmodule -lSceUserService -lScePad \
  -lSceAudioOut -lSceSystemService \
  "$tc/lib/crt1.o" -o "$out/ps4mc-probe.elf"
ls -la "$out/ps4mc-probe.elf"

echo "==> PKG..."
"$here/make-pkg.sh" "$out/ps4mc-probe.elf" "$out" \
  "PS4MC Probe" PSMC00001 \
  IV0000-PSMC00001_00-KODIPROBE0000000 01.00
