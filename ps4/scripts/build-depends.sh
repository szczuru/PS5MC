#!/bin/bash
# Convenience wrapper – run from repo root or from Actions
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exec "$ROOT/depends/scripts/build-all.sh"
