/* The game's model, skeleton, animation, scene and texture files, read in
 * the launcher for the MODS section (the viewer and the exporters). Each
 * reader follows the format document the project derived from the binary,
 * and the Python tool that validated it against every shipped file:
 *
 *   .meshset            docs/formats/MESHSET-FORMAT.md   tools/decomp/meshset.py
 *   .skin .bones
 *   .skinanim           docs/formats/SKIN-FORMAT.md      tools/decomp/skin.py
 *   .scene              docs/formats/SCENE-FORMAT.md     tools/decomp/scene.py
 *   .pvr                docs/formats/PVR-FORMAT.md       runtime/lime/pvr.c
 *   framelists/*.txt    docs/formats/FRAMELISTS.md       tools/decomp/animate.py
 *
 * Plain functions over Uint8Arrays: no DOM, no Node, so the same code runs
 * in the window and in the tests. */

const dv = b => new DataView(b.buffer, b.byteOffset, b.byteLength);

function cstr(b, off, len) {
    let s = '';
    for (let i = 0; i < len && b[off + i]; i++) s += String.fromCharCode(b[off + i]);
    return s;
}

/* ------------------------------------------------------------ .meshset --
 * Three variants, told apart by which one walks to the exact end of the
 * geometry (the exporter's text block after it, or the end of the file):
 *   A  int16 positions over the mesh's own radius, indexed (characters)
 *   B  float positions, indexed (stages)
 *   C  float positions, triangles already expanded (ROBO1/ROBO2) */
const TAIL_MARK = '//=====================================================\n// Ptr to each MESHINFO';

function geometryEnd(b) {
    const m = TAIL_MARK;
    outer: for (let i = 0; i + m.length <= b.length; i++) {
        if (b[i] !== 0x2f || b[i + 1] !== 0x2f) continue;
        for (let k = 2; k < m.length; k++) if (b[i + k] !== m.charCodeAt(k)) continue outer;
        return i;
    }
    return b.length;
}

function meshsetAs(b, variant) {
    const d = dv(b);
    const num = d.getInt32(0, true);
    if (num < 0 || num > 100000) throw new Error('mesh count');
    const hsize = variant === 'A' ? 140 : 136, vsize = variant === 'A' ? 26 : 20;
    let off = 4;
    const meshes = [];
    for (let n = 0; n < num; n++) {
        if (off + hsize > b.length) throw new Error('truncated');
        const name = cstr(b, off, 64), texture = cstr(b, off + 64, 64);
        const numVerts = d.getInt32(off + 128, true), numFaces = d.getInt32(off + 132, true);
        const radius = variant === 'A' ? d.getFloat32(off + 136, true) : 0;
        off += hsize;
        if (numVerts < 0 || numFaces < 0) throw new Error('negative');
        let faces, nv;
        if (variant !== 'C') {
            if (off + numFaces * 6 > b.length) throw new Error('truncated');
            faces = new Uint32Array(numFaces * 3);
            for (let i = 0; i < numFaces * 3; i++) faces[i] = d.getUint16(off + i * 2, true);
            off += numFaces * 6;
            nv = numVerts;
        } else {
            nv = numFaces * 3;
            faces = new Uint32Array(nv);
            for (let i = 0; i < nv; i++) faces[i] = i;
        }
        if (off + nv * vsize > b.length) throw new Error('truncated');
        const pos = new Float32Array(nv * 3), uv = new Float32Array(nv * 2);
        const div = radius !== 0 ? radius : 1;
        for (let i = 0; i < nv; i++, off += vsize) {
            if (variant === 'A') {
                pos[i * 3] = d.getInt16(off, true) / div;
                pos[i * 3 + 1] = d.getInt16(off + 2, true) / div;
                pos[i * 3 + 2] = d.getInt16(off + 4, true) / div;
                uv[i * 2] = d.getFloat32(off + 6, true);
                uv[i * 2 + 1] = d.getFloat32(off + 10, true);
            } else {
                pos[i * 3] = d.getFloat32(off, true);
                pos[i * 3 + 1] = d.getFloat32(off + 4, true);
                pos[i * 3 + 2] = d.getFloat32(off + 8, true);
                uv[i * 2] = d.getFloat32(off + 12, true);
                uv[i * 2 + 1] = d.getFloat32(off + 16, true);
            }
        }
        for (let i = 0; i < faces.length; i++) if (faces[i] >= nv) throw new Error('index');
        meshes.push({ name, texture: texStem(texture), pos, uv, faces, radius, variant });
    }
    return { meshes, end: off };
}

export function parseMeshset(b) {
    if (!b || b.length < 4) return [];
    const want = geometryEnd(b);
    for (const v of ['A', 'B', 'C']) {
        try {
            const r = meshsetAs(b, v);
            if (r.end === want) return r.meshes;
        } catch { /* the next variant */ }
    }
    throw new Error('not a .meshset this reader knows');
}

/* "KANO_DIFFUSE.???" -> "KANO_DIFFUSE"; NOTEXTURE -> '' */
export function texStem(t) {
    if (!t || /^NOTEXTURE$/i.test(t)) return '';
    const dot = t.lastIndexOf('.');
    return dot > 0 ? t.slice(0, dot) : t;
}

/* --------------------------------------------------------------- .skin --
 * One or two blocks. Per vertex: four packed bone bytes (0xFF unused), four
 * 16-bit weights, then the vertex once per influence in that bone's own
 * space, pre-multiplied by the weight (A) and the normal the same way (B).
 * Then, per triangle, three UV pairs and three vertex indices.
 * ROBO1/ROBO2 have no leading block count. */
function skinBlock(b, off) {
    const d = dv(b);
    const nv = d.getInt32(off, true), nt = d.getInt32(off + 4, true);
    if (nv < 0 || nt < 0 || nv > 1e6 || nt > 1e6) throw new Error('counts');
    off += 8;
    const bones = new Uint8Array(nv * 4), weights = new Float32Array(nv * 4);
    for (let i = 0; i < nv; i++) {
        const w = d.getUint32(off + i * 4, true);
        for (let k = 0; k < 4; k++) bones[i * 4 + k] = (w >>> (8 * k)) & 0xff;
    }
    off += nv * 4;
    for (let i = 0; i < nv * 4; i++) weights[i] = d.getUint16(off + i * 2, true) / 65536;
    off += nv * 8;
    const a = new Float32Array(nv * 12), n = new Float32Array(nv * 12);
    for (let i = 0; i < nv; i++) {
        for (let k = 0; k < 12; k++) {
            a[i * 12 + k] = d.getFloat32(off + k * 4, true);
            n[i * 12 + k] = d.getFloat32(off + 48 + k * 4, true);
        }
        off += 96;
    }
    const triUv = new Float32Array(nt * 6);
    for (let i = 0; i < nt * 6; i++) triUv[i] = d.getFloat32(off + i * 4, true);
    off += nt * 24;
    const tri = new Uint32Array(nt * 3);
    for (let i = 0; i < nt * 3; i++) tri[i] = d.getUint16(off + i * 2, true);
    off += nt * 6;
    if (off > b.length) throw new Error('truncated');
    return { block: { numVerts: nv, numTris: nt, bones, weights, a, n, triUv, tri }, off };
}

export function parseSkin(b) {
    const d = dv(b);
    const count = d.getInt32(0, true);
    if (count >= 1 && count <= 8) {
        try {
            let off = 4;
            const blocks = [];
            for (let i = 0; i < count; i++) {
                const r = skinBlock(b, off);
                blocks.push(r.block);
                off = r.off;
            }
            if (off === b.length) return Object.assign(blocks, { counted: true });
        } catch { /* the bare variant */ }
    }
    const r = skinBlock(b, 0);
    if (r.off !== b.length) throw new Error('not a .skin this reader knows');
    return Object.assign([r.block], { counted: false });
}

/* -------------------------------------------------------------- .bones --
 * int32 count, then 25 bytes a bone (24 on ROBO1/ROBO2): a word, the offset
 * from the parent, and nine (eight) child indices, -1 empty. */
export function parseBones(b) {
    const d = dv(b);
    const num = d.getInt32(0, true);
    const rest = b.length - 4;
    const size = rest === num * 25 ? 25 : rest === num * 24 ? 24 : 0;
    if (num <= 0 || !size) throw new Error('not a .bones this reader knows');
    const offset = new Float32Array(num * 3), children = [], parent = new Int32Array(num).fill(-1);
    for (let i = 0; i < num; i++) {
        const o = 4 + i * size;
        offset[i * 3] = d.getFloat32(o + 4, true);
        offset[i * 3 + 1] = d.getFloat32(o + 8, true);
        offset[i * 3 + 2] = d.getFloat32(o + 12, true);
        const c = [];
        for (let k = 0; k < size - 16; k++) {
            const v = d.getInt8(o + 16 + k);
            if (v !== -1 && v < num) c.push(v);
        }
        children.push(c);
    }
    for (let i = 0; i < num; i++) for (const c of children[i]) parent[c] = i;
    const roots = [];
    for (let i = 0; i < num; i++) if (parent[i] < 0) roots.push(i);
    if (roots.length !== 1) throw new Error('a skeleton with ' + roots.length + ' roots');
    /* depth-first, the order the animation frames store the bones in */
    const order = [];
    (function walk(i) { order.push(i); for (const c of children[i]) walk(c); })(roots[0]);
    return { num, offset, children, parent, root: roots[0], order };
}

/* ----------------------------------------------------------- .skinanim --
 * float scale, int32 frames, int32 frameSize (Sindel: one more float in
 * front, and the file holds more frames than the count says -- the size is
 * trusted). A frame: a word, the root's position, then 20 bytes a bone in
 * depth-first order, the first 16 a quaternion x y z w. */
export function parseAnim(b) {
    const d = dv(b);
    let hdr = 12, frames = d.getInt32(4, true), fsize = d.getInt32(8, true);
    if (!(frames >= 0 && fsize >= 16 && 12 + frames * fsize === b.length)) {
        hdr = 16;
        fsize = d.getInt32(12, true);
        if (fsize < 16 || (b.length - 16) % fsize) throw new Error('not a .skinanim this reader knows');
        frames = (b.length - 16) / fsize;
    }
    const nb = Math.floor((fsize - 16) / 20);
    const root = new Float32Array(frames * 3), quat = new Float32Array(frames * nb * 4);
    for (let f = 0; f < frames; f++) {
        const base = hdr + f * fsize;
        for (let k = 0; k < 3; k++) root[f * 3 + k] = d.getFloat32(base + 4 + k * 4, true);
        for (let i = 0; i < nb; i++)
            for (let k = 0; k < 4; k++)
                quat[(f * nb + i) * 4 + k] = d.getFloat32(base + 16 + i * 20 + k * 4, true);
    }
    return { frames, bones: nb, root, quat };
}

/* ---------------------------------------------------------- framelists --
 * Line N+1 names frame N. Clips are runs of names sharing a stem and
 * counting up (KNWALK1, KNWALK2, ...); padding lines (x, xx2) are not. */
export function parseFrameList(text) {
    return text.split(/\r?\n/).map(l => l.split('//')[0].trim());
}

export function findClips(names, frames) {
    const clips = [];
    let cur = null, start = 0;
    const n = Math.min(names.length, frames);
    for (let i = 0; i <= n; i++) {
        const m = i < n ? /^(.*?)(\d+)$/.exec(names[i]) : null;
        let stem = m ? m[1] : null;
        if (stem !== null && /^x+$/i.test(stem)) stem = null;
        if (stem !== cur) {
            if (cur && i - start >= 2) clips.push({ name: cur.replace(/_+$/, ''), from: start, to: i - 1 });
            cur = stem;
            start = i;
        }
    }
    /* the same stem twice (KNWALK at two places): told apart */
    const seen = {};
    for (const c of clips) {
        seen[c.name] = (seen[c.name] || 0) + 1;
        if (seen[c.name] > 1) c.name += '_' + seen[c.name];
    }
    return clips;
}

/* -------------------------------------------------------------- .scene --
 * Objects (a 64-byte record starting with its name, then one 12-byte key a
 * frame: alpha, a float, the palette index), then the palette: 40 bytes a
 * transform, quaternion x y z w, scale, translation. */
export function parseScene(b) {
    const d = dv(b);
    if (b.length < 8) throw new Error('short');
    const numObj = d.getInt32(0, true), frames = d.getInt32(4, true);
    if (numObj < 0 || frames < 0) throw new Error('counts');
    let off = 8;
    const objects = [];
    for (let i = 0; i < numObj; i++) {
        if (off + 64 + frames * 12 > b.length) throw new Error('truncated');
        const name = cstr(b, off, 64);
        const keys = [];
        for (let f = 0; f < frames; f++) {
            const k = off + 64 + f * 12;
            /* +4: the bone of the fighter's skin the object rides (Kung Lao's
             * hat on bone 7, the head; RenderScene.c scene_attach_to_bone), 0 none */
            keys.push({ alpha: d.getFloat32(k, true), bone: b[k + 4], pal: d.getUint16(k + 8, true) });
        }
        objects.push({ name, keys });
        off += 64 + frames * 12;
    }
    const count = d.getInt32(off, true);
    off += 4;
    if (count < 0 || off + count * 40 !== b.length) throw new Error('not a .scene this reader knows');
    const palette = [];
    for (let i = 0; i < count; i++, off += 40) {
        const v = [];
        for (let k = 0; k < 10; k++) v.push(d.getFloat32(off + k * 4, true));
        palette.push({ q: v.slice(0, 4), s: v.slice(4, 7), t: v.slice(7, 10) });
    }
    return { frames, objects, palette };
}

/* ---------------------------------------------------------------- .pvr --
 * The legacy 52-byte PVR v2 header; the pixels PVRTC 4bpp (0x19) or 2bpp
 * (0x18), decoded as runtime/lime/pvr.c decodes them for the game. */
function colourA(c, o) {
    if (c & 0x8000) {
        o[0] = (c & 0x7c00) >>> 10; o[1] = (c & 0x3e0) >>> 5;
        o[2] = (c & 0x1e) | ((c & 0x1e) >>> 4); o[3] = 0xf;
    } else {
        o[0] = ((c & 0xf00) >>> 7) | ((c & 0xf00) >>> 11);
        o[1] = ((c & 0xf0) >>> 3) | ((c & 0xf0) >>> 7);
        o[2] = ((c & 0xe) << 1) | ((c & 0xe) >>> 2);
        o[3] = (c & 0x7000) >>> 11;
    }
}

function colourB(c, o) {
    if (c & 0x80000000) {
        o[0] = (c & 0x7c000000) >>> 26; o[1] = (c & 0x3e00000) >>> 21;
        o[2] = (c & 0x1f0000) >>> 16; o[3] = 0xf;
    } else {
        o[0] = ((c & 0xf000000) >>> 23) | ((c & 0xf000000) >>> 27);
        o[1] = ((c & 0xf00000) >>> 19) | ((c & 0xf00000) >>> 23);
        o[2] = ((c & 0xf0000) >>> 15) | ((c & 0xf0000) >>> 19);
        o[3] = (c & 0x70000000) >>> 27;
    }
}

function morton(x, y, w, h) {
    const n = Math.min(w, h);
    let idx = 0, shift = 0;
    for (let k = n; k > 1; k >>= 1) {
        idx |= ((y & 1) << shift) | ((x & 1) << (shift + 1));
        x >>= 1; y >>= 1; shift += 2;
    }
    return idx | ((w > h ? x : y) << shift);
}

const wrap = (v, n) => (n <= 0 ? 0 : ((v % n) + n) % n);
const MOD_DIRECT = [0, 3, 5, 8], MOD_PUNCHT = [0, 4, 4 | 0x10, 8];

function decodePvrtc(raw, width, height, two) {
    const bw = two ? 8 : 4, bh = 4;
    const bx = Math.max(1, Math.ceil(width / bw)), by = Math.max(1, Math.ceil(height / bh));
    if (raw.length < bx * by * 8) return null;
    const d = dv(raw);
    const nb = bx * by, gw = bx * bw, gh = by * bh;
    const ca = new Int32Array(nb * 4), cb = new Int32Array(nb * 4), mod = new Uint8Array(gw * gh);
    const tmp = [0, 0, 0, 0];
    for (let j = 0; j < by; j++)
        for (let i = 0; i < bx; i++) {
            const off = morton(i, j, bx, by) * 8;
            let m = d.getUint32(off, true) >>> 0;
            const c = d.getUint32(off + 4, true) >>> 0;
            const k = j * bx + i, flag = c & 1;
            colourA(c, tmp); ca.set(tmp, k * 4);
            colourB(c, tmp); cb.set(tmp, k * 4);
            let how = 0;
            if (two && flag) {
                if (m & 1) {
                    how = (m & (1 << 20)) ? 2 : 1;
                    if (m & (1 << 21)) m |= (1 << 20); else m &= ~(1 << 20);
                }
                if (m & 2) m |= 1; else m &= ~1;
                m >>>= 0;
            }
            for (let y = 0; y < bh; y++) {
                const row = (j * bh + y) * gw + i * bw;
                for (let x = 0; x < bw; x++) {
                    let v;
                    if (!two) {
                        const sel = (m >>> ((y * 4 + x) * 2)) & 3;
                        v = flag ? MOD_PUNCHT[sel] : MOD_DIRECT[sel];
                    } else if (!flag) {
                        v = ((m >>> (y * 8 + x)) & 1) ? 8 : 0;
                    } else if (((x ^ y) & 1) === 0) {
                        v = MOD_DIRECT[m & 3];
                        m >>>= 2;
                    } else v = 0x20 | how;
                    mod[row + x] = v;
                }
            }
        }
    if (two)
        for (let y = 0; y < gh; y++)
            for (let x = 0; x < gw; x++) {
                const p = y * gw + x;
                if (!(mod[p] & 0x20)) continue;
                const l = mod[y * gw + wrap(x - 1, gw)] & 0xf, r = mod[y * gw + wrap(x + 1, gw)] & 0xf;
                const u = mod[wrap(y - 1, gh) * gw + x] & 0xf, dn = mod[wrap(y + 1, gh) * gw + x] & 0xf;
                const how = mod[p] & 3;
                mod[p] = how === 1 ? (l + r + 1) >> 1 : how === 2 ? (u + dn + 1) >> 1 : (l + r + u + dn + 2) >> 2;
            }
    const rgba = new Uint8ClampedArray(width * height * 4);
    for (let py = 0; py < height; py++)
        for (let px = 0; px < width; px++) {
            const fx = (px - (bw >> 1)) / bw, fy = (py - (bh >> 1)) / bh;
            let i0 = Math.floor(fx), j0 = Math.floor(fy);
            const u = fx - i0, v = fy - j0;
            i0 = wrap(i0, bx); j0 = wrap(j0, by);
            const i1 = wrap(i0 + 1, bx), j1 = wrap(j0 + 1, by);
            const k00 = (j0 * bx + i0) * 4, k10 = (j0 * bx + i1) * 4, k01 = (j1 * bx + i0) * 4, k11 = (j1 * bx + i1) * 4;
            const w00 = (1 - u) * (1 - v), w10 = u * (1 - v), w01 = (1 - u) * v, w11 = u * v;
            const mv = mod[py * gw + px], w = (mv & 0xf) / 8;
            const o = (py * width + px) * 4;
            for (let ch = 0; ch < 4; ch++) {
                const A = ca[k00 + ch] * w00 + ca[k10 + ch] * w10 + ca[k01 + ch] * w01 + ca[k11 + ch] * w11;
                const B = cb[k00 + ch] * w00 + cb[k10 + ch] * w10 + cb[k01 + ch] * w01 + cb[k11 + ch] * w11;
                rgba[o + ch] = (A * (1 - w) + B * w) * (ch < 3 ? 255 / 31 : 255 / 15) + 0.5;
            }
            if (mv & 0x10) rgba[o + 3] = 0;
        }
    return rgba;
}

/* { width, height, rgba } or null for a format the game does not use */
export function decodePvr(b) {
    if (!b || b.length < 52) return null;
    const d = dv(b);
    const hlen = d.getUint32(0, true), height = d.getUint32(4, true), width = d.getUint32(8, true);
    const fmt = d.getUint32(16, true) & 0xff, tag = d.getUint32(44, true);
    if (tag !== 0x21525650 || hlen !== 52) return null;
    if (fmt !== 0x18 && fmt !== 0x19) return null;
    const rgba = decodePvrtc(b.subarray(52), width, height, fmt === 0x18);
    return rgba ? { width, height, rgba } : null;
}
