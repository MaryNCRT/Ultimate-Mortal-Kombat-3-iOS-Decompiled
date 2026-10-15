#!/usr/bin/env python3
"""make_release.py -- the folder a player downloads: only what the build needs.

    python launcher/make_release.py <UMK3-Launcher.exe> <out dir> [--zip]

Copies the sources launcher/build_game.ps1 compiles (it globs the same
directories), the three table generators and their helpers, the launcher and
its scripts, and LEEME.txt. Nothing of the game: the player's own .ipa
supplies the data tables and res\\ when they press Compilar.

The list of runtime files matches build_game.ps1 and the umk3-game target in
CMakeLists.txt; when a source is added there, add it here too. The script
refuses to finish if a header one of the sources includes is missing (it
checks every #include "..." it can resolve against the copied tree).
"""

import os
import re
import shutil
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

RUNTIME_FILES = [
    "runtime/game_main.c", "runtime/debug_menu.c", "runtime/draw_gl.c",
    "runtime/gamecode_globals.c",
    "runtime/gamecode_globals.h", "runtime/gamecode_stubs.c",
    "runtime/lime_menu.c", "runtime/lime_platform.c", "runtime/lime_app.c",
    "runtime/wav.c", "runtime/wav.h", "runtime/fight_runtime.c",
    "runtime/fight_runtime.h", "runtime/arm_runtime.h", "runtime/debug_menu.h",
    "runtime/savestate.c", "runtime/savestate.h",
    "runtime/platform/platform.h", "runtime/platform/gl.h",
    "runtime/platform/win32_gl.c", "runtime/platform/win32_gl_cdecl.c",
    "runtime/platform/win32_audio.c",
]
SOURCE_DIRS = ["decomp/gamecode", "decomp/gamecode/logic", "decomp/lime",
               "runtime/lime"]
TOOLS = ["tools/logic_tables.py", "tools/logic_tables.map", "tools/macho.py",
         "tools/level_info.py", "tools/seq_data.py"]
LAUNCHER = ["launcher/build_game.ps1", "launcher/setup_res.ps1",
            "launcher/check_binary.py", "launcher/launcher.c",
            "launcher/LEEME.txt"]
TOP = ["LICENSE", "AI-DISCLOSURE.md"]


def main(argv):
    if len(argv) < 3:
        raise SystemExit(__doc__.strip().splitlines()[2].strip())
    launcher_exe, out = argv[1], os.path.abspath(argv[2])
    if os.path.exists(out):
        shutil.rmtree(out)

    files = list(RUNTIME_FILES) + TOOLS + LAUNCHER + TOP
    for d in SOURCE_DIRS:
        for n in sorted(os.listdir(os.path.join(ROOT, d))):
            if n.endswith((".c", ".h")):
                files.append(d + "/" + n)

    for rel in files:
        dst = os.path.join(out, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(os.path.join(ROOT, rel), dst)
    shutil.copy2(os.path.join(ROOT, "launcher", "LEEME.txt"),
                 os.path.join(out, "LEEME.txt"))
    shutil.copy2(launcher_exe, os.path.join(out, "UMK3-Launcher.exe"))

    # Every quoted include of every copied source has to resolve in the copy.
    inc_dirs = ["runtime", "decomp/lime", "decomp/gamecode/logic"]
    missing = set()
    for rel in files:
        if not rel.endswith((".c", ".h")):
            continue
        with open(os.path.join(out, rel), encoding="utf-8", errors="replace") as f:
            text = f.read()
        for m in re.finditer(r'^\s*#\s*include\s+"([^"]+)"', text, re.M):
            name = m.group(1)
            cands = [os.path.join(out, os.path.dirname(rel), name)]
            cands += [os.path.join(out, d, name) for d in inc_dirs]
            if not any(os.path.exists(c) for c in cands):
                in_repo = [os.path.join(ROOT, os.path.dirname(rel), name)]
                in_repo += [os.path.join(ROOT, d, name) for d in inc_dirs]
                if any(os.path.exists(c) for c in in_repo):
                    missing.add("%s (from %s)" % (name, rel))
    if missing:
        raise SystemExit("missing from the release:\n  " + "\n  ".join(sorted(missing)))

    print("%s: %d files" % (out, len(files) + 2))
    if "--zip" in argv:
        z = out.rstrip("\\/") + ".zip"
        base = os.path.basename(out.rstrip("\\/"))
        with zipfile.ZipFile(z, "w", zipfile.ZIP_DEFLATED) as zf:
            for dp, _, fns in os.walk(out):
                for fn in fns:
                    p = os.path.join(dp, fn)
                    zf.write(p, os.path.join(base, os.path.relpath(p, out)))
        print("%s: %.1f MB" % (z, os.path.getsize(z) / 1e6))


if __name__ == "__main__":
    main(sys.argv)
