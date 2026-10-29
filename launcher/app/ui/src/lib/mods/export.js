/* The exporters: OBJ (+MTL), USD (.usda, UsdSkel), FBX (binary 7.4) and
 * GLB (glTF 2.0 binary, its pictures inside), and PNG for the textures.
 *
 * They all take one description of what to write:
 *
 *   {
 *     name,
 *     meshes:    [{ name, pos (rest, Float32 xyz), uv (Float32, the game's v
 *                   down from the top), faces (Uint32 triangles), material,
 *                   skin?: { idx, wt } four bones a vertex }],
 *     materials: [{ name, texture (a file name beside the export, or '') }],
 *     skeleton?: { names, parent, rest: [{ R, T }] }       global, row vectors
 *     clips?:    [{ name, poses: [[{ R, T }] a frame] }]   global
 *     fps
 *   }
 *
 * Units are the game's, which are centimetres (a fighter is ~180 tall); Y is
 * up and the fighters face +Z. Each format says so in its own header, and
 * Blender converts on import. */
import { tr3, mul3, vmul, localOf, matToQuat } from './rig.js';
import { stageOf, writeUsda, writeUsdc, writeUsdz } from './usd.js';

const enc = new TextEncoder();
const f6 = x => (Math.abs(x) < 1e-9 ? '0' : (+x.toFixed(6)).toString());

/* ---------------------------------------------------------------- PNG -- */

const CRC = (() => {
    const t = new Uint32Array(256);
    for (let n = 0; n < 256; n++) {
        let c = n;
        for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
        t[n] = c >>> 0;
    }
    return t;
})();

function crc32(b, c = 0xffffffff) {
    for (let i = 0; i < b.length; i++) c = CRC[(c ^ b[i]) & 0xff] ^ (c >>> 8);
    return c;
}

async function deflate(data) {
    const cs = new CompressionStream('deflate');
    const out = new Response(new Blob([data]).stream().pipeThrough(cs)).arrayBuffer();
    return new Uint8Array(await out);
}

export async function encodePng(width, height, rgba) {
    const raw = new Uint8Array((width * 4 + 1) * height);
    for (let y = 0; y < height; y++)
        raw.set(rgba.subarray(y * width * 4, (y + 1) * width * 4), y * (width * 4 + 1) + 1);
    const idat = await deflate(raw);
    const chunk = (type, data) => {
        const b = new Uint8Array(12 + data.length), d = new DataView(b.buffer);
        d.setUint32(0, data.length);
        b.set(enc.encode(type), 4);
        b.set(data, 8);
        d.setUint32(8 + data.length, (crc32(b.subarray(4, 8 + data.length)) ^ 0xffffffff) >>> 0);
        return b;
    };
    const ihdr = new Uint8Array(13), hd = new DataView(ihdr.buffer);
    hd.setUint32(0, width); hd.setUint32(4, height);
    ihdr.set([8, 6, 0, 0, 0], 8);
    const parts = [new Uint8Array([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr), chunk('IDAT', idat), chunk('IEND', new Uint8Array(0))];
    const out = new Uint8Array(parts.reduce((a, p) => a + p.length, 0));
    let o = 0;
    for (const p of parts) { out.set(p, o); o += p.length; }
    return out;
}

/* ---------------------------------------------------------------- OBJ --
 * Static: the meshes as they stand in `desc` (the rest pose, or a frame the
 * caller posed them in). Metres, as Blender's OBJ importer expects. */
export function toObj(desc, scale = 0.01) {
    const mtlName = desc.name + '.mtl';
    let obj = '# ' + desc.name + ' -- Ultimate Mortal Kombat 3, exported by the UMK3 PC launcher\n';
    obj += 'mtllib ' + mtlName + '\n';
    let base = 0;
    for (const m of desc.meshes) {
        obj += 'o ' + m.name + '\n';
        const n = m.pos.length / 3;
        for (let i = 0; i < n; i++)
            obj += 'v ' + f6(m.pos[i * 3] * scale) + ' ' + f6(m.pos[i * 3 + 1] * scale) + ' ' + f6(m.pos[i * 3 + 2] * scale) + '\n';
        for (let i = 0; i < n; i++) obj += 'vt ' + f6(m.uv[i * 2]) + ' ' + f6(1 - m.uv[i * 2 + 1]) + '\n';
        obj += 'usemtl ' + desc.materials[m.material].name + '\n';
        for (let t = 0; t < m.faces.length; t += 3) {
            const a = m.faces[t] + base + 1, b = m.faces[t + 1] + base + 1, c = m.faces[t + 2] + base + 1;
            obj += 'f ' + a + '/' + a + ' ' + b + '/' + b + ' ' + c + '/' + c + '\n';
        }
        base += n;
    }
    let mtl = '';
    for (const mat of desc.materials) {
        mtl += 'newmtl ' + mat.name + '\nKd 1 1 1\nKa 0 0 0\nKs 0 0 0\nd 1\nillum 1\n';
        if (mat.texture) mtl += 'map_Kd ' + mat.texture + '\n';
        mtl += '\n';
    }
    return { [desc.name + '.obj']: enc.encode(obj), [mtlName]: enc.encode(mtl) };
}

/* ---------------------------------------------------------------- USD --
 * Text (.usda), binary (.usdc) or packaged with its pictures (.usdz):
 * usd.js. One SkelRoot: the Skeleton, its SkelAnimation (every clip one
 * after another on the timeline, their ranges in the layer's doc), the
 * meshes bound to it and their UsdPreviewSurface materials. */
export function toUsd(desc, kind = 'usda', pictures = {}) {
    const stage = stageOf(desc);
    if (kind === 'usdc') return { [desc.name + '.usdc']: writeUsdc(stage) };
    if (kind === 'usdz') {
        const entries = [[desc.name + '.usdc', writeUsdc(stage)]];
        for (const [name, bytes] of Object.entries(pictures)) entries.push([name, bytes]);
        return { [desc.name + '.usdz']: writeUsdz(entries) };
    }
    return { [desc.name + '.usda']: writeUsda(stage) };
}

/* ---------------------------------------------------------------- FBX --
 * Binary FBX 7.4 (Blender reads no ASCII FBX). Nodes are [name, props,
 * children]; a property is [type, value] in FBX's own letters. The layout
 * follows what Blender's own exporter writes, which both Blender and the
 * Autodesk SDK read. */
const KTIME = 46186158000;

class Out {
    constructor() { this.buf = new Uint8Array(1 << 20); this.n = 0; }
    need(k) {
        if (this.n + k <= this.buf.length) return;
        let c = this.buf.length * 2;
        while (c < this.n + k) c *= 2;
        const b = new Uint8Array(c); b.set(this.buf.subarray(0, this.n)); this.buf = b;
    }
    bytes(b) { this.need(b.length); this.buf.set(b, this.n); this.n += b.length; }
    u8(v) { this.need(1); this.buf[this.n++] = v; }
    u32(v) { this.need(4); new DataView(this.buf.buffer).setUint32(this.n, v >>> 0, true); this.n += 4; }
    at(o, v) { new DataView(this.buf.buffer).setUint32(o, v >>> 0, true); }
}

function prop(o, [t, v]) {
    const dvw = (k, f) => { const b = new Uint8Array(k); f(new DataView(b.buffer)); o.u8(t.charCodeAt(0)); o.bytes(b); };
    switch (t) {
        case 'C': o.u8(67); o.u8(v ? 1 : 0); break;
        case 'I': dvw(4, d => d.setInt32(0, v, true)); break;
        case 'L': dvw(8, d => d.setBigInt64(0, BigInt(Math.round(v)), true)); break;
        case 'D': dvw(8, d => d.setFloat64(0, v, true)); break;
        case 'F': dvw(4, d => d.setFloat32(0, v, true)); break;
        case 'S': case 'R': {
            const b = typeof v === 'string' ? enc.encode(v) : v;
            o.u8(t.charCodeAt(0)); o.u32(b.length); o.bytes(b);
            break;
        }
        default: {                                  /* arrays: i l f d */
            const size = { i: 4, l: 8, f: 4, d: 8 }[t];
            const b = new Uint8Array(v.length * size), d = new DataView(b.buffer);
            for (let i = 0; i < v.length; i++) {
                if (t === 'i') d.setInt32(i * 4, v[i], true);
                else if (t === 'l') d.setBigInt64(i * 8, BigInt(Math.round(v[i])), true);
                else if (t === 'f') d.setFloat32(i * 4, v[i], true);
                else d.setFloat64(i * 8, v[i], true);
            }
            o.u8(t.charCodeAt(0)); o.u32(v.length); o.u32(0); o.u32(b.length); o.bytes(b);
        }
    }
}

function node(o, [name, props = [], kids = []]) {
    const start = o.n;
    o.u32(0); o.u32(props.length); o.u32(0);
    const nb = enc.encode(name);
    o.u8(nb.length); o.bytes(nb);
    const p0 = o.n;
    for (const p of props) prop(o, p);
    o.at(start + 8, o.n - p0);
    if (kids.length) {
        for (const k of kids) node(o, k);
        o.bytes(new Uint8Array(13));
    } else if (!props.length) o.bytes(new Uint8Array(13));
    o.at(start, o.n);
}

const S = v => ['S', v], I = v => ['I', v], D = v => ['D', v], L = v => ['L', v], C = v => ['C', v];
const P = (name, type, label, flags, ...vals) => ['P', [S(name), S(type), S(label), S(flags), ...vals]];
const P70 = (...ps) => ['Properties70', [], ps];
const cls = (name, kind) => name + '\x00\x01' + kind;

function mat16(R, T) { return [R[0], R[1], R[2], 0, R[3], R[4], R[5], 0, R[6], R[7], R[8], 0, T[0], T[1], T[2], 1]; }
function inverse({ R, T }) { const Rt = tr3(R); return { R: Rt, T: vmul(T, Rt).map(x => -x) }; }

/* Euler XYZ in degrees for a row-form rotation (FBX: x first, then y, then z) */
function euler(R, prev) {
    const c20 = R[2], c21 = R[5], c22 = R[8], c10 = R[1], c00 = R[0];
    let x, y, z;
    if (Math.abs(c20) < 0.999999) {
        y = Math.asin(-c20); x = Math.atan2(c21, c22); z = Math.atan2(c10, c00);
    } else {
        y = c20 < 0 ? Math.PI / 2 : -Math.PI / 2; x = 0; z = Math.atan2(-R[3], R[4]);
    }
    let e = [x, y, z].map(a => a * 180 / Math.PI);
    if (prev) {
        const alt = [e[0] + 180, 180 - e[1], e[2] + 180];
        const near = (v, p) => v + 360 * Math.round((p - v) / 360);
        const a = e.map((v, i) => near(v, prev[i])), b = alt.map((v, i) => near(v, prev[i]));
        const da = a.reduce((s, v, i) => s + Math.abs(v - prev[i]), 0), db = b.reduce((s, v, i) => s + Math.abs(v - prev[i]), 0);
        e = da <= db ? a : b;
    }
    return e;
}

export function toFbx(desc) {
    let nextId = 1000000;
    const id = () => ++nextId;
    const objects = [], links = [];
    const OO = (c, p) => links.push(['C', [S('OO'), L(c), L(p)]]);
    const OP = (c, p, what) => links.push(['C', [S('OP'), L(c), L(p), S(what)]]);
    const counts = {};
    const add = (type, n) => { objects.push(n); counts[type] = (counts[type] || 0) + 1; };
    const model = (mid, name, kind, local, extra = []) => add('Model', ['Model', [L(mid), S(cls(name, 'Model')), S(kind)], [
        ['Version', [I(232)]],
        P70(P('Lcl Translation', 'Lcl Translation', '', 'A', D(local.T[0]), D(local.T[1]), D(local.T[2])),
            P('Lcl Rotation', 'Lcl Rotation', '', 'A', ...euler(local.R).map(D)),
            P('Lcl Scaling', 'Lcl Scaling', '', 'A', D(1), D(1), D(1)),
            P('DefaultAttributeIndex', 'int', 'Integer', '', I(0)),
            P('InheritType', 'enum', '', '', I(1)), ...extra),
        ['Shading', [C(true)]], ['Culling', [S('CullingOff')]]]]);
    const I3 = { R: [1, 0, 0, 0, 1, 0, 0, 0, 1], T: [0, 0, 0] };

    /* the materials, their pictures */
    const matIds = desc.materials.map(m => {
        const mid = id();
        add('Material', ['Material', [L(mid), S(cls(m.name, 'Material')), S('')], [
            ['Version', [I(102)]], ['ShadingModel', [S('Phong')]], ['MultiLayer', [I(0)]],
            P70(P('DiffuseColor', 'Color', '', 'A', D(0.8), D(0.8), D(0.8)), P('DiffuseFactor', 'Number', '', 'A', D(1)))]]);
        if (m.texture) {
            const tid = id(), vid = id();
            add('Texture', ['Texture', [L(tid), S(cls(m.texture, 'Texture')), S('')], [
                ['Type', [S('TextureVideoClip')]], ['Version', [I(202)]], ['TextureName', [S(cls(m.texture, 'Texture'))]],
                P70(P('UseMaterial', 'bool', '', '', I(1))),
                ['Media', [S(cls(m.texture, 'Video'))]], ['FileName', [S(m.texture)]], ['RelativeFilename', [S(m.texture)]],
                ['ModelUVTranslation', [D(0), D(0)]], ['ModelUVScaling', [D(1), D(1)]],
                ['Texture_Alpha_Source', [S('None')]], ['Cropping', [I(0), I(0), I(0), I(0)]]]]);
            add('Video', ['Video', [L(vid), S(cls(m.texture, 'Video')), S('Clip')], [
                ['Type', [S('Clip')]], P70(P('Path', 'KString', 'XRefUrl', '', S(m.texture))),
                ['UseMipMap', [I(0)]], ['Filename', [S(m.texture)]], ['RelativeFilename', [S(m.texture)]]]]);
            OP(tid, mid, 'DiffuseColor');
            OO(vid, tid);
        }
        return mid;
    });

    /* the skeleton: a Null holding the bones, as Blender writes an armature */
    const sk = desc.skeleton;
    let boneIds = [], armId = 0;
    if (sk) {
        armId = id();
        const aid = id();
        add('NodeAttribute', ['NodeAttribute', [L(aid), S(cls(desc.name, 'NodeAttribute')), S('Null')], [['TypeFlags', [S('Null')]]]]);
        model(armId, desc.name, 'Null', I3);
        OO(aid, armId);
        OO(armId, 0);
        boneIds = sk.names.map(() => id());
        sk.names.forEach((nm, b) => {
            const nid = id();
            add('NodeAttribute', ['NodeAttribute', [L(nid), S(cls(nm, 'NodeAttribute')), S('LimbNode')], [
                P70(P('Size', 'double', 'Number', '', D(3))), ['TypeFlags', [S('Skeleton')]]]]);
            model(boneIds[b], nm, 'LimbNode', localOf(sk.rest, { parent: sk.parent }, b));
            OO(nid, boneIds[b]);
            OO(boneIds[b], sk.parent[b] < 0 ? armId : boneIds[sk.parent[b]]);
        });
    }

    /* the meshes */
    const poseNodes = [];
    for (const m of desc.meshes) {
        const gid = id(), mid = id();
        const n = m.pos.length / 3, nt = m.faces.length / 3;
        const pvi = new Array(m.faces.length);
        for (let t = 0; t < nt; t++) {
            pvi[t * 3] = m.faces[t * 3]; pvi[t * 3 + 1] = m.faces[t * 3 + 1]; pvi[t * 3 + 2] = ~m.faces[t * 3 + 2];
        }
        const uv = new Array(n * 2);
        for (let i = 0; i < n; i++) { uv[i * 2] = m.uv[i * 2]; uv[i * 2 + 1] = 1 - m.uv[i * 2 + 1]; }
        add('Geometry', ['Geometry', [L(gid), S(cls(m.name, 'Geometry')), S('Mesh')], [
            P70(), ['GeometryVersion', [I(124)]],
            ['Vertices', [['d', Array.from(m.pos)]]],
            ['PolygonVertexIndex', [['i', pvi]]],
            ['LayerElementUV', [I(0)], [['Version', [I(101)]], ['Name', [S('UVMap')]],
                ['MappingInformationType', [S('ByPolygonVertex')]], ['ReferenceInformationType', [S('IndexToDirect')]],
                ['UV', [['d', uv]]], ['UVIndex', [['i', Array.from(m.faces)]]]]],
            ['LayerElementMaterial', [I(0)], [['Version', [I(101)]], ['Name', [S('')]],
                ['MappingInformationType', [S('AllSame')]], ['ReferenceInformationType', [S('IndexToDirect')]],
                ['Materials', [['i', [0]]]]]],
            ['Layer', [I(0)], [['Version', [I(100)]],
                ['LayerElement', [], [['Type', [S('LayerElementUV')]], ['TypedIndex', [I(0)]]]],
                ['LayerElement', [], [['Type', [S('LayerElementMaterial')]], ['TypedIndex', [I(0)]]]]]]]]);
        model(mid, m.name, 'Mesh', I3);
        OO(gid, mid);
        OO(matIds[m.material], mid);
        OO(mid, 0);
        poseNodes.push(['PoseNode', [], [['Node', [L(mid)]], ['Matrix', [['d', mat16(I3.R, I3.T)]]]]]);

        if (sk && m.skin) {
            const skid = id();
            add('Deformer', ['Deformer', [L(skid), S(cls(m.name, 'Deformer')), S('Skin')], [
                ['Version', [I(101)]], ['Link_DeformAcuracy', [D(50)]]]]);
            OO(skid, gid);
            const per = sk.names.map(() => ({ i: [], w: [] }));
            for (let v = 0; v < n; v++)
                for (let k = 0; k < 4; k++) {
                    const w = m.skin.wt[v * 4 + k];
                    if (w > 0) { per[m.skin.idx[v * 4 + k]].i.push(v); per[m.skin.idx[v * 4 + k]].w.push(w); }
                }
            per.forEach((c, b) => {
                const cid = id();
                const link = sk.rest[b], inv = inverse(link);
                add('Deformer', ['Deformer', [L(cid), S(cls(sk.names[b], 'SubDeformer')), S('Cluster')], [
                    ['Version', [I(100)]], ['UserData', [S(''), S('')]],
                    ['Indexes', [['i', c.i]]], ['Weights', [['d', c.w]]],
                    ['Transform', [['d', mat16(inv.R, inv.T)]]], ['TransformLink', [['d', mat16(link.R, link.T)]]]]]);
                OO(cid, skid);
                OO(boneIds[b], cid);
            });
        }
    }
    if (sk) {
        poseNodes.push(['PoseNode', [], [['Node', [L(armId)]], ['Matrix', [['d', mat16(I3.R, I3.T)]]]]]);
        sk.names.forEach((_, b) => poseNodes.push(['PoseNode', [], [['Node', [L(boneIds[b])]], ['Matrix', [['d', mat16(sk.rest[b].R, sk.rest[b].T)]]]]]));
        add('Pose', ['Pose', [L(id()), S(cls(desc.name, 'Pose')), S('BindPose')], [
            ['Type', [S('BindPose')]], ['Version', [I(100)]], ['NbPoseNodes', [I(poseNodes.length)]], ...poseNodes]]);
    }

    /* the clips: one stack each, a curve a channel a bone */
    let longest = 0;
    for (const clip of desc.clips || []) {
        if (!sk) break;
        const nf = clip.poses.length;
        longest = Math.max(longest, nf);
        const stop = Math.round((nf - 1) * KTIME / desc.fps);
        const st = id(), ly = id();
        add('AnimationStack', ['AnimationStack', [L(st), S(cls(clip.name, 'AnimStack')), S('')], [
            P70(P('LocalStop', 'KTime', 'Time', '', L(stop)), P('ReferenceStop', 'KTime', 'Time', '', L(stop)))]]);
        add('AnimationLayer', ['AnimationLayer', [L(ly), S(cls(clip.name, 'AnimLayer')), S('')], []]);
        OO(ly, st);
        const times = Array.from({ length: nf }, (_, f) => Math.round(f * KTIME / desc.fps));
        sk.names.forEach((_, b) => {
            const tv = [[], [], []], rv = [[], [], []], sv = [[], [], []];
            /* a bone's own scale, when a pose has one (an animated stage) */
            const scaled = clip.poses.some(p => p[b].S);
            let prev = null;
            for (const pose of clip.poses) {
                const loc = localOf(pose, { parent: sk.parent }, b);
                const e = euler(loc.R, prev);
                prev = e;
                for (let k = 0; k < 3; k++) {
                    tv[k].push(loc.T[k]); rv[k].push(e[k]);
                    if (scaled) sv[k].push(loc.S ? loc.S[k] : 1);
                }
            }
            const channels = [['Lcl Translation', tv, 'T'], ['Lcl Rotation', rv, 'R']];
            if (scaled) channels.push(['Lcl Scaling', sv, 'S']);
            for (const [what, vals, short] of channels) {
                const cn = id();
                add('AnimationCurveNode', ['AnimationCurveNode', [L(cn), S(cls(short, 'AnimCurveNode')), S('')], [
                    P70(P('d|X', 'Number', '', 'A', D(vals[0][0])), P('d|Y', 'Number', '', 'A', D(vals[1][0])), P('d|Z', 'Number', '', 'A', D(vals[2][0])))]]);
                OO(cn, ly);
                OP(cn, boneIds[b], what);
                ['d|X', 'd|Y', 'd|Z'].forEach((ch, k) => {
                    const cv = id();
                    add('AnimationCurve', ['AnimationCurve', [L(cv), S(cls('', 'AnimCurve')), S('')], [
                        ['Default', [D(vals[k][0])]], ['KeyVer', [I(4009)]],
                        ['KeyTime', [['l', times]]], ['KeyValueFloat', [['f', vals[k]]]],
                        ['KeyAttrFlags', [['i', [24836]]]], ['KeyAttrDataFloat', [['f', [0, 0, 9.419963346924634e-30, 0]]]],
                        ['KeyAttrRefCount', [['i', [nf]]]]]]);
                    OP(cv, cn, ch);
                });
            }
        });
    }

    const stopAll = Math.round(Math.max(0, longest - 1) * KTIME / desc.fps);
    const top = [
        ['FBXHeaderExtension', [], [
            ['FBXHeaderVersion', [I(1003)]], ['FBXVersion', [I(7400)]], ['EncryptionType', [I(0)]],
            ['CreationTimeStamp', [], [['Version', [I(1000)]], ['Year', [I(1970)]], ['Month', [I(1)]], ['Day', [I(1)]],
                ['Hour', [I(10)]], ['Minute', [I(0)]], ['Second', [I(0)]], ['Millisecond', [I(0)]]]],
            ['Creator', [S('UMK3 PC launcher')]]]],
        ['FileId', [['R', new Uint8Array([0x28, 0xb3, 0x2a, 0xeb, 0xb6, 0x24, 0xcc, 0xc2, 0xbf, 0xc8, 0xb0, 0x2a, 0xa9, 0x2b, 0xfc, 0xf1])]]],
        ['CreationTime', [S('1970-01-01 10:00:00:000')]],
        ['Creator', [S('UMK3 PC launcher')]],
        ['GlobalSettings', [], [['Version', [I(1000)]], P70(
            P('UpAxis', 'int', 'Integer', '', I(1)), P('UpAxisSign', 'int', 'Integer', '', I(1)),
            P('FrontAxis', 'int', 'Integer', '', I(2)), P('FrontAxisSign', 'int', 'Integer', '', I(1)),
            P('CoordAxis', 'int', 'Integer', '', I(0)), P('CoordAxisSign', 'int', 'Integer', '', I(1)),
            P('OriginalUpAxis', 'int', 'Integer', '', I(1)), P('OriginalUpAxisSign', 'int', 'Integer', '', I(1)),
            P('UnitScaleFactor', 'double', 'Number', '', D(1)), P('OriginalUnitScaleFactor', 'double', 'Number', '', D(1)),
            P('TimeMode', 'enum', '', '', I(desc.fps === 60 ? 3 : 6)),
            P('TimeSpanStart', 'KTime', 'Time', '', L(0)), P('TimeSpanStop', 'KTime', 'Time', '', L(stopAll)),
            P('CustomFrameRate', 'double', 'Number', '', D(desc.fps)))]],
        ['Documents', [], [['Count', [I(1)]], ['Document', [L(id()), S(''), S('Scene')], [
            P70(P('SourceObject', 'object', '', ''), P('ActiveAnimStackName', 'KString', '', '', S(''))),
            ['RootNode', [L(0)]]]]]],
        ['References', [], []],
        ['Definitions', [], [['Version', [I(100)]], ['Count', [I(Object.values(counts).reduce((a, b) => a + b, 0))]],
            ...Object.entries(counts).map(([t, n]) => ['ObjectType', [S(t)], [['Count', [I(n)]]]])]],
        ['Objects', [], objects],
        ['Connections', [], links],
        ['Takes', [], [['Current', [S('')]]]],
    ];

    const o = new Out();
    o.bytes(enc.encode('Kaydara FBX Binary  '));
    o.bytes(new Uint8Array([0x00, 0x1a, 0x00]));
    o.u32(7400);
    for (const t of top) node(o, t);
    o.bytes(new Uint8Array(13));
    o.bytes(new Uint8Array([0xfa, 0xbc, 0xab, 0x09, 0xd0, 0xc8, 0xd4, 0x66, 0xb1, 0x76, 0xfb, 0x83, 0x1c, 0xf7, 0x26, 0x7e]));
    o.bytes(new Uint8Array(4));
    o.bytes(new Uint8Array((16 - (o.n & 15)) & 15));
    o.u32(7400);
    o.bytes(new Uint8Array(120));
    o.bytes(new Uint8Array([0xf8, 0x5a, 0x8c, 0x6a, 0xde, 0xf5, 0xd9, 0x7e, 0xec, 0xe9, 0x0c, 0xe3, 0x75, 0x8f, 0x29, 0x0b]));
    return { [desc.name + '.fbx']: o.buf.slice(0, o.n) };
}

/* the meshes moved into a pose: each vertex through its bones, as the
 * skinning in Blender would (for OBJ, which has no skeleton) */
export function poseMeshes(desc, pose) {
    const sk = desc.skeleton;
    const delta = sk.rest.map((r, b) => {
        const inv = inverse(r);
        return { R: mul3(inv.R, pose[b].R), T: [0, 1, 2].map(k => vmul(inv.T, pose[b].R)[k] + pose[b].T[k]) };
    });
    return desc.meshes.map(m => {
        if (!m.skin) return m;
        const n = m.pos.length / 3, out = new Float32Array(n * 3);
        for (let v = 0; v < n; v++) {
            const p = [m.pos[v * 3], m.pos[v * 3 + 1], m.pos[v * 3 + 2]];
            for (let k = 0; k < 4; k++) {
                const w = m.skin.wt[v * 4 + k];
                if (!w) continue;
                const d = delta[m.skin.idx[v * 4 + k]], q = vmul(p, d.R);
                for (let c = 0; c < 3; c++) out[v * 3 + c] += w * (q[c] + d.T[c]);
            }
        }
        return Object.assign({}, m, { pos: out });
    });
}

/* ---------------------------------------------------------------- GLB --
 * glTF 2.0 binary: the meshes (skinned when there is a skeleton), the
 * bones as nodes, one animation a clip and the pictures inside the file
 * (`pictures`: { texture file name: PNG bytes }). glTF is in metres, Y up,
 * facing +Z, as the game is: a root node scales the centimetres down. */
export function toGlb(desc, pictures = {}) {
    const chunks = [], views = [], accessors = [];
    let blen = 0;
    const align = () => {
        const pad = (4 - (blen & 3)) & 3;
        if (pad) { chunks.push(new Uint8Array(pad)); blen += pad; }
    };
    const view = (bytes, target) => {
        align();
        const b = new Uint8Array(bytes.buffer, bytes.byteOffset, bytes.byteLength);
        views.push(Object.assign({ buffer: 0, byteOffset: blen, byteLength: b.length }, target ? { target } : {}));
        chunks.push(b);
        blen += b.length;
        return views.length - 1;
    };
    const SIZE = { SCALAR: 1, VEC2: 2, VEC3: 3, VEC4: 4, MAT4: 16 };
    const acc = (arr, type, ctype, target, minmax) => {
        const n = SIZE[type];
        const a = { bufferView: view(arr, target), componentType: ctype, count: arr.length / n, type };
        if (minmax) {
            a.min = []; a.max = [];
            for (let k = 0; k < n; k++) {
                let lo = Infinity, hi = -Infinity;
                for (let i = k; i < arr.length; i += n) { lo = Math.min(lo, arr[i]); hi = Math.max(hi, arr[i]); }
                a.min.push(lo); a.max.push(hi);
            }
        }
        accessors.push(a);
        return accessors.length - 1;
    };
    const FLOAT = 5126, U16 = 5123, U32 = 5125;

    const images = [], textures = [], picOf = new Map();
    const materials = desc.materials.map(m => {
        const out = { name: m.name, pbrMetallicRoughness: { metallicFactor: 0, roughnessFactor: 0.9 }, doubleSided: true };
        const png = m.texture && pictures[m.texture];
        if (png) {
            if (!picOf.has(m.texture)) {
                images.push({ name: m.texture.replace(/\.png$/i, ''), mimeType: 'image/png', bufferView: view(png) });
                textures.push({ source: images.length - 1, sampler: 0 });
                picOf.set(m.texture, textures.length - 1);
            }
            out.pbrMetallicRoughness.baseColorTexture = { index: picOf.get(m.texture) };
        }
        return out;
    });

    const nodes = [{ name: desc.name, scale: [0.01, 0.01, 0.01], children: [] }];
    const sk = desc.skeleton, tree = sk && { parent: sk.parent };
    const boneNode = [], skins = [];
    if (sk) {
        sk.names.forEach((nm, b) => {
            const l = localOf(sk.rest, tree, b);
            boneNode[b] = nodes.length;
            nodes.push({ name: nm, translation: l.T, rotation: matToQuat(l.R) });
        });
        sk.names.forEach((_, b) => {
            const p = sk.parent[b], into = p < 0 ? nodes[0] : nodes[boneNode[p]];
            (into.children = into.children || []).push(boneNode[b]);
        });
        const ibm = new Float32Array(sk.names.length * 16);
        sk.rest.forEach((r, b) => { const i = inverse(r); ibm.set(mat16(i.R, i.T), b * 16); });
        skins.push({ joints: boneNode.slice(), inverseBindMatrices: acc(ibm, 'MAT4', FLOAT) });
    }

    const meshes = desc.meshes.map(m => {
        const n = m.pos.length / 3;
        /* smooth normals from the faces */
        const nor = new Float32Array(n * 3);
        for (let t = 0; t < m.faces.length; t += 3) {
            const a = m.faces[t], b = m.faces[t + 1], c = m.faces[t + 2];
            const e1 = [0, 1, 2].map(k => m.pos[b * 3 + k] - m.pos[a * 3 + k]), e2 = [0, 1, 2].map(k => m.pos[c * 3 + k] - m.pos[a * 3 + k]);
            const nn = [e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]];
            for (const v of [a, b, c]) for (let k = 0; k < 3; k++) nor[v * 3 + k] += nn[k];
        }
        for (let v = 0; v < n; v++) {
            const l = Math.hypot(nor[v * 3], nor[v * 3 + 1], nor[v * 3 + 2]);
            for (let k = 0; k < 3; k++) nor[v * 3 + k] = l > 1e-12 ? nor[v * 3 + k] / l : (k === 1 ? 1 : 0);
        }
        const attributes = {
            POSITION: acc(Float32Array.from(m.pos), 'VEC3', FLOAT, 34962, true),
            NORMAL: acc(nor, 'VEC3', FLOAT, 34962),
            TEXCOORD_0: acc(Float32Array.from(m.uv), 'VEC2', FLOAT, 34962),
        };
        if (sk && m.skin) {
            attributes.JOINTS_0 = acc(Uint16Array.from(m.skin.idx), 'VEC4', U16, 34962);
            attributes.WEIGHTS_0 = acc(Float32Array.from(m.skin.wt), 'VEC4', FLOAT, 34962);
        }
        const indices = n > 65535 ? acc(Uint32Array.from(m.faces), 'SCALAR', U32, 34963) : acc(Uint16Array.from(m.faces), 'SCALAR', U16, 34963);
        return { name: m.name, primitives: [{ attributes, indices, material: m.material }] };
    });
    desc.meshes.forEach((m, i) => {
        const node = { name: m.name, mesh: i };
        if (sk && m.skin) node.skin = 0;
        nodes[0].children.push(nodes.length);
        nodes.push(node);
    });

    const animations = [];
    if (sk)
        for (const clip of desc.clips || []) {
            const nf = clip.poses.length;
            if (!nf) continue;
            const time = acc(Float32Array.from({ length: nf }, (_, f) => f / desc.fps), 'SCALAR', FLOAT, 0, true);
            const samplers = [], channels = [];
            sk.names.forEach((_, b) => {
                const T = new Float32Array(nf * 3), R = new Float32Array(nf * 4);
                const S = clip.poses.some(p => p[b].S) ? new Float32Array(nf * 3) : null;
                let prev = null;
                clip.poses.forEach((pose, f) => {
                    const l = localOf(pose, tree, b);
                    T.set(l.T, f * 3);
                    let q = matToQuat(l.R);
                    /* the short way from the frame before */
                    if (prev && prev[0] * q[0] + prev[1] * q[1] + prev[2] * q[2] + prev[3] * q[3] < 0) q = q.map(x => -x);
                    prev = q;
                    R.set(q, f * 4);
                    if (S) S.set(l.S || [1, 1, 1], f * 3);
                });
                const ch = [['translation', T, 'VEC3'], ['rotation', R, 'VEC4']];
                if (S) ch.push(['scale', S, 'VEC3']);
                for (const [path, arr, type] of ch) {
                    samplers.push({ input: time, output: acc(arr, type, FLOAT), interpolation: 'LINEAR' });
                    channels.push({ sampler: samplers.length - 1, target: { node: boneNode[b], path } });
                }
            });
            animations.push({ name: clip.name, samplers, channels });
        }

    align();
    const json = {
        asset: { version: '2.0', generator: 'UMK3 PC launcher' },
        scene: 0, scenes: [{ name: desc.name, nodes: [0] }], nodes, meshes, materials,
        accessors, bufferViews: views, buffers: [{ byteLength: blen }],
    };
    if (skins.length) json.skins = skins;
    if (animations.length) json.animations = animations;
    if (images.length) {
        json.images = images;
        json.textures = textures;
        json.samplers = [{ magFilter: 9729, minFilter: 9987, wrapS: 10497, wrapT: 10497 }];
    }

    let jb = enc.encode(JSON.stringify(json));
    if (jb.length & 3) { const t = new Uint8Array((jb.length + 3) & ~3).fill(0x20); t.set(jb); jb = t; }
    const total = 12 + 8 + jb.length + 8 + blen;
    const out = new Uint8Array(total), d = new DataView(out.buffer);
    d.setUint32(0, 0x46546c67, true); d.setUint32(4, 2, true); d.setUint32(8, total, true);
    d.setUint32(12, jb.length, true); d.setUint32(16, 0x4e4f534a, true);
    out.set(jb, 20);
    let o = 20 + jb.length;
    d.setUint32(o, blen, true); d.setUint32(o + 4, 0x004e4942, true);
    o += 8;
    for (const c of chunks) { out.set(c, o); o += c.length; }
    return { [desc.name + '.glb']: out };
}
