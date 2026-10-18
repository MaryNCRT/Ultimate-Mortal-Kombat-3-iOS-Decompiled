#!/usr/bin/env python3
"""check_banned_words.py -- refuse words that must never enter the repository.

    python tools/check_banned_words.py            # every tracked file
    python tools/check_banned_words.py --staged   # what is about to be committed

The words are listed only as SHA-256 hashes of their lower-case form, so this
file does not carry them. Every run of letters in every file (and, with
--staged, in the commit message file git passes a commit-msg hook) is hashed
and compared. Exit status 1 and the file:line of each hit when one is found.

Installed as hooks by `git config core.hooksPath .githooks`; run by CI too.
"""

import hashlib
import re
import subprocess
import sys

BANNED = {
    "00e48a815525529ba9d33f8761a167588fe00c47bc82f515cf791c482ed99ecc",
}

WORD = re.compile(rb"[A-Za-z]+")


def hits(name, data):
    out = []
    for n, line in enumerate(data.splitlines(), 1):
        for w in WORD.findall(line):
            if len(w) < 64 and hashlib.sha256(w.lower()).hexdigest() in BANNED:
                out.append("%s:%d" % (name, n))
                break
    return out


def git(*args):
    return subprocess.run(["git"] + list(args), capture_output=True,
                          check=True).stdout


def main(argv):
    found = []
    if "--message" in argv:                     # commit-msg hook: the file
        path = argv[argv.index("--message") + 1]
        found += hits("commit message", open(path, "rb").read())
    elif "--staged" in argv:
        for name in git("diff", "--cached", "--name-only", "-z").split(b"\0"):
            if name:
                try:
                    data = git("show", b":" + name)
                except subprocess.CalledProcessError:
                    continue                    # deleted
                found += hits(name.decode("utf-8", "replace"), data)
    else:
        for name in git("ls-files", "-z").split(b"\0"):
            if name:
                try:
                    data = open(name, "rb").read()
                except OSError:
                    continue
                found += hits(name.decode("utf-8", "replace"), data)
    for f in found:
        print("banned word at " + f)
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
