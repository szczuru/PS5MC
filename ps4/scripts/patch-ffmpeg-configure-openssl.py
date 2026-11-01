#!/usr/bin/env python3
"""Use a TLS symbol available in OpenOrbis' OpenSSL build for FFmpeg's probe."""

from pathlib import Path
import re
import sys

path = Path(sys.argv[1])
source = path.read_text()
message = 'die "OpenSSL <3.0.0 is incompatible with the gpl"'
if re.search(r'enabled openssl\s*\|\|\s*' + re.escape(message), source):
    print(f"FFmpeg OpenSSL probe already PS4-compatible: {path}")
    raise SystemExit(0)

# FFmpeg's configure is patched in two ExternalProject layers. Match across
# whitespace because Kodi's patch series can reflow this shell condition.
pattern = re.compile(r'check_pkg_config\s+openssl\b.*?(?=\s*\|\|\s*' + re.escape(message) + r')', re.DOTALL)
source, count = pattern.subn("enabled openssl", source, count=1)
if count != 1:
    raise SystemExit(f"Could not locate FFmpeg OpenSSL/GPL probe in {path}")

path.write_text(source)
print(f"Patched FFmpeg OpenSSL probe for PS4: {path}")
