#!/usr/bin/env python3
"""Konwertuje R_X86_64_PLT32_BND (40) -> R_X86_64_PLT32 (4) w libXcape.a.
libXcape została skompilowana starym clangiem z MSVC (Intel MPX BND prefix),
a nowy LLD 18 nie obsługuje tej relokacji w statycznych binariach.
Skrypt jest idempotentny."""
import struct, sys, os

path = sys.argv[1] if len(sys.argv) > 1 else 'ps4/libXcape/lib/libXcape.a'
data = bytearray(open(path, 'rb').read())
R_PLT32_BND, R_PLT32 = 40, 4
patched = 0
for i in range(0, len(data) - 8, 1):
    val = struct.unpack_from('<Q', data, i)[0]
    if (val & 0xFFFFFFFF) == R_PLT32_BND:
        struct.pack_into('<Q', data, i, (val & 0xFFFFFFFF00000000) | R_PLT32)
        patched += 1
open(path, 'wb').write(data)
print(f"patch-xcape: {patched} relokacji R_PLT32_BND -> R_PLT32 w {path}")
