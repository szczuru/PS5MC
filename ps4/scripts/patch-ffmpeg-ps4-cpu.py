#!/usr/bin/env python3
"""Disable FFmpeg's FreeBSD sysctl CPU query on PS4/OpenOrbis."""

from pathlib import Path
import sys

path = Path(sys.argv[1])
source = path.read_text()
marker = '#include "config.h"\n'
shim = (
    '#if defined(__PS4__)\n'
    '#undef HAVE_SYSCTL\n'
    '#define HAVE_SYSCTL 0\n'
    '#endif\n'
)

if shim in source:
    print(f"FFmpeg PS4 CPU sysctl workaround already applied: {path}")
    raise SystemExit(0)
if source.count(marker) != 1:
    raise SystemExit(f"Unexpected FFmpeg cpu.c config include in {path}")

path.write_text(source.replace(marker, marker + shim, 1))
print(f"Disabled FFmpeg sysctl CPU query for PS4: {path}")
