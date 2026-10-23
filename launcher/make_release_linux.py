#!/usr/bin/env python3
"""make_release_linux.py -- the Linux download, from the launcher built for Linux.

    python launcher/make_release_linux.py <linux-unpacked dir> <out .tar.gz>

<linux-unpacked dir> is `electron-builder --linux dir` in launcher/app (its
resources/ carries src/ and the table generators, package.json
extraResources). Beside the launcher go LEEME.txt, README-linux.md,
build-game.sh (the build from a terminal), install-shortcut.sh (a menu
entry), eos.ini.example and the icon. The EOS SDK (its headers, the Linux
library, the notices) and the product's eos.ini go into resources/TOOLS/eos/
when electron-builder's extraResources did not bring them (TOOLS/eos is not in
the repository, and a worktree has none): make_release.eos_dir, which refuses
to finish without them.

Written with Python's tarfile so the executable bits are right whatever
system packs it: electron-builder's own tar.gz, made on Windows, had the
launcher and its helpers without them.
"""
import os
import sys
import tarfile

from make_release import eos_dir, find_eos_ini

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXTRA = ["LEEME.txt", "README-linux.md", "build-game.sh", "install-shortcut.sh",
         "eos.ini.example", "umk3.png"]
EXEC = {"UMK3-Launcher", "chrome-sandbox", "chrome_crashpad_handler",
        "build-game.sh", "install-shortcut.sh"}


def mode_of(name, is_dir):
    base = os.path.basename(name)
    if is_dir or base in EXEC or base.endswith(".so") or ".so." in base:
        return 0o755
    return 0o644


def main(argv):
    if len(argv) < 3:
        raise SystemExit(__doc__.strip().splitlines()[2].strip())
    src, out = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    top = os.path.basename(out)
    for suffix in (".tar.gz", ".tgz"):
        if top.endswith(suffix):
            top = top[:-len(suffix)]
    top = top.replace("-linux-x64", "-linux")

    def add(tf, path, arc):
        ti = tf.gettarinfo(path, arc)
        ti.mode = mode_of(arc, ti.isdir())
        ti.uid = ti.gid = 0
        ti.uname = ti.gname = "root"
        if ti.isfile():
            with open(path, "rb") as f:
                tf.addfile(ti, f)
        else:
            tf.addfile(ti)

    n = 0
    with tarfile.open(out, "w:gz") as tf:
        for dp, dns, fns in os.walk(src):
            dns.sort()
            rel = os.path.relpath(dp, src)
            arc_dir = top if rel == "." else top + "/" + rel.replace("\\", "/")
            add(tf, dp, arc_dir)
            for fn in sorted(fns):
                add(tf, os.path.join(dp, fn), arc_dir + "/" + fn)
                n += 1
        for e in EXTRA:
            add(tf, os.path.join(ROOT, "launcher", e), top + "/" + e)
            n += 1
        eos = eos_dir()
        # the folder's real name in the package: made on Windows, TOOLS/eos
        # and tools/build share one folder, named as the first came; Linux
        # tells TOOLS from tools, so eos.ini goes beside the SDK, whatever it is
        res = os.path.join(src, "resources")
        tdir = next((n for n in os.listdir(res) if n.lower() == "tools"
                     and os.path.isdir(os.path.join(res, n, "eos"))), "TOOLS")
        have = os.path.join(res, tdir, "eos")
        if eos and not os.path.exists(os.path.join(have, "SDK", "Include", "eos_sdk.h")):
            for sub in ("SDK/Include", "ThirdPartyNotices"):
                for dp, dns, fns in os.walk(os.path.join(eos, sub)):
                    dns.sort()
                    rel = os.path.relpath(dp, eos).replace("\\", "/")
                    add(tf, dp, top + "/resources/" + tdir + "/eos/" + rel)
                    for fn in sorted(fns):
                        add(tf, os.path.join(dp, fn), top + "/resources/" + tdir + "/eos/" + rel + "/" + fn)
                        n += 1
            add(tf, os.path.join(eos, "SDK", "Bin", "libEOSSDK-Linux-Shipping.so"),
                top + "/resources/" + tdir + "/eos/SDK/Bin/libEOSSDK-Linux-Shipping.so")
            n += 1
        ini = find_eos_ini()
        if ini and not os.path.exists(os.path.join(have, "eos.ini")):
            add(tf, ini, top + "/resources/" + tdir + "/eos/eos.ini")
            n += 1
    print("%s: %d files, %.1f MB" % (out, n, os.path.getsize(out) / 1e6))


if __name__ == "__main__":
    main(sys.argv)
