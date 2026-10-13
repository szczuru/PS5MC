#!/usr/bin/env bash
# Turn a linked ELF into an installable fake-PKG (for GoldHEN / "Homebrew
# Enabler" package install).
#
#   make-pkg.sh <elf> <out-dir> <TITLE> <TITLE_ID> <CONTENT_ID> <VERSION> [extra-dir]
#
# TITLE_ID   : 9 chars, e.g. PS4MC0001
# CONTENT_ID : 36 chars, e.g. IV0000-PS4MC0001_00-KODIPS4000000000
# extra-dir  : optional folder copied into the package next to eboot.bin
#              (Kodi's share/, addons/, ...). Becomes /app0/ on the console.
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$here/env.sh"

elf="$(readlink -f "$1")"; out="$(readlink -f "${2}")"; title="$3"; tid="$4"; cid="$5"; ver="$6"
extra="${7:-}"
tc="$OO_PS4_TOOLCHAIN"; bin="$tc/bin/linux"
sample="$OO_PS4_SAMPLES/samples/piglet"
icon="$here/../title/sce_sys/icon0.png"

stage="$out/stage"
rm -rf "$stage"; mkdir -p "$stage/sce_sys/about" "$stage/sce_module"
cp "$sample/sce_sys/about/right.sprx" "$stage/sce_sys/about/right.sprx"
cp "$sample/sce_module/libc.prx" "$sample/sce_module/libSceFios2.prx" "$stage/sce_module/"
cp "$icon" "$stage/sce_sys/icon0.png"

# Same authinfo/paid values as OpenOrbis' own Piglet sample (title that loads
# Sony system modules).
AUTHINFO="000000000000000000000000001C004000FF000000000080000000000000000000000000000000000000008000400040000000000000008000000000000000080040FFFF000000F000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
"$bin/create-fself" -in="$elf" -out="$out/$(basename "$elf" .elf).oelf" \
  --eboot "$stage/eboot.bin" --paid 0x3800000000000011 --authinfo "$AUTHINFO"

sfo="$stage/sce_sys/param.sfo"
P="$bin/PkgTool.Core"
"$P" sfo_new "$sfo"
"$P" sfo_setentry "$sfo" APP_TYPE            --type Integer --maxsize 4   --value 1
"$P" sfo_setentry "$sfo" APP_VER             --type Utf8    --maxsize 8   --value "$ver"
"$P" sfo_setentry "$sfo" ATTRIBUTE           --type Integer --maxsize 4   --value 0
"$P" sfo_setentry "$sfo" CATEGORY            --type Utf8    --maxsize 4   --value 'gd'
"$P" sfo_setentry "$sfo" CONTENT_ID          --type Utf8    --maxsize 48  --value "$cid"
"$P" sfo_setentry "$sfo" DOWNLOAD_DATA_SIZE  --type Integer --maxsize 4   --value 0
"$P" sfo_setentry "$sfo" SYSTEM_VER          --type Integer --maxsize 4   --value 0
"$P" sfo_setentry "$sfo" TITLE               --type Utf8    --maxsize 128 --value "$title"
"$P" sfo_setentry "$sfo" TITLE_ID            --type Utf8    --maxsize 12  --value "$tid"
"$P" sfo_setentry "$sfo" VERSION             --type Utf8    --maxsize 8   --value "$ver"

if [ -n "$extra" ]; then cp -r "$extra"/. "$stage/"; fi

( cd "$stage"
  # file paths in a gp4 are relative to the gp4's own folder, so it lives in $stage
  files="$(find . -type f ! -name pkg.gp4 | sed 's#^\./##' | sort | tr '\n' ' ')"
  "$bin/create-gp4" -out "$stage/pkg.gp4" --content-id="$cid" --files "$files"
  "$P" pkg_build "$stage/pkg.gp4" "$out" )
ls -la "$out"/*.pkg
