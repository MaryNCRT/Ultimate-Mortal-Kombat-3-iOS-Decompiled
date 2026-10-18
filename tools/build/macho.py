"""
Parser Mach-O en Python puro (sin dependencias) para el binario UMK3 iOS.

Cubre lo necesario del paso 2 del plan:
  - lectura del fat header y extraccion (thin) de la slice armv7
  - volcado de load commands: segmentos/secciones, dylibs, cryptid, uuid
  - volcado de la tabla de simbolos (LC_SYMTAB)
  - volcado de funciones (simbolos en __text + LC_FUNCTION_STARTS)

Uso:
  python macho.py info   <binario>
  python macho.py thin   <binario> <cputype:cpusubtype|armv7> <salida>
  python macho.py syms   <slice_thin> <salida.txt>
  python macho.py funcs  <slice_thin> <salida.txt>
"""

import struct
import sys

FAT_MAGIC = 0xCAFEBABE
FAT_CIGAM = 0xBEBAFECA
MH_MAGIC = 0xFEEDFACE
MH_CIGAM = 0xCEFAEDFE
MH_MAGIC_64 = 0xFEEDFACF

CPU_TYPE_ARM = 12
ARM_SUBTYPES = {6: "armv6", 9: "armv7", 11: "armv7s"}

LC_REQ_DYLD = 0x80000000
LC_SEGMENT = 0x01
LC_SYMTAB = 0x02
LC_DYSYMTAB = 0x0B
LC_LOAD_DYLIB = 0x0C
LC_ID_DYLIB = 0x0D
LC_LOAD_WEAK_DYLIB = 0x18 | LC_REQ_DYLD
LC_UUID = 0x1B
LC_ENCRYPTION_INFO = 0x21
LC_VERSION_MIN_IPHONEOS = 0x25
LC_FUNCTION_STARTS = 0x26
LC_MAIN = 0x28 | LC_REQ_DYLD

# nlist n_type
N_STAB = 0xE0
N_TYPE = 0x0E
N_EXT = 0x01
N_UNDF = 0x0
N_ABS = 0x2
N_SECT = 0xE
N_PBUD = 0xC
N_INDR = 0xA

TYPE_NAMES = {N_UNDF: "UNDF", N_ABS: "ABS", N_SECT: "SECT", N_PBUD: "PBUD", N_INDR: "INDR"}


def read_file(path):
    with open(path, "rb") as f:
        return f.read()


class Section(object):
    def __init__(self, segname, sectname, addr, size, offset, flags,
                 reserved1=0, reserved2=0):
        self.segname = segname
        self.sectname = sectname
        self.addr = addr
        self.size = size
        self.offset = offset
        self.flags = flags
        # En secciones de stubs/punteros: reserved1 = indice en la tabla de
        # simbolos indirectos, reserved2 = tamano de cada stub.
        self.reserved1 = reserved1
        self.reserved2 = reserved2

    def contains_addr(self, a):
        return self.addr <= a < self.addr + self.size


class MachO(object):
    """Mach-O de 32 bits (little endian) — suficiente para armv6/armv7."""

    def __init__(self, data, base=0):
        self.data = data
        self.base = base  # offset de la slice dentro del fat, 0 si es thin
        magic = struct.unpack_from("<I", data, base)[0]
        if magic == MH_MAGIC_64:
            raise ValueError("Mach-O de 64 bits: no soportado por este parser")
        if magic != MH_MAGIC:
            raise ValueError("magic Mach-O inesperado: 0x%08x" % magic)
        (self.magic, self.cputype, self.cpusubtype, self.filetype,
         self.ncmds, self.sizeofcmds, self.flags) = struct.unpack_from("<7I", data, base)

        self.segments = []      # (segname, vmaddr, vmsize, fileoff, filesize)
        self.sections = []      # Section
        self.dylibs = []
        self.uuid = None
        self.encryption = None  # (cryptoff, cryptsize, cryptid)
        self.symtab = None      # (symoff, nsyms, stroff, strsize)
        self.dysymtab = None
        self.function_starts = None  # (dataoff, datasize)
        self.version_min = None
        self.entry = None
        self._parse_commands()

    def _parse_commands(self):
        off = self.base + 28  # mach_header de 32 bits
        for _ in range(self.ncmds):
            cmd, cmdsize = struct.unpack_from("<2I", self.data, off)
            if cmdsize == 0:
                break
            self._parse_one(cmd, cmdsize, off)
            off += cmdsize

    def _parse_one(self, cmd, cmdsize, off):
        d = self.data
        if cmd == LC_SEGMENT:
            segname = d[off + 8:off + 24].rstrip(b"\0").decode("ascii", "replace")
            vmaddr, vmsize, fileoff, filesize = struct.unpack_from("<4I", d, off + 24)
            nsects = struct.unpack_from("<I", d, off + 48)[0]
            self.segments.append((segname, vmaddr, vmsize, fileoff, filesize))
            so = off + 56
            for _ in range(nsects):
                sectname = d[so:so + 16].rstrip(b"\0").decode("ascii", "replace")
                ssegname = d[so + 16:so + 32].rstrip(b"\0").decode("ascii", "replace")
                addr, size, offset = struct.unpack_from("<3I", d, so + 32)
                flags, res1, res2 = struct.unpack_from("<3I", d, so + 56)
                self.sections.append(Section(ssegname, sectname, addr, size,
                                             offset, flags, res1, res2))
                so += 68
        elif cmd == LC_SYMTAB:
            self.symtab = struct.unpack_from("<4I", d, off + 8)
        elif cmd == LC_DYSYMTAB:
            self.dysymtab = struct.unpack_from("<18I", d, off + 8)
        elif cmd in (LC_LOAD_DYLIB, LC_LOAD_WEAK_DYLIB, LC_ID_DYLIB):
            nameoff = struct.unpack_from("<I", d, off + 8)[0]
            s = off + nameoff
            end = d.index(b"\0", s)
            self.dylibs.append(d[s:end].decode("utf-8", "replace"))
        elif cmd == LC_UUID:
            self.uuid = d[off + 8:off + 24].hex()
        elif cmd == LC_ENCRYPTION_INFO:
            self.encryption = struct.unpack_from("<3I", d, off + 8)
        elif cmd == LC_FUNCTION_STARTS:
            self.function_starts = struct.unpack_from("<2I", d, off + 8)
        elif cmd == LC_VERSION_MIN_IPHONEOS:
            ver, sdk = struct.unpack_from("<2I", d, off + 8)
            self.version_min = (ver, sdk)
        elif cmd == LC_MAIN:
            self.entry = struct.unpack_from("<Q", d, off + 8)[0]

    # ---- helpers ----

    def arch(self):
        if self.cputype == CPU_TYPE_ARM:
            return ARM_SUBTYPES.get(self.cpusubtype, "arm(sub=%d)" % self.cpusubtype)
        return "cpu=%d sub=%d" % (self.cputype, self.cpusubtype)

    def section_by_name(self, segname, sectname):
        for s in self.sections:
            if s.segname == segname and s.sectname == sectname:
                return s
        return None

    def section_for_addr(self, addr):
        for s in self.sections:
            if s.contains_addr(addr):
                return s
        return None

    def symbols(self):
        """Devuelve [(name, n_type, n_sect, n_desc, n_value)]."""
        if not self.symtab:
            return []
        symoff, nsyms, stroff, strsize = self.symtab
        d = self.data
        strbase = self.base + stroff
        out = []
        o = self.base + symoff
        for _ in range(nsyms):
            n_strx, n_type, n_sect, n_desc, n_value = struct.unpack_from("<IBBhI", d, o)
            o += 12
            if n_strx:
                s = strbase + n_strx
                e = d.index(b"\0", s)
                name = d[s:e].decode("utf-8", "replace")
            else:
                name = ""
            out.append((name, n_type, n_sect, n_desc, n_value))
        return out

    def stub_map(self):
        """
        Resuelve la seccion __symbol_stub4 -> {direccion_stub: nombre_importado}.

        Cada stub de la seccion corresponde, en orden, a una entrada de la tabla
        de simbolos indirectos empezando en section.reserved1; esa entrada es un
        indice dentro de la tabla de simbolos normal.
        """
        if not self.dysymtab or not self.symtab:
            return {}
        indirectsymoff, nindirectsyms = self.dysymtab[12], self.dysymtab[13]
        syms = self.symbols()
        out = {}
        for sec in self.sections:
            # S_SYMBOL_STUBS = 0x8, S_LAZY/NON_LAZY_SYMBOL_POINTERS = 0x7 / 0x6
            if (sec.flags & 0xFF) not in (0x6, 0x7, 0x8):
                continue
            stride = sec.reserved2 or 4
            count = sec.size // stride
            for i in range(count):
                idx = sec.reserved1 + i
                if idx >= nindirectsyms:
                    break
                pos = self.base + indirectsymoff + idx * 4
                symidx = struct.unpack_from("<I", self.data, pos)[0]
                # INDIRECT_SYMBOL_LOCAL / _ABS: no hay nombre asociado
                if symidx & 0x80000000 or symidx >= len(syms):
                    continue
                out[sec.addr + i * stride] = syms[symidx][0]
        return out

    def function_start_addrs(self):
        """Decodifica LC_FUNCTION_STARTS (deltas ULEB128 desde el primer segmento)."""
        if not self.function_starts:
            return []
        dataoff, datasize = self.function_starts
        d = self.data
        p = self.base + dataoff
        end = p + datasize
        # base = vmaddr del segmento __TEXT
        addr = 0
        for segname, vmaddr, vmsize, fileoff, filesize in self.segments:
            if segname == "__TEXT":
                addr = vmaddr
                break
        out = []
        while p < end:
            delta = 0
            shift = 0
            while True:
                b = d[p]
                p += 1
                delta |= (b & 0x7F) << shift
                shift += 7
                if not (b & 0x80):
                    break
                if p >= end:
                    break
            if delta == 0:
                break
            addr += delta
            out.append(addr)
        return out


def fat_slices(data):
    """Devuelve [(cputype, cpusubtype, offset, size, align)] o [] si no es fat."""
    magic = struct.unpack_from(">I", data, 0)[0]
    if magic != FAT_MAGIC:
        return []
    nfat = struct.unpack_from(">I", data, 4)[0]
    out = []
    for i in range(nfat):
        out.append(struct.unpack_from(">5I", data, 8 + i * 20))
    return out


# ------------------------- comandos -------------------------

def cmd_info(path):
    data = read_file(path)
    slices = fat_slices(data)
    lines = []
    if slices:
        lines.append("Binario FAT/universal con %d slices:" % len(slices))
        for ct, cs, off, size, align in slices:
            name = ARM_SUBTYPES.get(cs, "?") if ct == CPU_TYPE_ARM else "?"
            lines.append("  cputype=%d cpusubtype=%d (%s) offset=0x%x size=%d (%.2f MB) align=2^%d"
                         % (ct, cs, name, off, size, size / 1048576.0, align))
        targets = [(off, cs) for ct, cs, off, size, align in slices]
    else:
        lines.append("Binario thin (no fat).")
        targets = [(0, None)]

    for off, _cs in targets:
        m = MachO(data, off)
        lines.append("")
        lines.append("=== slice %s (offset 0x%x) ===" % (m.arch(), off))
        lines.append("filetype=%d ncmds=%d flags=0x%08x" % (m.filetype, m.ncmds, m.flags))
        if m.uuid:
            lines.append("uuid=%s" % m.uuid)
        if m.version_min:
            v, s = m.version_min
            fmt = lambda x: "%d.%d.%d" % (x >> 16, (x >> 8) & 0xFF, x & 0xFF)
            lines.append("min iOS=%s sdk=%s" % (fmt(v), fmt(s)))
        if m.encryption:
            co, cs_, cid = m.encryption
            lines.append("LC_ENCRYPTION_INFO: cryptoff=0x%x cryptsize=%d cryptid=%d%s"
                         % (co, cs_, cid, "  -> SIN CIFRAR" if cid == 0 else "  -> CIFRADO"))
        else:
            lines.append("LC_ENCRYPTION_INFO: ausente")
        syms = m.symbols()
        stabs = sum(1 for s in syms if s[1] & N_STAB)
        lines.append("simbolos: %d totales (%d STAB de depuracion, %d normales)"
                     % (len(syms), stabs, len(syms) - stabs))
        lines.append("stripped: %s" % ("NO" if len(syms) - stabs > 1000 else "SI/parcial"))
        fs = m.function_start_addrs()
        lines.append("LC_FUNCTION_STARTS: %d funciones" % len(fs))
        lines.append("")
        lines.append("-- segmentos/secciones --")
        for segname, vmaddr, vmsize, fileoff, filesize in m.segments:
            lines.append("  SEG %-12s vm=0x%08x-0x%08x file=0x%08x+%d"
                         % (segname, vmaddr, vmaddr + vmsize, fileoff, filesize))
            for s in m.sections:
                if s.segname == segname:
                    lines.append("      %-20s addr=0x%08x size=%-9d off=0x%08x flags=0x%08x"
                                 % (s.sectname, s.addr, s.size, s.offset, s.flags))
        lines.append("")
        lines.append("-- dylibs (%d) --" % len(m.dylibs))
        for dl in m.dylibs:
            lines.append("  " + dl)
    return "\n".join(lines)


def cmd_thin(path, want, outpath):
    data = read_file(path)
    slices = fat_slices(data)
    if not slices:
        raise SystemExit("el binario no es fat")
    chosen = None
    for ct, cs, off, size, align in slices:
        name = ARM_SUBTYPES.get(cs, "") if ct == CPU_TYPE_ARM else ""
        if name == want or want == "%d:%d" % (ct, cs):
            chosen = (off, size)
            break
    if not chosen:
        raise SystemExit("slice %s no encontrada" % want)
    off, size = chosen
    with open(outpath, "wb") as f:
        f.write(data[off:off + size])
    return "escrito %s (%d bytes) desde offset 0x%x" % (outpath, size, off)


def cmd_syms(path, outpath):
    data = read_file(path)
    m = MachO(data, 0)
    syms = m.symbols()
    # indice de secciones 1-based tal y como lo usa n_sect
    secnames = ["<abs>"] + ["%s,%s" % (s.segname, s.sectname) for s in m.sections]
    lines = ["# tabla de simbolos de %s  (slice %s)" % (path, m.arch()),
             "# %d simbolos" % len(syms),
             "# addr      tipo  ext seccion                 nombre"]
    for name, n_type, n_sect, n_desc, n_value in syms:
        if n_type & N_STAB:
            kind = "STAB:0x%02x" % (n_type & N_STAB)
        else:
            kind = TYPE_NAMES.get(n_type & N_TYPE, "?")
        ext = "E" if (n_type & N_EXT) else "-"
        sec = secnames[n_sect] if n_sect < len(secnames) else "<%d>" % n_sect
        lines.append("0x%08x  %-9s %s  %-22s %s" % (n_value, kind, ext, sec, name))
    with open(outpath, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    return "escrito %s (%d simbolos)" % (outpath, len(syms))


def cmd_funcs(path, outpath):
    data = read_file(path)
    m = MachO(data, 0)
    text = m.section_by_name("__TEXT", "__text")
    if not text:
        raise SystemExit("no hay seccion __TEXT,__text")

    syms = m.symbols()
    # simbolos de funcion: N_SECT, no STAB, direccion dentro de __text
    funcs = {}
    for name, n_type, n_sect, n_desc, n_value in syms:
        if n_type & N_STAB:
            continue
        if (n_type & N_TYPE) != N_SECT:
            continue
        addr = n_value & ~1  # bit 0 = marca thumb
        if not text.contains_addr(addr):
            continue
        # conserva el nombre mas informativo si hay alias en la misma direccion
        prev = funcs.get(addr)
        if prev is None or (len(name) > len(prev[0])):
            funcs[addr] = (name, bool(n_value & 1), bool(n_type & N_EXT))

    starts = set(a & ~1 for a in m.function_start_addrs())
    named = set(funcs)
    ordered = sorted(funcs.items())

    lines = ["# funciones en __TEXT,__text de %s (slice %s)" % (path, m.arch()),
             "# %d funciones con nombre; LC_FUNCTION_STARTS lista %d entradas"
             % (len(funcs), len(starts)),
             "# %d entradas de FUNCTION_STARTS sin simbolo asociado"
             % len(starts - named),
             "# addr      thumb ext  nombre"]
    for addr, (name, thumb, ext) in ordered:
        lines.append("0x%08x  %s     %s   %s" % (addr, "T" if thumb else "-",
                                                 "E" if ext else "-", name))
    with open(outpath, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    return "escrito %s (%d funciones con nombre, %d function starts)" % (
        outpath, len(funcs), len(starts))


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(__doc__)
        raise SystemExit(1)
    action = sys.argv[1]
    if action == "info":
        print(cmd_info(sys.argv[2]))
    elif action == "thin":
        print(cmd_thin(sys.argv[2], sys.argv[3], sys.argv[4]))
    elif action == "syms":
        print(cmd_syms(sys.argv[2], sys.argv[3]))
    elif action == "funcs":
        print(cmd_funcs(sys.argv[2], sys.argv[3]))
    else:
        raise SystemExit("accion desconocida: %s" % action)
