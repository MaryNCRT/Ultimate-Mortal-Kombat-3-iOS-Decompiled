#!/usr/bin/env python3
"""disasm_range.py -- disassemble an address range, stepping over literal pools.

`annotate.py` disassembles a function by name and is the tool to reach for
first: it resolves call targets, GOT-style slots and PC-relative literals. But
capstone stops at the first byte pattern it cannot decode, and in a function
large enough to need one, the compiler drops a **literal pool in the middle of
the code**. `annotate.py` therefore stops early -- and silently, which is worse.
Several functions in `gamecode` are affected; `FE_Task_Karnage_Summary` is a
clear example, where the last third of the function lives past a pool.

This tool does the crude thing that is right here: disassemble until capstone
gives up, print the undecodable halfword as a `.word`, advance two bytes, and
try again. Pool entries come out as `.word` lines and the code after them comes
out as code.

    python tools/disasm_range.py work/UMK3.armv7 0x00010348 0x00010474

It also resolves the two PC-relative idioms this binary uses constantly, so the
output is readable without cross-referencing by hand:

    ldr rN, [pc, #imm]      followed by      add rN, pc

is annotated with the address it produces and, where one exists, the symbol at
that address or the string it points at.

Addresses are vmaddrs (file offset + 0x1000), the same convention as everywhere
else in this repo.
"""

import struct
import sys

try:
    import capstone
except ImportError:                                     # pragma: no cover
    sys.exit("capstone is required: pip install capstone")

VM_BIAS = 0x1000


def load_symbols(path="work/symbols.txt"):
    """address -> name, first name wins (the STAB entry precedes the SECT one)."""
    syms = {}
    try:
        fh = open(path, encoding="utf-8", errors="replace")
    except OSError:
        return syms
    with fh:
        for line in fh:
            parts = line.split()
            if len(parts) < 5 or not parts[0].startswith("0x"):
                continue
            try:
                addr = int(parts[0], 16)
            except ValueError:
                continue
            name = parts[-1]
            if name.startswith("_") and addr not in syms:
                syms[addr] = name
    return syms


def read_string(data, va, limit=48):
    """Return a printable C string at `va`, or None."""
    off = va - VM_BIAS
    if off < 0 or off >= len(data):
        return None
    blob = data[off:off + limit]
    end = blob.find(b"\0")
    if end <= 0:
        return None
    text = blob[:end]
    if all(32 <= c < 127 for c in text):
        return text.decode("ascii")
    return None


def annotate_pc_relative(data, syms, insn, literals, out):
    """Track `ldr rN,[pc,#imm]` / `add rN,pc` pairs and describe the result."""
    if insn.mnemonic.startswith("ldr") and "[pc, #" in insn.op_str:
        reg, _, rest = insn.op_str.partition(",")
        imm = rest[rest.index("#") + 1:].rstrip("]")
        pool = ((insn.address + 4) & ~3) + int(imm, 16 if imm.startswith("0x") else 10)
        off = pool - VM_BIAS
        if 0 <= off + 4 <= len(data):
            literals[reg.strip()] = struct.unpack_from("<I", data, off)[0]
        return

    if insn.mnemonic == "add" and insn.op_str.endswith(", pc"):
        reg = insn.op_str.split(",")[0].strip()
        if reg in literals:
            target = (literals[reg] + insn.address + 4) & 0xFFFFFFFF
            note = syms.get(target, "")
            if not note:
                text = read_string(data, target)
                note = repr(text) if text else ""
            out.append("        ; -> 0x%08x  %s" % (target, note))
        return

    # Anything else writing a register invalidates the literal we tracked for
    # it. Getting this wrong is how annotate.py once printed a confident and
    # entirely wrong symbol name; see docs/PROGRESS.md.
    if insn.op_str:
        dest = insn.op_str.split(",")[0].strip()
        literals.pop(dest, None)


def disasm_range(binary, start, end, thumb=True):
    data = open(binary, "rb").read()
    syms = load_symbols()
    md = capstone.Cs(
        capstone.CS_ARCH_ARM,
        capstone.CS_MODE_THUMB if thumb else capstone.CS_MODE_ARM,
    )

    literals = {}
    pc = start
    while pc < end:
        decoded = 0
        for insn in md.disasm(data[pc - VM_BIAS:end - VM_BIAS], pc):
            out = ["0x%08x  %-9s %s" % (insn.address, insn.mnemonic, insn.op_str)]
            annotate_pc_relative(data, syms, insn, literals, out)
            print("\n".join(out))
            pc = insn.address + insn.size
            decoded += 1
        if decoded == 0:
            word = struct.unpack_from("<I", data, pc - VM_BIAS)[0]
            note = syms.get(word & ~1, "")
            print("0x%08x  .word     0x%08x%s"
                  % (pc, word, "   ; %s" % note if note else ""))
            pc += 2
            literals.clear()


def main(argv):
    if len(argv) < 4:
        sys.exit(__doc__.strip().splitlines()[0]
                 + "\n\nusage: disasm_range.py <binary> <start> <end> [--arm]")
    disasm_range(argv[1], int(argv[2], 0), int(argv[3], 0),
                 thumb="--arm" not in argv)


if __name__ == "__main__":
    main(sys.argv)
