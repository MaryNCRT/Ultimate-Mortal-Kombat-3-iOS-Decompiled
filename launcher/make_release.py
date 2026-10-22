#!/usr/bin/env python3
"""make_release.py -- the folder a player downloads: only what the build needs.

    python launcher/make_release.py <win-unpacked dir> <out dir> [--zip]

<win-unpacked dir> is the Electron launcher built unpacked
(`npm run dist` in launcher/app -> dist/win-unpacked). It goes to
launcher/bin/, and launcher/stub/umk3_launcher_stub.c is compiled into the
UMK3-Launcher.exe the player double-clicks: the portable single .exe took
9.3 s to open (it unpacked itself at every start), this takes 0.7 s.

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
    "src/runtime/game_main.c", "src/runtime/debug_menu.c", "src/runtime/draw_gl.c",
    "src/runtime/settings_menu.c", "src/runtime/settings_menu.h",
    "src/runtime/pad_nav.c", "src/runtime/pad_nav.h",
    "src/runtime/profiles.c", "src/runtime/profiles.h",
    "src/runtime/mp_net.c", "src/runtime/mp_net.h",
    "src/runtime/online_ui.c", "src/runtime/online_ui.h",
    "src/runtime/umk3_game.rc", "src/runtime/umk3_game.ico",
    "src/runtime/gamecode_globals.c",
    "src/runtime/gamecode_globals.h", "src/runtime/gamecode_stubs.c",
    "src/runtime/lime_menu.c", "src/runtime/lime_platform.c", "src/runtime/lime_app.c",
    "src/runtime/wav.c", "src/runtime/wav.h", "src/runtime/fight_runtime.c",
    "src/runtime/fight_runtime.h", "src/runtime/arm_runtime.h", "src/runtime/debug_menu.h",
    "src/runtime/savestate.c", "src/runtime/savestate.h",
    "src/runtime/rollback_probe.c",
    "src/runtime/platform/platform.h", "src/runtime/platform/gl.h",
    "src/runtime/platform/win32_gl.c", "src/runtime/platform/win32_gl_cdecl.c",
    "src/runtime/platform/win32_audio.c",
]
SOURCE_DIRS = ["src/decomp/gamecode", "src/decomp/gamecode/logic", "src/decomp/lime",
               "src/runtime/lime"]
TOOLS = ["tools/build/logic_tables.py", "tools/build/logic_tables.map", "tools/build/macho.py", "tools/build/low32_static.py",
         "tools/build/level_info.py", "tools/build/seq_data.py", "tools/build/check_binary.py", "tools/build/unpack_ipa.py"]
LAUNCHER = ["launcher/build_game.ps1", "launcher/setup_res.ps1",
            "launcher/LEEME.txt", "launcher/stub/umk3_launcher_stub.c",
            "launcher/stub/umk3_launcher_stub.rc"]
TOP = ["LICENSE", "docs/AI-DISCLOSURE.md", "CMakeLists.txt", "build.py"]


def find_mingw():
    """x86_64-w64-mingw32-clang and -windres: the game's own toolchain
    (toolchain/llvm-mingw-*/bin, which build_game.ps1 downloads) or PATH."""
    import glob
    for d in sorted(glob.glob(os.path.join(ROOT, "toolchain", "llvm-mingw-*", "bin"))):
        cc = os.path.join(d, "x86_64-w64-mingw32-clang.exe")
        if os.path.exists(cc):
            return cc, os.path.join(d, "x86_64-w64-mingw32-windres.exe")
    cc = shutil.which("x86_64-w64-mingw32-clang")
    if cc:
        return cc, shutil.which("x86_64-w64-mingw32-windres")
    raise SystemExit("no x86_64-w64-mingw32-clang: run build_game.ps1 once, or put llvm-mingw on PATH")


def build_stub(dst):
    import subprocess
    import tempfile
    cc, rc = find_mingw()
    stub = os.path.join(ROOT, "launcher", "stub")
    with tempfile.TemporaryDirectory(dir=os.path.dirname(dst)) as tmp:
        res = os.path.join(tmp, "stub.res")
        subprocess.check_call([rc, "umk3_launcher_stub.rc", "-O", "coff", "-o", res], cwd=stub)
        subprocess.check_call([cc, "-O2", "-municode", "-mwindows", "-s",
                               "umk3_launcher_stub.c", res, "-o", dst], cwd=stub)


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
    shutil.copytree(launcher_exe, os.path.join(out, "launcher", "bin"))
    # Mary's own pictures for the wide screens (FrontEnd.c, the continue
    # screens): under launcher\ so an update copies them, and build_game.ps1
    # puts them beside the game
    shutil.copytree(os.path.join(ROOT, "launcher", "aditional textures"),
                    os.path.join(out, "launcher", "aditional textures"))
    build_stub(os.path.join(out, "UMK3-Launcher.exe"))

    # Every quoted include of every copied source has to resolve in the copy.
    inc_dirs = ["src/runtime", "src/decomp/lime", "src/decomp/gamecode/logic"]
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
