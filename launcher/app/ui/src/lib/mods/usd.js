/* USD, three ways: .usda (text), .usdc (Pixar's binary "crate") and .usdz
 * (a zip holding the .usdc and its pictures, for viewers that want one file).
 *
 * The scene is built once as a small tree of prims, attributes and
 * relationships (stageOf), then written either way. The crate writer is
 * written from the format as Pixar's own library writes it (version 0.8.0)
 * and checked against it: every file it makes is read back by pxr 26.03 and
 * by Blender's importer (tests/launcher: see docs/PROGRESS.md).
 *
 * The crate, briefly: an 88-byte header (PXR-USDC, the version, where the
 * table of contents is), the values, then six sections -- TOKENS (every name
 * and string, LZ4), STRINGS (which tokens are strings), FIELDS (a name and a
 * value reference each), FIELDSETS (each spec's fields, -1 ends one), PATHS
 * (the prim and property tree, depth first, with jumps to siblings) and
 * SPECS (path, field set, kind) -- and the table of contents. Integer tables
 * are delta coded (a common delta and 2-bit width codes, then the rest) and
 * LZ4 compressed; this writer emits LZ4 blocks of literals only, which every
 * LZ4 reader accepts. Arrays are stored plain. */
import { localOf, matToQuat } from './rig.js';

const enc = new TextEncoder();

/* ------------------------------------------------------------ the tree --
 * prim: { name, type, api: [], children: [], props: [] }
 * prop: { name, type ('point3f[]' ...), value?, samples?: [[t, value]],
 *         uniform?, meta?: { interpolation, elementSize }, connect?: path,
 *         targets?: [path] (a relationship) } */

const ident = s => {
    let r = s.replace(/[^A-Za-z0-9_]/g, '_');
    if (!/^[A-Za-z_]/.test(r)) r = '_' + r;
    return r;
};

const prim = (name, type, extra = {}) => Object.assign({ name: ident(name), type, api: [], children: [], props: [] }, extra);

const mat4 = ({ R, T }) => [R[0], R[1], R[2], 0, R[3], R[4], R[5], 0, R[6], R[7], R[8], 0, T[0], T[1], T[2], 1];

/* parents before children: the order USD requires of a skeleton */
function boneOrder(sk) {
    const kids = sk.parent.map(() => []);
    const roots = [];
    sk.parent.forEach((p, i) => (p < 0 ? roots.push(i) : kids[p].push(i)));
    const order = [];
    const walk = i => { order.push(i); kids[i].forEach(walk); };
    roots.forEach(walk);
    return order;
}

export function stageOf(desc) {
    const rootName = ident(desc.name);
    const sk = desc.skeleton, clips = desc.clips || [];
    const frames = clips.reduce((a, c) => a + c.poses.length, 0);
    const layer = { defaultPrim: rootName, metersPerUnit: 0.01, upAxis: 'Y', doc: 'Ultimate Mortal Kombat 3 -- exported by the UMK3 PC launcher' };
    if (frames) {
        Object.assign(layer, { start: 0, end: frames - 1, fps: desc.fps });
        let at = 0;
        layer.doc += '. Animations on the timeline: ' + clips.map(c => {
            const s = c.name + ' ' + at + '-' + (at + c.poses.length - 1);
            at += c.poses.length;
            return s;
        }).join(', ');
    }
    const root = prim(desc.name, sk ? 'SkelRoot' : 'Xform');
    const base = '/' + rootName;

    let slot = [], paths = [], order = [];
    if (sk) {
        order = boneOrder(sk);
        slot = new Array(sk.parent.length);
        order.forEach((b, k) => { slot[b] = k; });
        const pathOf = b => (sk.parent[b] < 0 ? ident(sk.names[b]) : pathOf(sk.parent[b]) + '/' + ident(sk.names[b]));
        paths = order.map(pathOf);
        const skel = prim('Skel', 'Skeleton', { api: ['SkelBindingAPI'] });
        skel.props.push({ name: 'joints', type: 'token[]', value: paths, uniform: true });
        skel.props.push({ name: 'bindTransforms', type: 'matrix4d[]', value: order.map(b => mat4(sk.rest[b])), uniform: true });
        skel.props.push({ name: 'restTransforms', type: 'matrix4d[]', value: order.map(b => mat4(localOf(sk.rest, { parent: sk.parent }, b))), uniform: true });
        if (frames) {
            skel.props.push({ name: 'skel:animationSource', targets: [base + '/Skel/Anim'] });
            const anim = prim('Anim', 'SkelAnimation');
            anim.props.push({ name: 'joints', type: 'token[]', value: paths, uniform: true });
            const tr = [], rot = [];
            let t = 0;
            for (const c of clips)
                for (const pose of c.poses) {
                    const loc = order.map(b => localOf(pose, { parent: sk.parent }, b));
                    tr.push([t, loc.map(x => x.T)]);
                    rot.push([t, loc.map(x => matToQuat(x.R))]);   /* x y z w */
                    t++;
                }
            anim.props.push({ name: 'translations', type: 'float3[]', samples: tr });
            anim.props.push({ name: 'rotations', type: 'quatf[]', samples: rot });
            anim.props.push({ name: 'scales', type: 'half3[]', value: order.map(() => [1, 1, 1]) });
            skel.children.push(anim);
        }
        root.children.push(skel);
    }

    for (const m of desc.meshes) {
        const n = m.pos.length / 3;
        const mesh = prim(m.name, 'Mesh', { api: sk && m.skin ? ['SkelBindingAPI', 'MaterialBindingAPI'] : ['MaterialBindingAPI'] });
        const P = [], ST = [];
        for (let i = 0; i < n; i++) {
            P.push([m.pos[i * 3], m.pos[i * 3 + 1], m.pos[i * 3 + 2]]);
            ST.push([m.uv[i * 2], 1 - m.uv[i * 2 + 1]]);
        }
        mesh.props.push({ name: 'doubleSided', type: 'bool', value: true, uniform: true });
        mesh.props.push({ name: 'faceVertexCounts', type: 'int[]', value: new Array(m.faces.length / 3).fill(3) });
        mesh.props.push({ name: 'faceVertexIndices', type: 'int[]', value: Array.from(m.faces) });
        mesh.props.push({ name: 'points', type: 'point3f[]', value: P });
        mesh.props.push({ name: 'primvars:st', type: 'texCoord2f[]', value: ST, meta: { interpolation: 'vertex' } });
        mesh.props.push({ name: 'subdivisionScheme', type: 'token', value: 'none', uniform: true });
        if (sk && m.skin) {
            mesh.props.push({ name: 'primvars:skel:jointIndices', type: 'int[]', value: Array.from(m.skin.idx, b => slot[b]), meta: { elementSize: 4, interpolation: 'vertex' } });
            mesh.props.push({ name: 'primvars:skel:jointWeights', type: 'float[]', value: Array.from(m.skin.wt), meta: { elementSize: 4, interpolation: 'vertex' } });
            mesh.props.push({ name: 'primvars:skel:geomBindTransform', type: 'matrix4d', value: [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1] });
            mesh.props.push({ name: 'skel:skeleton', targets: [base + '/Skel'] });
        }
        mesh.props.push({ name: 'material:binding', targets: [base + '/Materials/' + ident(desc.materials[m.material].name)] });
        root.children.push(mesh);
    }

    const mats = prim('Materials', 'Scope');
    for (const mat of desc.materials) {
        const mb = base + '/Materials/' + ident(mat.name);
        const M = prim(mat.name, 'Material');
        M.props.push({ name: 'outputs:surface', type: 'token', connect: mb + '/Surface.outputs:surface' });
        const surf = prim('Surface', 'Shader');
        surf.props.push({ name: 'info:id', type: 'token', value: 'UsdPreviewSurface', uniform: true });
        if (mat.texture) {
            surf.props.push({ name: 'inputs:diffuseColor', type: 'color3f', connect: mb + '/Picture.outputs:rgb' });
            surf.props.push({ name: 'inputs:opacity', type: 'float', connect: mb + '/Picture.outputs:a' });
        } else surf.props.push({ name: 'inputs:diffuseColor', type: 'color3f', value: [0.8, 0.8, 0.8] });
        surf.props.push({ name: 'inputs:roughness', type: 'float', value: 0.8 });
        surf.props.push({ name: 'outputs:surface', type: 'token' });
        M.children.push(surf);
        if (mat.texture) {
            const pic = prim('Picture', 'Shader');
            pic.props.push({ name: 'info:id', type: 'token', value: 'UsdUVTexture', uniform: true });
            pic.props.push({ name: 'inputs:file', type: 'asset', value: './' + mat.texture });
            pic.props.push({ name: 'inputs:st', type: 'float2', connect: mb + '/UV.outputs:result' });
            pic.props.push({ name: 'inputs:wrapS', type: 'token', value: 'repeat' });
            pic.props.push({ name: 'inputs:wrapT', type: 'token', value: 'repeat' });
            pic.props.push({ name: 'outputs:rgb', type: 'float3' });
            pic.props.push({ name: 'outputs:a', type: 'float' });
            const uv = prim('UV', 'Shader');
            uv.props.push({ name: 'info:id', type: 'token', value: 'UsdPrimvarReader_float2', uniform: true });
            uv.props.push({ name: 'inputs:varname', type: 'string', value: 'st' });
            uv.props.push({ name: 'outputs:result', type: 'float2' });
            M.children.push(pic, uv);
        }
        mats.children.push(M);
    }
    root.children.push(mats);
    return { layer, root };
}

/* --------------------------------------------------------------- usda -- */

const num = x => (Math.abs(x) < 1e-9 ? '0' : (+(+x).toFixed(6)).toString());

function text(type, v) {
    const base = type.replace('[]', '');
    const one = x => {
        if (base === 'token' || base === 'string') return JSON.stringify(x);
        if (base === 'asset') return '@' + x + '@';
        if (base === 'bool') return x ? 'true' : 'false';
        if (base === 'matrix4d')
            return '( ' + [0, 1, 2, 3].map(r => '(' + x.slice(r * 4, r * 4 + 4).map(num).join(', ') + ')').join(', ') + ' )';
        if (base === 'quatf') return '(' + [x[3], x[0], x[1], x[2]].map(num).join(', ') + ')';
        if (Array.isArray(x)) return '(' + x.map(num).join(', ') + ')';
        return base === 'int' ? String(x | 0) : num(x);
    };
    return type.endsWith('[]') ? '[' + v.map(one).join(', ') + ']' : one(v);
}

function usdaPrim(p, ind, out) {
    const pad = '    '.repeat(ind);
    out.push(pad + 'def ' + p.type + ' "' + p.name + '"' + (p.api.length ? ' (' : ''));
    if (p.api.length) out.push(pad + '    prepend apiSchemas = [' + p.api.map(a => '"' + a + '"').join(', ') + ']', pad + ')');
    out.push(pad + '{');
    for (const a of p.props) {
        if (a.targets) {
            out.push(pad + '    rel ' + a.name + ' = ' + (a.targets.length === 1 ? '<' + a.targets[0] + '>' : '[' + a.targets.map(t => '<' + t + '>').join(', ') + ']'));
            continue;
        }
        const head = pad + '    ' + (a.uniform ? 'uniform ' : '') + a.type + ' ' + a.name;
        const meta = a.meta ? ' (\n' + Object.entries(a.meta).map(([k, v]) => pad + '        ' + k + ' = ' + (typeof v === 'string' ? '"' + v + '"' : v)).join('\n') + '\n' + pad + '    )' : '';
        if (a.connect) out.push(head + '.connect = <' + a.connect + '>');
        else if (a.samples) {
            out.push(head + '.timeSamples = {');
            for (const [t, v] of a.samples) out.push(pad + '        ' + t + ': ' + text(a.type, v) + ',');
            out.push(pad + '    }');
        } else if (a.value !== undefined) out.push(head + ' = ' + text(a.type, a.value) + meta);
        else out.push(head + meta);
    }
    for (const c of p.children) { out.push(''); usdaPrim(c, ind + 1, out); }
    out.push(pad + '}');
}

export function writeUsda(stage) {
    const L = stage.layer, out = ['#usda 1.0', '('];
    out.push('    defaultPrim = "' + L.defaultPrim + '"', '    doc = ' + JSON.stringify(L.doc), '    metersPerUnit = ' + L.metersPerUnit, '    upAxis = "' + L.upAxis + '"');
    if (L.fps) out.push('    startTimeCode = ' + L.start, '    endTimeCode = ' + L.end, '    timeCodesPerSecond = ' + L.fps, '    framesPerSecond = ' + L.fps);
    out.push(')', '');
    usdaPrim(stage.root, 0, out);
    out.push('');
    return enc.encode(out.join('\n'));
}

/* --------------------------------------------------------------- usdc -- */

class Buf {
    constructor() { this.b = new Uint8Array(1 << 20); this.n = 0; this.v = new DataView(this.b.buffer); }
    need(k) {
        if (this.n + k <= this.b.length) return;
        let c = this.b.length * 2;
        while (c < this.n + k) c *= 2;
        const nb = new Uint8Array(c); nb.set(this.b.subarray(0, this.n));
        this.b = nb; this.v = new DataView(nb.buffer);
    }
    u8(x) { this.need(1); this.b[this.n++] = x; }
    u32(x) { this.need(4); this.v.setUint32(this.n, x >>> 0, true); this.n += 4; }
    i32(x) { this.need(4); this.v.setInt32(this.n, x, true); this.n += 4; }
    u64(x) { this.need(8); this.v.setBigUint64(this.n, BigInt(x), true); this.n += 8; }
    i64(x) { this.need(8); this.v.setBigInt64(this.n, BigInt(x), true); this.n += 8; }
    f32(x) { this.need(4); this.v.setFloat32(this.n, x, true); this.n += 4; }
    f64(x) { this.need(8); this.v.setFloat64(this.n, x, true); this.n += 8; }
    f16(x) { this.need(2); this.v.setUint16(this.n, toHalf(x), true); this.n += 2; }
    bytes(a) { this.need(a.length); this.b.set(a, this.n); this.n += a.length; }
    rep(r) { this.need(8); this.v.setBigUint64(this.n, r, true); this.n += 8; }
    done() { return this.b.slice(0, this.n); }
}

function toHalf(x) {
    const f = new Float32Array([x]), u = new Uint32Array(f.buffer)[0];
    const s = (u >>> 16) & 0x8000, e = ((u >>> 23) & 0xff) - 127 + 15, m = u & 0x7fffff;
    if (e <= 0) return s;
    if (e >= 31) return s | 0x7c00;
    return s | (e << 10) | (m >>> 13);
}

/* LZ4 frame as TfFastCompression wants it: a 0 (one chunk) and one LZ4 block
 * of literals only */
function fastCompress(data) {
    const out = [0];
    const n = data.length;
    if (n >= 15) {
        out.push(0xf0);
        let k = n - 15;
        while (k >= 255) { out.push(255); k -= 255; }
        out.push(k);
    } else out.push(n << 4);
    const r = new Uint8Array(out.length + n);
    r.set(out, 0);
    r.set(data, out.length);
    return r;
}

/* the integer coding: a common delta, 2-bit codes (0 common, 1 int8, 2
 * int16, 3 int32), the other deltas */
function encodeInts(vals) {
    const d = new Array(vals.length);
    let prev = 0;
    for (let i = 0; i < vals.length; i++) { d[i] = vals[i] - prev; prev = vals[i]; }
    const count = new Map();
    for (const x of d) count.set(x, (count.get(x) || 0) + 1);
    let common = 0, best = -1;
    for (const [x, c] of count) if (c > best || (c === best && x < common)) { best = c; common = x; }
    const b = new Buf();
    b.i32(common);
    const codes = new Uint8Array(Math.ceil(vals.length * 2 / 8));
    const rest = new Buf();
    d.forEach((x, i) => {
        let c;
        if (x === common) c = 0;
        else if (x >= -128 && x <= 127) { c = 1; rest.need(1); rest.v.setInt8(rest.n++, x); }
        else if (x >= -32768 && x <= 32767) { c = 2; rest.need(2); rest.v.setInt16(rest.n, x, true); rest.n += 2; }
        else { c = 3; rest.i32(x); }
        codes[i >> 2] |= c << (2 * (i & 3));
    });
    b.bytes(codes);
    b.bytes(rest.done());
    return b.done();
}

function writeInts(b, vals) {
    const z = fastCompress(encodeInts(vals));
    b.u64(z.length);
    b.bytes(z);
}

const TYPE = {
    bool: 1, int: 3, float: 8, double: 9, string: 10, token: 11, asset: 12, matrix4d: 15, quatf: 17,
    float2: 20, texCoord2f: 20, float3: 24, point3f: 24, color3f: 24, half3: 25,
    tokenVector: 41, specifier: 42, variability: 44, pathListOp: 34, tokenListOp: 32, doubleVector: 48, timeSamples: 46,
};
const ARRAY = 1n << 63n, INLINE = 1n << 62n;
const repOf = (type, payload, flags = 0n) => flags | (BigInt(type) << 48n) | BigInt(payload);

export function writeUsdc(stage) {
    const tokens = [''], tokIdx = new Map([['', 0]]);
    const tok = s => {
        if (!tokIdx.has(s)) { tokIdx.set(s, tokens.length); tokens.push(s); }
        return tokIdx.get(s);
    };
    const strings = [], strIdx = new Map();
    const str = s => {
        if (!strIdx.has(s)) { strIdx.set(s, strings.length); strings.push(tok(s)); }
        return strIdx.get(s);
    };
    const data = new Buf();
    data.bytes(new Uint8Array(88));                     /* the header, filled in last */

    /* ---- the path tree: every prim and property, depth first */
    const nodes = [];                                   /* { path, elem, isProp, parent, kids } */
    const pathIndex = new Map();
    const addPath = (path, elem, isProp, parent) => {
        const n = { path, elem, isProp, kids: [], index: nodes.length };
        nodes.push(n);
        pathIndex.set(path, n.index);
        if (parent) parent.kids.push(n);
        return n;
    };
    const top = addPath('/', '', false, null);
    const walkPaths = (p, parentPath, parentNode) => {
        const path = (parentPath === '/' ? '' : parentPath) + '/' + p.name;
        const node = addPath(path, p.name, false, parentNode);
        p._path = path;
        for (const a of p.props) addPath(path + '.' + a.name, a.name, true, node);
        for (const c of p.children) walkPaths(c, path, node);
    };
    walkPaths(stage.root, '/', top);

    /* ---- values */
    const arrayOf = (type, vals) => {
        const at = data.n;
        data.u64(vals.length);
        const base = type.replace('[]', '');
        for (const v of vals) {
            if (base === 'int') data.i32(v);
            else if (base === 'float') data.f32(v);
            else if (base === 'token') data.u32(tok(v));
            else if (base === 'matrix4d') v.forEach(x => data.f64(x));
            else if (base === 'quatf') v.forEach(x => data.f32(x));          /* x y z w, as GfQuatf lies in memory */
            else if (base === 'half3') v.forEach(x => data.f16(x));
            else v.forEach(x => data.f32(x));                               /* float2/3 kinds */
        }
        const name = base === 'point3f' || base === 'color3f' ? 'float3' : base;
        return repOf(TYPE[name] ?? TYPE.float3, at, ARRAY);
    };
    const single = (type, v) => {
        switch (type) {
            case 'bool': return repOf(TYPE.bool, v ? 1 : 0, INLINE);
            case 'int': return repOf(TYPE.int, v >>> 0, INLINE);
            case 'float': { const f = new DataView(new ArrayBuffer(4)); f.setFloat32(0, v, true); return repOf(TYPE.float, f.getUint32(0, true), INLINE); }
            case 'token': return repOf(TYPE.token, tok(v), INLINE);
            case 'string': return repOf(TYPE.string, str(v), INLINE);
            case 'asset': return repOf(TYPE.asset, tok(v), INLINE);
            case 'double': { const at = data.n; data.f64(v); return repOf(TYPE.double, at); }
            case 'matrix4d': { const at = data.n; v.forEach(x => data.f64(x)); return repOf(TYPE.matrix4d, at); }
            default: { const at = data.n; v.forEach(x => data.f32(x)); return repOf(TYPE[type] ?? TYPE.float3, at); }
        }
    };
    const valueOf = (type, v) => (type.endsWith('[]') ? arrayOf(type, v) : single(type, v));
    const tokenVector = list => { const at = data.n; data.u64(list.length); list.forEach(s => data.u32(tok(s))); return repOf(TYPE.tokenVector, at); };
    const pathListOp = paths => {
        const at = data.n;
        data.u8(3);                                     /* explicit, with items */
        data.u64(paths.length);
        paths.forEach(p => {
            if (!pathIndex.has(p)) throw new Error('USD path not in the file: ' + p);
            data.u32(pathIndex.get(p));
        });
        return repOf(TYPE.pathListOp, at);
    };
    const tokenListOp = list => { const at = data.n; data.u8(32); data.u64(list.length); list.forEach(s => data.u32(tok(s))); return repOf(TYPE.tokenListOp, at); };
    const timeSamples = (type, samples) => {
        const reps = samples.map(([, v]) => valueOf(type, v));
        const at = data.n;
        data.i64(8 + 8 + 8 * samples.length);           /* to the times' reference */
        const timesAt = data.n;
        data.u64(samples.length);
        samples.forEach(([t]) => data.f64(t));
        data.rep(repOf(TYPE.doubleVector, timesAt));
        data.i64(8);                                    /* to the values */
        data.u64(samples.length);
        reps.forEach(r => data.rep(r));
        return repOf(TYPE.timeSamples, at);
    };

    /* ---- fields, field sets, specs */
    const fields = [], fieldIdx = new Map();
    const field = (name, rep) => {
        const k = name + ':' + rep;
        if (!fieldIdx.has(k)) { fieldIdx.set(k, fields.length); fields.push([tok(name), rep]); }
        return fieldIdx.get(k);
    };
    const fsets = [];
    const specs = [];                                   /* [path index, field set start, type] */
    const spec = (path, kind, list) => {
        specs.push([pathIndex.get(path), fsets.length, kind]);
        fsets.push(...list, -1);
    };
    const L = stage.layer;
    const rootFields = [field('defaultPrim', single('token', L.defaultPrim)), field('metersPerUnit', single('double', L.metersPerUnit)),
        field('upAxis', single('token', L.upAxis))];
    if (L.fps) rootFields.push(field('startTimeCode', single('double', L.start)), field('endTimeCode', single('double', L.end)),
        field('timeCodesPerSecond', single('double', L.fps)), field('framesPerSecond', single('double', L.fps)));
    rootFields.push(field('documentation', single('string', L.doc)), field('primChildren', tokenVector([stage.root.name])));
    spec('/', 7, rootFields);

    const SPEC_PRIM = 6, SPEC_ATTR = 1, SPEC_REL = 8;
    const walkSpecs = p => {
        const f = [field('specifier', repOf(TYPE.specifier, 0, INLINE)), field('typeName', single('token', p.type))];
        if (p.api.length) f.push(field('apiSchemas', tokenListOp(p.api)));
        if (p.children.length) f.push(field('primChildren', tokenVector(p.children.map(c => c.name))));
        if (p.props.length) f.push(field('properties', tokenVector(p.props.map(a => a.name))));
        spec(p._path, SPEC_PRIM, f);
        for (const a of p.props) {
            const path = p._path + '.' + a.name;
            if (a.targets) {
                spec(path, SPEC_REL, [field('variability', repOf(TYPE.variability, 1, INLINE)), field('targetPaths', pathListOp(a.targets))]);
                continue;
            }
            const af = [field('custom', single('bool', false)), field('typeName', single('token', a.type)),
                field('variability', repOf(TYPE.variability, a.uniform ? 1 : 0, INLINE))];
            if (a.connect) af.push(field('connectionPaths', pathListOp([a.connect])));
            if (a.value !== undefined) af.push(field('default', valueOf(a.type, a.value)));
            if (a.samples) af.push(field('timeSamples', timeSamples(a.type, a.samples)));
            if (a.meta)
                for (const [k, v] of Object.entries(a.meta))
                    af.push(field(k, typeof v === 'number' ? single('int', v) : single('token', v)));
            spec(path, SPEC_ATTR, af);
        }
        p.children.forEach(walkSpecs);
    };
    walkSpecs(stage.root);

    /* ---- the sections */
    const sections = [];
    const section = (name, fill) => {
        const start = data.n;
        fill(data);
        sections.push([name, start, data.n - start]);
    };
    section('TOKENS', b => {
        const raw = enc.encode(tokens.join('\0') + '\0');
        const z = fastCompress(raw);
        b.u64(tokens.length); b.u64(raw.length); b.u64(z.length); b.bytes(z);
    });
    section('STRINGS', b => { b.u64(strings.length); strings.forEach(t => b.u32(t)); });
    section('FIELDS', b => {
        b.u64(fields.length);
        writeInts(b, fields.map(f => f[0]));
        const raw = new Buf();
        fields.forEach(f => raw.rep(f[1]));
        const z = fastCompress(raw.done());
        b.u64(z.length); b.bytes(z);
    });
    section('FIELDSETS', b => { b.u64(fsets.length); writeInts(b, fsets); });
    section('PATHS', b => {
        /* depth first: each node's index, its name (negative for a property),
         * and a jump: -1 a child follows and no sibling, 0 a sibling
         * follows and no child, -2 neither, n > 0 a child follows and the
         * next sibling is n entries on */
        const pi = [], ei = [], jumps = [];
        const emit = (n, last) => {
            const me = pi.length;
            pi.push(n.index);
            ei.push(n === top ? 0 : (n.isProp ? -tok(n.elem) : tok(n.elem)));
            jumps.push(0);
            const hasKids = n.kids.length > 0;
            n.kids.forEach((k, i) => emit(k, i === n.kids.length - 1));
            const sib = !last;
            jumps[me] = hasKids && sib ? pi.length - me : hasKids ? -1 : sib ? 0 : -2;
        };
        emit(top, true);
        b.u64(nodes.length); b.u64(pi.length);
        writeInts(b, pi); writeInts(b, ei); writeInts(b, jumps);
    });
    section('SPECS', b => {
        b.u64(specs.length);
        writeInts(b, specs.map(s => s[0])); writeInts(b, specs.map(s => s[1])); writeInts(b, specs.map(s => s[2]));
    });
    const toc = data.n;
    data.u64(sections.length);
    for (const [name, start, size] of sections) {
        const nb = new Uint8Array(16);
        nb.set(enc.encode(name));
        data.bytes(nb); data.i64(start); data.i64(size);
    }
    const out = data.done();
    out.set(enc.encode('PXR-USDC'), 0);
    out.set([0, 8, 0, 0, 0, 0, 0, 0], 8);
    new DataView(out.buffer).setBigInt64(16, BigInt(toc), true);
    return out;
}

/* --------------------------------------------------------------- usdz --
 * A zip, nothing compressed, every file's data 64-byte aligned (the
 * package's rule), the scene first. */
const CRC = (() => {
    const t = new Uint32Array(256);
    for (let n = 0; n < 256; n++) {
        let c = n;
        for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
        t[n] = c >>> 0;
    }
    return t;
})();
const crc32 = b => { let c = 0xffffffff; for (let i = 0; i < b.length; i++) c = CRC[(c ^ b[i]) & 0xff] ^ (c >>> 8); return (c ^ 0xffffffff) >>> 0; };

export function writeUsdz(entries) {
    const b = new Buf(), central = new Buf();
    let count = 0;
    for (const [name, bytes] of entries) {
        const nb = enc.encode(name);
        const at = b.n;
        const pad = (64 - ((at + 30 + nb.length + 4) % 64)) % 64;
        const crc = crc32(bytes);
        b.u32(0x04034b50); b.need(2); b.v.setUint16(b.n, 20, true); b.n += 2;
        for (const v of [0, 0, 0, 0]) { b.v.setUint16(b.n, v, true); b.n += 2; }      /* flags, stored, time, date */
        b.u32(crc); b.u32(bytes.length); b.u32(bytes.length);
        b.need(4); b.v.setUint16(b.n, nb.length, true); b.v.setUint16(b.n + 2, pad + 4, true); b.n += 4;
        b.bytes(nb);
        b.need(4); b.v.setUint16(b.n, 0x1986, true); b.v.setUint16(b.n + 2, pad, true); b.n += 4;   /* padding field */
        b.bytes(new Uint8Array(pad));
        b.bytes(bytes);
        central.u32(0x02014b50);
        central.need(4); central.v.setUint16(central.n, 20, true); central.v.setUint16(central.n + 2, 20, true); central.n += 4;
        for (const v of [0, 0, 0, 0]) { central.need(2); central.v.setUint16(central.n, v, true); central.n += 2; }
        central.u32(crc); central.u32(bytes.length); central.u32(bytes.length);
        central.need(10); [nb.length, 0, 0, 0, 0].forEach((v, i) => central.v.setUint16(central.n + i * 2, v, true)); central.n += 10;
        central.u32(0); central.u32(at);
        central.bytes(nb);
        count++;
    }
    const cdAt = b.n, cd = central.done();
    b.bytes(cd);
    b.u32(0x06054b50);
    b.need(8); [0, 0, count, count].forEach((v, i) => b.v.setUint16(b.n + i * 2, v, true)); b.n += 8;
    b.u32(cd.length); b.u32(cdAt);
    b.need(2); b.v.setUint16(b.n, 0, true); b.n += 2;
    return b.done();
}
