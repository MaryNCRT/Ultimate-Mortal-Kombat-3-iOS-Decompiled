#!/usr/bin/env python3
"""check_binary.py -- is this the UMK3 binary the port was decompiled from?

    python tools/build/check_binary.py <Payload/UMK3.app/UMK3> <uuid>

Every address in src/decomp/ and tools/ belongs to one build: the armv7 slice of
UMK3 1.2.59 for iPhone. Another version would compile and then run on the
wrong tables, so the build stops here instead. An App Store binary that was
never decrypted (cryptid 1) is refused too: its data is unreadable.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import logic_tables  # noqa: E402  (load: thin or fat, armv7 slice)


def main(argv):
    if len(argv) != 3:
        raise SystemExit(__doc__.strip().splitlines()[2].strip())
    try:
        _, _, m = logic_tables.load(argv[1])
    except SystemExit as e:
        print("not a usable UMK3 binary: %s" % e)
        return 1
    if m.encryption and m.encryption[2]:
        print("the binary is encrypted (cryptid %d)" % m.encryption[2])
        return 1
    if m.uuid != argv[2]:
        print("binary uuid %s, the port needs %s" % (m.uuid, argv[2]))
        return 1
    print("binary ok: armv7, uuid %s" % m.uuid)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
