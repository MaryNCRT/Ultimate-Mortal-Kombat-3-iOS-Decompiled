/* Bringing Blender's work back: an FBX (binary, as Blender writes it) made
 * from one of the exporter's fighters -- the same skeleton, the bones under
 * the same names -- becomes the game's own files again:
 *
 *   - an animation: a take of the FBX over one of the fighter's clips, as a
 *     new .skinanim (the clip's frames replaced, every other frame kept);
 *   - a model: the FBX's skinned meshes as a new .skin over the same rig.
 *
 * Blender is free to change axes and units on the way (it imports at 0.01
 * and may write the armature scaled, or Z up), so nothing here trusts them:
 * each bone's rest in the file (the cluster's TransformLink) is matched to the
 * same bone's rest in the game -- the T-pose or the game's own pose, whichever
 * the export used -- and the one similarity (scale, turn, move) between the
 * two is fitted from the joints. What a frame does to a bone is then its
 * change from that rest, carried into the game's space.
 *
 * Matrices here are 4x4, column-major, for column vectors (FBX's own); the
 * game's poses are { R row form, T } (rig.js). */
import { quatToMat, matToQuat, mul3, tr3, vmul, fitRigid } from './rig.js';
import { fighterDesc } from './assets.js';

const KTIME = 46186158000;

/* ------------------------------------------------------------ reading -- */

async function inflate(bytes) {
    const ds = new DecompressionStream('deflate');
    return new Uint8Array(await new Response(new Blob([bytes]).stream().pipeThrough(ds)).arrayBuffer());
}

/* the node tree: { name, props: [], kids: [] } */
export async function readFbx(b) {
    const head = new TextDecoder().decode(b.subarray(0, 20));
    if (!head.startsWith('Kaydara FBX Binary')) throw new Error('not a binary FBX (in Blender: File > Export > FBX)');
    const d = new DataView(b.buffer, b.byteOffset, b.byteLength);
    const version = d.getUint32(23, true);
    const wide = version >= 7500;
    const pending = [];
    const u = o => wide ? Number(d.getBigUint64(o, true)) : d.getUint32(o, true);
    const W = wide ? 8 : 4;

    function node(o) {
        const end = u(o), nprops = u(o + W);
        const nameLen = d.getUint8(o + 3 * W);
        if (end === 0) return { node: null, next: o + 3 * W + 1 };
        const name = new TextDecoder().decode(b.subarray(o + 3 * W + 1, o + 3 * W + 1 + nameLen));
        let p = o + 3 * W + 1 + nameLen;
        const props = [];
        for (let i = 0; i < nprops; i++) {
            const t = String.fromCharCode(b[p++]);
            switch (t) {
            case 'Y': props.push(d.getInt16(p, true)); p += 2; break;
            case 'C': props.push(!!b[p]); p += 1; break;
            case 'I': props.push(d.getInt32(p, true)); p += 4; break;
            case 'F': props.push(d.getFloat32(p, true)); p += 4; break;
            case 'D': props.push(d.getFloat64(p, true)); p += 8; break;
            case 'L': props.push(Number(d.getBigInt64(p, true))); p += 8; break;
            case 'S': case 'R': {
                const n = d.getUint32(p, true);
                const raw = b.subarray(p + 4, p + 4 + n);
                props.push(t === 'S' ? new TextDecoder().decode(raw) : raw);
                p += 4 + n;
                break;
            }
            case 'f': case 'd': case 'l': case 'i': case 'b': {
                const n = d.getUint32(p, true), enc = d.getUint32(p + 4, true), clen = d.getUint32(p + 8, true);
                const raw = b.slice(p + 12, p + 12 + clen);
                p += 12 + clen;
                const at = props.length;
                props.push(null);
                const decode = data => {
                    const v = new DataView(data.buffer, data.byteOffset, data.byteLength);
                    const size = { f: 4, d: 8, l: 8, i: 4, b: 1 }[t];
                    const out = new Array(n);
                    for (let k = 0; k < n; k++) {
                        const q = k * size;
                        out[k] = t === 'f' ? v.getFloat32(q, true) : t === 'd' ? v.getFloat64(q, true)
                            : t === 'l' ? Number(v.getBigInt64(q, true)) : t === 'i' ? v.getInt32(q, true) : data[q];
                    }
                    props[at] = out;
                };
                if (enc === 1) pending.push(inflate(raw).then(decode));
                else decode(raw);
                break;
            }
            default: throw new Error('FBX: unknown property type ' + t);
            }
        }
        const kids = [];
        while (p < end) {
            const r = node(p);
            p = r.next;
            if (!r.node) break;
            kids.push(r.node);
        }
        return { node: { name, props, kids }, next: end };
    }

    const root = { name: '', props: [], kids: [] };
    let o = 27;
    while (o < b.length - 13) {
        const r = node(o);
        if (!r.node) break;
        root.kids.push(r.node);
        o = r.next;
    }
    await Promise.all(pending);
    return root;
}

const kid = (n, name) => n && n.kids.find(k => k.name === name);
const kids = (n, name) => (n ? n.kids.filter(k => k.name === name) : []);
const plain = s => String(s).split('\u0000')[0].replace(/^(Model|Geometry|Deformer|SubDeformer)::/, '');

function props70(n) {
    const out = {};
    for (const p of kids(kid(n, 'Properties70'), 'P')) out[p.props[0]] = p.props.slice(4);
    return out;
}

/* ----------------------------------------------------------- matrices -- */

const I4 = () => [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1];
function mm(a, b) {
    const o = new Array(16);
    for (let c = 0; c < 4; c++)
        for (let r = 0; r < 4; r++)
            o[c * 4 + r] = a[r] * b[c * 4] + a[4 + r] * b[c * 4 + 1] + a[8 + r] * b[c * 4 + 2] + a[12 + r] * b[c * 4 + 3];
    return o;
}
function inv4(m) {
    /* affine: [A t; 0 1] -> [A^-1, -A^-1 t] */
    const a = [m[0], m[1], m[2], m[4], m[5], m[6], m[8], m[9], m[10]];
    const det = a[0] * (a[4] * a[8] - a[7] * a[5]) - a[3] * (a[1] * a[8] - a[7] * a[2]) + a[6] * (a[1] * a[5] - a[4] * a[2]);
    const id = 1 / det;
    const b = [
        (a[4] * a[8] - a[5] * a[7]) * id, (a[2] * a[7] - a[1] * a[8]) * id, (a[1] * a[5] - a[2] * a[4]) * id,
        (a[5] * a[6] - a[3] * a[8]) * id, (a[0] * a[8] - a[2] * a[6]) * id, (a[2] * a[3] - a[0] * a[5]) * id,
        (a[3] * a[7] - a[4] * a[6]) * id, (a[1] * a[6] - a[0] * a[7]) * id, (a[0] * a[4] - a[1] * a[3]) * id];
    const t = [m[12], m[13], m[14]];
    return [b[0], b[1], b[2], 0, b[3], b[4], b[5], 0, b[6], b[7], b[8], 0,
        -(b[0] * t[0] + b[3] * t[1] + b[6] * t[2]), -(b[1] * t[0] + b[4] * t[1] + b[7] * t[2]), -(b[2] * t[0] + b[5] * t[1] + b[8] * t[2]), 1];
}
const trans = v => [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, v[0], v[1], v[2], 1];
const scal = v => [v[0], 0, 0, 0, 0, v[1], 0, 0, 0, 0, v[2], 0, 0, 0, 0, 1];
function rotX(a) { const c = Math.cos(a), s = Math.sin(a); return [1, 0, 0, 0, 0, c, s, 0, 0, -s, c, 0, 0, 0, 0, 1]; }
function rotY(a) { const c = Math.cos(a), s = Math.sin(a); return [c, 0, -s, 0, 0, 1, 0, 0, s, 0, c, 0, 0, 0, 0, 1]; }
function rotZ(a) { const c = Math.cos(a), s = Math.sin(a); return [c, s, 0, 0, -s, c, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]; }
/* Euler XYZ in degrees, as FBX means it: x first, then y, then z */
const eulerM = e => mm(rotZ(e[2] * Math.PI / 180), mm(rotY(e[1] * Math.PI / 180), rotX(e[0] * Math.PI / 180)));
/* a model's RotationOrder (0 XYZ, 1 XZY, 2 YZX, 3 YXZ, 4 ZXY, 5 ZYX: the
 * first letter turned first), as Blender's importer reads it; the pre and
 * post rotations are always XYZ */
const ORDERS = ['xyz', 'xzy', 'yzx', 'yxz', 'zxy', 'zyx'];
function eulerOrd(e, order) {
    if (!order) return eulerM(e);
    const one = { x: rotX(e[0] * Math.PI / 180), y: rotY(e[1] * Math.PI / 180), z: rotZ(e[2] * Math.PI / 180) };
    const o = ORDERS[order] || 'xyz';
    return mm(one[o[2]], mm(one[o[1]], one[o[0]]));
}
/* GlobalSettings' axes to the game's (Y up, the front +Z), as Blender's
 * importer turns a file: a Z-up file (3ds Max, Blender's own with its
 * defaults) stands up */
function axisM(gs) {
    const g = (k, d) => (gs[k] && Number.isFinite(Number(gs[k][0])) ? Number(gs[k][0]) : d);
    const up = g('UpAxis', 1), us = g('UpAxisSign', 1), fr = g('FrontAxis', 2), fs = g('FrontAxisSign', 1), co = g('CoordAxis', 0), cs = g('CoordAxisSign', 1);
    if (new Set([up, fr, co]).size !== 3 || ![up, fr, co].every(a => a >= 0 && a <= 2)) return null;
    if (up === 1 && fr === 2 && co === 0 && us > 0 && fs > 0 && cs > 0) return null;
    const M = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1];
    M[co * 4] = cs < 0 ? -1 : 1;
    M[up * 4 + 1] = us < 0 ? -1 : 1;
    M[fr * 4 + 2] = fs < 0 ? -1 : 1;
    return M;
}

/* a column matrix as the game's { R row form, T }, the scale taken out */
function toPose(m) {
    const cols = [0, 4, 8].map(o => [m[o], m[o + 1], m[o + 2]]);
    const R = [];
    for (const c of cols) {
        const n = Math.hypot(...c) || 1;
        R.push(c[0] / n, c[1] / n, c[2] / n);
    }
    /* re-orthonormalised through a quaternion: what is left of a shear */
    const q = matToQuat(R);
    return { R: quatToMat(...q), T: [m[12], m[13], m[14]] };
}
const fromPose = p => [p.R[0], p.R[1], p.R[2], 0, p.R[3], p.R[4], p.R[5], 0, p.R[6], p.R[7], p.R[8], 0, p.T[0], p.T[1], p.T[2], 1];

/* ------------------------------------------------------------ the scene -- */

export function fbxScene(root) {
    const objects = kid(root, 'Objects');
    const byId = new Map();
    for (const n of objects ? objects.kids : []) byId.set(n.props[0], n);
    const parentOf = new Map(), childrenOf = new Map(), propLinks = [];
    for (const c of kids(kid(root, 'Connections'), 'C')) {
        const [kind, child, parent, what] = c.props;
        if (kind === 'OO') {
            if (!parentOf.has(child)) parentOf.set(child, []);
            parentOf.get(child).push(parent);
            if (!childrenOf.has(parent)) childrenOf.set(parent, []);
            childrenOf.get(parent).push(child);
        } else if (kind === 'OP') propLinks.push({ child, parent, what });
    }
    const gs = props70(kid(root, 'GlobalSettings'));
    const fps = (gs.CustomFrameRate && gs.TimeMode && gs.TimeMode[0] === 14) ? gs.CustomFrameRate[0]
        : ({ 1: 120, 2: 100, 3: 60, 4: 50, 5: 48, 6: 30, 7: 30, 8: 25, 10: 24, 11: 1000 })[gs.TimeMode && gs.TimeMode[0]] || 30;

    const models = new Map();
    for (const [id, n] of byId) {
        if (n.name !== 'Model') continue;
        const p = props70(n);
        models.set(id, {
            id, name: plain(n.props[1]), type: n.props[2],
            T: p['Lcl Translation'] || [0, 0, 0], R: p['Lcl Rotation'] || [0, 0, 0], S: p['Lcl Scaling'] || [1, 1, 1],
            pre: p.PreRotation || [0, 0, 0], post: p.PostRotation || [0, 0, 0],
            /* the pivots (3ds Max, Maya: a part turned or scaled about a
             * point of its own) and the geometric offset, which moves the
             * mesh only, not the children */
            rOff: p.RotationOffset || [0, 0, 0], rPiv: p.RotationPivot || [0, 0, 0],
            sOff: p.ScalingOffset || [0, 0, 0], sPiv: p.ScalingPivot || [0, 0, 0],
            gT: p.GeometricTranslation || [0, 0, 0], gR: p.GeometricRotation || [0, 0, 0], gS: p.GeometricScaling || [1, 1, 1],
            order: p.RotationOrder ? Number(p.RotationOrder[0]) || 0 : 0,
            parent: (parentOf.get(id) || []).find(x => byId.get(x) && byId.get(x).name === 'Model') ?? null,
        });
    }
    const axis = axisM(gs);
    /* the bind pose (Pose, BindPose): each node's matrix when the skin was
     * bound -- Blender's rest for a bone with no cluster of its own */
    const bindPose = new Map();
    for (const [, n] of byId) {
        const type = kid(n, 'Type') ? plain(kid(n, 'Type').props[0]) : plain(n.props[2]);
        if (n.name !== 'Pose' || type !== 'BindPose') continue;
        for (const pn of kids(n, 'PoseNode')) {
            const id = kid(pn, 'Node') && kid(pn, 'Node').props[0], m = kid(pn, 'Matrix') && kid(pn, 'Matrix').props[0];
            if (id !== undefined && m && m.length >= 16 && !bindPose.has(id)) bindPose.set(id, axis ? mm(axis, Array.from(m)) : Array.from(m));
        }
    }
    return { byId, parentOf, childrenOf, propLinks, models, fps, axis, bindPose };
}

/* FBX's local transform: T Roff Rp Rpre R Rpost^-1 Rp^-1 Soff Sp S Sp^-1 */
function localM(m, over) {
    const T = (over && over.T) || m.T, R = (over && over.R) || m.R, S = (over && over.S) || m.S;
    const neg = v => [-v[0], -v[1], -v[2]];
    const rp = m.rPiv || [0, 0, 0], sp = m.sPiv || [0, 0, 0];
    const rot = mm(trans(rp), mm(eulerM(m.pre), mm(eulerOrd(R, m.order), mm(inv4(eulerM(m.post)), trans(neg(rp))))));
    const sc = mm(trans(m.sOff || [0, 0, 0]), mm(trans(sp), mm(scal(S), trans(neg(sp)))));
    return mm(trans(T), mm(trans(m.rOff || [0, 0, 0]), mm(rot, sc)));
}

/* the mesh's own offset under its model (GeometricTranslation...) */
function geomM(m) {
    return mm(trans(m.gT || [0, 0, 0]), mm(eulerM(m.gR || [0, 0, 0]), scal(m.gS || [1, 1, 1])));
}

function globalM(sc, id, over, memo) {
    if (memo.has(id)) return memo.get(id);
    const m = sc.models.get(id);
    const l = localM(m, over && over.get(id));
    const g = m.parent !== null && sc.models.has(m.parent) ? mm(globalM(sc, m.parent, over, memo), l) : sc.axis ? mm(sc.axis, l) : l;
    memo.set(id, g);
    return g;
}

/* the skin: per bone model, its rest (TransformLink) and its influences */
function clusters(sc) {
    const out = [];
    for (const [id, n] of sc.byId) {
        if (n.name !== 'Deformer' || n.props[2] !== 'Cluster') continue;
        const bone = (sc.childrenOf.get(id) || []).find(c => sc.models.has(c));
        const skin = (sc.parentOf.get(id) || [])[0];
        const geom = skin !== undefined ? (sc.parentOf.get(skin) || [])[0] : undefined;
        const link = kid(n, 'TransformLink');
        if (bone === undefined || !link) continue;
        out.push({ bone, geom, link: sc.axis ? mm(sc.axis, Array.from(link.props[0])) : link.props[0], idx: (kid(n, 'Indexes') || { props: [[]] }).props[0], wt: (kid(n, 'Weights') || { props: [[]] }).props[0] });
    }
    return out;
}

/* ------------------------------------------------- matching the game's -- */

/* The fighter's bones by the names the exporter gave them, and which of its
 * two rests the file was made from, with the similarity file <- game. */
function matchRig(f, sc) {
    const names = f.names;
    const byName = new Map(names.map((n, i) => [n.toLowerCase(), i]));
    const rest = new Map();                     /* game bone -> file rest matrix */
    for (const c of clusters(sc)) {
        const b = byName.get(sc.models.get(c.bone).name.toLowerCase());
        if (b !== undefined && !rest.has(b)) rest.set(b, c.link);
    }
    if (rest.size < 3) {
        /* no skin: the bones' own rest, from the scene as it stands */
        const memo = new Map();
        for (const [id, m] of sc.models) {
            const b = byName.get(m.name.toLowerCase());
            if (b !== undefined && !rest.has(b)) rest.set(b, globalM(sc, id, null, memo));
        }
    }
    if (rest.size < 3) throw new Error('the bones do not match ' + f.ch.name + "'s (export the rig from the exporter and keep its bone names)");
    let best = null;
    for (const [kind, pose] of [['tpose', f.tpose], ['bind', f.bind.pose]]) {
        if (!pose) continue;
        const P = [], Q = [], W = [];
        for (const [b, m] of rest) { P.push(...pose[b].T); Q.push(m[12], m[13], m[14]); W.push(1); }
        const g = similarity(P, Q, W);
        if (!best || g.err < best.g.err) best = { kind, pose, g };
    }
    return { rest, kind: best.kind, pose: best.pose, g: best.g, matched: rest.size, of: names.length };
}

/* q ~ s p R + t over the points; R row form */
function similarity(P, Q, W) {
    const n = W.length, cp = [0, 0, 0], cq = [0, 0, 0];
    for (let i = 0; i < n; i++) for (let k = 0; k < 3; k++) { cp[k] += P[i * 3 + k] / n; cq[k] += Q[i * 3 + k] / n; }
    let sp = 0, sq = 0;
    for (let i = 0; i < n; i++) for (let k = 0; k < 3; k++) { sp += (P[i * 3 + k] - cp[k]) ** 2; sq += (Q[i * 3 + k] - cq[k]) ** 2; }
    const s = Math.sqrt(sq / (sp || 1)) || 1;
    const Ps = P.map(x => x * s);
    const { R, T } = fitRigid(Ps, Q, W);
    let err = 0;
    for (let i = 0; i < n; i++) {
        const v = vmul([Ps[i * 3], Ps[i * 3 + 1], Ps[i * 3 + 2]], R);
        for (let k = 0; k < 3; k++) err += (v[k] + T[k] - Q[i * 3 + k]) ** 2;
    }
    /* as a column matrix: x_file = G x_game */
    const G = fromPose({ R, T });
    for (let i = 0; i < 12; i++) if (i % 4 !== 3) G[i] *= s;
    return { G, Ginv: inv4(G), s, err: Math.sqrt(err / n) / s };
}

/* ---------------------------------------------------------- animation -- */

/* the takes in the file: [{ name, frames }] */
export function fbxTakes(sc) {
    const out = [];
    for (const [id, n] of sc.byId) {
        if (n.name !== 'AnimationStack') continue;
        const span = takeSpan(sc, id);
        out.push({ id, name: plain(n.props[1]).replace(/^.*\|/, ''), frames: span ? span.frames : 0 });
    }
    return out.filter(t => t.frames > 0);
}

function takeCurves(sc, stackId) {
    const layers = (sc.childrenOf.get(stackId) || []).filter(id => sc.byId.get(id) && sc.byId.get(id).name === 'AnimationLayer');
    const nodes = [];
    for (const ly of layers)
        for (const cn of sc.childrenOf.get(ly) || []) {
            const n = sc.byId.get(cn);
            if (!n || n.name !== 'AnimationCurveNode') continue;
            const link = sc.propLinks.find(l => l.child === cn && sc.models.has(l.parent));
            if (!link) continue;
            const chans = {};
            for (const l of sc.propLinks)
                if (l.parent === cn) {
                    const cv = sc.byId.get(l.child);
                    if (cv && cv.name === 'AnimationCurve')
                        chans[l.what] = { t: kid(cv, 'KeyTime').props[0], v: kid(cv, 'KeyValueFloat').props[0] };
                }
            nodes.push({ model: link.parent, what: link.what, defaults: props70(n), chans });
        }
    return nodes;
}

function takeSpan(sc, stackId) {
    let lo = Infinity, hi = -Infinity;
    for (const n of takeCurves(sc, stackId))
        for (const c of Object.values(n.chans)) if (c.t.length) { lo = Math.min(lo, c.t[0]); hi = Math.max(hi, c.t[c.t.length - 1]); }
    if (!(hi >= lo)) return null;
    const step = KTIME / sc.fps;
    return { lo, step, frames: Math.round((hi - lo) / step) + 1 };
}

function curveAt(c, t) {
    const T = c.t, V = c.v;
    if (t <= T[0]) return V[0];
    if (t >= T[T.length - 1]) return V[V.length - 1];
    let a = 0, b = T.length - 1;
    while (b - a > 1) { const m = (a + b) >> 1; if (T[m] <= t) a = m; else b = m; }
    return V[a] + (V[b] - V[a]) * (t - T[a]) / (T[b] - T[a]);
}

/* A take over clip `ci` of fighter f: the .skinanim's bytes, the clip's
 * frames replaced (the take resampled to the clip's length; the game's
 * frame count and every other clip stay as they are). */
export function importAnim(f, sc, takeId, ci, animBytes) {
    const m = matchRig(f, sc);
    const span = takeSpan(sc, takeId);
    if (!span) throw new Error('the take has no keys');
    const curves = takeCurves(sc, takeId);
    const clip = f.clips[ci];
    const nt = clip.to - clip.from + 1;
    const byName = new Map(f.names.map((n, i) => [n.toLowerCase(), i]));
    const boneModel = new Map();
    for (const [id, md] of sc.models) {
        const b = byName.get(md.name.toLowerCase());
        if (b !== undefined && !boneModel.has(b)) boneModel.set(b, id);
    }
    const { bones, anim } = f.rig;
    const nb = anim.bones;
    const out = animBytes.slice();
    const d = new DataView(out.buffer);
    const frames0 = d.getInt32(4, true), fs0 = d.getInt32(8, true);
    const hdr = 12 + frames0 * fs0 === out.length ? 12 : 16;
    const fsize = hdr === 12 ? fs0 : d.getInt32(12, true);

    for (let k = 0; k < nt; k++) {
        const sf = nt > 1 ? Math.round(k * (span.frames - 1) / (nt - 1)) : 0;
        const t = span.lo + sf * span.step;
        const over = new Map();
        for (const n of curves) {
            const md = sc.models.get(n.model);
            const key = n.what === 'Lcl Translation' ? 'T' : n.what === 'Lcl Rotation' ? 'R' : n.what === 'Lcl Scaling' ? 'S' : null;
            if (!key) continue;
            const base = md[key];
            const v = ['d|X', 'd|Y', 'd|Z'].map((ch, i) => n.chans[ch] ? curveAt(n.chans[ch], t) : (n.defaults[ch] ? n.defaults[ch][0] : base[i]));
            if (!over.has(n.model)) over.set(n.model, {});
            over.get(n.model)[key] = v;
        }
        const memo = new Map();
        /* each bone: its change from the file's rest, into the game's space,
         * on the game's rest; bones the file lacks follow their parent */
        const glob = new Array(bones.num);
        for (const i of bones.order) {
            const p = bones.parent[i];
            const id = boneModel.get(i), restF = m.rest.get(i);
            if (id !== undefined && restF) {
                const delta = mm(globalM(sc, id, over, memo), inv4(restF));
                glob[i] = toPose(mm(m.g.Ginv, mm(delta, mm(m.g.G, fromPose(m.pose[i])))));
            } else if (p >= 0) {
                const loc = { R: mul3(m.pose[i].R, tr3(m.pose[p].R)), T: vmul(sub(m.pose[i].T, m.pose[p].T), tr3(m.pose[p].R)) };
                glob[i] = { R: mul3(loc.R, glob[p].R), T: add(vmul(loc.T, glob[p].R), glob[p].T) };
            } else glob[i] = m.pose[i];
        }
        const base = hdr + (clip.from + k) * fsize;
        const r = glob[bones.root].T;
        for (let c = 0; c < 3; c++) d.setFloat32(base + 4 + c * 4, r[c], true);
        bones.order.forEach((i, n) => {
            if (n >= nb) return;
            const p = bones.parent[i];
            const loc = p < 0 ? glob[i].R : mul3(glob[i].R, tr3(glob[p].R));
            const q = matToQuat(loc);
            for (let c = 0; c < 4; c++) d.setFloat32(base + 16 + n * 20 + c * 4, q[c], true);
        });
    }
    return { bytes: out, rest: m.kind, matched: m.matched, of: m.of, frames: span.frames, into: nt };
}

const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
const add = (a, b) => [a[0] + b[0], a[1] + b[1], a[2] + b[2]];

/* -------------------------------------------------------------- model -- */

/* The FBX's skinned meshes as a new .skin for fighter f, on the game's rig.
 * Materials go to the fighter's blocks in order (a block is one texture:
 * the first is <NAME>_DIFFUSE, the outfit; the rest as the game has them). */
export function importModel(f, sc) {
    const m = matchRig(f, sc);
    const byName = new Map(f.names.map((n, i) => [n.toLowerCase(), i]));
    const cl = clusters(sc);
    const nblocks = f.rig.skin.length;
    const blocks = Array.from({ length: nblocks }, () => ({ verts: [], map: new Map(), tris: [] }));
    const memo = new Map();
    let meshes = 0;
    for (const [gid, gn] of sc.byId) {
        if (gn.name !== 'Geometry' || gn.props[2] !== 'Mesh') continue;
        const mine = cl.filter(c => c.geom === gid);
        if (!mine.length) continue;
        meshes++;
        const modelId = (sc.parentOf.get(gid) || []).find(x => sc.models.has(x));
        const place = modelId !== undefined ? globalM(sc, modelId, null, memo) : I4();
        const toGame = mm(m.g.Ginv, place);
        const V = kid(gn, 'Vertices').props[0], PVI = kid(gn, 'PolygonVertexIndex').props[0];
        const nv = V.length / 3;
        /* the influences, four at most a vertex */
        const inf = Array.from({ length: nv }, () => []);
        for (const c of mine) {
            const b = byName.get(sc.models.get(c.bone).name.toLowerCase());
            if (b === undefined) continue;
            c.idx.forEach((v, k) => { if (c.wt[k] > 0) inf[v].push([b, c.wt[k]]); });
        }
        /* per corner: UV and normal; per polygon: material */
        const uvEl = kid(gn, 'LayerElementUV'), nEl = kid(gn, 'LayerElementNormal'), mEl = kid(gn, 'LayerElementMaterial');
        const corner = (el, field, idxField, k, v, poly, size) => {
            if (!el) return null;
            const mapType = kid(el, 'MappingInformationType').props[0], ref = kid(el, 'ReferenceInformationType').props[0];
            const data = kid(el, field).props[0];
            let i = mapType === 'ByPolygonVertex' ? k : mapType === 'ByVertice' || mapType === 'ByVertex' ? v : mapType === 'ByPolygon' ? poly : 0;
            if (ref === 'IndexToDirect' && kid(el, idxField)) i = kid(el, idxField).props[0][i];
            return data.slice(i * size, i * size + size);
        };
        let poly = 0, start = 0;
        for (let k = 0; k < PVI.length; k++) {
            if (PVI[k] >= 0) continue;
            const ring = [];
            for (let j = start; j <= k; j++) ring.push(j);
            const mat = mEl ? (corner(mEl, 'Materials', null, 0, 0, poly, 1) || [0])[0] : 0;
            const blk = blocks[Math.min(mat, nblocks - 1)];
            const put = j => {
                const v = PVI[j] < 0 ? ~PVI[j] : PVI[j];
                const uv = corner(uvEl, 'UV', 'UVIndex', j, v, poly, 2) || [0, 0];
                const nr = corner(nEl, 'Normals', 'NormalsIndex', j, v, poly, 3) || [0, 1, 0];
                /* one game vertex per place and weighting: the .skin keeps
                 * UVs per triangle corner, so Blender's UV seams need none */
                const p = xform(toGame, [V[v * 3], V[v * 3 + 1], V[v * 3 + 2]]);
                const key = p.map(x => Math.round(x * 1e3)).join(',') + '|' + inf[v].map(([bb, w]) => bb + ':' + w.toFixed(3)).join(',');
                let o = blk.map.get(key);
                if (o === undefined) {
                    o = blk.verts.length;
                    blk.map.set(key, o);
                    blk.verts.push({ p, n: [0, 0, 0], inf: inf[v] });
                }
                const nn = xdir(toGame, nr);
                const vv = blk.verts[o];
                for (let c = 0; c < 3; c++) vv.n[c] += nn[c];
                return { o, uv: [uv[0], 1 - uv[1]] };
            };
            for (let j = 1; j + 1 < ring.length; j++) blk.tris.push([put(ring[0]), put(ring[j]), put(ring[j + 1])]);
            poly++;
            start = k + 1;
        }
    }
    if (!meshes) throw new Error('no skinned mesh in the file (parent it to the armature, with weights)');
    for (const b of blocks) if (b.verts.length > 65535) throw new Error('a block has more than 65535 vertices');
    if (!blocks.some(b => b.tris.length)) throw new Error('the meshes have no faces');
    return { bytes: writeSkin(blocks, m.pose, f.rig.skin.counted !== false), rest: m.kind, matched: m.matched, of: m.of,
        verts: blocks.reduce((s, b) => s + b.verts.length, 0), tris: blocks.reduce((s, b) => s + b.tris.length, 0) };
}

const xform = (M, p) => [0, 1, 2].map(r => M[r] * p[0] + M[4 + r] * p[1] + M[8 + r] * p[2] + M[12 + r]);
function xdir(M, n) {
    const v = [0, 1, 2].map(r => M[r] * n[0] + M[4 + r] * n[1] + M[8 + r] * n[2]);
    const l = Math.hypot(...v) || 1;
    return v.map(x => x / l);
}

/* .skin: a block count, then per block the vertices (bones, weights, A and B
 * per influence in the bone's own space at rest, times the weight) and the
 * triangles (three UV pairs, three indices). */
function writeSkin(blocks, rest, counted) {
    let size = counted ? 4 : 0;
    for (const b of blocks) size += 8 + b.verts.length * (4 + 8 + 96) + b.tris.length * (24 + 6);
    const out = new Uint8Array(size), d = new DataView(out.buffer);
    let o = 0;
    if (counted) { d.setInt32(0, blocks.length, true); o = 4; }
    for (const blk of blocks) {
        const nv = blk.verts.length, nt = blk.tris.length;
        d.setInt32(o, nv, true); d.setInt32(o + 4, nt, true); o += 8;
        const infl = blk.verts.map(v => {
            const l = v.inf.slice().sort((a, b) => b[1] - a[1]).slice(0, 4);
            const s = l.reduce((a, x) => a + x[1], 0) || 1;
            return l.length ? l.map(([b, w]) => [b, w / s]) : [[0, 1]];
        });
        infl.forEach((l, i) => {
            let w = 0;
            for (let k = 0; k < 4; k++) w |= (k < l.length ? l[k][0] : 0xff) << (8 * k);
            d.setUint32(o + i * 4, w >>> 0, true);
        });
        o += nv * 4;
        infl.forEach((l, i) => { for (let k = 0; k < 4; k++) d.setUint16(o + (i * 4 + k) * 2, k < l.length ? Math.min(65535, Math.round(l[k][1] * 65536)) : 0, true); });
        o += nv * 8;
        blk.verts.forEach((v, i) => {
            const nl = Math.hypot(...v.n) || 1, n = v.n.map(x => x / nl);
            for (let k = 0; k < 4; k++) {
                const it = infl[i][k];
                let a = [0, 0, 0], bb = [0, 0, 0];
                if (it) {
                    const r = rest[it[0]], Rt = tr3(r.R);
                    a = vmul(sub(v.p, r.T), Rt).map(x => x * it[1]);
                    bb = vmul(n, Rt).map(x => x * it[1]);
                }
                for (let c = 0; c < 3; c++) {
                    d.setFloat32(o + k * 12 + c * 4, a[c], true);
                    d.setFloat32(o + 48 + k * 12 + c * 4, bb[c], true);
                }
            }
            o += 96;
        });
        for (const t of blk.tris) for (const c of t) { d.setFloat32(o, c.uv[0], true); d.setFloat32(o + 4, c.uv[1], true); o += 8; }
        for (const t of blk.tris) for (const c of t) { d.setUint16(o, c.o, true); o += 2; }
    }
    return out;
}

/* ------------------------------------------------------------ the rig -- */

/* "export the rig": the fighter as the exporter writes it, T-pose and no
 * animation, to model over in Blender and bring back with importModel */
export function rigDesc(f) {
    return fighterDesc(f, { rest: f.tpose ? 'tpose' : 'bind', clips: [] });
}


/* for modelin.js: the other model formats share the fit and the writer */
export { mm, inv4, similarity, writeSkin, clusters, globalM, geomM, xform, xdir, kid, kids, plain, I4 };
