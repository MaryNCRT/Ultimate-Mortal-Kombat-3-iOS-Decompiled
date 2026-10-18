"""
armrecomp — recompilador estatico Thumb/ARM -> C para UMK3 iOS.

Modelo (igual que N64Recomp): traduccion literal, una instruccion ARM por
sentencia C, una funcion C por cada funcion original, estado de CPU explicito
en un `arm_ctx`. No intenta reconstruir estructuras de alto nivel: eso es
precisamente lo que lo hace verificable.

Principio de diseno: **fallar ruidosamente**. Cualquier instruccion que no se
sepa traducir genera una llamada a `arm_unimplemented()` que aborta en tiempo
de ejecucion, y ademas se reporta al generar. Nunca se emite codigo que pueda
ser silenciosamente incorrecto.

Uso:
  python recomp.py <slice_thin> --file Matrix.cpp   --out <dir>
  python recomp.py <slice_thin> --func _RotMatrixZ  --out <dir>
"""

import argparse
import os
import sys

# tools/armrecomp/recomp.py -> the other tools live one level up.
sys.path.insert(0, os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")))

import umk3paths  # noqa: E402

from macho import MachO, read_file, N_STAB, N_TYPE, N_SECT  # noqa: E402

from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM, CS_MODE_THUMB, CS_MODE_LITTLE_ENDIAN
from capstone.arm_const import (ARM_OP_REG, ARM_OP_IMM, ARM_OP_MEM, ARM_OP_FP,
                                ARM_CC_AL, ARM_CC_INVALID,
                                ARM_SFT_ASR, ARM_SFT_LSL, ARM_SFT_LSR,
                                ARM_SFT_ROR)

N_ARM_THUMB_DEF = 0x0008

# Alias de registro que usa capstone -> indice
INT_REG = {"sb": 9, "sl": 10, "fp": 11, "ip": 12, "sp": 13, "lr": 14, "pc": 15,
           "r13": 13, "r14": 14, "r15": 15}
for _i in range(13):
    INT_REG["r%d" % _i] = _i

# Shims disponibles en el runtime, por nombre del simbolo importado
STUBS = {
    # libm
    "_cosf": "stub_cosf", "_sinf": "stub_sinf", "_tanf": "stub_tanf",
    "_sqrtf": "stub_sqrtf", "_cos": "stub_cos", "_sin": "stub_sin",
    "_tan": "stub_tan",
    # libc
    "_memcpy": "stub_memcpy", "_memset": "stub_memset",
    "_strlen": "stub_strlen", "_strcpy": "stub_strcpy", "_strcmp": "stub_strcmp",
    "_strstr": "stub_strstr",
    "_sprintf": "stub_sprintf", "_printf": "stub_printf",
}

# Funciones INTERNAS del binario que se sustituyen por una implementacion del
# anfitrion en vez de recompilarse.
#
# limeMalloc / limeFree / limeLoadFile son la capa de asignacion y de archivos
# del motor: por dentro llaman al malloc de iOS y a las APIs de archivos del
# sistema, que no tiene sentido recompilar. Se redirigen al monton del invitado
# y al sistema de archivos del anfitrion (ver runtime/arm_runtime.c).
OVERRIDES = {
    "_limeMalloc": "stub_limeMalloc",
    "_limeFree": "stub_limeFree",
    "_limeLoadFile": "stub_limeLoadFile",
    "_limeFileSize": "stub_limeFileSize",
}

# Condiciones ARM -> expresion C sobre los flags
COND_EXPR = {
    "eq": "ctx->zf", "ne": "!ctx->zf",
    "cs": "ctx->cf", "hs": "ctx->cf", "cc": "!ctx->cf", "lo": "!ctx->cf",
    "mi": "ctx->nf", "pl": "!ctx->nf",
    "vs": "ctx->vf", "vc": "!ctx->vf",
    "hi": "(ctx->cf && !ctx->zf)", "ls": "(!ctx->cf || ctx->zf)",
    "ge": "(ctx->nf == ctx->vf)", "lt": "(ctx->nf != ctx->vf)",
    "gt": "(!ctx->zf && ctx->nf == ctx->vf)", "le": "(ctx->zf || ctx->nf != ctx->vf)",
}

# Inverse predicates used by Thumb IT blocks.  The architecture permits the
# aliases hs/cs and lo/cc; keep the spelling that COND_EXPR already accepts.
INVERSE_COND = {
    "eq": "ne", "ne": "eq", "cs": "cc", "hs": "lo", "cc": "cs", "lo": "hs",
    "mi": "pl", "pl": "mi", "vs": "vc", "vc": "vs", "hi": "ls", "ls": "hi",
    "ge": "lt", "lt": "ge", "gt": "le", "le": "gt",
}


class Unsupported(Exception):
    pass


# ---------------------------------------------------------------- carga

def load_binary(path):
    m = MachO(read_file(path), 0)
    text = m.section_by_name("__TEXT", "__text")
    funcs = {}   # addr -> (nombre, thumb)
    for name, n_type, n_sect, n_desc, n_value in m.symbols():
        if n_type & N_STAB or (n_type & N_TYPE) != N_SECT:
            continue
        addr = n_value & ~1
        if not text.contains_addr(addr):
            continue
        thumb = bool(n_value & 1) or bool(n_desc & N_ARM_THUMB_DEF)
        prev = funcs.get(addr)
        if prev is None or len(name) > len(prev[0]):
            funcs[addr] = (name, thumb)
    return m, text, funcs


def load_file_map():
    """Function -> source file map, produced by tools/stabs.py."""
    return umk3paths.load_func_to_file(required=False)


def func_size(addr, funcs, text):
    later = sorted(a for a in funcs if a > addr)
    return (later[0] if later else text.addr + text.size) - addr


# ---------------------------------------------------------------- emisor

class Emitter(object):
    def __init__(self, macho, text, funcs, stubs):
        self.m = macho
        self.text = text
        self.funcs = funcs
        self.stubs = stubs
        self.problems = []      # (funcion, addr, texto) no traducibles
        self.literals = 0       # cargas del pool de literales resueltas
        self.overridden = set()  # funciones internas sustituidas por shims
        self.auto_shims = {}     # shim autogenerado -> simbolo importado
        self.tables = {}         # addr de tbb/tbh -> (indice, [destinos])

    # -- utilidades --


    called_out = set()   # internal callees this run does not generate

    def cname(self, addr):
        name, _ = self.funcs[addr]
        return "func_%08x_%s" % (addr, name.lstrip("_"))

    def rname(self, ins, reg):
        return ins.reg_name(reg)

    def ireg(self, ins, reg):
        n = self.rname(ins, reg)
        if n not in INT_REG:
            raise Unsupported("registro entero desconocido: %s" % n)
        return INT_REG[n]

    def vreg(self, ins, reg):
        """Devuelve ('s', n) o ('d', n) para un registro VFP."""
        n = self.rname(ins, reg)
        if n.startswith("s") and n[1:].isdigit():
            return ("s", int(n[1:]))
        if n.startswith("d") and n[1:].isdigit():
            return ("d", int(n[1:]))
        raise Unsupported("registro VFP desconocido: %s" % n)

    def is_vfp(self, ins, reg):
        n = self.rname(ins, reg)
        return (n.startswith("s") or n.startswith("d")) and n[1:].isdigit()

    def rd(self, ins, op):
        """Expresion C que LEE un operando."""
        if op.type == ARM_OP_IMM:
            return "0x%xu" % (op.imm & 0xFFFFFFFF)
        if op.type == ARM_OP_REG:
            if self.is_vfp(ins, op.reg):
                kind, n = self.vreg(ins, op.reg)
                return "ctx->v.%s[%d]" % (kind, n)
            i = self.ireg(ins, op.reg)
            if i == 15:
                # pc leido como dato = direccion de la instruccion + 4 (Thumb)
                return "0x%xu" % ((ins.address + 4) & 0xFFFFFFFF)
            return "ctx->r[%d]" % i
        raise Unsupported("operando no legible tipo %d" % op.type)

    @staticmethod
    def adr_target(ins):
        """
        Direccion que calcula `adr rd, #imm` (o `addw/subw rd, pc, #imm`).

        Capstone da el desplazamiento, no la direccion: el valor es
        Align(direccion + 4, 4) + imm. Antes se emitia el desplazamiento tal
        cual (`adr r2, #4` -> r2 = 4), y la tabla de saltos de
        do_next_a9_frame_pxob saltaba siempre al caso 0.
        """
        imm = ins.operands[-1].imm
        if ins.mnemonic.split(".")[0] == "subw":
            imm = -imm
        if not (-4096 < imm < 4096):        # ya es una direccion absoluta
            return imm & 0xFFFFFFFF
        return (((ins.address + 4) & ~3) + imm) & 0xFFFFFFFF

    def literal_addr(self, ins, op):
        """Si el operando es [pc, #N], devuelve la direccion del literal."""
        if op.type == ARM_OP_MEM and op.mem.base and self.rname(ins, op.mem.base) == "pc":
            return (((ins.address + 4) & ~3) + op.mem.disp) & 0xFFFFFFFF
        return None

    def read_const(self, addr, size=4):
        """Lee una constante del binario en tiempo de recompilacion."""
        for sec in self.m.sections:
            if sec.contains_addr(addr) and sec.offset:
                off = sec.offset + (addr - sec.addr)
                raw = self.m.data[off:off + size]
                if len(raw) == size:
                    return int.from_bytes(raw, "little")
        return None

    def mem_addr(self, ins, op):
        """Expresion C de la direccion efectiva de un operando de memoria."""
        mem = op.mem
        parts = []
        if mem.base:
            bn = self.rname(ins, mem.base)
            if bn == "pc":
                # Pool de literales: son constantes inmutables incrustadas en
                # __TEXT. Se resuelven aqui en vez de exigir que la memoria del
                # invitado tenga mapeado el segmento de codigo.
                return "0x%xu" % ((((ins.address + 4) & ~3) + mem.disp) & 0xFFFFFFFF)
            parts.append("ctx->r[%d]" % INT_REG[bn])
        if mem.index:
            idx = "ctx->r[%d]" % self.ireg(ins, mem.index)
            if mem.scale == -1:
                idx = "(uint32_t)(-(int32_t)%s)" % idx
            if op.shift.type and op.shift.value:
                idx = "(%s << %d)" % (idx, op.shift.value)
            parts.append(idx)
        if mem.disp:
            parts.append("%d" % mem.disp)
        if not parts:
            return "0u"
        return "(" + " + ".join(parts) + ")"

    # -- traduccion de una instruccion --

    def translate(self, ins, fname, out, it_cond=None):
        base = ins.mnemonic.split(".")[0]
        ops = ins.operands
        conditional = ins.cc not in (ARM_CC_AL, ARM_CC_INVALID, 0)

        # Dentro de un bloque IT, capstone incorpora la condicion al mnemonico
        # ("movne", "strlt", "mvnne"...). Hay que quitarla ANTES de despachar o
        # no reconoceriamos la operacion base. Solo se hace cuando capstone
        # confirma que la instruccion es condicional: asi "movs" (mov + flags)
        # no se confunde con "mov" + condicion "vs", ni "lsls" con "lsl" + "ls".
        # Capstone 5 does not propagate the predicate represented by a Thumb
        # IT instruction into `ins.cc`; it emits plain `vdiv`, `str`, etc.
        # `emit_function` carries that predicate explicitly as it_cond.
        # Older Capstone versions do propagate it, so retain suffix stripping
        # and require both sources to agree when both are present.
        cond_name = it_cond
        if conditional and not base.startswith("b"):
            for c in sorted(COND_EXPR, key=len, reverse=True):
                if base.endswith(c) and len(base) > len(c):
                    if cond_name is not None and cond_name != c:
                        raise Unsupported("IT %s contradice sufijo %s" % (cond_name, c))
                    cond_name = c
                    base = base[:-len(c)]
                    break

        # inside an IT block only cmp/cmn/tst/teq set flags (see below)
        self._flags_off = it_cond is not None and base not in ("cmp", "cmn", "tst", "teq")
        body = self.translate_body(ins, base, ops, fname)

        # **Instructions inside an IT block do not update the flags.**
        #
        # In Thumb a 16-bit data-processing instruction normally sets them --
        # `movs r3, #0` -- but inside an IT block the S bit is clear and the
        # disassembly reads `movne r3, #0`. Capstone still reports the
        # mnemonic as `movs`, and trusting it emitted a SET_NZ that clobbered
        # the very flags the NEXT predicated instruction was about to test.
        #
        # The effect is that BOTH halves of an ITE run. It cost a real
        # misreading: LIME_RenderMeshSingle appeared to take its vertex-colour
        # path when the mesh was flagged full-bright, which is backwards, and
        # the clean C was very nearly "corrected" to match a broken oracle.
        #
        # cmp/cmn/tst/teq are exempt -- they have no destination and setting
        # flags is their whole purpose, which is why they are legal here.
        if it_cond is not None and base not in ("cmp", "cmn", "tst", "teq"):
            body = [ln for ln in body if not ln.startswith("SET_NZ(")]

        if cond_name:
            body = ["if (%s) { %s }" % (COND_EXPR[cond_name], " ".join(body))]

        for line in body:
            out.append("    " + line)

    @staticmethod
    def it_predicates(ins):
        """Return the predicates for the instructions following a Thumb IT.

        `itt ne` means [ne, ne]; `ite eq` means [eq, ne].  The first slot is
        implicit in the condition operand and each trailing T/E selects the
        same or inverse predicate.  This must be tracked by the emitter: it
        is architectural control flow, not a no-op annotation.
        """
        mn = ins.mnemonic.split(".")[0].lower()
        if not (mn == "it" or (mn.startswith("it") and set(mn[2:]) <= set("te"))):
            return None
        cond = ins.op_str.strip().split(",")[0].lower()
        if cond not in COND_EXPR:
            raise Unsupported("condicion IT desconocida: %s" % cond)
        return [cond] + [cond if ch == "t" else INVERSE_COND[cond]
                         for ch in mn[2:]]

    def translate_body(self, ins, base, ops, fname):
        m = base
        R = lambda o: self.rd(ins, o)  # noqa: E731

        # ---------- nada que hacer ----------
        # "it"/"itt"/"ite"/"itte"... solo marcan el bloque condicional; capstone
        # ya propaga la condicion a las instrucciones que le siguen, asi que la
        # instruccion IT en si no genera codigo.
        if m in ("nop", "hint", "dmb", "dsb", "isb", "pld", "clrex", "yield"):
            return ["/* %s */" % ins.mnemonic]
        if m.startswith("it") and set(m[2:]) <= set("te"):
            return ["/* %s (marca de bloque IT) */" % ins.mnemonic]

        # ---------- extensiones de signo/cero ----------
        EXT = {"uxtb": "(uint8_t)", "uxth": "(uint16_t)",
               "sxtb": "(uint32_t)(int32_t)(int8_t)",
               "sxth": "(uint32_t)(int32_t)(int16_t)"}
        if m in EXT:
            d = self.ireg(ins, ops[0].reg)
            return ["ctx->r[%d] = %s(%s);" % (d, EXT[m], R(ops[1]))]

        # ---------- adr: direccion relativa al PC, conocida al recompilar ----------
        if m == "adr":
            d = self.ireg(ins, ops[0].reg)
            return ["ctx->r[%d] = 0x%08xu;" % (d, self.adr_target(ins))]

        # ---------- multiplicacion larga ----------
        if m in ("smull", "umull"):
            lo = self.ireg(ins, ops[0].reg)
            hi = self.ireg(ins, ops[1].reg)
            a, b = R(ops[2]), R(ops[3])
            if m == "smull":
                expr = "(int64_t)(int32_t)(%s) * (int64_t)(int32_t)(%s)" % (a, b)
                ty = "int64_t"
            else:
                expr = "(uint64_t)(%s) * (uint64_t)(%s)" % (a, b)
                ty = "uint64_t"
            return ["{ %s _p = %s;" % (ty, expr),
                    "  ctx->r[%d] = (uint32_t)_p; ctx->r[%d] = (uint32_t)((uint64_t)_p >> 32); }"
                    % (lo, hi)]

        # ---------- movimientos enteros ----------
        # `mov pc, rN` de una tabla de saltos resuelta (ver _branch_table)
        if m == "mov" and ins.address in self.tables:
            reg, targets = self.tables[ins.address]
            lines = ["switch (ctx->r[%d]) {" % reg]
            for i, t in enumerate(targets):
                lines.append("    case %d: goto L_%08x;" % (i, t))
            lines.append('    default: arm_unimplemented("%s", 0x%08xu, "indice fuera de tabla"); break;'
                         % (fname, ins.address))
            lines.append("}")
            return lines

        if m in ("mov", "movs", "movw", "mvn", "mvns"):
            if self.is_vfp(ins, ops[0].reg):
                return self.vfp_mov(ins, ops)
            d = self.ireg(ins, ops[0].reg)
            src = R(ops[1])
            if m.startswith("mvn"):
                src = "~(%s)" % src
            lines = ["ctx->r[%d] = %s;" % (d, src)]
            if self.uf(ins):
                lines.append("SET_NZ(ctx->r[%d]);" % d)
            return lines

        if m == "movt":
            d = self.ireg(ins, ops[0].reg)
            return ["ctx->r[%d] = (ctx->r[%d] & 0xFFFFu) | ((%s) << 16);"
                    % (d, d, R(ops[1]))]

        # ---------- aritmetica ----------
        ARITH = {"add": "+", "adds": "+", "sub": "-", "subs": "-",
                 "and": "&", "ands": "&", "orr": "|", "orrs": "|",
                 "eor": "^", "eors": "^", "bic": "&~", "bics": "&~"}
        if m in ARITH:
            d = self.ireg(ins, ops[0].reg)
            if len(ops) == 2:
                a, b = "ctx->r[%d]" % d, R(ops[1])
            else:
                a, b = R(ops[1]), self.shifted(ins, ops[2])
            op = ARITH[m]
            if op == "&~":
                expr = "%s & ~(%s)" % (a, b)
            else:
                expr = "%s %s %s" % (a, op, b)
            lines = ["ctx->r[%d] = %s;" % (d, expr)]
            if self.uf(ins):
                if m.startswith("add"):
                    lines = ["{ uint32_t _a = %s, _b = %s, _r = _a + _b;" % (a, b),
                             "  ctx->r[%d] = _r; set_flags_add(ctx, _a, _b, _r); }" % d]
                elif m.startswith("sub"):
                    lines = ["{ uint32_t _a = %s, _b = %s, _r = _a - _b;" % (a, b),
                             "  ctx->r[%d] = _r; set_flags_sub(ctx, _a, _b, _r); }" % d]
                else:
                    lines.append("SET_NZ(ctx->r[%d]);" % d)
                    # a logical op takes C from the shifter: `ands r3, r3,
                    # r2, asr #32` + `it lo` is how the compiler rounds a
                    # signed /16 (t_kiss_orb). Computed before the write,
                    # since d may be the shifted register.
                    c = self.shifter_carry(ins, ops[2]) if len(ops) > 2 else None
                    if c:
                        lines.insert(0, "ctx->cf = %s;" % c)
            return lines

        if m in ("rsb", "rsbs"):
            d = self.ireg(ins, ops[0].reg)
            a, b = R(ops[1]), self.shifted(ins, ops[2])
            if self.uf(ins):
                # `rsbs r3, r3, #1` + `it lo` is how q_six_button tests for
                # zero: the flags are those of b - a
                return ["{ uint32_t _a = %s, _b = %s, _r = _a - _b;" % (b, a),
                        "  ctx->r[%d] = _r; set_flags_sub(ctx, _a, _b, _r); }" % d]
            return ["ctx->r[%d] = (%s) - (%s);" % (d, b, a)]

        if m in ("mul", "muls"):
            d = self.ireg(ins, ops[0].reg)
            a = R(ops[1])
            b = R(ops[2]) if len(ops) > 2 else "ctx->r[%d]" % d
            return ["ctx->r[%d] = (uint32_t)((%s) * (%s));" % (d, a, b)]

        if m in ("lsl", "lsls", "lsr", "lsrs", "asr", "asrs"):
            d = self.ireg(ins, ops[0].reg)
            if len(ops) > 2:
                a, b = R(ops[1]), R(ops[2])
            else:
                # two-operand form `lsls rd, rm`: rd = rd << rm (the shifted
                # value is the destination, the amount is the operand)
                a, b = "ctx->r[%d]" % d, R(ops[1])
            if m.startswith("lsl"):
                expr = "(%s) << ((%s) & 31)" % (a, b)
            elif m.startswith("lsr"):
                expr = "(%s) >> ((%s) & 31)" % (a, b)
            else:
                expr = "(uint32_t)((int32_t)(%s) >> ((%s) & 31))" % (a, b)
            lines = ["ctx->r[%d] = %s;" % (d, expr)]
            if self.uf(ins):
                lines.append("SET_NZ(ctx->r[%d]);" % d)
                # C is the last bit shifted out (unchanged for a zero amount);
                # computed before the write, since d may be the source
                if m.startswith("lsl"):
                    c = "_n <= 32 ? (_v >> (32 - _n)) & 1u : 0u"
                elif m.startswith("lsr"):
                    c = "_n <= 32 ? (_v >> (_n - 1)) & 1u : 0u"
                else:
                    c = "_n >= 32 ? _v >> 31 : (_v >> (_n - 1)) & 1u"
                lines.insert(0, "{ uint32_t _v = %s, _n = (%s) & 0xffu;"
                                " if (_n) ctx->cf = %s; }" % (a, b, c))
            return lines

        if m in ("cmp", "cmn"):
            a, b = R(ops[0]), self.shifted(ins, ops[1])
            if m == "cmp":
                return ["{ uint32_t _a = %s, _b = %s; set_flags_sub(ctx, _a, _b, _a - _b); }"
                        % (a, b)]
            return ["{ uint32_t _a = %s, _b = %s; set_flags_add(ctx, _a, _b, _a + _b); }"
                    % (a, b)]

        if m in ("tst", "teq"):
            a, b = R(ops[0]), self.shifted(ins, ops[1])
            op = "&" if m == "tst" else "^"
            lines = ["SET_NZ((%s) %s (%s));" % (a, op, b)]
            c = self.shifter_carry(ins, ops[1])
            if c:
                lines.insert(0, "ctx->cf = %s;" % c)
            return lines

        # ---------- memoria ----------
        if m in ("ldr", "ldrb", "ldrh", "ldrsb", "ldrsh",
                 "str", "strb", "strh"):
            return self.mem_access(ins, m, ops)

        if m in ("ldrd", "strd"):
            addr = self.mem_addr(ins, ops[2])
            a = self.ireg(ins, ops[0].reg)
            b = self.ireg(ins, ops[1].reg)
            if m == "ldrd":
                return ["ctx->r[%d] = MEM_LD32(%s);" % (a, addr),
                        "ctx->r[%d] = MEM_LD32(%s + 4);" % (b, addr)]
            return ["MEM_ST32(%s, ctx->r[%d]);" % (addr, a),
                    "MEM_ST32(%s + 4, ctx->r[%d]);" % (addr, b)]

        if m in ("push", "pop", "vpush", "vpop"):
            return self.stack_op(ins, m, ops, fname)

        if m in ("ldm", "stm", "ldmia", "stmia", "ldmdb", "stmdb"):
            return self.ldm_stm(ins, m, ops)

        # ---------- saltos y llamadas ----------
        if m.startswith("b") and m not in ("bic", "bics", "bfi", "bfc"):
            return self.branch(ins, m, ops, fname)

        if m in ("cbz", "cbnz"):
            reg = R(ops[0])
            target = ops[1].imm
            test = "== 0" if m == "cbz" else "!= 0"
            return ["if ((%s) %s) goto L_%08x;" % (reg, test, target)]

        # ---------- tabla de saltos ----------
        # `tbb`/`tbh` saltan a traves de una tabla que ya se resolvio durante el
        # descenso recursivo, asi que aqui se emite como un switch explicito.
        # Sin la tabla resuelta no se puede traducir y se dice.
        if m in ("tbb", "tbh"):
            tbl = self.tables.get(ins.address)
            if not tbl:
                raise Unsupported("%s con tabla sin resolver" % m)
            reg, targets = tbl
            lines = ["switch (ctx->r[%d]) {" % reg]
            for i, t in enumerate(targets):
                lines.append("    case %d: goto L_%08x;" % (i, t))
            lines.append('    default: arm_unimplemented("%s", 0x%08xu, "indice fuera de tabla"); break;'
                         % (fname, ins.address))
            lines.append("}")
            return lines

        # ---------- accesos exclusivos ----------
        # El invitado es de un solo hilo y no hay contencion, asi que ldrex se
        # comporta como un load normal y strex siempre concede (devuelve 0).
        # Fiel para este uso; no lo seria en un recompilador multihilo.
        if m in ("ldrex", "ldrexb", "ldrexh"):
            w = {"ldrex": 32, "ldrexb": 8, "ldrexh": 16}[m]
            return ["ctx->r[%d] = mem_read%d(%s);"
                    % (self.ireg(ins, ops[0].reg), w, self.mem_addr(ins, ops[1]))]
        if m in ("strex", "strexb", "strexh"):
            w = {"strex": 32, "strexb": 8, "strexh": 16}[m]
            return ["mem_write%d(%s, %s);"
                    % (w, self.mem_addr(ins, ops[2]), R(ops[1])),
                    "ctx->r[%d] = 0u;   /* exclusivo concedido */"
                    % self.ireg(ins, ops[0].reg)]

        # ---------- multiplicacion con acumulacion ----------
        if m in ("mla", "mls") and len(ops) == 4:
            d = self.ireg(ins, ops[0].reg)
            op = "+" if m == "mla" else "-"
            return ["ctx->r[%d] = (uint32_t)(%s %s (uint32_t)(%s * %s));"
                    % (d, R(ops[3]), op, R(ops[1]), R(ops[2]))]
        if m == "smulbb" and len(ops) == 3:
            return ["ctx->r[%d] = (uint32_t)((int32_t)(int16_t)%s * (int32_t)(int16_t)%s);"
                    % (self.ireg(ins, ops[0].reg), R(ops[1]), R(ops[2]))]

        # ---------- addw/subw: inmediato de 12 bits, no altera flags ----------
        if m in ("addw", "subw") and len(ops) == 3:
            if ops[1].type == ARM_OP_REG and self.ireg(ins, ops[1].reg) == 15:
                # la forma `adr` de 32 bits: pc ALINEADO a 4, como `adr`
                return ["ctx->r[%d] = 0x%08xu;"
                        % (self.ireg(ins, ops[0].reg), self.adr_target(ins))]
            op = "+" if m == "addw" else "-"
            return ["ctx->r[%d] = (uint32_t)(%s %s %s);"
                    % (self.ireg(ins, ops[0].reg), R(ops[1]), op, R(ops[2]))]

        # ---------- extension con acumulacion / byte-reverse ----------
        if m == "sxtah" and len(ops) == 3:
            return ["ctx->r[%d] = (uint32_t)(%s + (int32_t)(int16_t)(uint16_t)%s);"
                    % (self.ireg(ins, ops[0].reg), R(ops[1]), R(ops[2]))]
        if m == "rev" and len(ops) == 2:
            return ["ctx->r[%d] = arm_rev32(%s);"
                    % (self.ireg(ins, ops[0].reg), R(ops[1]))]

        # ---------- VFP ----------
        if m.startswith("v"):
            return self.vfp(ins, m, ops, fname)

        raise Unsupported(ins.mnemonic + " " + ins.op_str)

    def uf(self, ins):
        """Does this instruction set the flags? Capstone says `rsbs`/`adds` for
        a 16-bit encoding inside an IT block, where the S bit is clear."""
        return ins.update_flags and not getattr(self, "_flags_off", False)

    def shifter_carry(self, ins, op):
        """C expression for the shifter's carry-out, or None if C is unchanged
        (no shift: a plain register leaves the carry alone)."""
        if op.type != ARM_OP_REG or not op.shift.type or not op.shift.value:
            return None
        val, st, n = self.rd(ins, op), op.shift.type, op.shift.value
        if st == ARM_SFT_LSL:
            return "((%s) >> %d) & 1u" % (val, 32 - n) if 0 < n < 32 else None
        if st in (ARM_SFT_LSR, ARM_SFT_ASR):
            if n >= 32:
                return "((%s) >> 31) & 1u" % val
            return "((%s) >> %d) & 1u" % (val, n - 1)
        if st == ARM_SFT_ROR:
            return "((%s) >> %d) & 1u" % (val, (n - 1) & 31)
        return None

    def shifted(self, ins, op):
        """Operando que puede llevar un desplazamiento incorporado."""
        val = self.rd(ins, op)
        if op.type == ARM_OP_REG and op.shift.type and op.shift.value:
            st = op.shift.type
            # Capstone's order is ASR, LSL, LSR, ROR, RRX -- NOT the intuitive
            # LSL, LSR, ASR. Getting this wrong turns "lsl #16" into ">> 16",
            # which compiles, runs, and is silently wrong.
            n = op.shift.value
            # ARM encodes "shift by 32" for LSR and ASR as shift_imm == 0, and
            # capstone reports it back as 32.  Emitting ">> 32" on a 32-bit type
            # is UNDEFINED BEHAVIOUR in C: gcc masks the count to 5 bits, so it
            # compiles to ">> 0" and leaves the value untouched -- the opposite
            # of what the instruction does.  The first differential run of
            # RenderMesh.cpp caught this as three -Wshift-count-overflow
            # warnings inside limeLoadTexture.
            if st == ARM_SFT_LSL:
                return "((%s) << %d)" % (val, n)
            if st == ARM_SFT_LSR:
                # every bit is shifted out
                if n >= 32:
                    return "((uint32_t)0)"
                return "((%s) >> %d)" % (val, n)
            if st == ARM_SFT_ASR:
                # the result is the sign bit replicated, which >> 31 already is
                if n >= 32:
                    n = 31
                return "((uint32_t)((int32_t)(%s) >> %d))" % (val, n)
            if st == ARM_SFT_ROR:
                n = op.shift.value & 31
                return "(((%s) >> %d) | ((%s) << %d))" % (val, n, val, (32 - n) & 31)
            raise Unsupported("tipo de shift %d" % st)
        return val

    def mem_access(self, ins, m, ops):
        # Carga desde el pool de literales: se pliega a un inmediato.
        lit = self.literal_addr(ins, ops[1])
        if lit is not None and m == "ldr":
            val = self.read_const(lit, 4)
            if val is None:
                raise Unsupported("literal en 0x%08x fuera de las secciones" % lit)
            self.literals += 1
            return ["ctx->r[%d] = 0x%08xu;   /* literal @0x%08x */"
                    % (self.ireg(ins, ops[0].reg), val, lit)]

        addr = self.mem_addr(ins, ops[1])
        lines = []
        if m.startswith("ldr"):
            d = self.ireg(ins, ops[0].reg)
            if m == "ldr":
                lines.append("ctx->r[%d] = MEM_LD32(%s);" % (d, addr))
            elif m == "ldrb":
                lines.append("ctx->r[%d] = MEM_LD8(%s);" % (d, addr))
            elif m == "ldrh":
                lines.append("ctx->r[%d] = MEM_LD16(%s);" % (d, addr))
            elif m == "ldrsb":
                lines.append("ctx->r[%d] = (uint32_t)(int32_t)(int8_t)MEM_LD8(%s);" % (d, addr))
            elif m == "ldrsh":
                lines.append("ctx->r[%d] = (uint32_t)(int32_t)(int16_t)MEM_LD16(%s);" % (d, addr))
        else:
            s = "ctx->r[%d]" % self.ireg(ins, ops[0].reg)
            if m == "str":
                lines.append("MEM_ST32(%s, %s);" % (addr, s))
            elif m == "strb":
                lines.append("MEM_ST8(%s, (uint8_t)%s);" % (addr, s))
            elif m == "strh":
                lines.append("MEM_ST16(%s, (uint16_t)%s);" % (addr, s))

        # post/pre-indexado con escritura de vuelta al registro base
        if ins.writeback and ops[1].mem.base:
            b = INT_REG[self.rname(ins, ops[1].mem.base)]
            if ins.post_index:
                # "ldr r3, [r1], #4": se accede a [r1] y DESPUES r1 += 4.
                # Cuidado: capstone no pone ese 4 en mem.disp (que vale 0),
                # sino en un TERCER operando. Leerlo de mem.disp incrementa por
                # cero y corrompe el puntero sin dar ningun error.
                inc = 0
                if len(ops) > 2 and ops[2].type == ARM_OP_IMM:
                    inc = ops[2].imm
                elif ops[1].mem.disp:
                    inc = ops[1].mem.disp
                lines.append("ctx->r[%d] += %d;   /* post-indexado */" % (b, inc))
            else:
                lines.append("ctx->r[%d] = %s;" % (b, addr))
        return lines

    def stack_op(self, ins, m, ops, fname):
        lines = []
        if m == "push":
            for op in reversed(ops):
                lines.append("PUSH_REG(%d);" % self.ireg(ins, op.reg))
        elif m == "pop":
            ret = False
            for op in ops:
                if self.rname(ins, op.reg) == "pc":
                    lines.append("ctx->r[SP] += 4;   /* pop pc = retorno */")
                    ret = True
                else:
                    lines.append("POP_REG(%d);" % self.ireg(ins, op.reg))
            if ret:
                lines.append("return;")
        elif m == "vpush":
            for op in reversed(ops):
                kind, n = self.vreg(ins, op.reg)
                if kind == "d":
                    lines.append("ctx->r[SP] -= 8; MEM_ST64(ctx->r[SP], ctx->v.d[%d]);" % n)
                else:
                    lines.append("ctx->r[SP] -= 4; MEM_ST32(ctx->r[SP], ctx->v.s[%d]);" % n)
        elif m == "vpop":
            for op in ops:
                kind, n = self.vreg(ins, op.reg)
                if kind == "d":
                    lines.append("ctx->v.d[%d] = MEM_LD64(ctx->r[SP]); ctx->r[SP] += 8;" % n)
                else:
                    lines.append("ctx->v.s[%d] = MEM_LD32(ctx->r[SP]); ctx->r[SP] += 4;" % n)
        return lines

    def ldm_stm(self, ins, m, ops):
        base = self.ireg(ins, ops[0].reg)
        regs = [self.ireg(ins, o.reg) for o in ops[1:]]
        load = m.startswith("ldm")
        decr = m.endswith("db")
        lines = ["{ uint32_t _a = ctx->r[%d];" % base]
        if decr:
            lines.append("  _a -= %d;" % (4 * len(regs)))
        for i, r in enumerate(regs):
            if load:
                lines.append("  ctx->r[%d] = MEM_LD32(_a + %d);" % (r, 4 * i))
            else:
                lines.append("  MEM_ST32(_a + %d, ctx->r[%d]);" % (4 * i, r))
        if ins.writeback:
            if decr:
                lines.append("  ctx->r[%d] = _a;" % base)
            else:
                lines.append("  ctx->r[%d] = _a + %d;" % (base, 4 * len(regs)))
        lines.append("}")
        if load and 15 in regs:
            lines.append("return;   /* ldm con pc = retorno */")
        return lines

    def branch(self, ins, m, ops, fname):
        # bx lr / bx reg
        if m == "bx":
            n = self.rname(ins, ops[0].reg)
            if n == "lr":
                return ["return;"]
            return ["arm_unimplemented(\"%s\", 0x%08x, \"bx indirecto\");" % (fname, ins.address)]

        if m == "blx" and ops[0].type == ARM_OP_REG:
            return ["arm_unimplemented(\"%s\", 0x%08x, \"blx indirecto\");"
                    % (fname, ins.address)]

        target = ops[0].imm

        if m in ("bl", "blx"):
            stub = self.stubs.get(target)
            if stub:
                shim = STUBS.get(stub)
                if not shim:
                    # Importada sin shim escrito a mano. Antes esto abortaba la
                    # GENERACION, que es lo contrario del principio de diseno:
                    # una importada que nunca se ejecuta no deberia impedir
                    # recompilar la funcion que la menciona.
                    #
                    # Ahora se emite una llamada a un shim autogenerado cuya
                    # implementacion por defecto llama a arm_unimplemented(). Si
                    # el codigo llega a ejecutarla, aborta y dice cual es; si no
                    # la ejecuta, no molesta. Fallar ruidosamente en EJECUCION,
                    # no en generacion.
                    shim = "stub_auto_" + "".join(
                        c if c.isalnum() else "_" for c in stub.lstrip("_"))
                    self.auto_shims[shim] = stub
                return ["ctx->r[LR] = 0x%08xu;" % (ins.address + ins.size),
                        "%s(ctx);" % shim]
            if target in self.funcs:
                # funcion interna sustituida por una implementacion del anfitrion
                tname = self.funcs[target][0]
                if tname in OVERRIDES:
                    self.overridden.add(tname)
                    return ["ctx->r[LR] = 0x%08xu;" % (ins.address + ins.size),
                            "%s(ctx);   /* override de %s */"
                            % (OVERRIDES[tname], tname)]
                # Recorded so the header can declare it. Without --with-deps
                # the callee is not generated, and the emitted C then calls an
                # undeclared function -- which C99 rejects and which, worse,
                # would give it an implicitly wrong signature. A caller can be
                # verified long before its callees exist; the header just has
                # to say they are somebody else's.
                self.called_out.add(self.cname(target))
                return ["ctx->r[LR] = 0x%08xu;" % (ins.address + ins.size),
                        "%s(ctx);" % self.cname(target)]
            raise Unsupported("llamada a destino desconocido 0x%08x" % target)

        # salto incondicional o condicional dentro de la funcion
        cond = None
        for c in COND_EXPR:
            if m == "b" + c:
                cond = c
                break
        if m == "b" or m == "b.w":
            return ["goto L_%08x;" % target]
        if cond:
            return ["if (%s) goto L_%08x;" % (COND_EXPR[cond], target)]
        raise Unsupported("salto no soportado: " + ins.mnemonic)

    # -- VFP --

    def vfp_mov(self, ins, ops):
        """vmov entre banco entero y VFP, en sus multiples formas."""
        a, b = ops[0], ops[1]
        # vmov <Sd>, <Rn>
        if self.is_vfp(ins, a.reg) and not self.is_vfp(ins, b.reg):
            kind, n = self.vreg(ins, a.reg)
            if len(ops) == 3:   # vmov <Dd>, <Rlo>, <Rhi>
                lo = self.ireg(ins, ops[1].reg)
                hi = self.ireg(ins, ops[2].reg)
                return ["ctx->v.d[%d] = ((uint64_t)ctx->r[%d] << 32) | ctx->r[%d];"
                        % (n, hi, lo)]
            return ["ctx->v.%s[%d] = ctx->r[%d];" % (kind, n, self.ireg(ins, b.reg))]
        # vmov <Rd>, <Sn>   /  vmov <Rlo>, <Rhi>, <Dm>
        if not self.is_vfp(ins, a.reg):
            if len(ops) == 3 and self.is_vfp(ins, ops[2].reg):
                lo = self.ireg(ins, ops[0].reg)
                hi = self.ireg(ins, ops[1].reg)
                _k, n = self.vreg(ins, ops[2].reg)
                return ["ctx->r[%d] = (uint32_t)(ctx->v.d[%d] & 0xFFFFFFFFu);" % (lo, n),
                        "ctx->r[%d] = (uint32_t)(ctx->v.d[%d] >> 32);" % (hi, n)]
            kind, n = self.vreg(ins, b.reg)
            return ["ctx->r[%d] = ctx->v.%s[%d];" % (self.ireg(ins, a.reg), kind, n)]
        # vmov <Sd>, <Sm>
        ka, na = self.vreg(ins, a.reg)
        kb, nb = self.vreg(ins, b.reg)
        return ["ctx->v.%s[%d] = ctx->v.%s[%d];" % (ka, na, kb, nb)]

    def vfp(self, ins, m, ops, fname):
        mn = ins.mnemonic
        is64 = mn.endswith(".f64")
        base = m

        # vmla / vmls / vnmls / vnmul: multiplicacion con acumulacion.
        # Solo la forma ESCALAR (registros S). Con registros D es NEON
        # empaquetado de 2 carriles y hay que tratarlo carril a carril, asi que
        # se deja fallar en vez de emitir algo que parezca correcto.
        if base in ("vmla", "vmls", "vnmls", "vnmul") and len(ops) == 3:
            ka, na = self.vreg(ins, ops[0].reg)
            kb, nb = self.vreg(ins, ops[1].reg)
            kc, nc = self.vreg(ins, ops[2].reg)
            if ka != "s" or kb != "s" or kc != "s":
                raise Unsupported("%s empaquetado (registros D)" % base)
            d = "ctx->v.s[%d]" % na
            prod = "F32MUL(ctx->v.s[%d], ctx->v.s[%d])" % (nb, nc)
            if base == "vmla":
                return ["%s = F32ADD(%s, %s);" % (d, d, prod)]
            if base == "vmls":
                return ["%s = F32SUB(%s, %s);" % (d, d, prod)]
            if base == "vnmls":
                return ["%s = F32SUB(%s, F32NEG(%s));" % (d, prod, d)]
            return ["%s = F32NEG(%s);" % (d, prod)]

        if base == "vmov":
            # inmediato en coma flotante: vmov.f64 d6, #1.0
            if len(ops) == 2 and ops[1].type == ARM_OP_FP:
                _k, n = self.vreg(ins, ops[0].reg)
                if is64:
                    return ["VD_SET(%d, %r);" % (n, float(ops[1].fp))]
                return ["VS_SET(%d, %rf);" % (n, float(ops[1].fp))]
            return self.vfp_mov(ins, ops)

        if base in ("vldr", "vstr"):
            kind, n = self.vreg(ins, ops[0].reg)
            lit = self.literal_addr(ins, ops[1])
            if lit is not None and base == "vldr":
                val = self.read_const(lit, 8 if kind == "d" else 4)
                if val is None:
                    raise Unsupported("literal VFP en 0x%08x fuera de secciones" % lit)
                self.literals += 1
                return ["ctx->v.%s[%d] = 0x%0*xu%s;   /* literal @0x%08x */"
                        % (kind, n, 16 if kind == "d" else 8, val,
                           # "uLL", not "uULL": the format already appends the
                           # u, so adding "ULL" made an invalid suffix. It only
                           # fires on a double literal loaded PC-relative, which
                           # neither calibration module does -- so calibration
                           # stayed green while the emitter could not compile.
                           "LL" if kind == "d" else "", lit)]
            addr = self.mem_addr(ins, ops[1])
            if kind == "d":
                return (["ctx->v.d[%d] = MEM_LD64(%s);" % (n, addr)] if base == "vldr"
                        else ["MEM_ST64(%s, ctx->v.d[%d]);" % (addr, n)])
            return (["ctx->v.s[%d] = MEM_LD32(%s);" % (n, addr)] if base == "vldr"
                    else ["MEM_ST32(%s, ctx->v.s[%d]);" % (addr, n)])

        BIN = {"vadd": "+", "vsub": "-", "vmul": "*", "vdiv": "/"}
        if base in BIN:
            k0, n0 = self.vreg(ins, ops[0].reg)
            k1, n1 = self.vreg(ins, ops[1].reg)
            k2, n2 = self.vreg(ins, ops[2].reg)
            if k0 == k1 == k2 == "d" and not is64:
                # NEON de 2 carriles f32 sobre un registro D: se traduce por
                # carriles usando el solapamiento con los registros S.
                op = BIN[base]
                return ["VS_SET(%d, VS(%d) %s VS(%d));" % (2 * n0, 2 * n1, op, 2 * n2),
                        "VS_SET(%d, VS(%d) %s VS(%d));" % (2 * n0 + 1, 2 * n1 + 1, op, 2 * n2 + 1)]
            if is64:
                return ["VD_SET(%d, VD(%d) %s VD(%d));" % (n0, n1, BIN[base], n2)]
            return ["VS_SET(%d, VS(%d) %s VS(%d));" % (n0, n1, BIN[base], n2)]

        if base in ("vcmp", "vcmpe"):
            k0, n0 = self.vreg(ins, ops[0].reg)
            # "vcmp.f32 s14, #0" puede llegar como operando FP o como inmediato
            # entero, segun la version de capstone.
            if len(ops) < 2:
                rhs = "0.0"
            elif ops[1].type == ARM_OP_FP:
                rhs = "%r" % float(ops[1].fp)
            elif ops[1].type == ARM_OP_IMM:
                rhs = "%r" % float(ops[1].imm)
            else:
                k1, n1 = self.vreg(ins, ops[1].reg)
                rhs = "VD(%d)" % n1 if k1 == "d" else "VS(%d)" % n1
            lhs = "VD(%d)" % n0 if k0 == "d" else "VS(%d)" % n0
            return ["VFP_CMP(%s, %s);" % (lhs, rhs)]

        if base == "vmrs":
            # vmrs APSR_nzcv, FPSCR -- pasa los flags de la comparacion al APSR
            return ["VMRS_APSR();"]

        if base == "vneg":
            k0, n0 = self.vreg(ins, ops[0].reg)
            k1, n1 = self.vreg(ins, ops[1].reg)
            if is64:
                return ["VD_SET(%d, -VD(%d));" % (n0, n1)]
            return ["VS_SET(%d, -VS(%d));" % (n0, n1)]

        if base == "vabs":
            _k0, n0 = self.vreg(ins, ops[0].reg)
            _k1, n1 = self.vreg(ins, ops[1].reg)
            if is64:
                return ["VD_SET(%d, VD(%d) < 0 ? -VD(%d) : VD(%d));" % (n0, n1, n1, n1)]
            return ["VS_SET(%d, VS(%d) < 0.0f ? -VS(%d) : VS(%d));" % (n0, n1, n1, n1)]

        if base == "vsqrt":
            _k0, n0 = self.vreg(ins, ops[0].reg)
            _k1, n1 = self.vreg(ins, ops[1].reg)
            if is64:
                return ["VD_SET(%d, sqrt(VD(%d)));" % (n0, n1)]
            return ["VS_SET(%d, sqrtf(VS(%d)));" % (n0, n1)]

        if base == "vcvt":
            # el mnemonico lleva destino.origen: vcvt.f64.f32 d8, s14
            parts = mn.split(".")
            if len(parts) < 3:
                raise Unsupported(mn)
            dst_t, src_t = parts[1], parts[2]
            _k0, n0 = self.vreg(ins, ops[0].reg)
            _k1, n1 = self.vreg(ins, ops[1].reg)
            conv = {
                ("f64", "f32"): "VD_SET(%d, (double)VS(%d));",
                ("f32", "f64"): "VS_SET(%d, (float)VD(%d));",
                ("f32", "s32"): "VS_SET(%d, (float)(int32_t)ctx->v.s[%d]);",
                ("f32", "u32"): "VS_SET(%d, (float)ctx->v.s[%d]);",
                ("s32", "f32"): "ctx->v.s[%d] = (uint32_t)(int32_t)VS(%d);",
                ("u32", "f32"): "ctx->v.s[%d] = (uint32_t)VS(%d);",
                ("s32", "f64"): "ctx->v.s[%d] = (uint32_t)(int32_t)VD(%d);",
                ("f64", "s32"): "VD_SET(%d, (double)(int32_t)ctx->v.s[%d]);",
            }
            key = (dst_t, src_t)
            if key not in conv:
                raise Unsupported(mn)
            return [conv[key] % (n0, n1)]

        raise Unsupported(mn + " " + ins.op_str)

    @staticmethod
    def is_terminal(ins):
        """
        Does control flow definitely stop here?

        Used to tell the end of a function from the literal pool that follows
        it. Only unconditional forms count: a conditional return still falls
        through.
        """
        if ins is None:
            return False
        mn = ins.mnemonic
        base = mn.split(".")[0]
        if ins.cc not in (ARM_CC_AL, ARM_CC_INVALID, 0):
            return False
        if base == "bx" and ins.op_str.strip() == "lr":
            return True
        if base == "pop" and "pc" in ins.op_str:
            return True
        if base == "b":                      # unconditional branch
            return True
        return False

    # -- descenso recursivo --

    #: Mnemonicos que terminan un camino de ejecucion sin caer al siguiente.
    _RETURNS = ("bx", "pop", "b", "tbb", "tbh")

    @staticmethod
    def _is_cond(ins):
        return ins.cc not in (ARM_CC_AL, ARM_CC_INVALID, 0)

    def _successors(self, ins, lo, hi):
        """(fall_through, [destinos]) de una instruccion, acotado a [lo, hi)."""
        base = ins.mnemonic.split(".")[0]
        targets = []
        for op in ins.operands:
            if op.type == ARM_OP_IMM and lo <= op.imm < hi:
                if base.startswith("b") and base not in (
                        "bic", "bics", "bfi", "bfc", "bl", "blx"):
                    targets.append(op.imm)
                elif base in ("cbz", "cbnz"):
                    targets.append(op.imm)

        if base in ("bx", "pop") and not self._is_cond(ins):
            # bx lr / pop {..,pc} incondicional: no cae
            if base == "bx" and ins.op_str.strip() != "lr":
                return False, targets           # bx a registro: indirecto
            if base == "pop" and "pc" not in ins.op_str:
                return True, targets
            return False, targets
        if base == "b" and not self._is_cond(ins):
            return False, targets               # salto incondicional
        if base in ("tbb", "tbh"):
            return False, targets               # tabla indirecta: no seguible
        if base == "mov" and ins.op_str.replace(" ", "").startswith("pc,"):
            return False, targets               # mov pc: salto indirecto
        return True, targets

    def decode_reachable(self, addr, size, thumb):
        """
        Descenso recursivo desde el punto de entrada.

        El barrido lineal falla en este binario porque los pools de literales
        estan INTERCALADOS en el codigo: Thumb-2 solo alcanza +-4 KB con
        `ldr rX,[pc,#N]`, asi que el compilador deja constantes a mitad de
        funcion. Capstone las decodifica como instrucciones, se desincroniza,
        y el resto de la funcion sale a basura plausible.

        Siguiendo los saltos solo se decodifica lo que de verdad es alcanzable
        como codigo. Lo que no se alcanza es dato, no codigo -- y esa es la
        distincion que el barrido lineal no puede hacer.

        Devuelve (dict {direccion: insn}, bytes_no_alcanzados).
        """
        off = self.text.offset + (addr - self.text.addr)
        code = self.m.data[off:off + size]
        md = Cs(CS_ARCH_ARM,
                (CS_MODE_THUMB if thumb else CS_MODE_ARM) | CS_MODE_LITTLE_ENDIAN)
        md.detail = True

        found = {}
        table_bytes = {}        # inicio -> tamano de cada tabla de saltos
        work = [addr]
        lo, hi = addr, addr + size
        while work:
            pc = work.pop()
            if pc in found or not (lo <= pc < hi):
                continue
            chunk = code[pc - addr:pc - addr + 8]
            got = list(md.disasm(chunk, pc, count=1))
            if not got:
                continue                        # byte indecodificable: es dato
            ins = got[0]
            found[pc] = ins
            fall, targets = self._successors(ins, lo, hi)

            base = ins.mnemonic.split(".")[0]
            if base in ("tbb", "tbh"):
                tbl, tsize = self._jump_table(ins, found, code, addr, lo, hi)
                targets.extend(tbl)
                table_bytes[pc + ins.size] = tsize
                if tbl and ins.operands:
                    idx = ins.operands[0].mem.index
                    if idx:
                        self.tables[pc] = (self.ireg(ins, idx), tbl)

            if base == "mov" and ins.op_str.replace(" ", "").startswith("pc,"):
                tbl = self._branch_table(ins, found)
                if tbl:
                    self.tables[pc] = tbl
                    targets.extend(t for t in tbl[1] if lo <= t < hi)

            work.extend(targets)
            if fall:
                work.append(pc + ins.size)

        covered = sum(i.size for i in found.values())
        return found, size - covered

    def _branch_table(self, ins, found):
        """
        Resuelve el otro idioma de switch denso del binario, una tabla de
        instrucciones `b.w` (4 bytes cada una) en vez de desplazamientos:

            cmp   rM, #limite
            bhi   <default>
            adr   rN, tabla            (o `addw rN, pc, #imm`)
            add.w rN, rN, rM, lsl #2
            mov   pc, rN
          tabla:
            b.w   caso_0
            b.w   caso_1 ...

        Devuelve (indice del registro rM, [direccion de cada entrada]) o None.
        Cada entrada es una instruccion `b.w`, que el descenso recursivo sigue
        como cualquier otro codigo. Lo usan do_next_a9_frame_pxob (other),
        moves y playback.
        """
        dst = self.ireg(ins, ins.operands[1].reg) if len(ins.operands) > 1 else None
        prev = sorted((a for a in found if a < ins.address), reverse=True)[:6]
        base = idx = limit = None
        for a in prev:
            p = found[a]
            pm = p.mnemonic.split(".")[0]
            po = p.operands
            if idx is None and pm == "add" and len(po) == 3                     and po[2].type == ARM_OP_REG and po[2].shift.value == 2                     and self.ireg(p, po[0].reg) == dst:
                idx = self.ireg(p, po[2].reg)
            elif base is None and idx is not None and (
                    pm == "adr" or (pm == "addw" and len(po) == 3
                                    and po[1].type == ARM_OP_REG
                                    and self.ireg(p, po[1].reg) == 15))                     and self.ireg(p, po[0].reg) == dst:
                base = self.adr_target(p)
            elif limit is None and base is not None and pm == "cmp"                     and po[1].type == ARM_OP_IMM and self.ireg(p, po[0].reg) == idx:
                limit = po[1].imm
                break
        if base is None or idx is None or limit is None or not (0 <= limit < 256):
            return None
        return idx, [base + 4 * i for i in range(limit + 1)]

    def _jump_table(self, ins, found, code, base_addr, lo, hi):
        """
        Resuelve una tabla de saltos `tbb`/`tbh` y devuelve (destinos, bytes).

        GCC emite siempre el mismo idioma para un switch denso:

            cmp   rN, #limite
            bhi   <default>
            tbh   [pc, rN, lsl #1]
            <tabla: limite+1 halfwords>

        La tabla arranca justo despues del `tbh` y cada entrada es el
        desplazamiento a su destino en unidades de 2 bytes desde `pc`, que
        para Thumb es la direccion del `tbh` mas 4.

        Sin esto el descenso recursivo se rinde en la primera tabla y da por
        dato el resto de la funcion: en `_seq_lookup` cubria 38 bytes de 7608.
        El motor de combate despacha por tablas, asi que resolverlas es lo que
        hace utilizable el descenso recursivo ahi.
        """
        half = ins.mnemonic.split(".")[0] == "tbh"

        # el limite lo pone el `cmp rN, #imm` que precede al salto de guarda
        limit = None
        for a in sorted((x for x in found if x < ins.address), reverse=True)[:6]:
            prev = found[a]
            if prev.mnemonic.split(".")[0] in ("cmp", "cmn"):
                for op in prev.operands:
                    if op.type == ARM_OP_IMM:
                        limit = op.imm
                break
        if limit is None or not (0 <= limit < 4096):
            return [], 0

        n = limit + 1
        tbase = ins.address + ins.size
        pcv = ins.address + 4
        step = 2 if half else 1
        targets = []
        for i in range(n):
            off = tbase - base_addr + i * step
            raw = code[off:off + step]
            if len(raw) < step:
                return targets, i * step
            val = int.from_bytes(raw, "little")
            t = pcv + 2 * val
            if lo <= t < hi:
                targets.append(t)
        return targets, n * step

    def callees(self, addr, size):
        """Direcciones de funciones internas a las que llama esta funcion."""
        thumb = self.funcs[addr][1]
        off = self.text.offset + (addr - self.text.addr)
        md = Cs(CS_ARCH_ARM,
                (CS_MODE_THUMB if thumb else CS_MODE_ARM) | CS_MODE_LITTLE_ENDIAN)
        md.detail = True
        out = set()
        for ins in md.disasm(self.m.data[off:off + size], addr):
            if ins.mnemonic.split(".")[0] not in ("bl", "blx"):
                continue
            ops = ins.operands
            if ops and ops[0].type == ARM_OP_IMM and ops[0].imm in self.funcs:
                out.add(ops[0].imm)
        return out

    # -- funcion completa --

    def emit_function(self, addr, out):
        name, thumb = self.funcs[addr]
        size = func_size(addr, self.funcs, self.text)
        off = self.text.offset + (addr - self.text.addr)
        code = self.m.data[off:off + size]

        # Descenso recursivo: solo lo alcanzable como codigo. Sustituye al
        # barrido lineal, que se desincronizaba en los pools de literales
        # intercalados y emitia basura para el resto de la funcion.
        reachable, data_bytes = self.decode_reachable(addr, size, thumb)
        insns = [reachable[a] for a in sorted(reachable)]

        # destinos de salto internos -> etiquetas
        labels = set()
        for ins in insns:
            b = ins.mnemonic.split(".")[0]
            if (b.startswith("b") and b not in ("bic", "bics", "bl", "blx", "bx", "bfi", "bfc")) \
                    or b in ("cbz", "cbnz"):
                for op in ins.operands:
                    if op.type == ARM_OP_IMM and addr <= op.imm < addr + size:
                        labels.add(op.imm)

            # tbb/tbh: los destinos NO estan en los operandos, salen de la tabla
            # resuelta durante el descenso recursivo. Sin esto el switch emitia
            # "goto L_xxxx" hacia etiquetas que nadie declaraba, y la funcion no
            # compilaba -- con el error a 15.000 lineas de distancia de la causa.
            # Lo encontro create_fx_param, arrastrada como dependencia por
            # --with-deps al verificar SwitchQueue.
            if b in ("tbb", "tbh", "mov"):
                tbl = self.tables.get(ins.address)
                if tbl:
                    for t in tbl[1]:
                        if addr <= t < addr + size:
                            labels.add(t)

        cn = self.cname(addr)
        out.append("/* %s @ 0x%08x  (%s, %d bytes, %d instrucciones, %d bytes de datos) */"
                   % (name, addr, "Thumb" if thumb else "ARM", size,
                      len(insns), data_bytes))
        out.append("void %s(arm_ctx *ctx)" % cn)
        out.append("{")

        decoded_bytes = 0
        pending_it = []
        for ins in insns:
            decoded_bytes += ins.size
            if ins.address in labels:
                out.append("L_%08x:" % ins.address)
            out.append("    /* %08x  %s %s */" % (ins.address, ins.mnemonic, ins.op_str))
            try:
                predicates = self.it_predicates(ins)
                if predicates is not None:
                    if pending_it:
                        raise Unsupported("bloque IT anidado")
                    pending_it = predicates
                    self.translate(ins, cn, out)
                else:
                    it_cond = pending_it.pop(0) if pending_it else None
                    self.translate(ins, cn, out, it_cond)
            except Unsupported as e:
                self.problems.append((name, ins.address,
                                      "%s %s -- %s" % (ins.mnemonic, ins.op_str, e)))
                out.append('    arm_unimplemented("%s", 0x%08x, "%s");'
                           % (cn, ins.address,
                              (ins.mnemonic + " " + ins.op_str).replace('"', "'")))
        out.append("}")
        out.append("")
        return len(insns), data_bytes


# ---------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("binary")
    ap.add_argument("--file", help="filtrar por archivo fuente, p.ej. Matrix.cpp")
    ap.add_argument("--func", action="append", help="funcion concreta (repetible)")
    ap.add_argument("--all", action="store_true",
                    help="todas las funciones (para medir cobertura)")
    ap.add_argument("--with-deps", action="store_true",
                    help="arrastrar tambien las funciones llamadas, en cadena")
    ap.add_argument("--dry-run", action="store_true",
                    help="no escribir archivos; solo reportar cobertura")
    ap.add_argument("--out", required=True, help="directorio de salida")
    ap.add_argument("--name", default="recompiled", help="nombre base del .c generado")
    args = ap.parse_args()

    m, text, funcs = load_binary(args.binary)
    stubs = m.stub_map()
    fmap = load_file_map()

    targets = []
    for addr, (name, _t) in funcs.items():
        if args.all:
            targets.append(addr)
        elif args.func and name in args.func:
            targets.append(addr)
        elif args.file and fmap.get(name, "") == args.file:
            targets.append(addr)
    targets.sort()

    if not targets:
        raise SystemExit("no se selecciono ninguna funcion")

    em = Emitter(m, text, funcs, stubs)

    if args.with_deps:
        # Cierre transitivo de las llamadas. Sin esto hay que ir listando a mano
        # cada funcion auxiliar hasta que enlaza, que es tedioso y se olvida.
        # Las funciones sustituidas por shims cortan la cadena: no se arrastran
        # sus dependencias porque no se recompilan.
        seen = set(targets)
        queue = list(targets)
        while queue:
            a = queue.pop()
            size = func_size(a, funcs, text)
            for callee in em.callees(a, size):
                if callee in seen:
                    continue
                if funcs[callee][0] in OVERRIDES:
                    continue
                seen.add(callee)
                queue.append(callee)
        added = len(seen) - len(targets)
        targets = sorted(seen)
        if added:
            print("dependencias arrastradas: %d" % added)
    body = []
    total_ins = total_gap = 0
    for a in targets:
        n, gap = em.emit_function(a, body)
        total_ins += n
        total_gap += gap

    if args.dry_run:
        import collections as _c
        kinds = _c.Counter()
        for _n, _a, txt in em.problems:
            kinds[txt.split()[0]] += 1
        print("funciones procesadas:     %d" % len(targets))
        print("instrucciones traducidas: %d" % total_ins)
        print("bytes no decodificados:   %d  (%.2f%% del codigo recorrido)"
              % (total_gap, 100.0 * total_gap / (total_gap + total_ins * 2.6)))
        print("literales resueltos:      %d" % em.literals)
        print("instrucciones NO soportadas: %d  (%.3f%%)"
              % (len(em.problems), 100.0 * len(em.problems) / max(total_ins, 1)))
        print("\n-- mnemonicos no soportados, por frecuencia --")
        for k, v in kinds.most_common(40):
            print("   %-16s %6d" % (k, v))
        nfunc = len(set(n for n, _a, _t in em.problems))
        print("\nfunciones afectadas: %d de %d (%.1f%%)"
              % (nfunc, len(targets), 100.0 * nfunc / len(targets)))
        print("funciones 100%% traducidas: %d (%.1f%%)"
              % (len(targets) - nfunc, 100.0 * (len(targets) - nfunc) / len(targets)))
        return 0

    os.makedirs(args.out, exist_ok=True)
    cpath = os.path.join(args.out, args.name + ".c")
    hpath = os.path.join(args.out, args.name + ".h")

    header = [
        "/* GENERADO POR tools/armrecomp/recomp.py -- NO EDITAR A MANO */",
        "#include \"%s.h\"" % args.name,
        "#include <math.h>",
        "",
    ]
    with open(cpath, "w", encoding="utf-8") as fh:
        fh.write("\n".join(header + body) + "\n")

    decls = ["/* GENERADO POR tools/armrecomp/recomp.py -- NO EDITAR A MANO */",
             "#ifndef %s_H" % args.name.upper(),
             "#define %s_H" % args.name.upper(),
             "#include \"arm_runtime.h\"",
             ""]
    generated = set()
    for a in targets:
        cn = em.cname(a)
        generated.add(cn)
        decls.append("void %s(arm_ctx *ctx);" % cn)

    missing = sorted(n for n in em.called_out if n not in generated)
    if missing:
        decls += ["",
                  "/* Llamadas internas que ESTA generacion no incluye (sin",
                  " * --with-deps). Se declaran para que el .c compile; quien",
                  " * las use tiene que definirlas -- normalmente un test que",
                  " * quiere aislar al llamador de sus llamados. */"]
        for n in missing:
            decls.append("void %s(arm_ctx *ctx);" % n)
    if em.auto_shims:
        decls += ["",
                  "/* Importadas del binario sin shim escrito a mano. La",
                  " * definicion por defecto vive en <nombre>_shims.c y aborta",
                  " * con arm_unimplemented(). Escribe la tuya y el enlazador",
                  " * usara la tuya. */"]
        for shim in sorted(em.auto_shims):
            decls.append("void %s(arm_ctx *ctx);   /* %s */"
                         % (shim, em.auto_shims[shim]))
    decls += ["", "#endif"]
    with open(hpath, "w", encoding="utf-8") as fh:
        fh.write("\n".join(decls) + "\n")

    spath = None
    if em.auto_shims:
        # Definiciones por defecto en su propio .c: sustituir una es borrar su
        # cuerpo y escribir la implementacion real.
        spath = os.path.join(args.out, args.name + "_shims.c")
        sh = ["/* GENERADO POR tools/armrecomp/recomp.py -- NO EDITAR A MANO */",
              "/*",
              " * Importadas sin implementacion en el anfitrion. Llamar a",
              " * cualquiera aborta y dice cual: fallar ruidosamente en",
              " * EJECUCION en vez de impedir la generacion. Solo hacen falta",
              " * las que el codigo bajo prueba ejecute de verdad.",
              " */",
              '#include "%s.h"' % args.name,
              ""]
        for shim in sorted(em.auto_shims):
            sh += ["void %s(arm_ctx *ctx)" % shim, "{",
                   '    arm_unimplemented("%s", 0u, "importada sin implementar: %s");'
                   % (shim, em.auto_shims[shim]),
                   "    (void)ctx;", "}", ""]
        with open(spath, "w", encoding="utf-8") as fh:
            fh.write("\n".join(sh) + "\n")

    print("funciones recompiladas: %d" % len(targets))
    print("instrucciones traducidas: %d" % total_ins)
    print("bytes no decodificados:   %d" % total_gap)
    print("literales resueltos:      %d" % em.literals)
    if em.overridden:
        print("funciones sustituidas por shims: %s" % ", ".join(sorted(em.overridden)))
    print("instrucciones NO soportadas: %d" % len(em.problems))
    for name, a, txt in em.problems[:30]:
        print("   %-32s 0x%08x  %s" % (name, a, txt))
    print("\nescrito %s" % cpath)
    print("escrito %s" % hpath)
    if spath:
        print("escrito %s" % spath)
    return 1 if em.problems else 0


if __name__ == "__main__":
    sys.exit(main())
