'use strict';

const fs = require('fs');

const FAT_MAGIC = 0xCAFEBABE;
const FAT_CIGAM = 0xBEBAFECA;
const MH_MAGIC = 0xFEEDFACE;
const MH_CIGAM = 0xCEFAEDFE;
const MH_MAGIC_64 = 0xFEEDFACF;

const CPU_TYPE_ARM = 12;
const ARM_SUBTYPES = { 6: 'armv6', 9: 'armv7', 11: 'armv7s' };

const LC_REQ_DYLD = 0x80000000;
const LC_SEGMENT = 0x01;
const LC_SYMTAB = 0x02;
const LC_DYSYMTAB = 0x0b;
const LC_LOAD_DYLIB = 0x0c;
const LC_ID_DYLIB = 0x0d;
const LC_LOAD_WEAK_DYLIB = 0x18 | LC_REQ_DYLD;
const LC_UUID = 0x1b;
const LC_ENCRYPTION_INFO = 0x21;
const LC_VERSION_MIN_IPHONEOS = 0x25;
const LC_FUNCTION_STARTS = 0x26;
const LC_MAIN = 0x28 | LC_REQ_DYLD;

// nlist n_type
const N_STAB = 0xe0;
const N_TYPE = 0x0e;
const N_EXT = 0x01;
const N_UNDF = 0x0;
const N_ABS = 0x2;
const N_SECT = 0xe;
const N_PBUD = 0xc;
const N_INDR = 0xa;

class Section {
    constructor(segname, sectname, addr, size, offset, flags, reserved1 = 0, reserved2 = 0) {
        this.segname = segname;
        this.sectname = sectname;
        this.addr = addr;
        this.size = size;
        this.offset = offset;
        this.flags = flags;
        this.reserved1 = reserved1;
        this.reserved2 = reserved2;
    }

    containsAddr(a) {
        return this.addr <= a && a < this.addr + this.size;
    }
}

class MachO {
    /**
     * 32-bit Mach-O parser (little-endian) for armv6/armv7.
     * @param {Buffer} data
     * @param {number} base Offset of the slice inside the fat binary, or 0 if thin.
     */
    constructor(data, base = 0) {
        this.data = data;
        this.base = base;

        const magic = data.readUInt32LE(base);
        if (magic === MH_MAGIC_64) {
            throw new Error('64-bit Mach-O: not supported');
        }
        if (magic !== MH_MAGIC) {
            throw new Error(`Unexpected Mach-O magic: 0x${magic.toString(16)}`);
        }

        this.magic = magic;
        this.cputype = data.readUInt32LE(base + 4);
        this.cpusubtype = data.readUInt32LE(base + 8);
        this.filetype = data.readUInt32LE(base + 12);
        this.ncmds = data.readUInt32LE(base + 16);
        this.sizeofcmds = data.readUInt32LE(base + 20);
        this.flags = data.readUInt32LE(base + 24);

        this.segments = [];     // [{ segname, vmaddr, vmsize, fileoff, filesize }]
        this.sections = [];     // [Section]
        this.dylibs = [];
        this.uuid = null;
        this.encryption = null; // [cryptoff, cryptsize, cryptid]
        this.symtab = null;     // [symoff, nsyms, stroff, strsize]
        this.dysymtab = null;   // [18 uint32s]
        this.function_starts = null; // [dataoff, datasize]
        this.version_min = null;
        this.entry = null;

        this._parseCommands();
    }

    _parseCommands() {
        let off = this.base + 28;
        const d = this.data;
        for (let i = 0; i < this.ncmds; i++) {
            const cmd = d.readUInt32LE(off);
            const cmdsize = d.readUInt32LE(off + 4);
            if (cmdsize === 0) break;
            this._parseOne(cmd, cmdsize, off);
            off += cmdsize;
        }
    }

    _parseOne(cmd, cmdsize, off) {
        const d = this.data;
        if (cmd === LC_SEGMENT) {
            const segname = d.toString('ascii', off + 8, off + 24).replace(/\0+$/, '');
            const vmaddr = d.readUInt32LE(off + 24);
            const vmsize = d.readUInt32LE(off + 28);
            const fileoff = d.readUInt32LE(off + 32);
            const filesize = d.readUInt32LE(off + 36);
            const nsects = d.readUInt32LE(off + 48);

            this.segments.push({ segname, vmaddr, vmsize, fileoff, filesize });

            let so = off + 56;
            for (let i = 0; i < nsects; i++) {
                const sectname = d.toString('ascii', so, so + 16).replace(/\0+$/, '');
                const ssegname = d.toString('ascii', so + 16, so + 32).replace(/\0+$/, '');
                const addr = d.readUInt32LE(so + 32);
                const size = d.readUInt32LE(so + 36);
                const offset = d.readUInt32LE(so + 40);
                const flags = d.readUInt32LE(so + 56);
                const res1 = d.readUInt32LE(so + 60);
                const res2 = d.readUInt32LE(so + 64);

                this.sections.push(new Section(ssegname, sectname, addr, size, offset, flags, res1, res2));
                so += 68;
            }
        } else if (cmd === LC_SYMTAB) {
            this.symtab = [
                d.readUInt32LE(off + 8),
                d.readUInt32LE(off + 12),
                d.readUInt32LE(off + 16),
                d.readUInt32LE(off + 20)
            ];
        } else if (cmd === LC_DYSYMTAB) {
            const vals = [];
            for (let i = 0; i < 18; i++) {
                vals.push(d.readUInt32LE(off + 8 + i * 4));
            }
            this.dysymtab = vals;
        } else if (cmd === LC_LOAD_DYLIB || cmd === LC_LOAD_WEAK_DYLIB || cmd === LC_ID_DYLIB) {
            const nameoff = d.readUInt32LE(off + 8);
            const s = off + nameoff;
            const end = d.indexOf(0, s);
            this.dylibs.push(d.toString('utf8', s, end >= 0 ? end : s + cmdsize));
        } else if (cmd === LC_UUID) {
            this.uuid = d.toString('hex', off + 8, off + 24);
        } else if (cmd === LC_ENCRYPTION_INFO) {
            this.encryption = [
                d.readUInt32LE(off + 8),
                d.readUInt32LE(off + 12),
                d.readUInt32LE(off + 16)
            ];
        } else if (cmd === LC_FUNCTION_STARTS) {
            this.function_starts = [
                d.readUInt32LE(off + 8),
                d.readUInt32LE(off + 12)
            ];
        } else if (cmd === LC_VERSION_MIN_IPHONEOS) {
            this.version_min = [
                d.readUInt32LE(off + 8),
                d.readUInt32LE(off + 12)
            ];
        }
    }

    arch() {
        if (this.cputype === CPU_TYPE_ARM) {
            return ARM_SUBTYPES[this.cpusubtype] || `arm(sub=${this.cpusubtype})`;
        }
        return `cpu=${this.cputype} sub=${this.cpusubtype}`;
    }

    sectionByName(segname, sectname) {
        for (const s of this.sections) {
            if (s.segname === segname && s.sectname === sectname) {
                return s;
            }
        }
        return null;
    }

    sectionForAddr(addr) {
        for (const s of this.sections) {
            if (s.containsAddr(addr)) {
                return s;
            }
        }
        return null;
    }

    symbols() {
        if (!this.symtab) return [];
        const [symoff, nsyms, stroff] = this.symtab;
        const d = this.data;
        const strbase = this.base + stroff;
        const out = [];
        let o = this.base + symoff;

        for (let i = 0; i < nsyms; i++) {
            const n_strx = d.readUInt32LE(o);
            const n_type = d.readUInt8(o + 4);
            const n_sect = d.readUInt8(o + 5);
            const n_desc = d.readInt16LE(o + 6);
            const n_value = d.readUInt32LE(o + 8);
            o += 12;

            let name = '';
            if (n_strx) {
                const s = strbase + n_strx;
                const e = d.indexOf(0, s);
                name = d.toString('utf8', s, e >= 0 ? e : s + 256);
            }
            out.push({ name, n_type, n_sect, n_desc, n_value });
        }
        return out;
    }
}

function fatSlices(data) {
    if (data.length < 8) return [];
    const magic = data.readUInt32BE(0);
    if (magic !== FAT_MAGIC) return [];

    const nfat = data.readUInt32BE(4);
    const out = [];
    for (let i = 0; i < nfat; i++) {
        const off = 8 + i * 20;
        out.push({
            cputype: data.readUInt32BE(off),
            cpusubtype: data.readUInt32BE(off + 4),
            offset: data.readUInt32BE(off + 8),
            size: data.readUInt32BE(off + 12),
            align: data.readUInt32BE(off + 16),
        });
    }
    return out;
}

function loadMachO(bufferOrPath) {
    const data = Buffer.isBuffer(bufferOrPath) ? bufferOrPath : fs.readFileSync(bufferOrPath);
    let base = 0;
    const slices = fatSlices(data);
    if (slices.length > 0) {
        let found = false;
        for (const sl of slices) {
            if (sl.cputype === CPU_TYPE_ARM && sl.cpusubtype === 9) { // armv7
                base = sl.offset;
                found = true;
                break;
            }
        }
        if (!found) {
            throw new Error('No armv7 slice found in fat Mach-O');
        }
    }
    const m = new MachO(data, base);
    if (m.arch() !== 'armv7') {
        throw new Error(`Expected armv7, got ${m.arch()}`);
    }
    return { data, base, m };
}

module.exports = {
    MachO,
    Section,
    fatSlices,
    loadMachO,
    CPU_TYPE_ARM,
    N_STAB,
    N_TYPE,
    N_EXT,
    N_UNDF,
    N_ABS,
    N_SECT,
    N_PBUD,
    N_INDR,
};
