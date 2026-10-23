/* What the MODS section shows and exports, from the game's own res/ folder:
 * the catalogue (fighters, stages, other models, textures, sounds), and each
 * thing turned into the exporters' description (export.js).
 *
 * `src` is the file access: { files: ['KANO_STANDARD.skin', 'Textures/x.pvr',
 * ...] relative to res/, read(path) -> Promise<Uint8Array|null> }. */
import { parseMeshset, parseSkin, parseBones, parseAnim, parseScene, parseFrameList, findClips, decodePvr } from './formats.js';
import { makeRig, bindPose, tPose, framePose, meshTopology, vertexWeights, posedVerts, boneNames, fitRest, vmul } from './rig.js';
import { toObj, toUsd, toFbx, encodePng, poseMeshes } from './export.js';

/* the fighters by the name the game files use (PlayerDefs, the roster) */
export const FIGHTERS = {
    KANO: 'Kano', SONYA: 'Sonya', JAX: 'Jax', NIGHTWOLF: 'Nightwolf', SUBZERO: 'Sub-Zero',
    STRYKER: 'Stryker', SINDEL: 'Sindel', SEKTOR: 'Sektor', CYRAX: 'Cyrax', KUNGLAO: 'Kung Lao',
    KABAL: 'Kabal', SHEEVA: 'Sheeva', SHANGTSUNG: 'Shang Tsung', LIUKANG: 'Liu Kang', SMOKE: 'Smoke',
    KITANA: 'Kitana', JADE: 'Jade', MILEENA: 'Mileena', SCORPION: 'Scorpion', REPTILE: 'Reptile',
    ERMAC: 'Ermac', OLDSUBZERO: 'Sub-Zero (classic)', OLDSMOKE: 'Smoke (human)', NOOBSAIBOT: 'Noob Saibot',
    MOTARO: 'Motaro', SHAOKAHN: 'Shao Kahn', DUMMY: 'Dummy',
};

const STAGE_NAMES = {
    BALCONY: 'The Balcony', BELLTOWER: 'Bell Tower', BRIDGE: 'The Bridge', CAVE: 'Cave',
    GRAVEYARD: 'The Graveyard', JADESDESERT: "Jade's Desert", LAIR: "Scorpion's Lair",
    NOOBSDORFEN: "Noob's Dorfen", PIT: 'The Pit III', ROOFTOP: 'Rooftop', SCISLACBUSOREZ: 'Scislac Busorez',
    SCISLAC: 'Scislac', SOULCHAMBER: 'Soul Chamber', STREET: 'The Street', SUBWAY: 'Subway',
    TEMPLE: 'Temple', WATERFRONT: 'Waterfront', KAHNSKAVE: "Kahn's Kave", KOMBATTEMPLE: 'Kombat Temple',
};

const title = s => s.toLowerCase().replace(/_/g, ' ').replace(/\b\w/g, c => c.toUpperCase());

export function catalog(files) {
    const set = new Set(files.map(f => f.toUpperCase()));
    const has = f => set.has(f.toUpperCase());
    const characters = [], stages = [], models = [], textures = [], sounds = [];
    const used = new Set();
    for (const f of files) {
        const m = /^([A-Z0-9]+)_STANDARD\.bones$/i.exec(f);
        /* ROBO1 and ROBO2 are out: a different exporter made them
         * (docs/formats/SKIN-FORMAT.md) and they do not pose; no fighter
         * uses them */
        if (m && !/^ROBO\d$/i.test(m[1]) && has(m[1] + '_STANDARD.skin') && has(m[1] + '_STANDARD.skinanim')) {
            const id = m[1].toUpperCase();
            characters.push({ id, file: m[1] + '_STANDARD', name: FIGHTERS[id] || title(id) });
            used.add((m[1] + '_STANDARD.meshset').toUpperCase());
        }
        const s = /^([A-Z0-9_]+)_LEVEL_SCENE\.scene$/i.exec(f);
        if (s && !/^IPAD_/i.test(s[1]) && has(s[1] + '_LEVEL_SCENE.meshset')) {
            const id = s[1].toUpperCase();
            stages.push({ id, scene: f, meshset: s[1] + '_LEVEL_SCENE.meshset', name: STAGE_NAMES[id] || title(id) });
            used.add((s[1] + '_LEVEL_SCENE.meshset').toUpperCase());
        }
    }
    for (const f of files) {
        if (/\.meshset$/i.test(f) && !used.has(f.toUpperCase()) && !f.includes('/'))
            models.push({ id: f, name: f.replace(/\.meshset$/i, '') });
        if (/^Textures\/[^/]+\.(pvr|png)$/i.test(f)) textures.push({ id: f, name: f.slice(9).replace(/\.(pvr|png)$/i, '') });
        if (/^audio\/[^/]+\.(wav|mp3)$/i.test(f)) sounds.push({ id: f, name: f.slice(6) });
    }
    const by = (a, b) => a.name.localeCompare(b.name);
    return { characters: characters.sort(by), stages: stages.sort(by), models: models.sort(by), textures: textures.sort(by), sounds: sounds.sort(by) };
}

/* ----------------------------------------------------------- textures -- */

/* the file the game would use for a texture named `stem` */
export function textureFile(files, stem) {
    const want = stem.toUpperCase();
    for (const dir of ['Textures/', ''])
        for (const ext of ['.PVR', '.PNG']) {
            const hit = files.find(f => f.toUpperCase() === dir.toUpperCase() + want + ext);
            if (hit) return hit;
        }
    return null;
}

/* { width, height, rgba } for a .pvr; a .png is decoded by the caller's
 * `decodePng` (the window's own decoder) */
/* by its bytes: a mod's .png can stand where the game has a .pvr */
export const isPng = b => b.length > 8 && b[0] === 0x89 && b[1] === 0x50 && b[2] === 0x4e && b[3] === 0x47;

export async function loadPicture(src, file, decodePng) {
    const b = await src.read(file);
    if (!b) return null;
    if (!isPng(b)) return decodePvr(b);
    return decodePng ? decodePng(b) : null;
}

/* the picture as PNG bytes: a .png as it is, a .pvr decoded and encoded */
export async function pngBytes(src, file) {
    const b = await src.read(file);
    if (!b) return null;
    if (isPng(b)) return b;
    const img = decodePvr(b);
    return img ? encodePng(img.width, img.height, img.rgba) : null;
}

/* ---------------------------------------------------------- fighters -- */

export async function loadFighter(src, ch) {
    const rd = async ext => {
        const b = await src.read(ch.file + ext);
        if (!b) throw new Error(ch.file + ext + ' is missing');
        return b;
    };
    const bones = parseBones(await rd('.bones'));
    const skin = parseSkin(await rd('.skin'));
    const anim = parseAnim(await rd('.skinanim'));
    const rig = makeRig(bones, skin, anim);
    const bind = bindPose(rig);
    const tpose = tPose(rig, bind.pose);
    const names = boneNames(rig, tpose || bind.pose);
    const topo = meshTopology(rig);
    const weights = vertexWeights(rig, topo);

    /* each skin block's texture: the .meshset mesh with the same triangle
     * count, else the mesh in the same place (Sindel's hair, block 1, is
     * mesh 1, "hair", SINDEL_HAIR_DIFF), else <NAME>_DIFFUSE */
    let meshset = [];
    try { meshset = parseMeshset((await src.read(ch.file + '.meshset')) || new Uint8Array(0)); } catch { /* none */ }
    /* A fighter's meshset can hold another's skin too: Reptile's and Noob
     * Saibot's carry a mesh textured SCORPION_DIFFUSE of the same size as
     * their own, and the game dresses the fighter in <NAME>_DIFFUSE
     * (Players.c), so the fighter's own texture wins. */
    const own = ch.id + '_DIFFUSE';
    const hasOwn = !!textureFile(src.files, own);
    const blockTex = skin.map((blk, i) => {
        const same = meshset.filter(m => m.faces.length / 3 === blk.numTris);
        const hit = same.find(m => m.texture && m.texture.toUpperCase().startsWith(ch.id)) || same[0] || meshset[i];
        const t = (hit && hit.texture) || own;
        return hasOwn && /_DIFFUSE\d*$/i.test(t) && !t.toUpperCase().startsWith(ch.id) ? own : t;
    });
    /* the outfits the game ships: <NAME>_DIFFUSE, _DIFFUSE2 (player two),
     * _ICE (frozen), ... -- the full-size ones */
    const base = blockTex[0];
    const variants = src.files
        .filter(f => f.toUpperCase().startsWith(('Textures/' + base).toUpperCase()) && !/_LOW\d*\.|_LITE\d*\./i.test(f))
        .map(f => f.slice(9).replace(/\.(pvr|png)$/i, ''))
        .filter((v, i, a) => a.indexOf(v) === i);

    /* What rides the fighter's bones (Kung Lao's hat on his head or in a
     * hand): the scene objects whose key names a bone, each a mesh of the
     * meshset, placed by its key's matrix and then the bone's, frame by
     * frame (RenderScene.c LIME_RenderScene, scene_attach_to_bone).
     * Particles and markers (a quad or two, EVENT...) are left out. */
    const attach = [];
    try {
        const sb = await src.read(ch.file + '.scene');
        const sc = sb && parseScene(sb);
        for (const obj of sc ? sc.objects : []) {
            const mi = meshset.findIndex(m => m.name === obj.name);
            const m = meshset[mi];
            if (!m || /^EVENT/i.test(m.name) || m.faces.length / 3 < 20) continue;
            if (!obj.keys.some(k => k.alpha > 0.03 && k.bone)) continue;
            const R = sc.palette.map(p => {
                const [x, y, z, w] = p.q, n = Math.hypot(x, y, z, w) || 1;
                return { R: rowQuat(x / n, y / n, z / n, w / n), s: p.s, t: p.t };
            });
            attach.push({ name: m.name, texture: m.texture, pos: m.pos, uv: m.uv, faces: m.faces, keys: obj.keys, pal: R });
        }
    } catch { /* no scene, or one this reader does not know */ }

    let frameNames = [];
    const fl = await src.read('framelists/' + ch.id.toLowerCase() + 'frames.txt');
    if (fl) frameNames = parseFrameList(new TextDecoder().decode(fl));
    const clips = findClips(frameNames, anim.frames);
    return { ch, rig, bind, tpose, names, topo, weights, blockTex, variants: variants.length ? variants : [base], clips, frameNames, attach };
}

/* opts: { rest: 'tpose' | 'bind', variant, clips: [index into f.clips] or
 * 'all', frame (OBJ: a frame to pose the mesh in, or null) } */
export function fighterDesc(f, opts) {
    const restPose = opts.rest === 'tpose' && f.tpose ? f.tpose : f.bind.pose;
    /* the bind pose is the mesh as the game skins it; any other rest is
     * fitted (rig.js, fitRest) */
    const all = restPose === f.bind.pose ? posedVerts(f.rig, f.topo, restPose)
        : (f.fitted ||= fitRest(f.rig, f.topo, f.weights, restPose));
    const meshes = [], materials = [];
    f.topo.blocks.forEach((blk, bi) => {
        /* the block's triangles, with its own vertices */
        const map = new Map(), pos = [], uv = [], idx = [], wt = [], faces = [];
        for (let t = blk.first * 3; t < (blk.first + blk.count) * 3; t++) {
            const o = f.topo.faces[t];
            let k = map.get(o);
            if (k === undefined) {
                k = map.size;
                map.set(o, k);
                pos.push(all[o * 3], all[o * 3 + 1], all[o * 3 + 2]);
                uv.push(f.topo.uv[o * 2], f.topo.uv[o * 2 + 1]);
                for (let j = 0; j < 4; j++) { idx.push(f.weights.idx[o * 4 + j]); wt.push(f.weights.wt[o * 4 + j]); }
            }
            faces.push(k);
        }
        let tex = f.blockTex[bi];
        if (opts.variant && bi === 0) tex = opts.variant;
        materials.push({ name: tex, texture: tex + '.png', stem: tex });
        meshes.push({ name: f.ch.file + (bi ? '_' + bi : ''), pos: Float32Array.from(pos), uv: Float32Array.from(uv),
            faces: Uint32Array.from(faces), material: bi, skin: { idx: Int32Array.from(idx), wt: Float32Array.from(wt) } });
    });
    const skeleton = { names: f.names, parent: Array.from(f.rig.bones.parent), rest: restPose };
    const pick = opts.clips === 'all' ? f.clips : (opts.clips || []).map(i => f.clips[i]).filter(Boolean);
    const clips = pick.map(c => ({ name: c.name, poses: Array.from({ length: c.to - c.from + 1 }, (_, k) => framePose(f.rig, c.from + k)) }));
    const desc = { name: f.ch.file, meshes, materials, skeleton, clips, fps: 30 };
    if (opts.frame !== null && opts.frame !== undefined)
        desc.meshes = poseMeshes(desc, framePose(f.rig, opts.frame));
    return desc;
}

/* ------------------------------------------------------------ stages --
 * The stage as RenderLevelBG draws it: every object of its scene that is
 * shown on the first frame (a key above 0.03), its mesh from the scene's
 * meshset, through its palette transform -- the mesh scaled, turned, moved,
 * as ConvertQSTMatrixtoPCMatrix builds it. The palette already stands the
 * scene up (node 0 is -90 degrees about X: the floor lies in X-Z, Y up), so
 * RenderLevelBG's own glRotatef(90, 1, 0, 0), which lays it back into the
 * game's Z-up world, is left out: the export is Y-up like the fighters.
 *
 * And it is brought to the fighters' scale: the game draws the stage at
 * SceneScale (0.0133, from the default; each stage's LEVEL_INFO may set
 * its own) and a fighter at PlayerSize (0.0102), so a stage unit is 1.31
 * fighter units. */
const STAGE_SCALE = 0.0133138057 / 0.0101562496;

export async function stageDesc(src, st, animated = false) {
    const scene = parseScene(await src.read(st.scene));
    const set = parseMeshset(await src.read(st.meshset));
    return animated ? animatedDesc(st.id + '_STAGE', scene, set) : placedDesc(st.id + '_STAGE', scene, set, 0);
}

/* The stage with its animation (issue #101): the scene's keys, frame by
 * frame. Each object that is ever shown is a bone of its own (no parent)
 * and its mesh is bound to it whole; a frame's key gives the bone its
 * palette transform -- turned, moved and scaled, as placedDesc places it --
 * and a key at alpha 0.03 or below (not drawn, RenderLevelBG) scales it to
 * nothing. The rest is the object's first shown key, without its scale:
 * the scale rides the animation, relative to that key's (FBX Lcl Scaling,
 * USD SkelAnimation scales). One clip, the scene's frames at 30 a second. */
function animatedDesc(name, scene, set) {
    const byName = new Map();
    for (const [i, m] of set.entries()) {
        if (!byName.has(m.name)) byName.set(m.name, i);
        if (!byName.has(m.name.toUpperCase())) byName.set(m.name.toUpperCase(), i);
    }
    const shown = k => k && k.alpha > 0.03 && scene.palette[k.pal];
    const xf = pal => {
        const [x, y, z, w] = pal.q, n = Math.hypot(x, y, z, w) || 1;
        return { R: rowQuat(x / n, y / n, z / n, w / n), T: pal.t.map(v => v * STAGE_SCALE) };
    };
    const meshes = [], materials = [], matOf = new Map(), used = new Map();
    const names = [], rest = [], objs = [];
    let frames = 1;
    for (const obj of scene.objects) {
        const mi = byName.get(obj.name) ?? byName.get(obj.name.toUpperCase());
        const first = obj.keys.find(shown);
        if (mi === undefined || !first) continue;
        frames = Math.max(frames, obj.keys.length);
        const m = set[mi], pal = scene.palette[first.pal], r = xf(pal), s0 = pal.s;
        const b = names.length;
        const pos = new Float32Array(m.pos.length);
        for (let i = 0; i < m.pos.length; i += 3) {
            const p = vmul([m.pos[i] * s0[0], m.pos[i + 1] * s0[1], m.pos[i + 2] * s0[2]], r.R);
            for (let k = 0; k < 3; k++) pos[i + k] = p[k] * STAGE_SCALE + r.T[k];
        }
        const tex = m.texture || '';
        if (!matOf.has(tex)) {
            matOf.set(tex, materials.length);
            materials.push({ name: tex || 'untextured', texture: tex ? tex + '.png' : '', stem: tex });
        }
        const nm0 = obj.name.replace(/\W+/g, '_') || 'object';
        const k = (used.get(nm0) || 0) + 1;
        used.set(nm0, k);
        const nm = k > 1 ? nm0 + '_' + k : nm0;
        const nv = m.pos.length / 3;
        const idx = new Int32Array(nv * 4), wt = new Float32Array(nv * 4);
        for (let v = 0; v < nv; v++) { idx[v * 4] = b; wt[v * 4] = 1; }
        meshes.push({ name: nm, pos, uv: m.uv, faces: m.faces, material: matOf.get(tex), skin: { idx, wt } });
        names.push(nm);
        rest.push(r);
        objs.push({ keys: obj.keys, s0 });
    }
    const poses = [];
    for (let f = 0; f < frames; f++)
        poses.push(objs.map((o, b) => {
            const key = o.keys[Math.min(f, o.keys.length - 1)];
            if (!shown(key)) return { R: rest[b].R, T: rest[b].T, S: [0, 0, 0] };
            const pal = scene.palette[key.pal], r = xf(pal);
            return { R: r.R, T: r.T, S: pal.s.map((v, i) => (o.s0[i] ? v / o.s0[i] : 1)) };
        }));
    const skeleton = { names, parent: names.map(() => -1), rest };
    return { name, meshes, materials, skeleton, clips: [{ name: 'Stage', poses }], fps: 30 };
}

function placedDesc(name, scene, set, frame) {
    const byName = new Map();
    for (const [i, m] of set.entries()) {
        if (!byName.has(m.name)) byName.set(m.name, i);
        if (!byName.has(m.name.toUpperCase())) byName.set(m.name.toUpperCase(), i);
    }
    const meshes = [], materials = [], matOf = new Map();
    const used = new Map();
    for (const obj of scene.objects) {
        const key = obj.keys[Math.min(frame, obj.keys.length - 1)];
        if (!key || key.alpha <= 0.03) continue;
        const mi = byName.get(obj.name) ?? byName.get(obj.name.toUpperCase());
        const pal = scene.palette[key.pal];
        if (mi === undefined || !pal) continue;
        const m = set[mi];
        const [x, y, z, w] = pal.q;
        const n = Math.hypot(x, y, z, w) || 1;
        const R = rowQuat(x / n, y / n, z / n, w / n);
        const pos = new Float32Array(m.pos.length);
        for (let i = 0; i < m.pos.length; i += 3) {
            const p = vmul([m.pos[i] * pal.s[0], m.pos[i + 1] * pal.s[1], m.pos[i + 2] * pal.s[2]], R);
            for (let k = 0; k < 3; k++) pos[i + k] = (p[k] + pal.t[k]) * STAGE_SCALE;
        }
        const tex = m.texture || '';
        if (!matOf.has(tex)) {
            matOf.set(tex, materials.length);
            materials.push({ name: tex || 'untextured', texture: tex ? tex + '.png' : '', stem: tex });
        }
        const nm = obj.name.replace(/\W+/g, '_') || 'object';
        const k = (used.get(nm) || 0) + 1;
        used.set(nm, k);
        meshes.push({ name: k > 1 ? nm + '_' + k : nm, pos, uv: m.uv, faces: m.faces, material: matOf.get(tex) });
    }
    return { name, meshes, materials, fps: 30 };
}

/* An attachment's vertices (loadFighter's `attach`) in a pose, or null when
 * it is not shown. `frame` is the animation frame the pose is; below 0 a
 * rest pose, where it shows if the animation mostly shows it (the hat on
 * the head, not the one in a hand). */
export function attachVerts(a, frame, pose) {
    let key;
    if (frame >= 0)
        key = a.keys[Math.min(frame, a.keys.length - 1)];
    else {
        const on = a.keys.filter(k => k.alpha > 0.03 && k.bone);
        if (on.length * 2 < a.keys.length) return null;
        key = on[0];
    }
    if (!key || key.alpha <= 0.03 || !key.bone || !pose[key.bone]) return null;
    const P = a.pal[key.pal], B = pose[key.bone];
    if (!P) return null;
    const out = new Float32Array(a.pos.length);
    for (let i = 0; i < a.pos.length; i += 3) {
        const q = vmul([a.pos[i] * P.s[0], a.pos[i + 1] * P.s[1], a.pos[i + 2] * P.s[2]], P.R);
        const r = vmul([q[0] + P.t[0], q[1] + P.t[1], q[2] + P.t[2]], B.R);
        out[i] = r[0] + B.T[0]; out[i + 1] = r[1] + B.T[1]; out[i + 2] = r[2] + B.T[2];
    }
    return out;
}

const rowQuat = (x, y, z, w) => {
    const xx = 2 * x * x, yy = 2 * y * y, zz = 2 * z * z, xy = 2 * x * y, xz = 2 * x * z, yz = 2 * y * z;
    const wx = 2 * w * x, wy = 2 * w * y, wz = 2 * w * z;
    return [1 - (yy + zz), xy + wz, xz - wy, xy - wz, 1 - (xx + zz), yz + wx, xz + wy, yz - wx, 1 - (xx + yy)];
};

/* -------------------------------------------------------- other models --
 * A .meshset on its own (finishers, effects, props): its meshes as they are
 * stored, side by side; `only` picks some by index. */
export async function meshsetDesc(src, file, only) {
    const set = parseMeshset(await src.read(file));
    const meshes = [], materials = [], matOf = new Map();
    set.forEach((m, i) => {
        if (only && !only.includes(i)) return;
        const tex = m.texture || '';
        if (!matOf.has(tex)) {
            matOf.set(tex, materials.length);
            materials.push({ name: tex || 'untextured', texture: tex ? tex + '.png' : '', stem: tex });
        }
        meshes.push({ name: (m.name || 'mesh').replace(/\W+/g, '_') + '_' + i, pos: m.pos, uv: m.uv, faces: m.faces, material: matOf.get(tex) });
    });
    return { name: file.replace(/\.meshset$/i, ''), meshes, materials, fps: 30 };
}

/* ------------------------------------------------------------ writing --
 * The files for one export: the model in `format` (obj, fbx, usda, usdc,
 * usdz) and its textures as PNG beside it -- inside it, for usdz.
 * { name: bytes }. */
export async function exportFiles(src, desc, format) {
    const pics = {};
    for (const mat of desc.materials) {
        const file = mat.stem && textureFile(src.files, mat.stem);
        const png = file && await pngBytes(src, file);
        if (png) pics[mat.texture] = png;
        else mat.texture = '';
    }
    if (format === 'usdz') return toUsd(desc, 'usdz', pics);       /* the pictures inside */
    const out = format === 'fbx' ? toFbx(desc) : /^usd[ac]?$/.test(format) ? toUsd(desc, format === 'usd' ? 'usda' : format) : toObj(desc);
    return Object.assign(out, pics);
}
