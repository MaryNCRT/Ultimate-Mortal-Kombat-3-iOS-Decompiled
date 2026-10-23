#!/usr/bin/env python3
"""make_release.py -- the folder a player downloads: only what the build needs.

    python launcher/make_release.py <win-unpacked dir> <out dir> [--zip]

<win-unpacked dir> is the Electron launcher built unpacked
(`npm run dist` in launcher/app -> dist/win-unpacked). It goes to
launcher/bin/, and launcher/stub/umk3_launcher_stub.c is compiled into the
UMK3-Launcher.exe the player double-clicks: the portable single .exe took
9.3 s to open (it unpacked itself at every start), this takes 0.7 s.

Copies all of src/ (the tree launcher/app/lib/compile.js compiles from), the
table generators and their helpers, the launcher stub's sources, LEEME.txt,
and the Epic Online Services SDK from TOOLS/eos when it is there (online over
the internet; not in the repository). Nothing of the game: the player's own
.ipa supplies the data tables and res\\ when they press Compilar.

The script refuses to finish if a header one of the sources includes is
missing (it checks every #include "..." it can resolve against the copied tree).
"""

import os
import re
import shutil
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

TOOLS = ["tools/build/logic_tables.py", "tools/build/logic_tables.map", "tools/build/macho.py", "tools/build/low32_static.py",
         "tools/build/level_info.py", "tools/build/seq_data.py", "tools/build/check_binary.py"]
LAUNCHER = ["launcher/LEEME.txt", "launcher/stub/umk3_launcher_stub.c",
            "launcher/build-game.cmd", "launcher/eos.ini.example",
            "launcher/stub/umk3_launcher_stub.rc"]
TOP = ["LICENSE", "docs/AI-DISCLOSURE.md", "CMakeLists.txt", "build.py"]


def find_mingw():
    """x86_64-w64-mingw32-clang and -windres: the game's own toolchain
    (toolchain/llvm-mingw-*/bin, which build_game.ps1 downloads) or PATH;
    None when neither is there (build_stub then uses zig)."""
    import glob
    for d in sorted(glob.glob(os.path.join(ROOT, "toolchain", "llvm-mingw-*", "bin"))):
        cc = os.path.join(d, "x86_64-w64-mingw32-clang.exe")
        if os.path.exists(cc):
            return cc, os.path.join(d, "x86_64-w64-mingw32-windres.exe")
    cc = shutil.which("x86_64-w64-mingw32-clang")
    if cc:
        return cc, shutil.which("x86_64-w64-mingw32-windres")
    return None


def find_zig():
    """The zig the launcher downloads (%LOCALAPPDATA%/umk3-launcher/toolchain),
    UMK3_ZIG, or PATH."""
    import glob
    if os.environ.get("UMK3_ZIG"):
        return os.environ["UMK3_ZIG"]
    base = os.path.join(os.environ.get("LOCALAPPDATA", ""), "umk3-launcher", "toolchain")
    for z in sorted(glob.glob(os.path.join(base, "zig-*", "zig.exe"))):
        return z
    z = shutil.which("zig")
    if z:
        return z
    raise SystemExit("no x86_64-w64-mingw32-clang and no zig: run the launcher's "
                     "compile once, set UMK3_ZIG, or put llvm-mingw on PATH")


def build_stub(dst):
    import subprocess
    import tempfile
    mingw = find_mingw()
    stub = os.path.join(ROOT, "launcher", "stub")
    with tempfile.TemporaryDirectory(dir=os.path.dirname(dst)) as tmp:
        res = os.path.join(tmp, "stub.res")
        if mingw:
            cc, rc = mingw
            subprocess.check_call([rc, "umk3_launcher_stub.rc", "-O", "coff", "-o", res], cwd=stub)
            subprocess.check_call([cc, "-O2", "-municode", "-mwindows", "-s",
                                   "umk3_launcher_stub.c", res, "-o", dst], cwd=stub)
        else:
            zig = find_zig()
            subprocess.check_call([zig, "rc", "-fo", res, "umk3_launcher_stub.rc"], cwd=stub)
            subprocess.check_call([zig, "cc", "-target", "x86_64-windows-gnu", "-O2",
                                   "-municode", "-Wl,--subsystem,windows", "-s",
                                   "umk3_launcher_stub.c", res, "-o", dst], cwd=stub)


def copy_eos(out):
    """ONLINE over the internet (src/runtime/mp_eos.c): the Epic Online
    Services SDK's headers, its Windows library and the product's eos.ini,
    from TOOLS/eos (not in the repository), to TOOLS/eos in the release,
    where the launcher's compile finds them. Without them the release
    still builds, with online on the local network alone."""
    src = os.environ.get("UMK3_EOS_DIR") or os.path.join(ROOT, "TOOLS", "eos")
    if not os.path.exists(os.path.join(src, "SDK", "Include", "eos_sdk.h")):
        print("no EOS SDK in %s: online on the local network alone" % src)
        return
    dst = os.path.join(out, "TOOLS", "eos")
    shutil.copytree(os.path.join(src, "SDK", "Include"), os.path.join(dst, "SDK", "Include"))
    os.makedirs(os.path.join(dst, "SDK", "Bin"))
    shutil.copy2(os.path.join(src, "SDK", "Bin", "EOSSDK-Win64-Shipping.dll"),
                 os.path.join(dst, "SDK", "Bin"))
    for n in ("eos.ini", "ThirdPartyNotices"):
        p = os.path.join(src, n)
        if os.path.isdir(p):
            shutil.copytree(p, os.path.join(dst, n))
        elif os.path.exists(p):
            shutil.copy2(p, os.path.join(dst, n))
    print("EOS SDK from %s" % src)


def main(argv):
    if len(argv) < 3:
        raise SystemExit(__doc__.strip().splitlines()[2].strip())
    launcher_exe, out = argv[1], os.path.abspath(argv[2])
    if os.path.exists(out):
        shutil.rmtree(out)

    # all of src/: the tree the launcher's compile (launcher/app/lib/compile.js)
    # picks its sources from
    files = []
    for dp, _, fns in os.walk(os.path.join(ROOT, "src")):
        for n in sorted(fns):
            files.append(os.path.relpath(os.path.join(dp, n), ROOT).replace("\\", "/"))
    files += TOOLS + LAUNCHER + TOP

    for rel in files:
        dst = os.path.join(out, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(os.path.join(ROOT, rel), dst)
    shutil.copy2(os.path.join(ROOT, "launcher", "LEEME.txt"),
                 os.path.join(out, "LEEME.txt"))
    # the build from a terminal (launcher/app/cli.js) and the online example
    for n in ("build-game.cmd", "eos.ini.example"):
        shutil.copy2(os.path.join(ROOT, "launcher", n), os.path.join(out, n))
    shutil.copytree(launcher_exe, os.path.join(out, "launcher", "bin"))
    # Mary's own pictures for the wide screens (FrontEnd.c, the continue
    # screens): under launcher\ so an update copies them, and build_game.ps1
    # puts them beside the game
    shutil.copytree(os.path.join(ROOT, "launcher", "aditional textures"),
                    os.path.join(out, "launcher", "aditional textures"))
    build_stub(os.path.join(out, "UMK3-Launcher.exe"))
    copy_eos(out)

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
