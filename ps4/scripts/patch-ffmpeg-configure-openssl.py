#!/usr/bin/env python3
"""Use a TLS symbol available in OpenOrbis' OpenSSL build for FFmpeg's probe."""

from pathlib import Path
import sys

path = Path(sys.argv[1])
source = path.read_text()
old = 'check_pkg_config openssl "openssl >= 3.0.0" openssl/ssl.h DTLS_get_data_mtu'
new = 'check_pkg_config openssl "openssl >= 3.0.0" openssl/ssl.h SSL_new'
if old not in source:
    if new in source:
        print(f"FFmpeg OpenSSL probe already PS4-compatible: {path}")
        raise SystemExit(0)
    raise SystemExit(f"Expected FFmpeg OpenSSL probe not found in {path}")
path.write_text(source.replace(old, new, 1))
print(f"Patched FFmpeg OpenSSL probe for PS4: {path}")
