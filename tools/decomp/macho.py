#!/usr/bin/env python3
"""Forwarder so scripts in tools/decomp/ can `import macho` directly."""
import os
import sys

_BUILD = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "build"))
if _BUILD not in sys.path:
    sys.path.insert(0, _BUILD)

from macho import *  # noqa: F401,F403,E402

if __name__ == "__main__":
    import runpy
    runpy.run_path(os.path.join(_BUILD, "macho.py"), run_name="__main__")
