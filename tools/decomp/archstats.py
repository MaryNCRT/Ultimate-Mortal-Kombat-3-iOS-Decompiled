"""
Mide la proporcion ARM vs Thumb del codigo, que es lo que define la dificultad
real del recompilador.

Clasifica cada funcion de __TEXT,__text por el flag N_ARM_THUMB_DEF (0x0008) de
n_desc -- este binario NO usa el bit 0 del valor del simbolo para marcar Thumb.
El tamano de cada funcion se deduce de la direccion del siguiente simbolo.

Tambien inventaria, con capstone, que mnemonicos aparecen y con que frecuencia:
esa lista es exactamente el trabajo pendiente del recompilador.

Uso:
  python archstats.py <slice_thin> [archivo_fuente]

Si se indica un archivo fuente (p.ej. Matrix.cpp) el desglose se limita a el.
"""

import collections
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from macho import MachO, read_file, N_STAB, N_TYPE, N_SECT  # noqa: E402

from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM, CS_MODE_THUMB, CS_MODE_LITTLE_ENDIAN
from capstone.arm_const import ARM_CC_AL, ARM_CC_INVALID

N_ARM_THUMB_DEF = 0x0008
OUTDIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")


def collect(path):
    """Devuelve [(addr, size, thumb, nombre)] ordenado por direccion."""
    m = MachO(read_file(path), 0)
    text = m.section_by_name("__TEXT", "__text")

    best = {}
    for name, n_type, n_sect, n_desc, n_value in m.symbols():
        if n_type & N_STAB or (n_type & N_TYPE) != N_SECT:
            continue
        addr = n_value & ~1
        if not text.contains_addr(addr):
            continue
        thumb = bool(n_value & 1) or bool(n_desc & N_ARM_THUMB_DEF)
        prev = best.get(addr)
        if prev is None or len(name) > len(prev[0]):
            best[addr] = (name, thumb)

    addrs = sorted(best)
    end = text.addr + text.size
    out = []
    for i, a in enumerate(addrs):
        nxt = addrs[i + 1] if i + 1 < len(addrs) else end
        name, thumb = best[a]
        out.append((a, nxt - a, thumb, name))
    return m, text, out


def load_file_map():
    """Lee func-to-file.txt -> {nombre_funcion: archivo_fuente}."""
    path = os.path.join(OUTDIR, "func-to-file.txt")
    mapping = {}
    if not os.path.exists(path):
        return mapping
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            if not line.startswith("0x"):
                continue
            parts = line.split()
            if len(parts) >= 3:
                mapping[parts[1]] = parts[2].replace("\\", "/").rsplit("/", 1)[-1]
    return mapping


def main(path, only_file=None):
    m, text, funcs = collect(path)
    fmap = load_file_map()

    if only_file:
        funcs = [f for f in funcs if fmap.get(f[3].lstrip("_"), "") == only_file
                 or fmap.get(f[3], "") == only_file]
        print("filtrado a %s: %d funciones\n" % (only_file, len(funcs)))

    n_thumb = sum(1 for f in funcs if f[2])
    b_thumb = sum(f[1] for f in funcs if f[2])
    n_arm = len(funcs) - n_thumb
    b_arm = sum(f[1] for f in funcs if not f[2])
    total_b = b_thumb + b_arm

    print("funciones: %d   (Thumb %d / ARM %d)" % (len(funcs), n_thumb, n_arm))
    if len(funcs):
        print("  por conteo: Thumb %5.1f%%   ARM %5.1f%%"
              % (100.0 * n_thumb / len(funcs), 100.0 * n_arm / len(funcs)))
    if total_b:
        print("  por bytes:  Thumb %5.1f%%   ARM %5.1f%%   (%d bytes de codigo)"
              % (100.0 * b_thumb / total_b, 100.0 * b_arm / total_b, total_b))

    # ---- inventario de instrucciones ----
    md_t = Cs(CS_ARCH_ARM, CS_MODE_THUMB | CS_MODE_LITTLE_ENDIAN)
    md_a = Cs(CS_ARCH_ARM, CS_MODE_ARM | CS_MODE_LITTLE_ENDIAN)
    md_t.detail = True
    md_a.detail = True
    mnem = collections.Counter()
    cond_branch = 0
    cond_nonbranch = 0
    itblocks = 0
    decoded = 0
    undecoded_bytes = 0
    tail_gap = collections.Counter()

    for addr, size, thumb, _name in funcs:
        off = text.offset + (addr - text.addr)
        code = m.data[off:off + size]
        md = md_t if thumb else md_a
        n = 0
        for ins in md.disasm(code, addr):
            mnem[ins.mnemonic] += 1
            decoded += 1
            n += ins.size
            base = ins.mnemonic.split(".")[0]
            if base == "it" or (base.startswith("it") and set(base[2:]) <= set("te")):
                itblocks += 1
            # condicion real, segun capstone -- no por el sufijo del texto
            # ("movs"/"lsls" acaban en "vs"/"ls" pero no son condicionales)
            cc = ins.cc
            if cc not in (ARM_CC_AL, ARM_CC_INVALID, 0):
                if base.startswith("b") and not base.startswith("bic"):
                    cond_branch += 1
                else:
                    cond_nonbranch += 1
        undecoded_bytes += size - n
        if size - n:
            tail_gap[size - n] += 1

    print("\ninstrucciones decodificadas: %d   (%d bytes no decodificados = %.2f%%)"
          % (decoded, undecoded_bytes,
             100.0 * undecoded_bytes / total_b if total_b else 0))
    print("mnemonicos distintos: %d" % len(mnem))
    print("bloques IT (ejecucion condicional Thumb): %d" % itblocks)
    print("saltos condicionales: %d" % cond_branch)
    print("instrucciones condicionales NO-salto (dentro de IT): %d" % cond_nonbranch)
    print("funciones con bytes sin decodificar al final: %d de %d"
          % (sum(tail_gap.values()), len(funcs)))

    print("\n-- 40 mnemonicos mas frecuentes --")
    for k, v in mnem.most_common(40):
        print("  %-12s %7d  %5.2f%%" % (k, v, 100.0 * v / decoded if decoded else 0))

    # cobertura acumulada: cuantos mnemonicos hacen falta para cubrir el 95/99%
    running = 0
    marks = {}
    for i, (_k, v) in enumerate(mnem.most_common(), 1):
        running += v
        for pct in (90, 95, 99):
            if pct not in marks and decoded and running >= decoded * pct / 100.0:
                marks[pct] = i
    print("\ncobertura: %d mnemonicos cubren el 90%%, %d el 95%%, %d el 99%%"
          % (marks.get(90, 0), marks.get(95, 0), marks.get(99, 0)))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else None)
