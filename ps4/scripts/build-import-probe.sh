#!/usr/bin/env bash
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
source "$here/env.sh"

kodi_elf="$(readlink -f "${1:?usage: build-import-probe.sh kodi.elf [output-dir]}")"
out="$(readlink -m "${2:-$root/../dist/import-probe}")"
tc="$OO_PS4_TOOLCHAIN"
mkdir -p "$out"

nm_tool="$(command -v llvm-nm || command -v llvm-nm-18 || command -v nm || true)"
readelf_tool="$(command -v llvm-readelf || command -v llvm-readelf-18 || command -v readelf || true)"
if [ -z "$nm_tool" ] || [ -z "$readelf_tool" ]; then
  echo "ERROR: nm and readelf are required to build the Kodi import probe" >&2
  exit 1
fi

# Recreate Kodi's strong undefined dynamic references in a tiny executable.
# The PS4 loader must resolve the same imported symbols before this probe can
# reach its constructor or main, while Kodi's own global constructors are absent.
python3 - "$nm_tool" "$kodi_elf" "$root/ps4/probe/imports_startup.c" \
  "$out/kodi-imports.generated.c" "$out/kodi-import-count.h" <<'PY'
import json
import subprocess
import sys
from pathlib import Path

nm, elf, template, generated, count_header = sys.argv[1:]
result = subprocess.run(
    [nm, "-D", "--undefined-only", "--format=posix", elf],
    check=True, capture_output=True, text=True,
)
symbols = []
for line in result.stdout.splitlines():
    fields = line.split()
    if len(fields) >= 2 and fields[1] == "U":
        symbols.append(fields[0])
symbols = sorted(set(symbols))
if not symbols:
    raise SystemExit("Kodi ELF has no strong undefined dynamic imports")

declarations = []
references = []
for index, symbol in enumerate(symbols):
    declarations.append(f'extern void kodi_import_{index}(void) __asm__({json.dumps(symbol)});')
    references.append(f'  (void*)&kodi_import_{index},')
source = Path(template).read_text()
source = source.replace(
    "extern void* const g_kodi_imports[];\nextern const unsigned long g_kodi_import_count;",
    "\n".join(declarations) + "\n" +
    "__attribute__((used)) void* const g_kodi_imports[] = {\n" +
    "\n".join(references) + "\n};\n" +
    f"const unsigned long g_kodi_import_count = {len(symbols)}UL;",
)
Path(generated).write_text(source)
Path(count_header).write_text(str(len(symbols)) + "\n")
print(f"Generated runtime smoke test for {len(symbols)} strong Kodi imports")
PY

clang --target=x86_64-pc-freebsd12-elf -std=c11 -fPIC -funwind-tables \
  -D_GNU_SOURCE -D__PS4__=1 -isysroot "$tc" -isystem "$tc/include" \
  -c "$out/kodi-imports.generated.c" -o "$out/kodi-imports.o"
ld.lld -m elf_x86_64 -pie --script "$tc/link.x" --eh-frame-hdr \
  -L"$tc/lib" \
  "$out/kodi-imports.o" \
  -lc -lm -lkernel \
  --no-as-needed -lSceVideoOut -lSceAudioOut -lScePad -lSceUserService \
  -lSceSystemService -lSceSysmodule -lScePigletv2VSH -lSceNet \
  --as-needed "$tc/lib/crt1.o" -o "$out/ps4-kodi-import-probe.elf"

kodi_dynamic="$("$readelf_tool" -W -d "$kodi_elf")"
probe_dynamic="$("$readelf_tool" -W -d "$out/ps4-kodi-import-probe.elf")"
while IFS= read -r dependency; do
  [ -n "$dependency" ] || continue
  grep -F "Shared library: [$dependency]" <<<"$probe_dynamic" >/dev/null || {
    echo "ERROR: import probe is missing Kodi dependency $dependency" >&2
    exit 1
  }
done < <(sed -n 's/.*Shared library: \[\([^]]*\)\].*/\1/p' <<<"$kodi_dynamic")

"$here/make-pkg.sh" "$out/ps4-kodi-import-probe.elf" "$out" \
  "Kodi import loader probe" PSMC00013 \
  IV0000-PSMC00013_00-KODIPROBE0000000 01.00
