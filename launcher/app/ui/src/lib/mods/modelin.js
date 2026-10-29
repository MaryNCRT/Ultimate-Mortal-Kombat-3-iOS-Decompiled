/* REPLACE MODEL, from any of three formats: an FBX (Blender's), a GLB (glTF
 * binary, read with three.js's GLTFLoader) or an OBJ (three.js's OBJLoader)
 * becomes one description --
 *
 *   { bones: [{ name, rest }],            rest: global, 4x4 column-major
 *     meshes: [{ name, pos,               the vertices, in the file's space
 *                inf,                     per vertex [[bone, weight]], or null
 *                tv, tuv, tn, tm }],      per triangle: its three vertices,
 *                                         UVs (v down, as the game), normals
 *                                         (or null) and material
 *     materials: [{ name, image, file }] } image: something to draw, or null
 *
 * -- and that becomes the fighter's .skin (buildSkin). A file with the
 * fighter's bones (the exporter's rig, its names kept) is fitted to them as
 * fbxin.js fits an FBX; one without (an OBJ, a statue) is stood where the
 * fighter stands, at its height, and each vertex weighted to the bones
 * nearest it. Several materials share a block of the fighter as one texture
 * sheet: their pictures side by side, the UVs moved onto each one's part. */
import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';
import { OBJLoader } from 'three/addons/loaders/OBJLoader.js';
import { readFbx, fbxScene, similarity, writeSkin, clusters, globalM, xform, xdir, kid, plain, I4 } from './fbxin.js';
import { posedVerts, makeRig, meshTopology, vertexWeights } from './rig.js';
import { parseSkin } from './formats.js';
import { decodePicture } from './image.js';

const PICTURE = /\.(png|jpe?g|jfif|webp|gif|bmp|ico|avif|svg|tga)$/i;

/* file: { name, bytes, beside: { name: bytes } } (main.js file:pick) */
export async function readModel(file) {
    const ext = file.name.split('.').pop().toLowerCase();
    const beside = file.beside || {};
    let mdl;
    if (ext === 'fbx') mdl = await fbxModel(fbxScene(await readFbx(file.bytes)), beside);
    else if (ext === 'glb') mdl = await glbModel(file.bytes, beside);
    else if (ext === 'obj') mdl = await objModel(file.bytes, beside);
    else throw new Error('FBX, GLB or OBJ');
    if (!mdl.meshes.some(m => m.tm.length)) throw new Error('the file has no faces');
    if (!mdl.materials.length) mdl.materials.push({ name: 'default', image: null, file: '' });
    mdl.format = ext;
    return mdl;
}

/* a texture the file names, from beside it: by its name, any folder */
function findBeside(beside, ref) {
    const base = String(ref || '').split(/[\\/]/).pop().toLowerCase();
    if (!base) return null;
    for (const [n, b] of Object.entries(beside)) if (n.toLowerCase() === base) return b;
    const stem = base.replace(/\.\w+$/, '');
    for (const [n, b] of Object.entries(beside)) if (PICTURE.test(n) && n.toLowerCase().replace(/\.\w+$/, '') === stem) return b;
    return null;
}
const picture = async bytes => (bytes ? decodePicture(bytes).catch(() => null) : null);

/* a mesh's arrays from plain lists */
function meshOf(name, pos, inf, tv, tuv, tn, tm) {
    return { name, pos: Float32Array.from(pos), inf, tv: Int32Array.from(tv), tuv: Float32Array.from(tuv),
        tn: tn ? Float32Array.from(tn) : null, tm: Int32Array.from(tm) };
}

/* ---------------------------------------------------------------- FBX -- */

async function fbxModel(sc, beside) {
    const cl = clusters(sc);
    const memo = new Map();
    const bones = [], boneOf = new Map();
    const addBone = (id, rest) => {
        if (!boneOf.has(id)) { boneOf.set(id, bones.length); bones.push({ name: sc.models.get(id).name, rest }); }
    };
    for (const c of cl) addBone(c.bone, c.link);
    if (!bones.length)
        for (const [id, m] of sc.models) if (m.type === 'LimbNode') addBone(id, globalM(sc, id, null, memo));

    /* a material and its diffuse picture: embedded (Video Content) or a
     * file beside the FBX */
    const materials = [], matOf = new Map();
    async function material(id) {
        if (matOf.has(id)) return matOf.get(id);
        const m = { name: plain(sc.byId.get(id).props[1]), image: null, file: '' };
        matOf.set(id, materials.length);
        materials.push(m);
        const link = sc.propLinks.find(l => l.parent === id && /diffuse|basecolor/i.test(l.what || ''));
        const tex = link && sc.byId.get(link.child);
        if (tex) {
            const vid = (sc.childrenOf.get(link.child) || []).map(x => sc.byId.get(x)).find(v => v && v.name === 'Video');
            const raw = vid && kid(vid, 'Content') && kid(vid, 'Content').props[0];
            m.file = String((kid(tex, 'RelativeFilename') || kid(tex, 'FileName') || { props: [''] }).props[0] || '');
            m.image = await picture(raw instanceof Uint8Array && raw.length ? raw : findBeside(beside, m.file));
        }
        return matOf.get(id);
    }

    const meshes = [];
    for (const [gid, gn] of sc.byId) {
        if (gn.name !== 'Geometry' || gn.props[2] !== 'Mesh') continue;
        const modelId = (sc.parentOf.get(gid) || []).find(x => sc.models.has(x));
        const place = modelId !== undefined ? globalM(sc, modelId, null, memo) : I4();
        const mats = [];
        for (const id of (modelId !== undefined && sc.childrenOf.get(modelId)) || [])
            if (sc.byId.get(id) && sc.byId.get(id).name === 'Material') mats.push(await material(id));
        const V = kid(gn, 'Vertices').props[0], PVI = kid(gn, 'PolygonVertexIndex').props[0];
        const nv = V.length / 3;
        const pos = [];
        for (let v = 0; v < nv; v++) pos.push(...xform(place, [V[v * 3], V[v * 3 + 1], V[v * 3 + 2]]));
        const mine = cl.filter(c => c.geom === gid);
        let inf = null;
        if (mine.length) {
            inf = Array.from({ length: nv }, () => []);
            for (const c of mine) c.idx.forEach((v, k) => { if (c.wt[k] > 0) inf[v].push([boneOf.get(c.bone), c.wt[k]]); });
        }
        const uvEl = kid(gn, 'LayerElementUV'), nEl = kid(gn, 'LayerElementNormal'), mEl = kid(gn, 'LayerElementMaterial');
        const corner = (el, field, idxField, k, v, poly, size) => {
            if (!el) return null;
            const mapType = kid(el, 'MappingInformationType').props[0], ref = kid(el, 'ReferenceInformationType').props[0];
            const data = kid(el, field).props[0];
            let i = mapType === 'ByPolygonVertex' ? k : mapType === 'ByVertice' || mapType === 'ByVertex' ? v : mapType === 'ByPolygon' ? poly : 0;
            if (ref === 'IndexToDirect' && idxField && kid(el, idxField)) i = kid(el, idxField).props[0][i];
            return data.slice(i * size, i * size + size);
        };
        const tv = [], tuv = [], tn = [], tm = [];
        let poly = 0, start = 0;
        for (let k = 0; k < PVI.length; k++) {
            if (PVI[k] >= 0) continue;
            const mi = mEl ? (corner(mEl, 'Materials', null, 0, 0, poly, 1) || [0])[0] : 0;
            const mat = mats.length ? mats[Math.min(mi, mats.length - 1)] : 0;
            const put = j => {
                const v = PVI[j] < 0 ? ~PVI[j] : PVI[j];
                const uv = corner(uvEl, 'UV', 'UVIndex', j, v, poly, 2) || [0, 0];
                const n = corner(nEl, 'Normals', 'NormalsIndex', j, v, poly, 3) || [0, 1, 0];
                tv.push(v);
                tuv.push(uv[0], 1 - uv[1]);
                tn.push(...xdir(place, n));
            };
            for (let j = start + 1; j + 1 <= k; j++) { put(start); put(j); put(j + 1); tm.push(mat); }
            poly++;
            start = k + 1;
        }
        meshes.push(meshOf(modelId !== undefined ? sc.models.get(modelId).name : 'mesh', pos, inf, tv, tuv, tn, tm));
    }
    return { bones, meshes, materials };
}

/* ---------------------------------------------------------------- GLB -- */

/* GLB: { json, bin }; the pictures are taken out here, so the loader
 * does not fetch them (the page allows no blob: or data: fetches) */
function splitGlb(bytes) {
    const d = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    if (d.getUint32(0, true) !== 0x46546c67) throw new Error('not a GLB (glTF binary)');
    let o = 12, json = null, bin = null;
    while (o + 8 <= bytes.length) {
        const len = d.getUint32(o, true), type = d.getUint32(o + 4, true);
        const body = bytes.subarray(o + 8, o + 8 + len);
        if (type === 0x4e4f534a) json = JSON.parse(new TextDecoder().decode(body));
        else if (type === 0x004e4942) bin = body;
        o += 8 + len;
    }
    if (!json) throw new Error('the GLB has no JSON');
    return { json, bin };
}

function joinGlb(json, bin) {
    const enc = new TextEncoder().encode(JSON.stringify(json));
    const jl = (enc.length + 3) & ~3, bl = bin ? (bin.length + 3) & ~3 : 0;
    const out = new Uint8Array(12 + 8 + jl + (bin ? 8 + bl : 0));
    const d = new DataView(out.buffer);
    d.setUint32(0, 0x46546c67, true); d.setUint32(4, 2, true); d.setUint32(8, out.length, true);
    d.setUint32(12, jl, true); d.setUint32(16, 0x4e4f534a, true);
    out.fill(0x20, 20, 20 + jl);
    out.set(enc, 20);
    if (bin) {
        d.setUint32(20 + jl, bl, true); d.setUint32(24 + jl, 0x004e4942, true);
        out.set(bin, 28 + jl);
    }
    return out;
}

async function glbModel(bytes, beside) {
    const { json, bin } = splitGlb(bytes);
    /* each glTF material's base colour picture */
    const images = await Promise.all((json.images || []).map(async im => {
        if (im.bufferView !== undefined && bin) {
            const bv = json.bufferViews[im.bufferView];
            return picture(bin.subarray(bv.byteOffset || 0, (bv.byteOffset || 0) + bv.byteLength));
        }
        if (/^data:/.test(im.uri || '')) {
            const b64 = im.uri.slice(im.uri.indexOf(',') + 1);
            return picture(Uint8Array.from(atob(b64), c => c.charCodeAt(0)));
        }
        return picture(findBeside(beside, decodeURIComponent(im.uri || '')));
    }));
    const matPic = (json.materials || []).map(m => {
        const t = m.pbrMetallicRoughness && m.pbrMetallicRoughness.baseColorTexture;
        const tex = t && json.textures && json.textures[t.index];
        const src = tex && (tex.source ?? (tex.extensions && Object.values(tex.extensions).map(e => e.source).find(x => x !== undefined)));
        return src !== undefined ? { image: images[src], file: (json.images[src] && (json.images[src].name || json.images[src].uri)) || '' } : null;
    });
    const bare = JSON.parse(JSON.stringify(json));
    delete bare.images; delete bare.textures; delete bare.samplers;
    for (const m of bare.materials || []) {
        if (m.pbrMetallicRoughness) { delete m.pbrMetallicRoughness.baseColorTexture; delete m.pbrMetallicRoughness.metallicRoughnessTexture; }
        delete m.normalTexture; delete m.occlusionTexture; delete m.emissiveTexture;
        delete m.extensions;
    }
    if (bare.extensionsRequired) bare.extensionsRequired = bare.extensionsRequired.filter(e => !/texture/i.test(e));
    const glb = joinGlb(bare, bin);
    const gltf = await new Promise((res, rej) => new GLTFLoader().parse(glb.buffer, '', res, rej));
    const root = gltf.scene;
    root.updateMatrixWorld(true);

    const bones = [], boneOf = new Map();
    root.traverse(o => {
        if (o.isSkinnedMesh)
            for (const b of o.skeleton.bones)
                if (!boneOf.has(b)) { boneOf.set(b, bones.length); bones.push({ name: b.name, rest: b.matrixWorld.elements.slice() }); }
    });
    const materials = [], matOf = new Map();
    const material = m => {
        if (!matOf.has(m)) {
            const a = gltf.parser.associations.get(m);
            const p = a && a.materials !== undefined ? matPic[a.materials] : null;
            matOf.set(m, materials.length);
            materials.push({ name: m.name || 'material' + materials.length, image: p ? p.image : null, file: p ? p.file : '' });
        }
        return matOf.get(m);
    };
    const meshes = [];
    const v3 = new THREE.Vector3(), nm = new THREE.Matrix3();
    root.traverse(o => {
        if (!o.isMesh) return;
        const g = o.geometry, P = g.attributes.position, N = g.attributes.normal, U = g.attributes.uv;
        const n = P.count;
        const pos = new Array(n * 3), nrm = N ? new Array(n * 3) : null;
        nm.getNormalMatrix(o.matrixWorld);
        for (let i = 0; i < n; i++) {
            v3.fromBufferAttribute(P, i);
            if (o.isSkinnedMesh) o.applyBoneTransform(i, v3);
            v3.applyMatrix4(o.matrixWorld);
            pos[i * 3] = v3.x; pos[i * 3 + 1] = v3.y; pos[i * 3 + 2] = v3.z;
            if (N) {
                v3.fromBufferAttribute(N, i).applyMatrix3(nm).normalize();
                nrm[i * 3] = v3.x; nrm[i * 3 + 1] = v3.y; nrm[i * 3 + 2] = v3.z;
            }
        }
        let inf = null;
        if (o.isSkinnedMesh && g.attributes.skinIndex) {
            const SI = g.attributes.skinIndex, SW = g.attributes.skinWeight;
            inf = [];
            for (let i = 0; i < n; i++) {
                const l = [];
                for (let k = 0; k < 4; k++) {
                    const w = SW.getComponent(i, k);
                    if (w > 0) l.push([boneOf.get(o.skeleton.bones[SI.getComponent(i, k)]), w]);
                }
                inf.push(l);
            }
        }
        const mats = (Array.isArray(o.material) ? o.material : [o.material]).map(material);
        const index = g.index ? g.index.array : null;
        const count = index ? index.length : n;
        const groups = g.groups.length ? g.groups : [{ start: 0, count, materialIndex: 0 }];
        const tv = [], tuv = [], tn = nrm ? [] : null, tm = [];
        for (const gr of groups)
            for (let t = gr.start; t + 2 < Math.min(count, gr.start + gr.count); t += 3) {
                for (let c = 0; c < 3; c++) {
                    const v = index ? index[t + c] : t + c;
                    tv.push(v);
                    tuv.push(U ? U.getX(v) : 0, U ? U.getY(v) : 0);
                    if (tn) tn.push(nrm[v * 3], nrm[v * 3 + 1], nrm[v * 3 + 2]);
                }
                tm.push(mats[Math.min(gr.materialIndex || 0, mats.length - 1)]);
            }
        meshes.push(meshOf(o.name || 'mesh', pos, inf, tv, tuv, tn, tm));
    });
    return { bones, meshes, materials };
}

/* ---------------------------------------------------------------- OBJ -- */

/* the .mtl's diffuse picture per material name */
function readMtl(text) {
    const out = new Map();
    let cur = null;
    for (const line of text.split(/\r?\n/)) {
        const t = line.trim();
        if (/^newmtl\s/i.test(t)) cur = t.slice(7).trim();
        else if (cur && /^map_Kd\s/i.test(t)) out.set(cur, t.slice(7).trim().split(/\s+-\S+\s+\S+\s*/).pop());
    }
    return out;
}

async function objModel(bytes, beside) {
    const text = new TextDecoder().decode(bytes);
    const lib = /^\s*mtllib\s+(.+)$/m.exec(text);
    const mtlBytes = lib ? findBeside(beside, lib[1].trim()) : Object.entries(beside).find(([n]) => /\.mtl$/i.test(n))?.[1];
    const mtl = mtlBytes ? readMtl(new TextDecoder().decode(mtlBytes)) : new Map();
    const root = new OBJLoader().parse(text);
    const materials = [], matOf = new Map();
    async function material(name) {
        if (!matOf.has(name)) {
            const file = mtl.get(name) || '';
            matOf.set(name, materials.length);
            materials.push({ name: name || 'material' + materials.length, image: null, file });
            materials[matOf.get(name)].image = await picture(findBeside(beside, file));
        }
        return matOf.get(name);
    }
    const meshes = [];
    const objs = [];
    root.traverse(o => { if (o.isMesh) objs.push(o); });
    for (const o of objs) {
        const g = o.geometry, P = g.attributes.position, N = g.attributes.normal, U = g.attributes.uv;
        const n = P.count;
        const mats = [];
        for (const m of Array.isArray(o.material) ? o.material : [o.material]) mats.push(await material(m.name));
        const groups = g.groups.length ? g.groups : [{ start: 0, count: n, materialIndex: 0 }];
        const tv = [], tuv = [], tn = N ? [] : null, tm = [];
        for (const gr of groups)
            for (let t = gr.start; t + 2 < Math.min(n, gr.start + gr.count); t += 3) {
                for (let c = 0; c < 3; c++) {
                    const v = t + c;
                    tv.push(v);
                    tuv.push(U ? U.getX(v) : 0, U ? 1 - U.getY(v) : 0);
                    if (tn) tn.push(N.getX(v), N.getY(v), N.getZ(v));
                }
                tm.push(mats[Math.min(gr.materialIndex || 0, mats.length - 1)]);
            }
        meshes.push(meshOf(o.name || 'mesh', Array.from(P.array), null, tv, tuv, tn, tm));
    }
    return { bones: [], meshes, materials };
}

/* ------------------------------------------------- onto the fighter -- */

/* Where the file goes in the game's space (x_game = toGame x_file) and on
 * which of the fighter's rests: by its bones when they are the fighter's,
 * else stood on the fighter's feet at its height (the game is Y up, the
 * fighter facing +Z, as glTF and OBJ are), turned `turn` quarter turns. */
function placeModel(f, mdl, turn) {
    const byName = new Map(f.names.map((n, i) => [n.toLowerCase(), i]));
    const restOf = new Map();
    mdl.bones.forEach((b, i) => {
        const g = byName.get(String(b.name).toLowerCase());
        if (g !== undefined && !restOf.has(g)) restOf.set(g, i);
    });
    if (restOf.size >= 3) {
        let best = null;
        for (const [kind, pose] of [['tpose', f.tpose], ['bind', f.bind.pose]]) {
            if (!pose) continue;
            const P = [], Q = [], W = [];
            for (const [g, i] of restOf) { const m = mdl.bones[i].rest; P.push(...pose[g].T); Q.push(m[12], m[13], m[14]); W.push(1); }
            const s = similarity(P, Q, W);
            if (!best || s.err < best.s.err) best = { kind, pose, s };
        }
        return { toGame: best.s.Ginv, kind: best.kind, rest: best.pose, matched: restOf.size, byFile: mdl.bones.map(b => byName.get(String(b.name).toLowerCase()) ?? -1) };
    }
    const rest = f.tpose || f.bind.pose;
    const gp = posedVerts(f.rig, f.topo, rest);
    const box = a => {
        const lo = [1e30, 1e30, 1e30], hi = [-1e30, -1e30, -1e30];
        for (let i = 0; i < a.length; i += 3) for (let k = 0; k < 3; k++) { lo[k] = Math.min(lo[k], a[i + k]); hi[k] = Math.max(hi[k], a[i + k]); }
        return { lo, hi };
    };
    const g = box(gp);
    const all = [];
    for (const m of mdl.meshes) for (const v of m.pos) all.push(v);
    const t = turn * Math.PI / 2, c = Math.round(Math.cos(t)), sn = Math.round(Math.sin(t));
    /* turned about Y first, then scaled to the fighter's height */
    const R = [c, 0, -sn, 0, 0, 1, 0, 0, sn, 0, c, 0, 0, 0, 0, 1];
    const turned = [];
    for (let i = 0; i < all.length; i += 3) turned.push(...xform(R, [all[i], all[i + 1], all[i + 2]]));
    const b = box(turned);
    const k = (g.hi[1] - g.lo[1]) / ((b.hi[1] - b.lo[1]) || 1);
    const at = [(g.lo[0] + g.hi[0]) / 2 - k * (b.lo[0] + b.hi[0]) / 2, g.lo[1] - k * b.lo[1], (g.lo[2] + g.hi[2]) / 2 - k * (b.lo[2] + b.hi[2]) / 2];
    const toGame = R.map((x, i) => (i % 4 !== 3 && i < 12 ? x * k : x));
    toGame[12] = at[0]; toGame[13] = at[1]; toGame[14] = at[2];
    return { toGame, kind: f.tpose ? 'tpose' : 'bind', rest, matched: 0, byFile: mdl.bones.map(() => -1) };
}

/* Each bone the fighter has, as segments from its joint to its children's:
 * a vertex goes to the two nearest, by inverse square distance. */
function autoWeights(f, rest, p) {
    const segs = [];
    const { parent } = f.rig.bones;
    for (let b = 0; b < parent.length; b++) {
        const kids = [];
        for (let c = 0; c < parent.length; c++) if (parent[c] === b) kids.push(c);
        if (!kids.length) segs.push([b, rest[b].T, rest[b].T]);
        for (const c of kids) segs.push([b, rest[b].T, rest[c].T]);
    }
    const d2 = (a, b0, b1) => {
        const ab = [b1[0] - b0[0], b1[1] - b0[1], b1[2] - b0[2]], ap = [a[0] - b0[0], a[1] - b0[1], a[2] - b0[2]];
        const l = ab[0] * ab[0] + ab[1] * ab[1] + ab[2] * ab[2];
        const t = l ? Math.max(0, Math.min(1, (ap[0] * ab[0] + ap[1] * ab[1] + ap[2] * ab[2]) / l)) : 0;
        return (ap[0] - ab[0] * t) ** 2 + (ap[1] - ab[1] * t) ** 2 + (ap[2] - ab[2] * t) ** 2;
    };
    const best = new Map();
    for (const [b, s0, s1] of segs) {
        const d = d2(p, s0, s1);
        if (!best.has(b) || d < best.get(b)) best.set(b, d);
    }
    const two = [...best].sort((a, b) => a[1] - b[1]).slice(0, 2);
    const w = two.map(([b, d]) => [b, 1 / (d + 1e-3)]);
    /* the second only where it is near as close: a joint, not the far arm */
    if (w.length > 1 && two[1][1] > two[0][1] * 4) w.pop();
    return w;
}

/* The description as the fighter's .skin.
 *   o.turn       quarter turns, for a file without the fighter's bones
 *   o.blockOf    per material, the fighter's block it goes to (default:
 *                one each in order, the rest sharing the last)
 * Returns { bytes, verts, tris, kind, matched, of, auto (vertices weighted
 * here), sheets: per block, a canvas of its materials' pictures or null }. */
export function buildSkin(f, mdl, o = {}) {
    const nblocks = f.rig.skin.length;
    const place = placeModel(f, mdl, o.turn || 0);
    const blockOf = mdl.materials.map((_, i) => (o.blockOf && o.blockOf[i] !== undefined ? o.blockOf[i] : Math.min(i, nblocks - 1)));

    /* the sheets: a block's materials' pictures side by side */
    const sheets = [], cell = [];
    for (let b = 0; b < nblocks; b++) {
        const mats = mdl.materials.map((m, i) => i).filter(i => blockOf[i] === b);
        if (!mats.some(i => mdl.materials[i].image)) { sheets.push(null); continue; }
        const cols = Math.ceil(Math.sqrt(mats.length)), rows = Math.ceil(mats.length / cols);
        const big = Math.max(...mats.map(i => { const im = mdl.materials[i].image; return im ? Math.max(im.width || im.naturalWidth, im.height || im.naturalHeight) : 64; }));
        const side = Math.max(16, Math.min(big, Math.floor(2048 / Math.max(cols, rows))));
        const cv = new OffscreenCanvas(cols * side, rows * side), g = cv.getContext('2d');
        g.fillStyle = '#808080';
        g.fillRect(0, 0, cv.width, cv.height);
        mats.forEach((i, k) => {
            const cx = k % cols, cy = Math.floor(k / cols);
            cell[i] = mats.length > 1 ? { cx, cy, cols, rows, pad: 0.5 / side } : null;
            if (mdl.materials[i].image) g.drawImage(mdl.materials[i].image, cx * side, cy * side, side, side);
        });
        sheets.push(cv);
    }
    const onSheet = (i, u, v) => {
        const c = cell[i];
        if (!c) return [u, v];
        const fr = x => (x < 0 || x > 1 ? x - Math.floor(x) : x);
        const cl = x => Math.min(1 - c.pad, Math.max(c.pad, fr(x)));
        return [(c.cx + cl(u)) / c.cols, (c.cy + cl(v)) / c.rows];
    };

    const blocks = Array.from({ length: nblocks }, () => ({ verts: [], map: new Map(), tris: [] }));
    let auto = 0;
    for (const m of mdl.meshes) {
        const nv = m.pos.length / 3;
        const gpos = [], ginf = [];
        for (let v = 0; v < nv; v++) {
            const p = xform(place.toGame, [m.pos[v * 3], m.pos[v * 3 + 1], m.pos[v * 3 + 2]]);
            gpos.push(p);
            let l = m.inf ? m.inf[v].map(([b, w]) => [place.byFile[b], w]).filter(([b]) => b >= 0) : [];
            if (!l.length) { l = autoWeights(f, place.rest, p); auto++; }
            ginf.push(l);
        }
        const nt = m.tm.length;
        for (let t = 0; t < nt; t++) {
            const mi = m.tm[t], blk = blocks[blockOf[mi]];
            const tri = [];
            for (let c = 0; c < 3; c++) {
                const k = t * 3 + c, v = m.tv[k], p = gpos[v];
                const key = p.map(x => Math.round(x * 1e3)).join(',') + '|' + ginf[v].map(([b, w]) => b + ':' + w.toFixed(3)).join(',');
                let oi = blk.map.get(key);
                if (oi === undefined) {
                    oi = blk.verts.length;
                    blk.map.set(key, oi);
                    blk.verts.push({ p, n: [0, 0, 0], inf: ginf[v] });
                }
                const n = m.tn ? xdir(place.toGame, [m.tn[k * 3], m.tn[k * 3 + 1], m.tn[k * 3 + 2]]) : null;
                if (n) for (let q = 0; q < 3; q++) blk.verts[oi].n[q] += n[q];
                tri.push({ o: oi, uv: onSheet(mi, m.tuv[k * 2], m.tuv[k * 2 + 1]) });
            }
            blk.tris.push(tri);
        }
    }
    /* normals the file did not give: from the faces */
    for (const blk of blocks)
        for (const tri of blk.tris) {
            const [a, b, c] = tri.map(x => blk.verts[x.o]);
            if (a.n.some(x => x) && b.n.some(x => x) && c.n.some(x => x)) continue;
            const e1 = [0, 1, 2].map(i => b.p[i] - a.p[i]), e2 = [0, 1, 2].map(i => c.p[i] - a.p[i]);
            const n = [e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]];
            for (const v of [a, b, c]) for (let i = 0; i < 3; i++) v.n[i] += n[i];
        }
    for (const b of blocks) if (b.verts.length > 65535) throw new Error('a block has more than 65535 vertices');
    /* the game draws every block: an empty one keeps one invisible triangle */
    for (const b of blocks)
        if (!b.tris.length) {
            const p = place.rest[f.rig.bones.root].T;
            b.verts.push({ p, n: [0, 1, 0], inf: [[f.rig.bones.root, 1]] });
            b.tris.push([0, 0, 0].map(() => ({ o: 0, uv: [0, 0] })));
        }
    return {
        bytes: writeSkin(blocks, place.rest, f.rig.skin.counted !== false),
        verts: blocks.reduce((s, b) => s + b.verts.length, 0), tris: blocks.reduce((s, b) => s + b.tris.length, 0),
        kind: place.kind, matched: place.matched, of: f.names.length, auto, sheets,
    };
}

/* the fighter with another .skin: the same bones and animations */
export function withSkin(f, bytes) {
    const rig = makeRig(f.rig.bones, parseSkin(bytes), f.rig.anim);
    const topo = meshTopology(rig);
    return { ...f, rig, topo, weights: vertexWeights(rig, topo), fitted: null };
}

/* a sheet as PNG bytes */
export async function sheetPng(cv) {
    return new Uint8Array(await (await cv.convertToBlob({ type: 'image/png' })).arrayBuffer());
}
