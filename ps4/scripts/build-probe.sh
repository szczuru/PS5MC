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

echo "==> kompilacja sondy importu libSceNet..."
clang "${CFLAGS[@]}" -c "$root/probe/net_dependency.c" -o "$out/net-dependency.o"
ld.lld -m elf_x86_64 -pie --script "$tc/link.x" --eh-frame-hdr \
  -L"$tc/lib" \
  "$out/net-dependency.o" \
  -lc -lkernel -lm \
  --no-as-needed -lSceNet --as-needed \
  "$tc/lib/crt1.o" -o "$out/ps4-kodi-net-dependency-probe.elf"
readelf_tool="$(command -v llvm-readelf || command -v llvm-readelf-18 || command -v readelf || true)"
if [ -z "$readelf_tool" ] || ! "$readelf_tool" -W -d "$out/ps4-kodi-net-dependency-probe.elf" | \
  grep -F 'Shared library: [libSceNet.so]' >/dev/null; then
  echo "ERROR: net dependency probe does not require libSceNet.so" >&2
  exit 1
fi
"$here/make-pkg.sh" "$out/ps4-kodi-net-dependency-probe.elf" "$out" \
  "Kodi Net dependency probe" PSMC00010 \
  IV0000-PSMC00010_00-KODIPROBE0000000 01.00

echo "==> kompilacja minimalnego C++/TLS startup probe..."
clang++ --target=x86_64-pc-freebsd12-elf -std=c++17 -fPIC -funwind-tables \
  -ftls-model=global-dynamic \
  -fno-exceptions -fno-rtti -nostdinc++ \
  -isysroot "$tc" -isystem "$tc/include" \
  -c "$root/probe/cxx_startup.cpp" -o "$out/cxx-startup.o"
ld.lld -m elf_x86_64 -pie --script "$tc/link.x" --eh-frame-hdr \
  -L"$tc/lib" \
  "$out/cxx-startup.o" \
  -lc -lkernel -lm \
  "$tc/lib/crt1.o" -o "$out/ps4-kodi-cxx-startup-probe.elf"
nm_tool="$(command -v llvm-nm || command -v llvm-nm-18 || command -v nm || true)"
if [ -z "$nm_tool" ]; then
  echo "ERROR: nm is required to validate the dynamic TLS probe" >&2
  exit 1
fi
if ! "$nm_tool" -a "$out/ps4-kodi-cxx-startup-probe.elf" | grep -F '__tls_get_addr' >/dev/null; then
  echo "ERROR: global-dynamic TLS probe does not reference __tls_get_addr" >&2
  exit 1
fi
"$here/make-pkg.sh" "$out/ps4-kodi-cxx-startup-probe.elf" "$out" \
  "Kodi C++ startup probe" PSMC00008 \
  IV0000-PSMC00008_00-KODIPROBE0000000 01.00

echo "==> kompilacja emulowanego TLS probe..."
clang++ --target=x86_64-pc-freebsd12-elf -std=c++17 -fPIC -funwind-tables \
  -femulated-tls \
  -fno-exceptions -fno-rtti -nostdinc++ \
  -isysroot "$tc" -isystem "$tc/include" \
  -c "$root/probe/cxx_startup.cpp" -o "$out/cxx-emulated-tls.o"
compiler_rt="$(clang++ --target=x86_64-linux-gnu -rtlib=compiler-rt \
  -print-libgcc-file-name)"
if [ ! -f "$compiler_rt" ]; then
  echo "ERROR: compiler-rt builtins archive not found: $compiler_rt" >&2
  exit 1
fi
ld.lld -m elf_x86_64 -pie --script "$tc/link.x" --eh-frame-hdr \
  -L"$tc/lib" \
  "$out/cxx-emulated-tls.o" "$compiler_rt" \
  -lc -lkernel -lm \
  "$tc/lib/crt1.o" -o "$out/ps4-kodi-cxx-emulated-tls-probe.elf"
if ! "$nm_tool" -a "$out/ps4-kodi-cxx-emulated-tls-probe.elf" | \
    grep -F '__emutls_get_address' >/dev/null; then
  echo "ERROR: emulated TLS probe does not contain __emutls_get_address" >&2
  exit 1
fi
"$here/make-pkg.sh" "$out/ps4-kodi-cxx-emulated-tls-probe.elf" "$out" \
  "Kodi emulated TLS probe" PSMC00007 \
  IV0000-PSMC00007_00-KODIPROBE0000000 01.00

echo "==> kompilacja emulated TLS z kluczami Orbis libkernel..."
clang++ --target=x86_64-pc-freebsd12-elf -std=c++17 -fPIC -funwind-tables \
  -femulated-tls -DPS4_PROBE_ORBIS_KEY_EMUTLS \
  -fno-exceptions -fno-rtti -nostdinc++ \
  -isysroot "$tc" -isystem "$tc/include" \
  -c "$root/probe/cxx_startup.cpp" -o "$out/cxx-orbis-emulated-tls.o"
clang --target=x86_64-pc-freebsd12-elf -std=c11 -fPIC -funwind-tables \
  -isysroot "$tc" -isystem "$tc/include" \
  -c "$root/probe/emutls_orbis.c" -o "$out/emutls-orbis.o"
ld.lld -m elf_x86_64 -pie --script "$tc/link.x" --eh-frame-hdr \
  -L"$tc/lib" \
  "$out/cxx-orbis-emulated-tls.o" "$out/emutls-orbis.o" \
  -lc -lkernel -lm \
  "$tc/lib/crt1.o" -o "$out/ps4-kodi-cxx-orbis-emulated-tls-probe.elf"
if ! "$nm_tool" -a "$out/ps4-kodi-cxx-orbis-emulated-tls-probe.elf" | \
    grep -F '__emutls_get_address' >/dev/null; then
  echo "ERROR: Orbis-key emulated TLS probe lacks __emutls_get_address" >&2
  exit 1
fi
"$here/make-pkg.sh" "$out/ps4-kodi-cxx-orbis-emulated-tls-probe.elf" "$out" \
  "Kodi Orbis-key TLS probe" PSMC00006 \
  IV0000-PSMC00006_00-KODIPROBE0000000 01.00
