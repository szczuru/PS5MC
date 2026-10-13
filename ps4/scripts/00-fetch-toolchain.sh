#!/usr/bin/env bash
# Download the OpenOrbis PS4 toolchain (LLVM 18 build) and the three tiny
# package asset files (right.sprx, libc.prx, libSceFios2.prx) from the
# OpenOrbis repository. Idempotent.
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$here/env.sh"

if [ ! -f "$OO_PS4_TOOLCHAIN/link.x" ]; then
  echo "==> toolchain $PS4_TC_VERSION -> $OO_PS4_TOOLCHAIN"
  tmp="$(mktemp -d)"
  curl -fL --retry 3 -o "$tmp/tc.tgz" \
    "https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/releases/download/${PS4_TC_VERSION}/${PS4_TC_ASSET}"
  mkdir -p "$tmp/x"
  tar -xzf "$tmp/tc.tgz" -C "$tmp/x"
  mkdir -p "$(dirname "$OO_PS4_TOOLCHAIN")"
  rm -rf "$OO_PS4_TOOLCHAIN"
  mv "$tmp/x/OpenOrbis/PS4Toolchain" "$OO_PS4_TOOLCHAIN"
  rm -rf "$tmp"
fi
chmod +x "$OO_PS4_TOOLCHAIN"/bin/linux/* 2>/dev/null || true

if [ ! -f "$OO_PS4_SAMPLES/samples/piglet/sce_sys/about/right.sprx" ]; then
  echo "==> package assets from OpenOrbis samples"
  rm -rf "$OO_PS4_SAMPLES"
  git clone --depth 1 --branch "$PS4_TC_VERSION" \
    https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain.git "$OO_PS4_SAMPLES" 2>/dev/null \
  || git clone --depth 1 https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain.git "$OO_PS4_SAMPLES"
fi
echo "toolchain ok: $OO_PS4_TOOLCHAIN"
