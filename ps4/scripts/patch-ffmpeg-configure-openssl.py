#!/usr/bin/env python3
"""Use a TLS symbol available in OpenOrbis' OpenSSL build for FFmpeg's probe."""

from pathlib import Path
import sys

path = Path(sys.argv[1])
source = path.read_text()
new = "enabled openssl"
old_probes = (
    'check_pkg_config openssl "openssl >= 3.0.0" openssl/ssl.h DTLS_get_data_mtu',
    'check_pkg_config openssl "openssl >= 3.0.0" openssl/ssl.h SSL_new',
)
for old in old_probes:
    if old in source:
        path.write_text(source.replace(old, new, 1))
        print(f"Patched FFmpeg OpenSSL probe for PS4: {path}")
        break
else:
    if f'{new} || die "OpenSSL <3.0.0 is incompatible with the gpl"' in source:
        print(f"FFmpeg OpenSSL probe already PS4-compatible: {path}")
    else:
        raise SystemExit(f"Expected FFmpeg OpenSSL probe not found in {path}")
