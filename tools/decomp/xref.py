"""
Busca todas las llamadas (bl/blx) a un simbolo importado y, cuando el destino
es __assert_rtn, reconstruye sus argumentos.

__assert_rtn(const char *func, const char *file, int line, const char *expr)
recibe los tres punteros a cadena y el numero de linea en r0..r3, y en Thumb-2
el compilador los carga casi siempre desde el pool de literales justo antes de
la llamada. Eso permite recuperar de forma ESTATICA que asercion falla, sin
necesidad de ejecutar nada.

Uso:
  python xref.py <slice_thin> <nombre_simbolo>
  python xref.py <slice_thin> ___assert_rtn --assert
"""

import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from macho import MachO, read_file, N_STAB, N_TYPE, N_SECT  # noqa: E402

from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_LITTLE_ENDIAN
from capstone.arm_const import ARM_OP_IMM

N_ARM_THUMB_DEF = 0x0008
OUT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))


def load(path):
    m = MachO(read_file(path), 0)
    text = m.section_by_name("__TEXT", "__text")
    funcs = {}
    for name, n_type, n_sect, n_desc, n_value in m.symbols():
        if n_type & N_STAB or (n_type & N_TYPE) != N_SECT:
            continue
        a = n_value & ~1
        if not text.contains_addr(a):
            continue
        prev = funcs.get(a)
        if prev is None or len(name) > len(prev):
            funcs[a] = name
    return m, text, funcs


def read_at(m, addr, size=4):
    for sec in m.sections:
        if sec.contains_addr(addr) and sec.offset:
            off = sec.offset + (addr - sec.addr)
            raw = m.data[off:off + size]
            if len(raw) == size:
                return int.from_bytes(raw, "little")
    return None


def cstring(m, addr, limit=300):
    for sec in m.sections:
        if sec.contains_addr(addr) and sec.offset:
            off = sec.offset + (addr - sec.addr)
            end = m.data.find(b"\0", off, off + limit)
            if end < 0:
                return None
            try:
                return m.data[off:end].decode("utf-8", "replace")
            except Exception:  # noqa: BLE001
                return None
    return None


def scan_bl(m, text, targets):
    """
    Encuentra todas las BL/BLX de Thumb-2 que apuntan a alguna de `targets`.

    No se usa el desensamblador lineal porque se corta en el primer pool de
    literales y se pierde el resto de la funcion. Aqui se decodifica el
    encoding a mano en cada posicion alineada a 2 bytes, que es exhaustivo.

      hw1 = 11110 S imm10
      hw2 = 11 J1 1 J2 imm11   -> BL
      hw2 = 11 J1 0 J2 imm11   -> BLX (destino alineado a 4)
      I1 = ~(J1 ^ S), I2 = ~(J2 ^ S)
      offset = SignExtend(S:I1:I2:imm10:imm11:0)
    """
    want = set(targets)
    data = m.data
    base = text.offset
    out = []
    for i in range(0, text.size - 4, 2):
        hw1 = struct.unpack_from("<H", data, base + i)[0]
        if (hw1 & 0xF800) != 0xF000:
            continue
        hw2 = struct.unpack_from("<H", data, base + i + 2)[0]
        if (hw2 & 0xC000) != 0xC000:
            continue
        is_bl = bool(hw2 & 0x1000)
        s = (hw1 >> 10) & 1
        imm10 = hw1 & 0x3FF
        j1 = (hw2 >> 13) & 1
        j2 = (hw2 >> 11) & 1
        imm11 = hw2 & 0x7FF
        i1 = 1 - (j1 ^ s)
        i2 = 1 - (j2 ^ s)
        off = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1)
        if s:
            off -= 1 << 25
        addr = text.addr + i
        if is_bl:
            tgt = addr + 4 + off
        else:
            tgt = ((addr + 4) & ~3) + off
        if tgt in want:
            out.append(addr)
    return out


def owner(addr, funcs):
    """Funcion que contiene esa direccion."""
    best = None
    for a in funcs:
        if a <= addr and (best is None or a > best):
            best = a
    return best, funcs.get(best, "?")


def file_map():
    mp = {}
    p = os.path.join(OUT, "func-to-file.txt")
    if os.path.exists(p):
        with open(p, encoding="utf-8") as fh:
            for line in fh:
                if line.startswith("0x"):
                    q = line.split()
                    if len(q) >= 3:
                        mp[q[1]] = q[2].replace("\\", "/").rsplit("/", 1)[-1]
    return mp


def main():
    path, target = sys.argv[1], sys.argv[2]
    want_assert = "--assert" in sys.argv

    m, text, funcs = load(path)
    stubs = m.stub_map()
    fmap = file_map()

    stub_addrs = [a for a, n in stubs.items() if n == target]
    if not stub_addrs:
        cands = sorted(set(n for n in stubs.values() if target.strip("_") in n))
        raise SystemExit("simbolo no encontrado. candidatos: %s" % cands[:20])
    print("stub(s) de %s: %s" % (target, ", ".join("0x%08x" % a for a in stub_addrs)))

    md = Cs(CS_ARCH_ARM, CS_MODE_THUMB | CS_MODE_LITTLE_ENDIAN)
    md.detail = True

    hits = sorted(set(scan_bl(m, text, stub_addrs)))
    print("llamadas encontradas: %d\n" % len(hits))

    for h in hits:
        fa, fn = owner(h, funcs)
        src = fmap.get(fn, "?")
        print("0x%08x  en %s   [%s]" % (h, fn, src))

        if not want_assert:
            continue

        # Reconstruir r0..r3. Se desensambla desde el INICIO de la funcion:
        # empezar en una direccion arbitraria desincroniza el flujo Thumb
        # (16 y 32 bits mezclados) y produce basura.
        if fa is None:
            print()
            continue
        off = text.offset + (fa - text.addr)
        regs = {}
        for ins in md.disasm(m.data[off:off + (h - fa) + 4], fa):
            if ins.address > h:
                break
            mn = ins.mnemonic.split(".")[0]
            ops = ins.operands
            if not ops or ops[0].type != 1:
                continue
            dst = ins.reg_name(ops[0].reg)
            if dst not in ("r0", "r1", "r2", "r3"):
                continue
            if mn == "ldr" and len(ops) > 1 and ops[1].type == 3 \
                    and ops[1].mem.base and ins.reg_name(ops[1].mem.base) == "pc":
                lit = (((ins.address + 4) & ~3) + ops[1].mem.disp)
                regs[dst] = ("lit", read_at(m, lit))
            elif mn in ("mov", "movs", "movw") and len(ops) > 1 and ops[1].type == ARM_OP_IMM:
                regs[dst] = ("imm", ops[1].imm)
            elif mn == "movt" and dst in regs and regs[dst][0] == "imm":
                regs[dst] = ("imm", (regs[dst][1] & 0xFFFF) | (ops[1].imm << 16))
            elif mn in ("adds", "add") and len(ops) >= 2:
                # "ldr rX,[pc,#n]; add rX, pc" -> el literal es un offset
                last = ops[-1]
                if last.type == 1 and ins.reg_name(last.reg) == "pc" \
                        and dst in regs and regs[dst][0] == "lit" \
                        and regs[dst][1] is not None:
                    regs[dst] = ("abs", (regs[dst][1] + ins.address + 4) & 0xFFFFFFFF)
                elif last.type == ARM_OP_IMM and dst in regs and regs[dst][0] == "imm":
                    regs[dst] = ("imm", (regs[dst][1] + last.imm) & 0xFFFFFFFF)
            elif mn == "subs" and len(ops) >= 2 and ops[-1].type == ARM_OP_IMM \
                    and dst in regs and regs[dst][0] == "imm":
                regs[dst] = ("imm", (regs[dst][1] - ops[-1].imm) & 0xFFFFFFFF)

        for i, lab in enumerate(("func", "file", "line", "expr")):
            r = "r%d" % i
            if r not in regs:
                continue
            kind, val = regs[r]
            if val is None:
                continue
            if lab == "line":
                print("        %-5s = %d" % (lab, val if val < 0x80000000 else val - (1 << 32)))
            elif lab == "file":
                s = cstring(m, val)
                print("        %-5s = %s" % (lab, (s.rsplit("/", 1)[-1] if s else "0x%08x" % val)))
            else:
                s = cstring(m, val)
                print("        %-5s = %s" % (lab, ("%r" % s) if s else "0x%08x" % val))
        print()


if __name__ == "__main__":
    main()
