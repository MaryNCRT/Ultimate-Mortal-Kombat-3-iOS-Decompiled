"""
Desensambla una funcion del binario armv7 por nombre o direccion, usando capstone.

Resuelve nombres desde la tabla de simbolos, detecta ARM vs Thumb por el bit 0
del valor del simbolo, y delimita la funcion con la direccion del siguiente
simbolo. Anota los destinos de bl/blx con el nombre del simbolo llamado y
resuelve las cargas literales pc-relativas (muy comunes en Thumb-2).

Uso:
  python disasm.py <slice_thin> <nombre_o_0xADDR> [longitud_bytes]
"""

import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from macho import MachO, read_file, N_STAB, N_TYPE, N_SECT  # noqa: E402

N_ARM_THUMB_DEF = 0x0008

from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM, CS_MODE_THUMB, CS_MODE_LITTLE_ENDIAN


def load(path):
    m = MachO(read_file(path), 0)
    syms = {}       # addr -> nombre
    byname = {}     # nombre -> (addr, thumb)
    for name, n_type, n_sect, n_desc, n_value in m.symbols():
        if n_type & N_STAB:
            continue
        if (n_type & N_TYPE) != N_SECT:
            continue
        addr = n_value & ~1
        if addr not in syms or len(name) > len(syms[addr]):
            syms[addr] = name
        # Este binario no marca Thumb con el bit 0 del valor; usa el flag
        # N_ARM_THUMB_DEF (0x0008) de n_desc.
        thumb = bool(n_value & 1) or bool(n_desc & N_ARM_THUMB_DEF)
        byname[name] = (addr, thumb)

    # Calls to imported functions land on a stub, not on a symbol, so without
    # this every `bl` to OpenGL or libc reads as a bare address. That matters
    # most in the armv6 slice, which is where the renderer has to be read
    # because armv7 is packed NEON -- and the renderer is almost entirely
    # calls out to GL.
    try:
        for stub_addr, stub_name in m.stub_map().items():
            syms.setdefault(stub_addr, stub_name)
    except Exception:                                   # noqa: BLE001
        pass                                            # no stub section

    return m, syms, byname


def file_off(m, vmaddr):
    for s in m.sections:
        if s.contains_addr(vmaddr):
            return s.offset + (vmaddr - s.addr)
    return None


def read_word(m, vmaddr):
    off = file_off(m, vmaddr)
    if off is None:
        return None
    return struct.unpack_from("<I", m.data, off)[0]


def cstring(m, vmaddr):
    off = file_off(m, vmaddr)
    if off is None:
        return None
    end = m.data.index(b"\0", off)
    if end - off > 200:
        return None
    try:
        return m.data[off:end].decode("ascii")
    except UnicodeDecodeError:
        return None


def disassemble(path, target, length=None):
    m, syms, byname = load(path)

    if target.startswith("0x"):
        addr = int(target, 16)
        thumb = True
        name = syms.get(addr, "sub_%08x" % addr)
    else:
        if target not in byname:
            cands = [k for k in byname if target in k]
            raise SystemExit("simbolo no encontrado. candidatos: %s" % cands[:10])
        addr, thumb = byname[target]
        name = target

    if length is None:
        later = sorted(a for a in syms if a > addr)
        length = (later[0] - addr) if later else 512

    off = file_off(m, addr)
    code = m.data[off:off + length]

    mode = (CS_MODE_THUMB if thumb else CS_MODE_ARM) | CS_MODE_LITTLE_ENDIAN
    md = Cs(CS_ARCH_ARM, mode)
    md.detail = False

    out = ["; %s @ 0x%08x  (%s, %d bytes, file offset 0x%x)"
           % (name, addr, "Thumb" if thumb else "ARM", length, off), ""]

    for ins in md.disasm(code, addr):
        line = "0x%08x  %-8s %s" % (ins.address, ins.mnemonic, ins.op_str)
        comment = ""

        # destino de llamada -> nombre del simbolo
        if ins.mnemonic.startswith(("bl", "b.w", "b")) and ins.op_str.startswith("#"):
            try:
                dst = int(ins.op_str[1:], 0)
            except ValueError:
                dst = None
            if dst is not None and dst in syms:
                comment = "  ; -> %s" % syms[dst]

        # carga literal pc-relativa: ldr rX, [pc, #N]
        if ins.mnemonic.startswith("ldr") and "[pc, #" in ins.op_str:
            try:
                disp = int(ins.op_str.split("[pc, #")[1].rstrip("]"), 0)
                lit = ((ins.address + 4) & ~3) + disp
                val = read_word(m, lit)
                if val is not None:
                    comment = "  ; =0x%08x" % val
                    s = cstring(m, val)
                    if s:
                        comment += ' "%s"' % s
                    elif (val & ~1) in syms:
                        comment += " -> %s" % syms[val & ~1]
            except (ValueError, IndexError):
                pass

        out.append(line + comment)

    return "\n".join(out)


if __name__ == "__main__":
    ln = int(sys.argv[3]) if len(sys.argv) > 3 else None
    print(disassemble(sys.argv[1], sys.argv[2], ln))
