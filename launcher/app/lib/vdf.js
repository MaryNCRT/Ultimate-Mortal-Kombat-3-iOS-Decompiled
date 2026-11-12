/* Valve's two VDF formats, as Steam itself writes them.
 *
 * Binary (shortcuts.vdf): a node is one type byte, a NUL-terminated UTF-8
 * key and a value:
 *     0x00 map     -- nodes until 0x08
 *     0x01 string  -- NUL-terminated UTF-8
 *     0x02 int32   -- 4 bytes little-endian (kept as the raw uint32)
 *     0x03 float32, 0x04 pointer, 0x06 colour -- 4 bytes, kept raw
 *     0x07 uint64, 0x0A int64                 -- 8 bytes, kept raw
 *     0x08 end of the map
 * The file is one map (no key of its own at the top: the file starts with
 * the "shortcuts" node) closed by a last 0x08.  The parse keeps every node in
 * order with its type, so an unchanged file serializes to the same bytes:
 * other programs' shortcuts are written back exactly as they were (checked
 * in test/steam.test.js).  Anything it does not understand (0x05 wide
 * strings, a truncated file) throws, and the caller then refuses to write.
 *
 * Text (loginusers.vdf, libraryfolders.vdf): "key" "value" and "key" { ... }.
 */
'use strict';

const T = { MAP: 0, STR: 1, INT: 2, FLOAT: 3, PTR: 4, WSTR: 5, COLOR: 6, U64: 7, END: 8, I64: 10 };
const RAW_SIZE = { [T.FLOAT]: 4, [T.PTR]: 4, [T.COLOR]: 4, [T.U64]: 8, [T.I64]: 8 };

function parseBinary(buf) {
    let p = 0;
    function cstr() {
        const end = buf.indexOf(0, p);
        if (end < 0)
            throw new Error('vdf: unterminated string at ' + p);
        const s = buf.toString('utf8', p, end);
        p = end + 1;
        return s;
    }
    function map() {
        const out = [];
        for (;;) {
            if (p >= buf.length)
                throw new Error('vdf: file ends inside a map');
            const type = buf[p++];
            if (type === T.END)
                return out;
            const key = cstr();
            if (type === T.MAP)
                out.push({ type, key, value: map() });
            else if (type === T.STR)
                out.push({ type, key, value: cstr() });
            else if (type === T.INT) {
                if (p + 4 > buf.length) throw new Error('vdf: truncated int');
                out.push({ type, key, value: buf.readUInt32LE(p) });
                p += 4;
            } else if (RAW_SIZE[type]) {
                const n = RAW_SIZE[type];
                if (p + n > buf.length) throw new Error('vdf: truncated value');
                out.push({ type, key, value: Buffer.from(buf.subarray(p, p + n)) });
                p += n;
            } else
                throw new Error('vdf: unsupported node type 0x' + type.toString(16) + ' at ' + (p - 1));
        }
    }
    if (buf.length === 0)
        return [];
    const root = map();
    if (p !== buf.length)
        throw new Error('vdf: ' + (buf.length - p) + ' bytes after the end');
    return root;
}

function serializeBinary(nodes) {
    const parts = [];
    const cstr = s => { parts.push(Buffer.from(String(s), 'utf8'), Buffer.from([0])); };
    function map(list) {
        for (const n of list) {
            parts.push(Buffer.from([n.type]));
            cstr(n.key);
            if (n.type === T.MAP)
                map(n.value);
            else if (n.type === T.STR)
                cstr(n.value);
            else if (n.type === T.INT) {
                const b = Buffer.alloc(4);
                b.writeUInt32LE(n.value >>> 0);
                parts.push(b);
            } else if (RAW_SIZE[n.type])
                parts.push(n.value);
            else
                throw new Error('vdf: cannot write node type ' + n.type);
        }
        parts.push(Buffer.from([T.END]));
    }
    map(nodes);
    return Buffer.concat(parts);
}

/* helpers over a node list */
function get(list, key) {
    const k = key.toLowerCase();
    return list.find(n => n.key.toLowerCase() === k);
}
function val(list, key, d) {
    const n = get(list, key);
    return n ? n.value : d;
}
function set(list, key, type, value) {
    const n = get(list, key);
    if (n) { n.type = type; n.value = value; }
    else list.push({ type, key, value });
}

/* text VDF -> plain nested objects (keys as written) */
function parseText(src) {
    let p = 0;
    const n = src.length;
    function skip() {
        for (;;) {
            while (p < n && /\s/.test(src[p])) p++;
            if (src[p] === '/' && src[p + 1] === '/') {
                while (p < n && src[p] !== '\n') p++;
                continue;
            }
            return;
        }
    }
    function token() {
        skip();
        if (p >= n) return null;
        const c = src[p];
        if (c === '{' || c === '}') { p++; return { brace: c }; }
        if (c === '"') {
            p++;
            let s = '';
            while (p < n && src[p] !== '"') {
                if (src[p] === '\\' && p + 1 < n) {
                    const e = src[p + 1];
                    s += e === 'n' ? '\n' : e === 't' ? '\t' : e;
                    p += 2;
                } else s += src[p++];
            }
            p++;
            return { s };
        }
        let s = '';
        while (p < n && !/[\s{}"]/.test(src[p])) s += src[p++];
        return { s };
    }
    function obj(top) {
        const o = {};
        for (;;) {
            const k = token();
            if (!k) { if (top) return o; throw new Error('vdf text: unexpected end'); }
            if (k.brace === '}') { if (top) throw new Error('vdf text: stray }'); return o; }
            if (k.brace) throw new Error('vdf text: unexpected {');
            const v = token();
            if (!v) throw new Error('vdf text: key without value');
            o[k.s] = v.brace === '{' ? obj(false) : v.s;
        }
    }
    return obj(true);
}

module.exports = { T, parseBinary, serializeBinary, parseText, get, val, set };
