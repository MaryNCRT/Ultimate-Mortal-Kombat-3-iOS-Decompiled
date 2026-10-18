'use strict';

const fs = require('fs');
const path = require('path');
const { loadMachO, N_STAB, N_TYPE, N_SECT } = require('./macho');

const N_GSYM = 0x20;
const N_FUN = 0x24;
const N_STSYM = 0x26;
const N_SO = 0x64;
const LOGIC_DIR = '/gamecode/logic/';

function hex8(n) {
    return (n >>> 0).toString(16).padStart(8, '0');
}

function bisectRight(arr, x) {
    let lo = 0, hi = arr.length;
    while (lo < hi) {
        const mid = (lo + hi) >>> 1;
        if (x < arr[mid]) hi = mid;
        else lo = mid + 1;
    }
    return lo;
}

function readMap(mapPath) {
    const native = {};
    const alias = {};
    const objects = new Set();
    const forcedInts = [];

    const lines = fs.readFileSync(mapPath, 'utf8').split('\n');
    for (const rawLine of lines) {
        const line = rawLine.split('#')[0].trim().split(/\s+/).filter(Boolean);
        if (line.length === 0) continue;

        if (line[0] === 'native' && line.length === 3) {
            native[line[1]] = line[2];
        } else if (line[0] === 'alias' && line.length === 3) {
            alias[parseInt(line[2], 16)] = line[1];
        } else if (line[0] === 'object' && line.length === 2) {
            objects.add(line[1]);
        } else if (line[0] === 'int' && line.length === 3) {
            forcedInts.push([line[1], parseInt(line[2], 16)]);
        } else if (line[0] === 'number') {
            // Checked by check_logic_tables
        } else {
            throw new Error(`${mapPath}: bad line ${rawLine}`);
        }
    }
    return { native, alias, objects, forcedInts };
}

function cName(binaryName) {
    const n = binaryName.startsWith('_') ? binaryName.slice(1) : binaryName;
    return n.replace(/\./g, '_');
}

/**
 * Extracts fight engine data tables from the binary into C code.
 * @param {Buffer|string} machoInput
 * @param {string} outPath
 * @param {string} mapPath
 * @param {string} [reportPath]
 */
function extractLogicTables(machoInput, outPath, mapPath, reportPath = null) {
    const { native, alias, objects, forcedInts } = readMap(mapPath);
    const { data, base, m } = loadMachO(machoInput);
    const secs = m.sections;
    const syms = m.symbols();

    // 1. Which names belong to gamecode/logic, per STABs
    let cu = null;
    const logicData = new Set();
    const logicFuncs = new Set();
    for (const { name, n_type } of syms) {
        if (n_type === N_SO && name) {
            cu = name;
        } else if (cu && cu.includes(LOGIC_DIR)) {
            if (n_type === N_GSYM || n_type === N_STSYM) {
                logicData.add(name);
            } else if (n_type === N_FUN && name) {
                logicFuncs.add(name.split(':')[0]);
            }
        }
    }

    // 2. Real (non-STAB) symbols with their sections, sorted by addr
    const real = [];
    for (const { name, n_type, n_sect, n_desc, n_value } of syms) {
        if ((n_type & N_STAB) || (n_type & N_TYPE) !== N_SECT) {
            continue;
        }
        real.push({ val: n_value, name, sect: secs[n_sect - 1], desc: n_desc });
    }
    real.sort((a, b) => a.val - b.val);

    function extent(i) {
        const a = real[i].val;
        const s = real[i].sect;
        let end = s.addr + s.size;
        for (let j = i + 1; j < real.length; j++) {
            if (real[j].val > a) {
                if (real[j].sect === s || real[j].val < end) {
                    end = Math.min(end, real[j].val);
                }
                break;
            }
        }
        return end - a;
    }

    const forcedIntMap = new Map();
    for (const [bname, off] of forcedInts) {
        const hit = real.filter(r => r.name === bname).map(r => r.val);
        if (hit.length === 0) {
            throw new Error(`logic_tables.map: int ${bname}: not in binary`);
        }
        forcedIntMap.set(hit[0] + off, `${bname}+0x${off.toString(16)}`);
    }

    const thumbFuncs = new Map();
    for (const { val, name, sect, desc } of real) {
        if (sect.segname === '__TEXT' && sect.sectname === '__text' && (desc & 0x8) && logicFuncs.has(name)) {
            thumbFuncs.set(val, cName(name));
        }
    }

    const allFnNames = Array.from(thumbFuncs.values());
    const fnCounts = new Map();
    for (const fn of allFnNames) {
        fnCounts.set(fn, (fnCounts.get(fn) || 0) + 1);
    }
    const ambiguous = new Set(allFnNames.filter(fn => fnCounts.get(fn) > 1));

    const objs = []; // [addr, size, cname, bname, sect, zerofill]
    const seen = new Set();
    for (let i = 0; i < real.length; i++) {
        const { val: a, name, sect } = real[i];
        if (!logicData.has(name) || seen.has(a)) continue;
        seen.add(a);
        const zerofill = (sect.flags & 0xff) === 0x1 || (sect.flags & 0xff) === 0xc;
        let cname = alias[a] || cName(name);
        if (zerofill && !objects.has(name)) {
            cname = 'umk3_common_' + cname;
        }
        objs.push([a, extent(i), cname, name, sect, zerofill]);
    }

    const emitted = objs.filter(o => !o[5]);
    const common = objs.filter(o => o[5]);

    const ranges = objs.map(o => [o[0], o[0] + o[1], o[2]]);
    for (let i = 0; i < real.length; i++) {
        const { val: a, name } = real[i];
        if (native[name] && !logicData.has(name)) {
            ranges.push([a, a + extent(i), native[name]]);
        }
    }
    ranges.sort((a, b) => a[0] - b[0]);
    const starts = ranges.map(r => r[0]);
    const nativeTargets = new Set(Object.values(native));

    function target(v) {
        if ((v & 1) && thumbFuncs.has(v & ~1)) {
            const fnName = thumbFuncs.get(v & ~1);
            if (ambiguous.has(fnName)) {
                throw new Error(`a table points at 0x${(v & ~1).toString(16)}, one of several functions named ${fnName}`);
            }
            return ['fn', fnName, 0];
        }
        const i = bisectRight(starts, v) - 1;
        if (i >= 0 && ranges[i][0] <= v && v < ranges[i][1]) {
            return ['data', ranges[i][2], v - ranges[i][0]];
        }
        return null;
    }

    function readBytes(a, n) {
        const s = m.sectionForAddr(a);
        const off = base + s.offset + (a - s.addr);
        return data.subarray(off, off + n);
    }

    const out = [];
    const rep = [];
    const fnRefs = new Set();
    const dataDefs = [];
    const fixups = [];
    let nrel = 0;

    for (const [a, size, cname, bname, s] of emitted) {
        const pad = (4 - (size % 4)) % 4;
        const rawBuf = readBytes(a, size);
        const totalWords = (size + pad) / 4;
        const words = [];
        for (let w = 0; w < totalWords; w++) {
            let val = 0;
            for (let b = 0; b < 4; b++) {
                const idx = w * 4 + b;
                const byte = idx < size ? rawBuf[idx] : 0;
                val |= (byte << (b * 8)) >>> 0;
            }
            words.push(val >>> 0);
        }

        const items = [];
        let orel = 0;
        for (let k = 0; k < words.length; k++) {
            const v = words[k];
            let t = target(v);
            const addrAtWord = a + k * 4;

            if (forcedIntMap.has(addrAtWord)) {
                if (t === null) {
                    throw new Error(`logic_tables.map: int ${forcedIntMap.get(addrAtWord)} is not a relocation candidate; drop the entry`);
                }
                const label = forcedIntMap.get(addrAtWord);
                forcedIntMap.delete(addrAtWord);
                if (reportPath) {
                    rep.push(`int!  ${cname.padEnd(28)} +0x${(k * 4).toString(16).padStart(4, '0')} ${hex8(v)}  (map: a number, not ${t[1]}+0x${t[2].toString(16)})`);
                }
                t = null;
                items.push(`0x${v.toString(16)}`);
                continue;
            }

            if (t === null) {
                items.push(`0x${v.toString(16)}`);
                continue;
            }

            const [kind, tname, off] = t;
            nrel++;
            orel++;
            if (kind === 'fn') {
                fnRefs.add(tname);
            }
            items.push(`UMK3_REL(${tname}, 0x${off.toString(16)})`);
            fixups.push([cname, k, tname, off]);
            if (reportPath) {
                rep.push(`${kind.padEnd(5)} ${cname.padEnd(28)} +0x${(k * 4).toString(16).padStart(4, '0')} ${hex8(v)} -> ${tname}${off ? '+0x' + off.toString(16) : ''}`);
            }
        }

        const exact = a % 4 === 0 && size % 4 === 0;
        if (!exact && orel > 0) {
            throw new Error(`${bname} at 0x${a.toString(16)} is not word-aligned and holds ${orel} relocation(s)`);
        }
        dataDefs.push([cname, bname, a, s, items, size, exact ? null : rawBuf]);
    }

    if (forcedIntMap.size > 0) {
        throw new Error(`logic_tables.map: int ${Array.from(forcedIntMap.values()).sort().join(', ')} is in no emitted object`);
    }

    out.push('/* GENERATED by tools/logic_tables.py -- do not edit, do not commit.');
    out.push(' *');
    out.push(" * Every data object of src/gamecode/logic, read out of the user's own");
    out.push(` * binary (uuid ${m.uuid}) at build time.`);
    out.push(` * ${dataDefs.length} objects, ${nrel} relocated words. */`);
    out.push('#include <stdint.h>');
    out.push('/* A word is 32 bits, as on the device. A 64-bit build keeps the game');
    out.push(' * below 4 GB (UMK3_LOW32): its words are uint32_t (runtime/savestate.h');
    out.push(' * makes uintptr_t one there) and the addresses, which C cannot put in');
    out.push(' * a 32-bit initializer, are written by umk3_logic_tables_fixup(). */');
    out.push('#ifdef UMK3_LOW32');
    out.push('#define UMK3_REL(p, o) 0');
    out.push('#else');
    out.push('typedef char umk3_logic_tables_need_32bit_pointers[sizeof(void *) == 4 ? 1 : -1];');
    out.push('#define UMK3_REL(p, o) ((uintptr_t)(p) + (o))');
    out.push('#endif');
    out.push('');
    out.push('/* One section, binary order: see Layout in tools/logic_tables.py. */');
    out.push('#if defined(__APPLE__)');
    out.push('#define UMK3_LOGIC __attribute__((section("__DATA,__umk3logic")))');
    out.push('#elif defined(__wasm__)');
    out.push('#define UMK3_LOGIC  /* no named sections: order is not guaranteed */');
    out.push('#elif defined(_WIN32) && defined(UMK3_STATE_FILE)');
    out.push("/* the game's save states copy this block too (runtime/savestate.h);");
    out.push(' * "$l" merges it into .umkstd in one piece, its order kept */');
    out.push('#define UMK3_LOGIC __attribute__((section(".umkstd$l")))');
    out.push('#else');
    out.push('#define UMK3_LOGIC __attribute__((section(".data.umk3logic")))');
    out.push('#endif');
    out.push('');

    for (const f of Array.from(fnRefs).sort()) {
        out.push(`extern char ${f}[];`);
    }
    for (const t of Array.from(nativeTargets).sort()) {
        out.push(`extern char ${t}[];`);
    }
    for (const [cname, , , , , , raw] of dataDefs) {
        out.push(`extern ${raw ? 'unsigned char' : 'uintptr_t'} ${cname}[];`);
    }
    for (const [a, size, cname] of common) {
        out.push(`extern ${a % 4 === 0 && size % 4 === 0 ? 'uintptr_t' : 'unsigned char'} ${cname}[];`);
    }
    out.push('');

    for (const [cname, bname, a, s, items, size, raw] of dataDefs) {
        out.push(`/* ${bname}  0x${hex8(a)}  ${s.segname},${s.sectname} */`);
        if (raw !== null) {
            const align = (a & 3) ? (a & -a) : 4;
            out.push(`unsigned char ${cname}[${size}] UMK3_LOGIC __attribute__((aligned(${Math.min(align, 4)}))) = {`);
            for (let k = 0; k < raw.length; k += 16) {
                const slice = raw.subarray(k, Math.min(k + 16, raw.length));
                out.push('    ' + Array.from(slice).join(', ') + ',');
            }
        } else {
            out.push(`uintptr_t ${cname}[${items.length}] UMK3_LOGIC __attribute__((aligned(4))) = {`);
            for (let k = 0; k < items.length; k += 6) {
                const slice = items.slice(k, k + 6);
                out.push('    ' + slice.join(', ') + ',');
            }
        }
        out.push('};');
    }
    out.push('');

    out.push('/* Runtime state: zero, owned through the slot pointers. Zero-filled');
    out.push(' * on the device too, but kept in the section so it stays in order. */');
    for (const [a, size, cname, bname] of common) {
        if (a % 4 === 0 && size % 4 === 0) {
            out.push(`uintptr_t ${cname}[${size / 4}] UMK3_LOGIC __attribute__((aligned(4))) = {0};  /* ${bname} 0x${hex8(a)}, ${size} bytes */`);
        } else {
            const align = (a & 3) ? (a & -a) : 4;
            out.push(`unsigned char ${cname}[${size}] UMK3_LOGIC __attribute__((aligned(${Math.min(align, 4)}))) = {0};  /* ${bname} 0x${hex8(a)}, ${size} bytes */`);
        }
    }
    out.push('');

    out.push('void umk3_logic_tables_fixup(void)');
    out.push('{');
    out.push('#ifdef UMK3_LOW32');
    out.push('    static const struct { uint32_t *at; const char *to; } fix[] = {');
    for (const [cname, k, tname, off] of fixups) {
        out.push(`        { (uint32_t *)${cname} + ${k}, (const char *)${tname} + 0x${off.toString(16)} },`);
    }
    out.push('    };');
    out.push('    unsigned i;');
    out.push('');
    out.push('    for (i = 0; i < sizeof fix / sizeof fix[0]; i++)');
    out.push('        *fix[i].at = (uint32_t)(uintptr_t)fix[i].to;');
    out.push('#endif');
    out.push('}');

    const content = out.join('\n') + '\n';
    if (outPath) {
        fs.mkdirSync(path.dirname(outPath), { recursive: true });
        fs.writeFileSync(outPath, content, { encoding: 'utf-8' });
    }
    if (reportPath) {
        fs.writeFileSync(reportPath, rep.join('\n') + '\n', { encoding: 'utf-8' });
    }
    return { dataDefsCount: dataDefs.length, nrel, fnRefsCount: fnRefs.size, content };
}

module.exports = { extractLogicTables };
