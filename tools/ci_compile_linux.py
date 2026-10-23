#!/usr/bin/env python3
"""ci_compile_linux.py -- every source the Linux game is built from, compiled
the way launcher/app/lib/compile.js compiles it on a player's machine, with
only the headers the game ships (src/runtime/platform/linux-include,
vulkan-include): no SDL2 or OpenGL -dev package.

    python3 tools/ci_compile_linux.py [clang]
    python3 tools/ci_compile_linux.py zig cc -target x86_64-linux-gnu

What it cannot do is link: the data tables (logic_tables.c, level_info.c,
seq_data.c, gamecode_globals64.c) are generated from the player's own .ipa,
which is not in the repository and cannot be in CI. So this stops one step
short of an executable, and says so. Exit code 1 if any source fails.
"""
import os
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CC = sys.argv[1:] or ["clang"]          # the compiler, with any leading arguments

# launcher/app/lib/compile.js, the Linux 64-bit build
RUNTIME = ["game_main.c", "debug_menu.c", "settings_menu.c", "pad_nav.c", "profiles.c", "mods.c",
           "draw_gl.c", "gamecode_stubs.c", "mp_net.c", "mp_eos.c", "online_ui.c", "lime_menu.c",
           "lime_platform.c", "lime_app.c", "wav.c", "fight_runtime.c", "savestate.c",
           "rollback_probe.c", "rollback.c",
           "platform/sdl_gl.c", "platform/sdl_audio.c", "platform/ui_alert.c",
           "render/rgl_front.c", "render/rgl_ff.c", "render/rgl_vk.c"]
DIRS = ["src/decomp/gamecode", "src/decomp/lime", "src/runtime/lime", "src/decomp/gamecode/logic"]


def sources():
    out = [os.path.join(ROOT, "src", "runtime", f) for f in RUNTIME]
    for d in DIRS:
        p = os.path.join(ROOT, d)
        out += sorted(os.path.join(p, f) for f in os.listdir(p) if f.endswith(".c"))
    return out


def flags():
    rt = os.path.join(ROOT, "src", "runtime")
    inc = os.path.join(rt, "platform", "linux-include")
    return ["-std=gnu11", "-w", "-O2", "-fno-strict-aliasing", "-fwrapv",
            "-DUMK3_HAVE_MK3", "-DUMK3_REAL_GL", "-DUMK3_SHELL", "-DUMK3_LOW32",
            "-fms-extensions", "-fno-jump-tables",
            "-D_GNU_SOURCE=1", "-D_REENTRANT", "-DUMK3_NO_SDL_MIXER=1",
            "-D_strdup=strdup", "-D_snprintf=snprintf",
            "-I", rt, "-I", os.path.join(ROOT, "src", "decomp", "lime"),
            "-I", os.path.join(ROOT, "src", "decomp", "gamecode", "logic"),
            "-I", os.path.join(rt, "platform", "vulkan-include"),
            "-include", os.path.join(rt, "savestate.h"),
            "-idirafter", inc, "-idirafter", os.path.join(inc, "SDL2")]


def main():
    srcs = sources()
    fl = flags()
    with tempfile.TemporaryDirectory() as tmp:
        def one(i_src):
            i, src = i_src
            r = subprocess.run(CC + fl + ["-c", src, "-o", os.path.join(tmp, "%d.o" % i)],
                               capture_output=True, text=True)
            return src, r.returncode, r.stderr
        with ThreadPoolExecutor(os.cpu_count() or 2) as ex:
            results = list(ex.map(one, enumerate(srcs)))
    bad = [(s, e) for s, rc, e in results if rc != 0]
    for s, e in bad:
        print("FAILED %s\n%s" % (os.path.relpath(s, ROOT), e[-3000:]))
    print("%d of %d sources compiled for Linux with the bundled headers only"
          % (len(srcs) - len(bad), len(srcs)))
    print("not linked: the data tables come from the player's .ipa, which CI does not have")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
