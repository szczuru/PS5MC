#!/usr/bin/env python3
"""Use a TLS symbol available in OpenOrbis' OpenSSL build for FFmpeg's probe."""

from pathlib import Path
import sys

path = Path(sys.argv[1])
source = path.read_text()
message = 'die "ERROR: OpenSSL <3.0.0 is incompatible with the gpl"'
message_at = source.find(message)
if message_at < 0:
    print(f"FFmpeg configure has no OpenSSL/GPL guard; no PS4 patch needed: {path}")
    raise SystemExit(0)

probe_at = source.rfind("check_pkg_config openssl", max(0, message_at - 2048), message_at)
if probe_at < 0:
    context = source[max(0, message_at - 256):message_at + len(message)]
    raise SystemExit(f"Could not locate FFmpeg OpenSSL probe near GPL guard in {path}: {context}")

# FFmpeg's configure is patched in two ExternalProject layers. Replace only
# the DTLS-dependent symbol check nearest the GPL guard, preserving its version
# check and the separate OpenSSL < 1.1.1 fallback.
probe_end = source.find("\n", probe_at)
probe = source[probe_at:probe_end]
dtls_probe = "DTLS_get_data_mtu"
if dtls_probe not in probe:
    if "SSL_new" in probe:
        print(f"FFmpeg OpenSSL probe already PS4-compatible: {path}")
        raise SystemExit(0)
    raise SystemExit(f"Unexpected FFmpeg OpenSSL probe in {path}: {probe}")
source = source[:probe_at] + probe.replace(dtls_probe, "SSL_new", 1) + source[probe_end:]

path.write_text(source)
print(f"Patched FFmpeg OpenSSL probe for PS4: {path}")
