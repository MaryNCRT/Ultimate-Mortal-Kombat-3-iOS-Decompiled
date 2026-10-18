"""
Parser y validador del formato .meshset de UMK3 iOS.

El layout se derivo del desensamblado de _LIME_LoadMeshSet (0x0005ea34 en la
slice armv7) y se valida aqui contra los archivos reales: si el layout es
correcto, el offset final tras leer todas las mallas debe caer exactamente
donde empieza el bloque de texto vestigial ("//====...") o el fin del archivo.

Uso:
  python meshset.py validate <dir_res>          valida todos los .meshset
  python meshset.py dump     <archivo.meshset>  imprime cabeceras
  python meshset.py obj      <archivo.meshset> <dir_salida>   exporta OBJ
"""

import os
import struct
import sys

# Variante A ("comprimida", personajes y efectos): la que carga _LIME_LoadMeshSet.
#   cabecera 140 B = char name[64], char texture[64], int numVerts, int numFaces, float
#   vertice   26 B = int16 x,y,z (/32767) + float u,v + 12 B que el motor ignora
# Variante B ("float indexada", geometria de escenario): sin el campo float.
#   cabecera 136 B = char name[64], char texture[64], int numVerts, int numFaces
#   indices  6 B x numFaces, luego vertice 20 B = float x,y,z + float u,v
# Variante C ("float sin indexar"): misma cabecera de 136 B, pero NO hay buffer
#   de indices: siguen directamente numFaces*3 vertices de 20 B (lista de
#   triangulos ya expandida). numVerts guarda el conteo previo a duplicar.
HEADER_A = "<64s64siif"
HEADER_B = "<64s64sii"
HEADER_SIZE_A = struct.calcsize(HEADER_A)   # 140
HEADER_SIZE_B = struct.calcsize(HEADER_B)   # 136
FACE_SIZE = 6                    # 3 x uint16
VERT_SIZE_A = 26
VERT_SIZE_B = 20
POS_SCALE = 32767.0

# El bloque de texto vestigial que el exportador dejaba al final. Se usa la
# cadena larga completa: un simple "//=====" aparece por casualidad dentro de
# los datos binarios de algunas mallas.
TAIL_MARK = b"//=====================================================\n// Ptr to each MESHINFO"


def cstr(b):
    return b.split(b"\0", 1)[0].decode("ascii", "replace")


class Mesh(object):
    def __init__(self):
        self.name = ""
        self.texture = ""
        self.num_verts = 0
        self.num_faces = 0
        self.radius = 0.0
        self.variant = "A"
        self.faces = []    # [(a,b,c)]
        self.verts = []    # [(x,y,z,u,v)] con x,y,z ya normalizados
        self.tail = b""    # los 12 bytes por vertice que el motor ignora


def parse_as(data, variant, keep_tail=False):
    """Parsea con una variante concreta ('A' o 'B'). Devuelve (meshes, offset_final)."""
    (num,) = struct.unpack_from("<i", data, 0)
    if num < 0 or num > 100000:
        raise ValueError("numMeshes fuera de rango: %d" % num)
    hfmt = HEADER_A if variant == "A" else HEADER_B
    hsize = HEADER_SIZE_A if variant == "A" else HEADER_SIZE_B
    vsize = VERT_SIZE_A if variant == "A" else VERT_SIZE_B
    indexed = variant in ("A", "B")

    off = 4
    meshes = []
    for _ in range(num):
        m = Mesh()
        m.variant = variant
        fields = struct.unpack_from(hfmt, data, off)
        off += hsize
        m.name, m.texture = cstr(fields[0]), cstr(fields[1])
        m.num_verts, m.num_faces = fields[2], fields[3]
        m.radius = fields[4] if variant == "A" else 0.0
        if m.num_verts < 0 or m.num_faces < 0:
            raise ValueError("conteos negativos en '%s'" % m.name)

        if indexed:
            for _f in range(m.num_faces):
                m.faces.append(struct.unpack_from("<3H", data, off))
                off += FACE_SIZE
            nverts = m.num_verts
        else:
            # sin indices: los vertices vienen ya expandidos, 3 por triangulo
            nverts = m.num_faces * 3
            m.faces = [(i, i + 1, i + 2) for i in range(0, nverts, 3)]

        for _v in range(nverts):
            if variant == "A":
                x, y, z = struct.unpack_from("<3h", data, off)
                u, v = struct.unpack_from("<2f", data, off + 6)
                x, y, z = x / POS_SCALE, y / POS_SCALE, z / POS_SCALE
                if keep_tail:
                    m.tail += data[off + 14:off + 26]
            else:
                x, y, z, u, v = struct.unpack_from("<5f", data, off)
            m.verts.append((x, y, z, u, v))
            off += vsize

        meshes.append(m)
    return meshes, off


def parse(data, keep_tail=False):
    """
    Autodetecta la variante: se prueba cada una y se acepta la que termina
    exactamente donde empieza el bloque de texto final (o el fin del archivo).
    """
    want = expected_end(data)
    errors = []
    for variant in ("A", "B", "C"):
        try:
            meshes, off = parse_as(data, variant, keep_tail)
        except Exception as e:  # noqa: BLE001
            errors.append("%s: %s" % (variant, e))
            continue
        if off == want:
            return meshes, off
        errors.append("%s: termina en %d, esperado %d" % (variant, off, want))
    raise ValueError("ninguna variante encaja (%s)" % "; ".join(errors))


def expected_end(data):
    """Donde deberia terminar la geometria: inicio del bloque de texto o EOF."""
    i = data.find(TAIL_MARK)
    return i if i >= 0 else len(data)


def cmd_validate(resdir):
    files = sorted(f for f in os.listdir(resdir) if f.lower().endswith(".meshset"))
    ok = bad = empty = 0
    problems = []
    by_variant = {"A": [], "B": [], "C": []}
    tot_meshes = tot_verts = tot_faces = 0

    for fn in files:
        path = os.path.join(resdir, fn)
        data = open(path, "rb").read()
        if not data:
            empty += 1
            continue
        try:
            meshes, _off = parse(data)
        except Exception as e:  # noqa: BLE001
            bad += 1
            problems.append("%-40s %s" % (fn, e))
            continue
        ok += 1
        by_variant[meshes[0].variant if meshes else "A"].append(fn)
        tot_meshes += len(meshes)
        tot_verts += sum(m.num_verts for m in meshes)
        tot_faces += sum(m.num_faces for m in meshes)

    print("archivos .meshset:   %d" % len(files))
    print("  parseados exactos: %d" % ok)
    print("    A  int16 indexada  (cab 140B, vert 26B): %d" % len(by_variant["A"]))
    print("    B  float indexada  (cab 136B, vert 20B): %d" % len(by_variant["B"]))
    print("    C  float sin index (cab 136B, vert 20B): %d" % len(by_variant["C"]))
    print("  vacios (0 bytes):  %d" % empty)
    print("  sin encajar:       %d" % bad)
    for v in ("B", "C"):
        if by_variant[v]:
            print("\n-- archivos en variante %s --" % v)
            for fn in by_variant[v]:
                print("  " + fn)
    print("mallas totales:      %d" % tot_meshes)
    print("vertices totales:    %d" % tot_verts)
    print("triangulos totales:  %d" % tot_faces)
    if problems:
        print("\n-- discrepancias --")
        for p in problems[:40]:
            print("  " + p)
    return bad


def cmd_dump(path):
    data = open(path, "rb").read()
    meshes, off = parse(data)
    print("%s: %d bytes, %d mallas, geometria termina en %d (esperado %d)"
          % (os.path.basename(path), len(data), len(meshes), off, expected_end(data)))
    print("%-4s %-34s %-26s %8s %8s %10s" % ("#", "malla", "textura", "verts", "tris", "radio"))
    for i, m in enumerate(meshes):
        print("%-4d %-34s %-26s %8d %8d %10.3f"
              % (i, m.name, m.texture, m.num_verts, m.num_faces, m.radius))


def cmd_obj(path, outdir):
    data = open(path, "rb").read()
    meshes, _ = parse(data)
    os.makedirs(outdir, exist_ok=True)
    for i, m in enumerate(meshes):
        safe = "".join(c if c.isalnum() or c in "._-" else "_" for c in m.name)
        fp = os.path.join(outdir, "%d_%s.obj" % (i, safe))
        with open(fp, "w", encoding="utf-8") as f:
            f.write("# %s  textura=%s\n" % (m.name, m.texture))
            for x, y, z, _u, _v in m.verts:
                f.write("v %f %f %f\n" % (x, y, z))
            for _x, _y, _z, u, v in m.verts:
                f.write("vt %f %f\n" % (u, 1.0 - v))
            for a, b, c in m.faces:
                f.write("f %d/%d %d/%d %d/%d\n" % (a + 1, a + 1, b + 1, b + 1, c + 1, c + 1))
    print("exportadas %d mallas a %s" % (len(meshes), outdir))


if __name__ == "__main__":
    action = sys.argv[1]
    if action == "validate":
        raise SystemExit(1 if cmd_validate(sys.argv[2]) else 0)
    elif action == "dump":
        cmd_dump(sys.argv[2])
    elif action == "obj":
        cmd_obj(sys.argv[2], sys.argv[3])
    else:
        raise SystemExit("accion desconocida")
