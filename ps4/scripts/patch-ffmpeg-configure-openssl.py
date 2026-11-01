#!/usr/bin/env python3
"""Use a TLS symbol available in OpenOrbis' OpenSSL build for FFmpeg's probe."""

from pathlib import Path
import sys

path = Path(sys.argv[1])
source = path.read_text()
message = 'die "OpenSSL <3.0.0 is incompatible with the gpl"'
message_at = source.find(message)
if message_at < 0:
    print(f"FFmpeg configure has no OpenSSL/GPL guard; no PS4 patch needed: {path}")
    raise SystemExit(0)

probe_at = source.rfind("check_pkg_config openssl", max(0, message_at - 2048), message_at)
patched_at = source.rfind("enabled openssl", max(0, message_at - 2048), message_at)
if patched_at >= 0 and probe_at < 0:
    print(f"FFmpeg OpenSSL probe already PS4-compatible: {path}")
    raise SystemExit(0)

# FFmpeg's configure is patched in two ExternalProject layers. Replace the
# OpenSSL check nearest the GPL guard, preserving the surrounding shell syntax.
if probe_at < 0:
    context = source[max(0, message_at - 256):message_at + len(message)]
    raise SystemExit(f"Could not locate FFmpeg OpenSSL probe near GPL guard in {path}: {context}")
source = source[:probe_at] + "enabled openssl || " + source[message_at:]

path.write_text(source)
print(f"Patched FFmpeg OpenSSL probe for PS4: {path}")
