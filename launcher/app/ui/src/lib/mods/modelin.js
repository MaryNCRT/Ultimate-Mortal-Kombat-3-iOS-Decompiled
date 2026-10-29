/* REPLACE MODEL, from any of three formats: an FBX (Blender's), a GLB (glTF
 * binary, read with three.js's GLTFLoader) or an OBJ (three.js's OBJLoader)
 * becomes one description --
 *
 *   { bones: [{ name, rest, parent }],    rest: global, 4x4 column-major;
 *                                         parent: index, -1 at the top
 *     meshes: [{ name, pos,               the vertices, in the file's space
 *                inf,                     per vertex [[bone, weight]], or null
 *                tv, tuv, tn, tm }],      per triangle: its three vertices,
 *                                         UVs (v down, as the game), normals
 *                                         (or null) and material
 *     materials: [{ name, image, file,    image: something to draw, or null
 *                   alpha }] }            alpha: it sees through somewhere
 *
 * -- and that becomes the fighter's .skin (buildSkin). A file with the
 * fighter's bones (the exporter's rig, its names kept) is fitted to them as
 * fbxin.js fits an FBX; one without (an OBJ, a statue) is stood where the
 * fighter stands, at its height, and each vertex weighted to the bones
 * nearest it. Each material keeps its own picture and the file's UVs as
 * they are (past 0..1 they repeat): a block's triangles are written grouped
 * by material, and the parts listed in the skin's .mats (the game's
 * runtime/skin_mats.c draws each with its texture, the transparent ones
 * last). */
import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';
import { OBJLoader } from 'three/addons/loaders/OBJLoader.js';
import { readFbx, fbxScene, similarity, writeSkin, clusters, globalM, geomM, mm, xform, xdir, kid, kids, plain, I4 } from './fbxin.js';
import { posedVerts, makeRig, meshTopology, vertexWeights, rotBetween, axisAngle, mul3, vmul, tr3, localOf, withOffsets, withRootY } from './rig.js';
export { parseBonesText } from './rig.js';
import { mapBones } from './retarget.js';
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
    for (const m of mdl.materials) m.alpha = !!m.alpha || seeThrough(m.image);
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

/* does a picture see through anywhere (looked at no larger than 256) */
export function seeThrough(image) {
    if (!image) return false;
    const w = image.width || image.naturalWidth, h = image.height || image.naturalHeight;
    const k = Math.min(1, 256 / Math.max(w, h));
    const c = new OffscreenCanvas(Math.max(1, Math.round(w * k)), Math.max(1, Math.round(h * k)));
    const g = c.getContext('2d', { willReadFrequently: true });
    g.drawImage(image, 0, 0, c.width, c.height);
    const d = g.getImageData(0, 0, c.width, c.height).data;
    for (let i = 3; i < d.length; i += 4) if (d[i] < 250) return true;
    return false;
}

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
    /* each bone's parent among them (past nulls between), -1 at the top */
    for (const [id, i] of boneOf) {
        let p = sc.models.get(id).parent;
        while (p !== null && p !== undefined && !boneOf.has(p)) p = sc.models.has(p) ? sc.models.get(p).parent : null;
        bones[i].parent = p !== null && p !== undefined ? boneOf.get(p) : -1;
    }

    /* a material: its colour and its pictures -- diffuse (what the game
     * draws), normal and specular (seen in the preview; the game's renderer
     * has no use for them) -- each embedded (Video Content) or a file beside
     * the FBX */
    const materials = [], matOf = new Map();
    async function texOf(id, what) {
        const link = sc.propLinks.find(l => l.parent === id && what.test(l.what || '') && sc.byId.get(l.child) && sc.byId.get(l.child).name === 'Texture');
        const tex = link && sc.byId.get(link.child);
        if (!tex) return { image: null, file: '' };
        const vid = (sc.childrenOf.get(link.child) || []).map(x => sc.byId.get(x)).find(v => v && v.name === 'Video');
        const raw = vid && kid(vid, 'Content') && kid(vid, 'Content').props[0];
        const file = String((kid(tex, 'RelativeFilename') || kid(tex, 'FileName') || { props: [''] }).props[0] || '');
        return { image: await picture(raw instanceof Uint8Array && raw.length ? raw : findBeside(beside, file)), file };
    }
    async function material(id) {
        if (matOf.has(id)) return matOf.get(id);
        const node = sc.byId.get(id);
        const P = kids(kid(node, 'Properties70'), 'P');
        const val = n => { const p = P.find(q => q.props[0] === n); return p ? p.props.slice(4).map(Number) : null; };
        const dc = val('DiffuseColor') || val('Diffuse'), df = val('DiffuseFactor');
        const k = df && Number.isFinite(df[0]) ? df[0] : 1;
        const m = { name: plain(node.props[1]), image: null, file: '',
            color: dc ? dc.slice(0, 3).map(x => Math.round(255 * Math.min(1, Math.max(0, (Number(x) || 0) * k)))) : null };
        matOf.set(id, materials.length);
        materials.push(m);
        const d = await texOf(id, /diffuse|basecolor|base_color|albedo/i);
        m.image = d.image;
        m.file = d.file;
        m.normal = (await texOf(id, /normal|bump/i)).image;
        m.specular = (await texOf(id, /specular|reflection|shininess|gloss/i)).image;
        return matOf.get(id);
    }
    /* a part with no material of its own: one, grey, named after it */
    function bare(name) {
        if (matOf.has('bare:' + name)) return matOf.get('bare:' + name);
        matOf.set('bare:' + name, materials.length);
        materials.push({ name, image: null, file: '', color: null });
        return matOf.get('bare:' + name);
    }
    /* a part that is not skinned but hangs under a bone (a helmet, a
     * weapon): it moves with that bone */
    const boneAbove = id => {
        for (let p = id; p !== null && p !== undefined; p = sc.models.has(p) ? sc.models.get(p).parent : null)
            if (boneOf.has(p)) return boneOf.get(p);
        return -1;
    };

    const meshes = [];
    for (const [gid, gn] of sc.byId) {
        if (gn.name !== 'Geometry' || gn.props[2] !== 'Mesh') continue;
        const modelId = (sc.parentOf.get(gid) || []).find(x => sc.models.has(x));
        const place = modelId !== undefined ? mm(globalM(sc, modelId, null, memo), geomM(sc.models.get(modelId))) : I4();
        const mats = [];
        for (const id of (modelId !== undefined && sc.childrenOf.get(modelId)) || [])
            if (sc.byId.get(id) && sc.byId.get(id).name === 'Material') mats.push(await material(id));
        if (!mats.length) mats.push(bare(modelId !== undefined ? sc.models.get(modelId).name : 'mesh'));
        const V = kid(gn, 'Vertices').props[0], PVI = kid(gn, 'PolygonVertexIndex').props[0];
        const nv = V.length / 3;
        const pos = [];
        for (let v = 0; v < nv; v++) pos.push(...xform(place, [V[v * 3], V[v * 3 + 1], V[v * 3 + 2]]));
        const mine = cl.filter(c => c.geom === gid);
        let inf = null;
        if (mine.length) {
            inf = Array.from({ length: nv }, () => []);
            for (const c of mine) c.idx.forEach((v, k) => { if (c.wt[k] > 0) inf[v].push([boneOf.get(c.bone), c.wt[k]]); });
        } else if (modelId !== undefined && boneAbove(modelId) >= 0) {
            const b = boneAbove(modelId);
            inf = Array.from({ length: nv }, () => [[b, 1]]);
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
    /* and which UV set it reads, moved by KHR_texture_transform if it says
     * so, and whether the material sees through */
    const matPic = (json.materials || []).map(m => {
        const t = m.pbrMetallicRoughness && m.pbrMetallicRoughness.baseColorTexture;
        const tex = t && json.textures && json.textures[t.index];
        const src = tex && (tex.source ?? (tex.extensions && Object.values(tex.extensions).map(e => e.source).find(x => x !== undefined)));
        const tt = t && t.extensions && t.extensions.KHR_texture_transform;
        const out = { image: null, file: '', set: (tt && tt.texCoord) ?? (t && t.texCoord) ?? 0, xf: tt || null,
            alpha: m.alphaMode === 'BLEND' || m.alphaMode === 'MASK' };
        if (src !== undefined) { out.image = images[src]; out.file = (json.images[src] && (json.images[src].name || json.images[src].uri)) || ''; }
        return out;
    });
    /* KHR_texture_transform: scale, then rotate, then offset */
    const moveUv = (xf, u, v) => {
        if (!xf) return [u, v];
        const s = xf.scale || [1, 1], r = xf.rotation || 0, o = xf.offset || [0, 0];
        const x = u * s[0], y = v * s[1], c = Math.cos(r), sn = Math.sin(r);
        return [c * x + sn * y + o[0], -sn * x + c * y + o[1]];
    };
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
    for (const [b, i] of boneOf) {
        let p = b.parent;
        while (p && !boneOf.has(p)) p = p.parent;
        bones[i].parent = p ? boneOf.get(p) : -1;
    }
    const materials = [], matOf = new Map();
    const material = m => {
        if (!matOf.has(m)) {
            const a = gltf.parser.associations.get(m);
            const p = a && a.materials !== undefined ? matPic[a.materials] : null;
            matOf.set(m, materials.length);
            const c = m.color ? m.color.clone().convertLinearToSRGB() : null;
            materials.push({ name: m.name || 'material' + materials.length, image: p ? p.image : null, file: p ? p.file : '',
                color: c ? [c.r, c.g, c.b].map(x => Math.round(255 * x)) : null,
                alpha: !!(p && p.alpha) || m.transparent || m.alphaTest > 0, set: p ? p.set : 0, xf: p ? p.xf : null });
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
        /* the UV set each material reads (three.js names TEXCOORD_n uv, uv1,
         * uv2, ...), else the first there is */
        const uvSet = s => g.attributes[s ? 'uv' + s : 'uv'] || U || g.attributes.uv1 || null;
        const tv = [], tuv = [], tn = nrm ? [] : null, tm = [];
        for (const gr of groups) {
            const mi = mats[Math.min(gr.materialIndex || 0, mats.length - 1)];
            const M = materials[mi], A = uvSet(M.set);
            for (let t = gr.start; t + 2 < Math.min(count, gr.start + gr.count); t += 3) {
                for (let c = 0; c < 3; c++) {
                    const v = index ? index[t + c] : t + c;
                    tv.push(v);
                    tuv.push(...(A ? moveUv(M.xf, A.getX(v), A.getY(v)) : [0, 0]));
                    if (tn) tn.push(nrm[v * 3], nrm[v * 3 + 1], nrm[v * 3 + 2]);
                }
                tm.push(mi);
            }
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
 * which of the fighter's rests: by its bones when they are the fighter's
 * or a humanoid rig's (retarget.js; fitted on the body's joints), else
 * stood on the fighter's feet at its height (the game is Y up, the fighter
 * facing +Z, as glTF and OBJ are), turned `turn` quarter turns. */
function placeModel(f, mdl, turn) {
    const map = mapBones(f, mdl);
    if (map) {
        let best = null;
        for (const [kind, pose] of [['tpose', f.tpose], ['bind', f.bind.pose]]) {
            if (!pose) continue;
            for (const core of [true, false]) {
                const P = [], Q = [], W = [];
                for (const [g, i] of map.prim) {
                    if (core && !map.core(g)) continue;
                    const m = mdl.bones[i].rest;
                    P.push(...pose[g].T); Q.push(m[12], m[13], m[14]); W.push(1);
                }
                if (W.length < 3) continue;
                const s = similarity(P, Q, W);
                if (!best || s.err < best.s.err) best = { kind, pose, s };
                break;
            }
        }
        if (best) return { toGame: best.s.Ginv, kind: best.kind, rest: best.pose, matched: map.prim.size, byFile: map.byFile, map };
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
    return { toGame, kind: f.tpose ? 'tpose' : 'bind', rest, matched: 0, byFile: mdl.bones.map(() => -1), map: null };
}

/* the file's joints in the game's space, per fighter bone */
function fileJoints(mdl, place) {
    const J = new Map();
    for (const [g, i] of place.map.prim) {
        const m = mdl.bones[i].rest;
        J.set(g, xform(place.toGame, [m[12], m[13], m[14]]));
    }
    return J;
}

const vsub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
const vlen = a => Math.hypot(a[0], a[1], a[2]);
const I3 = [1, 0, 0, 0, 1, 0, 0, 0, 1];

/* RETARGET (FIT TO THE FIGHTER): the model brought to the fighter's own
 * rest, bone by bone. Each fighter bone with a joint in the file takes the
 * file's segment (its joint to the next one's) onto its own: turned to
 * point the same way and stretched along it to the same length, so the
 * model fits the bones the game's animations were made for. An end bone
 * (hand, head, toe) turns with the bone it hangs from. A vertex is moved by
 * its weights, as a skin moves it. */
function conformers(place, J, rest) {
    const out = new Map();
    for (const [g, Pf] of J) {
        const c = (place.map.link.get(g) || { child: -1 }).child;
        const Pg = rest[g].T;
        if (c >= 0 && J.has(c)) {
            const df = vsub(J.get(c), Pf), dg = vsub(rest[c].T, Pg);
            const lf = vlen(df), lg = vlen(dg);
            if (lf > 1e-6 && lg > 1e-6) {
                out.set(g, { Pf, Pg, R: rotBetween(df, dg), ax: dg.map(x => x / lg), s: Math.min(2, Math.max(0.5, lg / lf)) });
                continue;
            }
        }
        out.set(g, { Pf, Pg, R: null, ax: null, s: 1 });
    }
    for (const [g, c] of out)
        if (!c.R) {
            const fr = (place.map.link.get(g) || { from: -1 }).from;
            c.R = (fr >= 0 && out.get(fr) && out.get(fr).R) || I3;
        }
    return out;
}
function conformPoint(conf, p, inf) {
    let sw = 0;
    const o = [0, 0, 0];
    for (const [g, w] of inf) {
        const c = conf.get(g);
        let q = p;
        if (c) {
            const d = vmul(vsub(p, c.Pf), c.R);
            if (c.ax) {
                const along = (c.s - 1) * (d[0] * c.ax[0] + d[1] * c.ax[1] + d[2] * c.ax[2]);
                for (let k = 0; k < 3; k++) d[k] += along * c.ax[k];
            }
            q = [c.Pg[0] + d[0], c.Pg[1] + d[1], c.Pg[2] + d[2]];
        }
        for (let k = 0; k < 3; k++) o[k] += w * q[k];
        sw += w;
    }
    return sw > 0 ? o.map(x => x / sw) : p;
}
function conformDir(conf, n, inf) {
    const o = [0, 0, 0];
    for (const [g, w] of inf) {
        const c = conf.get(g);
        const q = c ? vmul(n, c.R) : n;
        for (let k = 0; k < 3; k++) o[k] += w * q[k];
    }
    return o;
}

/* ARM / LEG ANGLE AND ROLL, and the T or A pose.
 *
 * The limbs: each side's upper arm and thigh, with every bone under it.
 * The file's pose is read from its joints: the upper arm's rise from level
 * (a T pose holds it near 0, an A pose some 30-50 degrees down) and the
 * thigh's spread from straight down, beside the fighter's own rest.
 *
 * o.adjust: { armAngle, armRoll, legAngle, legRoll } in degrees, the same
 * on both sides, mirrored; auto (default on): KEEP PROPORTIONS turns the
 * file's arms to the fighter's rise first, so an A-pose model plays the
 * game's moves with the arms where the fighter's are (FIT TO THE FIGHTER
 * turns each part to its bone anyway).
 *   angle  raises the arms / spreads the legs (negative: lowers, closes)
 *   roll   turns the limb about its own length
 * The roll and, keeping proportions, the angle turn the model itself (each
 * vertex by its weights, and its joints) before it is bound. Fitting, the
 * angle moves the fighter's joints the model is fitted to instead -- the
 * bones' offsets, written with the skin (<skin>.bones), so the game's
 * moves carry the limb at that angle. */
const DEG = Math.PI / 180;
const LIMB_OF = { upperarm: ['forearm', 'arm'], thigh: ['shin', 'leg'] };

function limbsOf(f) {
    const out = [], b = f.rig.bones;
    for (const [base, [next, kind]] of Object.entries(LIMB_OF))
        for (const sd of ['.L', '.R']) {
            const g = f.names.indexOf(base + sd), c = f.names.indexOf(next + sd);
            if (g < 0 || c < 0) continue;
            const sub = new Set();
            const walk = i => { sub.add(i); for (const k of b.children[i]) walk(k); };
            walk(g);
            out.push({ kind, g, c, sub });
        }
    return out;
}

/* an arm's rise from level, a leg's spread from straight down, degrees */
const riseOf = d => Math.atan2(d[1], Math.hypot(d[0], d[2])) / DEG;
const spreadOf = d => Math.atan2(Math.abs(d[0]), -d[1]) / DEG;
const angleOf = (kind, d) => (kind === 'arm' ? riseOf(d) : spreadOf(d));

/* The pose a file is in, read from its joints: 'T' or 'A' */
export function poseName(rise) {
    return rise > -20 ? 'T' : 'A';
}

function adjustments(f, mdl, place, o) {
    const J0 = fileJoints(mdl, place), rest = place.rest, a = o.adjust || {};
    const hips = f.names.indexOf('hips');
    const cx = hips >= 0 ? (J0.get(hips) || rest[hips].T)[0] : rest[f.rig.bones.root].T[0];
    const limbs = limbsOf(f);
    /* a limb's side: +1 or -1, so one setting moves both alike */
    const side = (L, P, d) => (L.kind === 'arm' ? Math.sign(d[0]) : Math.sign(P[0] - cx)) || 1;
    const sum = { file: { arm: [], leg: [] }, fighter: { arm: [], leg: [] } };

    const tf = new Map(), J = new Map(J0);
    for (const L of limbs) {
        const fd = vsub(rest[L.c].T, rest[L.g].T);
        sum.fighter[L.kind].push(angleOf(L.kind, fd));
        if (!J0.has(L.g) || !J0.has(L.c)) continue;
        const P = J0.get(L.g), d = vsub(J0.get(L.c), P), s = side(L, P, d);
        sum.file[L.kind].push(angleOf(L.kind, d));
        let ang = 0;
        if (o.keep) {
            ang = (L.kind === 'arm' ? a.armAngle : a.legAngle) || 0;
            if (L.kind === 'arm' && a.auto !== false) ang += riseOf(fd) - riseOf(d);
        }
        const roll = (L.kind === 'arm' ? a.armRoll : a.legRoll) || 0;
        if (!ang && !roll) continue;
        const l = vlen(d) || 1;
        const M = mul3(axisAngle(d.map(x => x / l), roll * s * DEG), axisAngle([0, 0, 1], ang * s * DEG));
        for (const g of L.sub) {
            tf.set(g, { M, P });
            if (J0.has(g)) { const q = vmul(vsub(J0.get(g), P), M); J.set(g, [q[0] + P[0], q[1] + P[1], q[2] + P[2]]); }
        }
    }

    /* fitting: the angle on the fighter's joints, as bone offsets */
    let rest2 = rest, ov = null;
    if (!o.keep)
        for (const L of limbs) {
            const ang = (L.kind === 'arm' ? a.armAngle : a.legAngle) || 0;
            if (!ang) continue;
            const P = rest[L.g].T, d = vsub(rest[L.c].T, P);
            const M = axisAngle([0, 0, 1], ang * side(L, P, d) * DEG);
            if (rest2 === rest) { rest2 = rest.slice(); ov = { offsets: new Map(), rootY: 1 }; }
            for (const g of L.sub) {
                if (g === L.g) continue;
                const q = vmul(vsub(rest[g].T, P), M);
                rest2[g] = { ...rest[g], T: [q[0] + P[0], q[1] + P[1], q[2] + P[2]] };
            }
            for (const g of L.sub)
                if (g !== L.g) {
                    const par = f.rig.bones.parent[g];
                    ov.offsets.set(g, vmul(vsub(rest2[g].T, rest2[par].T), tr3(rest2[par].R)));
                }
        }

    const mean = l => (l.length ? l.reduce((x, y) => x + y, 0) / l.length : null);
    const info = {
        file: { arm: mean(sum.file.arm), leg: mean(sum.file.leg) },
        fighter: { arm: mean(sum.fighter.arm), leg: mean(sum.fighter.leg) },
    };
    /* a vertex, a normal: through its bones' turns, by weight */
    const point = (p, inf) => {
        if (!tf.size) return p;
        let sw = 0;
        const out = [0, 0, 0];
        for (const [g, w] of inf) {
            const t = tf.get(g);
            const q = t ? vmul(vsub(p, t.P), t.M).map((x, k) => x + t.P[k]) : p;
            for (let k = 0; k < 3; k++) out[k] += w * q[k];
            sw += w;
        }
        return sw > 0 ? out.map(x => x / sw) : p;
    };
    const dir = (n, inf) => {
        if (!tf.size) return n;
        const out = [0, 0, 0];
        for (const [g, w] of inf) {
            const t = tf.get(g), q = t ? vmul(n, t.M) : n;
            for (let k = 0; k < 3; k++) out[k] += w * q[k];
        }
        return out;
    };
    return { J, rest: rest2, ov, info, point, dir };
}

/* PROPORTION TRICK (KEEP THE MODEL'S PROPORTIONS): the model is left as it
 * is and the fighter's bones are moved to its joints instead -- each bone's
 * offset from its parent (the .bones file's, which the game's animations
 * turn but never change) becomes the model's, written beside the skin as
 * <skin>.bones (runtime/skin_mats.c reads it). The game's animations then
 * play on the model's own lengths: long legs stay long. The root's height
 * in the animations is scaled by the model's hips over the fighter's, so
 * the feet stay on the floor. Returns the rest to bind to and the
 * override. */
function proportions(f, J, rest) {
    const bones = f.rig.bones;
    /* the root's height, as the game scales the animations' (about y 0,
     * the floor the fighters stand on): the model's hips over the
     * fighter's */
    const hips = f.names.indexOf('hips');
    let rootY = 1;
    if (hips >= 0 && J.has(hips) && rest[hips].T[1] > 1e-3 && J.get(hips)[1] > 1e-3)
        rootY = Math.min(3, Math.max(0.3, J.get(hips)[1] / rest[hips].T[1]));
    const out = new Array(bones.num), offs = new Map();
    const walk = i => {
        const p = bones.parent[i];
        if (p < 0) out[i] = { R: rest[i].R, T: [rest[i].T[0], rest[i].T[1] * rootY, rest[i].T[2]] };
        else {
            let T;
            if (J.has(i)) T = J.get(i);
            else {
                const v = vmul(localOf(rest, bones, i).T, rest[p].R);
                T = [out[p].T[0] + v[0], out[p].T[1] + v[1], out[p].T[2] + v[2]];
            }
            out[i] = { R: rest[i].R, T };
            if (J.has(i)) offs.set(i, vmul(vsub(T, out[p].T), tr3(rest[p].R)));
        }
        for (const c of bones.children[i]) walk(c);
    };
    walk(bones.root);
    return { rest: out, bones: { offsets: offs, rootY } };
}

/* The bones' offsets and the root's height times the scale: every bone's
 * (the proportion trick's where it has one, else the fighter's own) */
function scaledBones(f, ov, k) {
    if (k === 1) return ov;
    const b = f.rig.bones, offsets = new Map();
    for (let i = 0; i < b.num; i++) {
        if (i === b.root) continue;
        const v = (ov && ov.offsets.get(i)) || [b.offset[i * 3], b.offset[i * 3 + 1], b.offset[i * 3 + 2]];
        offsets.set(i, v.map(x => x * k));
    }
    return { offsets, rootY: (ov ? ov.rootY : 1) * k };
}

/* <skin>.bones: a comment, `rooty <scale>`, then `<bone> <x> <y> <z>` a line */
export function bonesText(ov) {
    const lines = ["# bone x y z: the model's offsets from the parent (REPLACE MODEL, proportion trick)", 'rooty ' + ov.rootY.toFixed(5)];
    for (const [i, v] of [...ov.offsets].sort((a, b) => a[0] - b[0])) lines.push([i, ...v.map(x => x.toFixed(5))].join(' '));
    return new TextEncoder().encode(lines.join('\n') + '\n');
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
 *   o.keep       a humanoid rig: keep the model's proportions (the
 *                proportion trick) instead of fitting it to the fighter's
 *   o.blockOf    per material, the fighter's block it goes to (default:
 *                one each in order, the rest sharing the last)
 *   o.adjust     a humanoid rig: { armAngle, armRoll, legAngle, legRoll,
 *                auto } (adjustments above)
 *   o.scale      the model's size, about its pivot: the floor under the
 *                fighter's root. Mesh, bones and the root's height together
 *                (written as <skin>.bones), so the animations play on it
 * Returns { bytes, verts, tris, kind, matched, of, auto (vertices weighted
 * here), parts: per block, its runs of triangles [{ mat, first, count }],
 * rig ('names', 'humanoid' or ''), bones (the proportion trick's override,
 * for <skin>.bones, or null), pose (a humanoid rig: the file's and the
 * fighter's arm rise and leg spread, degrees, or null) }. */
export function buildSkin(f, mdl, o = {}) {
    const nblocks = f.rig.skin.length;
    const place = placeModel(f, mdl, o.turn || 0);
    const human = place.map && place.map.kind === 'humanoid';
    const adj = human ? adjustments(f, mdl, place, o) : null;
    const prop = human && o.keep ? proportions(f, adj.J, place.rest) : null;
    const conf = human && !o.keep ? conformers(place, adj.J, adj.rest) : null;
    const k = o.scale && o.scale !== 1 ? o.scale : 1;
    const rest0 = prop ? prop.rest : adj ? adj.rest : place.rest;
    const rT = rest0[f.rig.bones.root].T;
    /* about the pivot: x and z about the root's, y about the floor */
    const scaled = p => (k === 1 ? p : [rT[0] + (p[0] - rT[0]) * k, p[1] * k, rT[2] + (p[2] - rT[2]) * k]);
    const bindRest = k === 1 ? rest0 : rest0.map(r => ({ ...r, T: scaled(r.T) }));
    const blockOf = mdl.materials.map((_, i) => (o.blockOf && o.blockOf[i] !== undefined ? o.blockOf[i] : Math.min(i, nblocks - 1)));

    /* a block's triangles, per material, put in a row at the end */
    const blocks = Array.from({ length: nblocks }, () => ({ verts: [], map: new Map(), tris: [], byMat: new Map() }));
    let auto = 0;
    for (const m of mdl.meshes) {
        const nv = m.pos.length / 3;
        const gpos = [], ginf = [];
        for (let v = 0; v < nv; v++) {
            let p = xform(place.toGame, [m.pos[v * 3], m.pos[v * 3 + 1], m.pos[v * 3 + 2]]);
            /* the file's weights on the fighter's bones, those that land on
             * the same bone summed */
            const sum = new Map();
            if (m.inf) for (const [b, w] of m.inf[v]) { const g = place.byFile[b]; if (g >= 0) sum.set(g, (sum.get(g) || 0) + w); }
            let l = [...sum];
            if (!l.length) { l = autoWeights(f, rest0, p); auto++; }
            else {
                if (adj) p = adj.point(p, l);
                if (conf) p = conformPoint(conf, p, l);
            }
            gpos.push(scaled(p));
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
                let n = m.tn ? xdir(place.toGame, [m.tn[k * 3], m.tn[k * 3 + 1], m.tn[k * 3 + 2]]) : null;
                if (n && adj && m.inf) n = adj.dir(n, ginf[v]);
                if (n && conf) n = conformDir(conf, n, ginf[v]);
                if (n) for (let q = 0; q < 3; q++) blk.verts[oi].n[q] += n[q];
                tri.push({ o: oi, uv: [m.tuv[k * 2], m.tuv[k * 2 + 1]] });
            }
            if (!blk.byMat.has(mi)) blk.byMat.set(mi, []);
            blk.byMat.get(mi).push(tri);
        }
    }
    /* the parts: { mat, first, count } per block, in the order written */
    const parts = blocks.map(blk => {
        const out = [];
        for (const [mat, tris] of [...blk.byMat].sort((a, b) => a[0] - b[0])) {
            out.push({ mat, first: blk.tris.length, count: tris.length });
            for (const t of tris) blk.tris.push(t);
        }
        return out;
    });
    /* normals the file did not give: from the faces */
    for (const blk of blocks)
        for (const tri of blk.tris) {
            const [a, b, c] = tri.map(x => blk.verts[x.o]);
            if (a.n.some(x => x) && b.n.some(x => x) && c.n.some(x => x)) continue;
            const e1 = [0, 1, 2].map(i => b.p[i] - a.p[i]), e2 = [0, 1, 2].map(i => c.p[i] - a.p[i]);
            const n = [e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]];
            for (const v of [a, b, c]) for (let i = 0; i < 3; i++) v.n[i] += n[i];
        }
    /* the game unwelds a block (a vertex a UV) into uint16 indices */
    for (const b of blocks) {
        const seen = new Set();
        for (const t of b.tris) for (const x of t) seen.add(x.o + '|' + x.uv[0] + ',' + x.uv[1]);
        if (Math.max(seen.size, b.verts.length) > 65535) throw new Error('a block has more than 65535 vertices once unwelded (' + seen.size + ')');
    }
    /* the game draws every block: an empty one keeps one invisible triangle */
    for (const b of blocks)
        if (!b.tris.length) {
            const p = bindRest[f.rig.bones.root].T;
            b.verts.push({ p, n: [0, 1, 0], inf: [[f.rig.bones.root, 1]] });
            b.tris.push([0, 0, 0].map(() => ({ o: 0, uv: [0, 0] })));
        }
    return {
        bytes: writeSkin(blocks, bindRest, f.rig.skin.counted !== false),
        verts: blocks.reduce((s, b) => s + b.verts.length, 0), tris: blocks.reduce((s, b) => s + b.tris.length, 0),
        kind: place.kind, matched: place.matched, of: f.names.length, auto, parts,
        rig: place.map ? place.map.kind : '', bones: scaledBones(f, prop ? prop.bones : adj && adj.ov, k),
        pose: adj ? adj.info : null,
    };
}

/* The skin's .mats (runtime/skin_mats.c): a line a part, `block first
 * count alpha texture`; texFile(mat) the part's picture file or '-' for
 * the block's own texture */
export function matsText(parts, mdl, texFile) {
    const lines = ['# block first count alpha texture (REPLACE MODEL)'];
    parts.forEach((ps, b) => {
        for (const p of ps) lines.push([b, p.first, p.count, mdl.materials[p.mat].alpha ? 1 : 0, texFile(p.mat)].join(' '));
    });
    return new TextEncoder().encode(lines.join('\n') + '\n');
}

/* the fighter with another .skin: the same bones and animations, or with
 * the proportion trick's offsets (`ov`, buildSkin's bones) */
export function withSkin(f, bytes, ov) {
    const rig = makeRig(ov ? withOffsets(f.rig.bones, ov) : f.rig.bones, parseSkin(bytes), ov ? withRootY(f.rig.anim, ov.rootY) : f.rig.anim);
    const topo = meshTopology(rig);
    return { ...f, rig, topo, weights: vertexWeights(rig, topo), fitted: null };
}

/* A material with no picture of its own: its colour (the file's, else a
 * grey) as a small picture, so the part has a texture of its own and never
 * draws with one the game shares with other models. */
export function solidPicture(color) {
    const c = new OffscreenCanvas(8, 8), g = c.getContext('2d');
    const [r, gr, b] = color || [180, 180, 180];
    g.fillStyle = 'rgb(' + r + ',' + gr + ',' + b + ')';
    g.fillRect(0, 0, 8, 8);
    return c;
}

/* a picture as PNG bytes */
export async function sheetPng(pic) {
    let cv = pic;
    if (!cv.convertToBlob) {
        cv = new OffscreenCanvas(pic.width || pic.naturalWidth, pic.height || pic.naturalHeight);
        cv.getContext('2d').drawImage(pic, 0, 0);
    }
    return new Uint8Array(await (await cv.convertToBlob({ type: 'image/png' })).arrayBuffer());
}
