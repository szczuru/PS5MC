#!/usr/bin/env bash
# Buduje PS4MC Probe -> dist/probe/*.pkg
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$here/env.sh"
"$here/00-fetch-toolchain.sh"

root="$(cd "$here/.." && pwd)"
out="${1:-$root/../dist/probe}"; mkdir -p "$out"; out="$(readlink -f "$out")"
tc="$OO_PS4_TOOLCHAIN"

CFLAGS=(
  --target=x86_64-pc-freebsd12-elf -std=c11 -D_GNU_SOURCE -fPIC -funwind-tables
  -isysroot "$tc" -isystem "$tc/include"
)

echo "==> kompilacja probe v12..."
clang "${CFLAGS[@]}" -c "$root/probe/main.c" -o "$out/probe.o"

# The probe uses the same priority-101 constructor as Kodi. OpenOrbis v0.5.4
# drops priority-suffixed .init_array inputs unless its linker script retains
# and sorts them.
python3 - "$tc/link.x" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
source = path.read_text()
old = "\t.init_array : {\n\t\t*(.init_array);\n\t}"
new = (
    "\t.init_array : {\n"
    "\t\tKEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*)))\n"
    "\t\tKEEP (*(.init_array))\n"
    "\t}"
)
if new in source:
    print("OpenOrbis linker script already retains priority constructors")
elif old in source:
    path.write_text(source.replace(old, new, 1))
    print("Patched OpenOrbis linker script for the startup probe")
else:
    raise SystemExit(f"Unexpected .init_array block in {path}; refusing to patch")
PY

echo "==> linkowanie..."
ld.lld -m elf_x86_64 -pie --script "$tc/link.x" --eh-frame-hdr \
  -L"$tc/lib" \
  "$out/probe.o" \
  -lc -lkernel -lm \
  -lSceSysmodule -lSceUserService -lScePad \
  -lSceAudioOut -lSceSystemService -lSceVideoOut \
  -lScePigletv2VSH \
  "$tc/lib/crt1.o" -o "$out/ps4-kodi-startup-probe.elf"
ls -la "$out/ps4-kodi-startup-probe.elf"

echo "==> PKG..."
"$here/make-pkg.sh" "$out/ps4-kodi-startup-probe.elf" "$out" \
  "Kodi startup probe" PSMC00009 \
  IV0000-PSMC00009_00-KODIPROBE0000000 01.00
