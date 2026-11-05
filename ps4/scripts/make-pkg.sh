#!/usr/bin/env bash
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$here/env.sh"

elf="$(readlink -f "$1")"; out="$(readlink -f "$2")"; title="$3"; tid="$4"; cid="$5"; ver="$6"; payload="${7:-}"
tc="$OO_PS4_TOOLCHAIN"; bin="$tc/bin/linux"
sample="$OO_PS4_SAMPLES/samples/piglet"
icon="$here/../title/sce_sys/icon0.png"

stage="$out/stage"
rm -rf "$stage"; mkdir -p "$stage/sce_sys/about" "$stage/sce_module"
cp "$sample/sce_sys/about/right.sprx" "$stage/sce_sys/about/right.sprx"
cp "$sample/sce_module/libc.prx" "$sample/sce_module/libSceFios2.prx" "$stage/sce_module/"
cp "$icon" "$stage/sce_sys/icon0.png"

if [ -n "$payload" ]; then
  cp -a "$payload"/. "$stage"/
fi

# NOTE: --authinfo powoduje crash CE-34878-0 na PS4 FW 9.00 + GoldHEN.
# PAID 0x3800000000000035 bez authinfo dziala stabilnie.
"$bin/create-fself" -in="$elf" -out="$out/$(basename "$elf" .elf).oelf" \
  --eboot "$stage/eboot.bin" --paid 0x3800000000000035

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

( cd "$stage"
  # Kodi's installed resource tree exceeds Linux's per-argument size limit.
  # create-gp4 supports walking a source directory, so pass the directory
  # instead of flattening thousands of paths into one --files argument.
  "$bin/create-gp4" -out "$stage/pkg.gp4" --content-id="$cid" --path "$stage"
  "$P" pkg_build "$stage/pkg.gp4" "$out" )
ls -la "$out"/*.pkg
