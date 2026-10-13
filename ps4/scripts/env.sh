#!/usr/bin/env bash
# Source this file:  source ps4/scripts/env.sh
PS4_TC_VERSION="${PS4_TC_VERSION:-v0.5.4}"
PS4_TC_ASSET="${PS4_TC_ASSET:-toolchain-llvm-18.tar.gz}"
export OO_PS4_TOOLCHAIN="${OO_PS4_TOOLCHAIN:-$HOME/OpenOrbis/PS4Toolchain}"
export OO_PS4_SAMPLES="${OO_PS4_SAMPLES:-$HOME/OpenOrbis/samples-src}"
export PATH="$OO_PS4_TOOLCHAIN/bin/linux:$PATH"
# PkgTool.Core is a .NET program; make it work without ICU on minimal hosts.
export DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1
