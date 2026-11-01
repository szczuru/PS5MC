#!/usr/bin/env python3
"""Skip OpenOrbis' conflicting STREAMS ioctl declaration in curl."""

from pathlib import Path
import sys

path = Path(sys.argv[1])
source = path.read_text()
old = "#ifdef HAVE_STROPTS_H\n#  include <stropts.h>\n#endif"
new = "#if defined(HAVE_STROPTS_H) && !defined(__PS4__)\n#  include <stropts.h>\n#endif"
if old not in source:
    if new in source:
        raise SystemExit(0)
    raise SystemExit(f"Expected curl stropts include not found in {path}")
path.write_text(source.replace(old, new, 1))
