#!/usr/bin/env python3
"""imports.py -- name the C library functions the binary calls.

A `blx #0xddd04` in the disassembly lands in `__symbol_stub4`, and the stub is
three words of indirection:

    ldr ip, [pc, #0]
    ldr pc, [ip]
    .word 0x000f3d34        <- a slot in __la_symbol_ptr

The slot's name is not in the symbol table at that address. It is in the
**indirect symbol table**: `__la_symbol_ptr`'s `reserved1` field is an index
into it, and entry `reserved1 + k` is the symbol index for the k-th slot.

    python tools/imports.py work/UMK3.armv7                 # every import
    python tools/imports.py work/UMK3.armv7 0x000f3d34      # one slot

Without this, a call to an imported function is an unnamed address, and
guessing which libc function it is from the arguments is exactly the kind of
inference this project does not accept. `0x000f3d34` is `_sprintf`, and that is
read, not assumed.
"""

import struct
import sys

VM_BIAS = 0x1000


def parse(data):
    ncmds = struct.unpack_from("<I", data, 16)[0]
    off = 28
    symoff = stroff = indsymoff = 0
    sects = []
    for _ in range(ncmds):
        cmd, size = struct.unpack_from("<II", data, off)
        if cmd == 0x2:                       # LC_SYMTAB
            symoff, _nsyms, stroff, _strsize = struct.unpack_from("<4I", data, off + 8)
        elif cmd == 0xB:                     # LC_DYSYMTAB
            indsymoff = struct.unpack_from("<18I", data, off + 8)[12]
        elif cmd == 0x1:                     # LC_SEGMENT
            nsects = struct.unpack_from("<I", data, off + 48)[0]
            for i in range(nsects):
                s = off + 56 + i * 68
                name = data[s:s + 16].rstrip(b"\0").decode("ascii", "replace")
                addr, size_, _fo, _al, _ro, _nr, flags, r1, _r2 = \
                    struct.unpack_from("<9I", data, s + 32)
                sects.append((name, addr, size_, flags & 0xFF, r1))
        off += size
    return symoff, stroff, indsymoff, sects


def main(argv):
    if len(argv) < 2:
        sys.exit("usage: imports.py <binary> [slot-address]")
    data = open(argv[1], "rb").read()
    want = int(argv[2], 16) if len(argv) > 2 else None
    symoff, stroff, indsymoff, sects = parse(data)

    def symname(i):
        strx = struct.unpack_from("<I", data, symoff + i * 12)[0]
        end = data.index(b"\0", stroff + strx)
        return data[stroff + strx:end].decode("ascii", "replace")

    for name, addr, size, kind, r1 in sects:
        # 6 = S_NON_LAZY_SYMBOL_POINTERS, 7 = S_LAZY_SYMBOL_POINTERS
        if kind not in (6, 7):
            continue
        for k in range(size // 4):
            slot = addr + k * 4
            if want is not None and slot != want:
                continue
            idx = struct.unpack_from("<I", data, indsymoff + (r1 + k) * 4)[0]
            if idx in (0x40000000, 0x80000000, 0xC0000000):
                continue                     # ABS / LOCAL, no name
            print("%-16s 0x%08x  %s" % (name, slot, symname(idx)))


if __name__ == "__main__":
    main(sys.argv)
