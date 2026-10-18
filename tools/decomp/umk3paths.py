"""
Shared path resolution for the UMK3 tooling.

Every tool needs to find the same handful of things: the repository root, the
working directory where binary-derived files live, Ghidra, and a JDK. Rather
than hard-coding paths in each script, they all ask this module.

Nothing derived from the retail binary is committed, so all of it lives under
a working directory that `.gitignore` excludes. By default that is `work/` at
the repository root; override it with the `UMK3_WORK` environment variable.

Expected layout of the working directory:

    work/
      UMK3.armv7          the armv7 slice, extracted from YOUR copy
      symbols.txt         produced by tools/macho.py syms
      functions.txt       produced by tools/macho.py funcs
      func-to-file.txt    produced by tools/stabs.py
      source-tree.md      produced by tools/stabs.py
      ghidra/             Ghidra project directory
      worklist/           per-module work lists
      decomp-raw/         raw Ghidra output

Environment variables:

    UMK3_WORK     working directory        (default: <repo>/work)
    UMK3_SLICE    path to the armv7 slice  (default: <work>/UMK3.armv7)
    GHIDRA_HOME   Ghidra installation      (required by decomp_driver.py)
    JAVA_HOME     JDK 21+ installation     (optional; Ghidra may find its own)
"""

import os
import sys

# tools/decomp/umk3paths.py -> the repository root is two levels up.
REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
TOOLS = os.path.join(REPO, "tools", "decomp")
BUILD_TOOLS = os.path.join(REPO, "tools", "build")
if BUILD_TOOLS not in sys.path:
    sys.path.insert(0, BUILD_TOOLS)


def work_dir():
    """Where binary-derived files live. Created on demand."""
    d = os.environ.get("UMK3_WORK") or os.path.join(REPO, "work")
    os.makedirs(d, exist_ok=True)
    return d


def slice_path():
    """The armv7 slice extracted from the user's own copy of the game."""
    return os.environ.get("UMK3_SLICE") or os.path.join(work_dir(), "UMK3.armv7")


def require_slice():
    """Return the slice path, or explain how to produce it and exit."""
    p = slice_path()
    if not os.path.exists(p):
        sys.exit(
            "Could not find the armv7 slice at:\n"
            "    %s\n\n"
            "Extract it from your own copy of the game first:\n"
            "    python tools/macho.py thin <path/to/UMK3> armv7 %s\n\n"
            "Or point UMK3_SLICE at an existing one."
            % (p, p)
        )
    return p


def work_file(name):
    """Path to a file inside the working directory."""
    return os.path.join(work_dir(), name)


def func_to_file():
    """
    The function -> source file map produced by tools/stabs.py.

    Almost every tool depends on it, so the error message says how to make it.
    """
    p = work_file("func-to-file.txt")
    if not os.path.exists(p):
        sys.exit(
            "Could not find func-to-file.txt at:\n"
            "    %s\n\n"
            "Produce it from the slice:\n"
            "    python tools/stabs.py %s %s"
            % (p, slice_path(), work_dir())
        )
    return p


def load_func_to_file(required=True):
    """Parse func-to-file.txt into {function name: source file basename}."""
    p = func_to_file() if required else work_file("func-to-file.txt")
    mapping = {}
    if not os.path.exists(p):
        return mapping
    with open(p, encoding="utf-8") as fh:
        for line in fh:
            if line.startswith("0x"):
                parts = line.split()
                if len(parts) >= 3:
                    mapping[parts[1]] = parts[2].replace("\\", "/").rsplit("/", 1)[-1]
    return mapping


def ghidra_home(required=True):
    """Ghidra installation directory."""
    d = os.environ.get("GHIDRA_HOME")
    if not d and required:
        sys.exit(
            "GHIDRA_HOME is not set.\n\n"
            "Install Ghidra 11+ and point GHIDRA_HOME at it, for example:\n"
            "    Windows:  set GHIDRA_HOME=C:\\ghidra_11.3_PUBLIC\n"
            "    Linux:    export GHIDRA_HOME=~/ghidra_11.3_PUBLIC"
        )
    return d


def analyze_headless():
    """Full path to Ghidra's headless analyzer for this platform."""
    home = ghidra_home()
    name = "analyzeHeadless.bat" if os.name == "nt" else "analyzeHeadless"
    p = os.path.join(home, "support", name)
    if not os.path.exists(p):
        sys.exit("Could not find %s\nIs GHIDRA_HOME correct?" % p)
    return p
