#!/usr/bin/env python3
"""Desensamblador Thumb manual minimalista (solo para detectar dispatch indirecto).

Analiza el binario armv7 buscando:
  - blx reg / bx reg  (salto indirecto -> function pointer / jump table)
  - tbb / tbh         (tabla de salto Thumb)
  - ldr reg, [pc, #imm] (carga literal -> posible tabla)
  - patrones de jump table (ldr pc, [reg]  / add reg, pc)

Uso:
  python3 tools/thumb_scan.py /ruta/UMK3.armv7 <start_addr> <size>
  (addr en la VM; file = vmaddr - 0x1000)
"""
import sys

def disasm_thumb16(code, addr):
    """Devuelve (mnem, operands, is_indirect, is_branch). Decodifica lo esencial."""
    import struct
    res = []
    n = len(code) // 2
    off = 0
    for i in range(n):
        ins = struct.unpack_from('<H', code, off)[0]
        word = addr + off
        off += 2
        cond = ''
        # TBB/TBH: 0xD8xx/0xD9xx (range: 0xD800-0xDFFF es cond branch; tbb/tbh son D800/D900 con bits)
        if (ins & 0xFFF0) == 0xE8D0 or (ins & 0xFFF0) == 0xE8C0:
            pass
        # Tablas de salto Thumb: 0xE8DF (tbb), 0xE8DF (tbh) variantes
        # tbb [pc, reg]  = 0xE8DF 0x00xx ; tbh [pc,reg,lsl#1] = 0xE8DF 0x00xx|0x80
        if (ins & 0xFF00) == 0xD800:  # cond branch (varios)
            pass
        # BX reg: 0100 0111 0 reg (0x4700 | reg<<3)
        if (ins & 0xFF87) == 0x4700:
            rm = (ins >> 3) & 0xF
            res.append((word, 'bx', f'r{rm}', True, False))
            continue
        # BLX reg: 0100 0111 1 reg (0x4780)
        if (ins & 0xFF87) == 0x4780:
            rm = (ins >> 3) & 0xF
            res.append((word, 'blx', f'r{rm}', True, False))
            continue
        # BLX/BL immediate 32-bit (0xF000/0xF800 + 0xF800) -> direct
        if (ins & 0xF800) == 0xF000 or (ins & 0xF800) == 0xF800:
            # next halfword needed
            if off < len(code):
                ins2 = struct.unpack_from('<H', code, off)[0]
                off += 2
                mnem = 'bl' if (ins & 0x1000) == 0x1000 else 'blx'
                res.append((word, mnem, '<imm>', False, True))
                continue
        # B (cond) 0xD000-0xDFFF
        if (ins & 0xF000) == 0xD000:
            res.append((word, 'b.cond', f'<{0x8000 + ((ins & 0xFF)*2) - 0x800}>', False, True))
            continue
        # B unconditional 0xE000
        if (ins & 0xF800) == 0xE000:
            res.append((word, 'b', f'<imm>', False, True))
            continue
        # LDR reg,[pc,#imm] o ADD reg,pc -> posible acceso a tabla
        if (ins & 0xF800) == 0x4800:  # LDR Rt,[pc,#imm]
            rt = (ins >> 8) & 7
            imm = (ins & 0xFF) * 4
            res.append((word, 'ldr.pc', f'r{rt},[pc,#0x{imm:x}]', False, False))
            continue
        res.append((word, '??', f'0x{ins:04x}', False, False))
    return res

def main():
    if len(sys.argv) < 4:
        print(__doc__); return
    path = sys.argv[1]
    start = int(sys.argv[2], 16)
    size = int(sys.argv[3], 16)
    base = 0x1000
    with open(path, 'rb') as f:
        f.seek(start - base)
        code = f.read(size)
    print(f"# Desensamblado Thumb manual @ 0x{start:x}..0x{start+size:x}")
    ind = 0
    for word, mnem, ops, is_ind, is_br in disasm_thumb16(code, start):
        tag = ''
        if is_ind:
            tag = '  <<< INDIRECTO'
            ind += 1
        if is_br:
            tag += '  <branch>'
        if is_ind or is_br or mnem.startswith('ldr.pc'):
            print(f"0x{word:08x}: {mnem:8s} {ops}{tag}")
    print(f"\n# total indirectos: {ind}")

if __name__ == '__main__':
    main()
