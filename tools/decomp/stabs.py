"""
Analiza los simbolos STAB del binario para reconstruir el arbol de fuentes original.

El binario no fue stripped y conserva la informacion de depuracion en formato STABS.
Las entradas relevantes:
  N_SO  (0x64) - par (directorio, archivo) del translation unit
  N_OSO (0x66) - ruta del .o intermedio y su timestamp
  N_FUN (0x24) - funcion: "nombre:F<tipo>" en n_value = direccion
  N_BNSYM/N_ENSYM - inicio/fin de simbolo
  N_SLINE (0x44) - numero de linea -> direccion

Genera:
  OUTPUT/source-tree.md      arbol de directorios/archivos reconstruido
  OUTPUT/func-to-file.txt    mapa direccion -> funcion -> archivo fuente

Uso: python stabs.py <slice_thin> <dir_salida>
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from macho import MachO, read_file, N_STAB  # noqa: E402

N_SO = 0x64
N_OSO = 0x66
N_FUN = 0x24
N_SLINE = 0x44
N_BNSYM = 0x2E
N_ENSYM = 0x4E
N_STSYM = 0x26
N_LCSYM = 0x28
N_GSYM = 0x20

STAB_NAMES = {
    0x20: "N_GSYM", 0x22: "N_FNAME", 0x24: "N_FUN", 0x26: "N_STSYM",
    0x28: "N_LCSYM", 0x2E: "N_BNSYM", 0x3C: "N_OPT", 0x40: "N_RSYM",
    0x44: "N_SLINE", 0x4E: "N_ENSYM", 0x60: "N_SSYM", 0x64: "N_SO",
    0x66: "N_OSO", 0x80: "N_LSYM", 0x82: "N_BINCL", 0x84: "N_SOL",
    0xA0: "N_PSYM", 0xA2: "N_EINCL", 0xA4: "N_ENTRY", 0xC0: "N_LBRAC",
    0xC2: "N_EXCL", 0xE0: "N_RBRAC", 0xE2: "N_BCOMM", 0xE4: "N_ECOMM",
    0xE8: "N_ECOML", 0xFE: "N_LENG",
}


class TU(object):
    """Translation unit: un archivo .cpp/.c/.m con sus funciones."""

    def __init__(self, directory, filename):
        self.directory = directory
        self.filename = filename
        self.objfile = None
        self.funcs = []   # (addr, nombre, tipo_stab)
        self.statics = [] # (addr, nombre)

    @property
    def path(self):
        return (self.directory or "") + (self.filename or "")


def parse(path):
    m = MachO(read_file(path), 0)
    tus = []
    cur = None
    pending_dir = None

    for name, n_type, n_sect, n_desc, n_value in m.symbols():
        if not (n_type & N_STAB):
            continue
        st = n_type & 0xFF

        if st == N_SO:
            if name == "":
                # N_SO vacio = fin del translation unit
                cur = None
                pending_dir = None
            elif name.endswith("/"):
                pending_dir = name
                cur = None
            else:
                cur = TU(pending_dir, name)
                tus.append(cur)
                pending_dir = None
        elif st == N_OSO:
            if cur is not None:
                cur.objfile = name
            else:
                # algunos linkers emiten N_OSO antes del par N_SO completo
                cur = TU(None, None)
                cur.objfile = name
                tus.append(cur)
        elif st == N_FUN and cur is not None:
            if ":" in name:
                fname, sig = name.split(":", 1)
            else:
                fname, sig = name, ""
            if fname:  # N_FUN con nombre vacio marca el tamano de la funcion previa
                cur.funcs.append((n_value, fname, sig))
        elif st in (N_STSYM, N_LCSYM) and cur is not None:
            sname = name.split(":", 1)[0]
            cur.statics.append((n_value, sname))

    return m, tus


def build_tree(tus):
    """Agrupa las rutas por directorio."""
    tree = {}
    for tu in tus:
        p = tu.path
        if not p:
            p = tu.objfile or "<desconocido>"
        p = p.replace("\\", "/")
        d, _, f = p.rpartition("/")
        tree.setdefault(d or "<raiz>", []).append((f, tu))
    return tree


def main(binpath, outdir):
    m, tus = parse(binpath)
    tree = build_tree(tus)

    total_funcs = sum(len(t.funcs) for t in tus)

    # ---- source-tree.md ----
    lines = ["# Arbol de fuentes reconstruido (STABS del binario armv7)", "",
             "Generado por `OUTPUT/tools/stabs.py` a partir de la informacion de",
             "depuracion que el binario conserva (no fue stripped).", "",
             "- Translation units: **%d**" % len(tus),
             "- Funciones con entrada N_FUN: **%d**" % total_funcs,
             "- Directorios distintos: **%d**" % len(tree), "", "---", ""]

    for d in sorted(tree):
        files = sorted(tree[d], key=lambda x: x[0] or "")
        nf = sum(len(t.funcs) for _, t in files)
        lines.append("## `%s/`  — %d archivos, %d funciones" % (d, len(files), nf))
        lines.append("")
        for f, tu in files:
            lines.append("- `%s` — %d funciones" % (f, len(tu.funcs)))
        lines.append("")

    with open(os.path.join(outdir, "source-tree.md"), "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines) + "\n")

    # ---- func-to-file.txt ----
    rows = []
    for tu in tus:
        src = tu.path or tu.objfile or "?"
        for addr, fname, sig in tu.funcs:
            rows.append((addr, fname, src))
    rows.sort()
    out = ["# direccion -> funcion -> archivo fuente  (%d entradas)" % len(rows), ""]
    for addr, fname, src in rows:
        out.append("0x%08x  %-52s %s" % (addr, fname, src))
    with open(os.path.join(outdir, "func-to-file.txt"), "w", encoding="utf-8") as fh:
        fh.write("\n".join(out) + "\n")

    print("translation units: %d" % len(tus))
    print("funciones N_FUN:   %d" % total_funcs)
    print("directorios:       %d" % len(tree))
    for d in sorted(tree):
        print("  %-46s %3d archivos" % (d, len(tree[d])))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
