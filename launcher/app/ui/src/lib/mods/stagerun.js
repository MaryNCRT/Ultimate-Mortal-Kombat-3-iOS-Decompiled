/* A stage as the engine plays it: STAGE VIEWER, the stage export and the
 * STAGE SELECT pictures all draw from here.
 *
 *   - the scene (RenderScene.c LIME_RenderScene): one node a scene object, on
 *     the frame AnimateBG has reached -- one frame a game tick, 30 a second;
 *     a frame whose key is not above 0.03 is hidden, a mesh named EVENT... is
 *     a marker and never drawn. A key at 0.97 or more is drawn in the opaque
 *     pass (no blending; ALPHA... blends with the texture's alpha and writes
 *     no depth, ATST... keeps texels above 0.9); below 0.97 it goes to the
 *     transparent list, drawn ADDITIVELY (GL_SRC_ALPHA, GL_ONE) at that alpha
 *     with no depth writes and no culling (RenderMeshSingle left it off);
 *   - the effects (Events.c): the scene's .events tracks fire when the frame
 *     reaches a key, each one spawning its own scene placed by the key's
 *     matrix -- the fire, the smoke, a whole sky (Scislac's vortex is one).
 *     They play by their own rules (step, repeat, repeat2, delay, cap, group),
 *     tinted by the track's colour, and fire their own events in turn;
 *   - the level's own draws (GameCode.c RenderLevelBG): the Temple's window
 *     glow, 32 additive slices of TEMPLEWINDOW.meshset;
 *   - the light: a stage's .lights says unlit (all of them, the fourth word
 *     0), an effect's may say lit, and then its meshset's .lighting bytes
 *     plus the level's ambient colour the vertices (CreateFadedRGBS), unless
 *     its texture is in nolight.txt.
 *
 * The numbers per level are LEVEL_INFO's (0x0014e8d4, 0xf4 a level), read
 * from the binary: the scene's place and scale, whether it loops, the
 * second scene of the levels that have one (the Subway under the Street,
 * the Soul Chamber under the Balcony, the Lair under the Kave) and the
 * players' ambient light, which effects take.
 *
 * Space: the game's world is z-up and the scene is drawn through
 * RealBGSceneMatrix (moved, turned 90 degrees about x, scaled). Out of here
 * it is the scene's own y-up space times the fighters' scale -- a stage unit
 * is SceneScale / PlayerSize fighter units -- as the export and the viewer
 * have always had it. Matrices are row form (v * M), 16 floats, the
 * translation at 12..14, as the engine keeps them. */
import * as THREE from 'three';
import { parseScene, parseMeshset, texStem, decodePvr } from './formats.js';
import { textureFile, isPng } from './assets.js';
import { localMatrix, shapeAt, frameOf } from './stageedit.js';

export const PLAYER_SIZE = 0.0101562496;
export const TICKS_PER_SECOND = 30;
const SLOTS = 192;                      /* the event pool, 0xBA00 / 0xf8 */
const KILL = 100;                       /* EVENT_KILL_VALUE */
const KEY_MIN = 0.03, OPAQUE_MIN = 0.97;

/* LEVEL_INFO, per scene. pos/scale +0x0c..+0x18, loops +0x20, ambient
 * +0x00 (scene 1) or +0x34 (scene 2); a second scene's own place +0x40..
 * +0x4c, inside the first's. */
const LV = (pos, scale, loops, amb) => ({ pos, scale, loops, amb });
const STREET_S = 0.010651, STAGE_S = 0.013314;
export const LEVELS = {
    BALCONY:     LV([128, 0, -98.2], STAGE_S, 0, [0.495, 0.605, 0.33]),
    TEMPLE:      LV([235, 0, -98.2], STAGE_S, 1, [0.495, 0.44, 0.275]),
    GRAVEYARD:   LV([235, 0, -98.2], STAGE_S, 0, [0.44, 0.44, 0.44]),
    PIT:         LV([128, 0, -98.2], STAGE_S, 0, [0.44, 0.44, 0.44]),
    STREET:      LV([128, 0, -98.2], STREET_S, 1, [0.7, 0.7, 0.7]),
    NOOBSDORFEN: LV([128, 0, -98.2], STAGE_S, 0, [0.495, 0.605, 0.33]),
    ROOFTOP:     LV([128, 0, -98.2], STAGE_S, 0, [0.5, 0.4, 0.4]),
    BELLTOWER:   LV([128, 0, -98.2], STAGE_S, 0, [0.55, 0.55, 0.55]),
    BRIDGE:      LV([128, 0, -98.2], STAGE_S, 1, [0.4, 0.4, 0.4]),
    CAVE:        LV([128, 0, -98.2], STAGE_S, 0, [0.5, 0.4, 0.4]),
    JADESDESERT: LV([128, 0, -98.2], STAGE_S, 0, [0.7, 0.7, 0.7]),
    WATERFRONT:  LV([128, 0, -98.2], STAGE_S, 0, [0.4, 0.4, 0.4]),
    SCISLAC:     LV([128, 0, -98.2], STAGE_S, 0, [0.3, 0.3, 0.5]),
    /* the second scenes */
    SUBWAY:      Object.assign(LV([128, 0, -98.2], STREET_S, 1, [0.715, 0.385, 0.385]), { inner: [0, -1173, 151, 1] }),
    SOULCHAMBER: Object.assign(LV([128, 0, -98.2], STAGE_S, 0, [0.44, 0.44, 0.44]), { inner: [0, -938.4, 0, 1] }),
    LAIR:        Object.assign(LV([128, 0, -98.2], STAGE_S, 1, [0.55, 0.55, 0.44]), { inner: [0, -938.4, 0, 1] }),
};
const LEVEL_DEFAULT = LV([128, 0, -98.2], STAGE_S, 1, [0.5, 0.5, 0.5]);
export const levelOf = id => LEVELS[id] || LEVEL_DEFAULT;

/* ------------------------------------------------------------ matrices -- */

const ident = () => { const m = new Float32Array(16); m[0] = m[5] = m[10] = m[15] = 1; return m; };
export function mul(a, b, out = new Float32Array(16)) {
    const r = new Float32Array(16);
    for (let i = 0; i < 4; i++)
        for (let j = 0; j < 4; j++)
            r[i * 4 + j] = a[i * 4] * b[j] + a[i * 4 + 1] * b[4 + j] + a[i * 4 + 2] * b[8 + j] + a[i * 4 + 3] * b[12 + j];
    out.set(r);
    return out;
}
const trans = (x, y, z) => { const m = ident(); m[12] = x; m[13] = y; m[14] = z; return m; };
const scale = s => { const m = ident(); m[0] = m[5] = m[10] = s; return m; };
function invert(m) {
    const t = new THREE.Matrix4().fromArray(m).invert();
    return Float32Array.from(t.elements);
}
/* a palette entry (ConvertQSTMatrixtoPCMatrix): scaled, turned, moved */
export function palMatrix(p) {
    let [x, y, z, w] = p.q;
    const n = Math.hypot(x, y, z, w) || 1;
    x /= n; y /= n; z /= n; w /= n;
    const xx = 2 * x * x, yy = 2 * y * y, zz = 2 * z * z, xy = 2 * x * y, xz = 2 * x * z, yz = 2 * y * z;
    const wx = 2 * w * x, wy = 2 * w * y, wz = 2 * w * z;
    const R = [1 - (yy + zz), xy + wz, xz - wy, xy - wz, 1 - (xx + zz), yz + wx, xz + wy, yz - wx, 1 - (xx + yy)];
    const m = new Float32Array(16);
    for (let r = 0; r < 3; r++) for (let c = 0; c < 3; c++) m[r * 4 + c] = R[r * 3 + c] * p.s[r];
    m[12] = p.t[0]; m[13] = p.t[1]; m[14] = p.t[2]; m[15] = 1;
    return m;
}
/* RealBGSceneMatrix (AnimateBG): moved, turned PI/2 about x, scaled; a
 * second scene's moved and scaled again inside it */
function realBG(lv) {
    const rot = ident();
    rot[5] = 0; rot[6] = 1; rot[9] = -1; rot[10] = 0;
    let m = mul(mul(trans(lv.pos[0], lv.pos[1], lv.pos[2]), rot), scale(lv.scale));
    if (lv.inner) {
        const [x, y, z, s] = lv.inner;
        m = mul(mul(trans(x, y, z), scale(s)), m);
    }
    return m;
}

/* --------------------------------------------------------------- files -- */

function fileIn(files, name) {
    const want = name.toUpperCase();
    return files.find(f => f.toUpperCase() === want) || null;
}
async function readF(src, name) {
    const f = fileIn(src.files, name);
    return f ? src.read(f) : null;
}
const f32s = (b, off, n) => { const d = new DataView(b.buffer, b.byteOffset, b.byteLength); return Array.from({ length: n }, (_, i) => d.getFloat32(off + i * 4, true)); };
function cstr(b, off, len) { let s = ''; for (let i = 0; i < len && b[off + i]; i++) s += String.fromCharCode(b[off + i]); return s; }

/* .events (Events.c LIME_LoadEvents): per track a 0x10c header and 0x38
 * a key -- the frame, a word, a 48-byte DS matrix (int / 4096) */
function parseEvents(b) {
    if (!b || b.length < 4) return [];
    const d = new DataView(b.buffer, b.byteOffset, b.byteLength);
    const n = d.getInt32(0, true), out = [];
    let o = 4;
    for (let i = 0; i < n && o + 0x10c <= b.length; i++) {
        const nk = d.getInt32(o + 0x108, true), keys = [];
        const t = {
            scene: cstr(b, o, 64).toUpperCase(),
            color: f32s(b, o + 0x40, 4),
            repeat2: d.getInt32(o + 0x54, true),     /* +0x1c */
            delay: d.getInt32(o + 0x5c, true),       /* +0x6c */
            follow: d.getInt32(o + 0x6c, true),      /* +0x24 */
            step: d.getFloat32(o + 0xb0, true),      /* +0xc4 */
            repeat: d.getInt32(o + 0xb4, true),      /* +0x70 */
            cap: d.getInt32(o + 0xb8, true),         /* +0xd0 */
            shadow: d.getInt32(o + 0xc0, true),      /* +0x78 */
            group: d.getInt32(o + 0xc4, true),       /* +0x7c */
            slot: cstr(b, o + 0xc8, 64),
            keys,
        };
        o += 0x10c;
        for (let k = 0; k < nk; k++, o += 0x38) {
            const m = new Float32Array(16);
            for (let r = 0; r < 4; r++) for (let c = 0; c < 3; c++) m[r * 4 + c] = d.getInt32(o + 8 + (r * 3 + c) * 4, true) / 4096;
            m[15] = 1;
            keys.push({ frame: d.getInt32(o, true), m });
        }
        out.push(t);
    }
    return out;
}

/* MASTER.eventoffsets: a count, then a 64-byte name and x y z */
async function masterOffsets(src) {
    const b = await readF(src, 'MASTER.eventoffsets');
    const map = new Map();
    if (!b) return map;
    const n = new DataView(b.buffer, b.byteOffset).getInt32(0, true);
    for (let i = 0; i < n; i++) {
        const o = 4 + i * 0x4c;
        if (o + 0x4c > b.length) break;
        map.set(cstr(b, o, 64), f32s(b, o + 64, 3));
    }
    return map;
}

async function fullBright(src) {
    const b = await readF(src, 'nolight.txt');
    const set = new Set();
    if (b) for (const l of new TextDecoder().decode(b).split(/\r?\n/)) {
        const s = l.trim();
        if (s && s[0] !== '#') set.add(texStem(s).toUpperCase());
    }
    return set;
}

/* One scene with everything LIME_LoadScene brings along: its meshset, its
 * .events (and the scenes they spawn, loaded the same way), .offsets and
 * STATICLIGHTING .lights / .lighting. */
async function loadScene(src, file, ctx) {
    const key = file.toUpperCase();
    if (ctx.scenes.has(key)) return ctx.scenes.get(key);
    const b = await readF(src, file);
    if (!b) { ctx.scenes.set(key, null); return null; }
    const stem = file.replace(/\.scene$/i, '');
    const sc = parseScene(b);
    const node = { file: stem, frames: Math.max(1, sc.frames), objects: sc.objects, palette: sc.palette, set: [], tracks: [], pos: [0, 0, 0], lit: false };
    ctx.scenes.set(key, node);
    const ms = await readF(src, stem + '.meshset');
    node.set = ms ? parseMeshset(ms) : [];
    const offs = await readF(src, stem + '.offsets');
    if (offs && offs.length >= 12) node.pos = f32s(offs, 0, 3);
    const lights = await readF(src, 'STATICLIGHTING/' + stem + '.lights');
    if (lights && lights.length >= 16 && Math.trunc(f32s(lights, 12, 1)[0]) !== 0) {
        node.lit = true;
        const lb = await readF(src, 'STATICLIGHTING/' + stem + '.lighting');
        let at = 0;
        for (const m of node.set) {
            const nv = m.pos.length / 3;
            m.light = lb && at + nv <= lb.length ? lb.subarray(at, at + nv) : null;
            at += nv;
        }
    }
    const byName = new Map();
    node.set.forEach((m, i) => { if (!byName.has(m.name)) byName.set(m.name, i); if (!byName.has(m.name.toUpperCase())) byName.set(m.name.toUpperCase(), i); });
    for (const o of node.objects) {
        const mi = byName.get(o.name) ?? byName.get(o.name.toUpperCase());
        o.mesh = mi === undefined ? -1 : mi;
        o.marker = o.mesh >= 0 && /^EVENT/.test(node.set[o.mesh].name);
        o.mats = o.keys.map(k => (k.alpha > KEY_MIN && sc.palette[k.pal] ? k.pal : -1));
    }
    node.palM = sc.palette.map(palMatrix);
    node.tracks = parseEvents(await readF(src, stem + '.events'));
    for (const t of node.tracks) {
        t.offset = ctx.offsets.get(t.slot) || [0, 0, 0];
        t.sceneNode = t.keys.length ? await loadScene(src, t.scene + '.scene', ctx) : null;
    }
    return node;
}

/* ----------------------------------------------------------- the stage -- */

export async function loadStage(src, st) {
    const lv = levelOf(st.id);
    const ctx = { scenes: new Map(), offsets: await masterOffsets(src) };
    const root = await loadScene(src, st.scene, ctx);
    if (!root) throw new Error(st.scene + ' is missing');
    const stage = { id: st.id, level: lv, root, bright: await fullBright(src), extras: [] };
    /* the Temple's window glow (RenderLevelBG, level 1, ExtraEffects on) */
    if (st.id === 'TEMPLE') {
        const b = await readF(src, 'TEMPLEWINDOW.meshset');
        const set = b ? parseMeshset(b) : [];
        if (set.length) for (let i = 0; i < 32; i++)
            stage.extras.push({ name: 'GLASS_SLICE_' + (i + 1), mesh: set[0], texture: 'WINDOWGLOW2', alpha: (1 + i * -0.03125) / 10,
                world: mul(trans(320, 361 - 40 * i, 4), scale(0.0095)) });
    }
    stage.bg = realBG(lv);
    stage.toOut = mul(invert(stage.bg), scale(lv.scale / PLAYER_SIZE));
    stage.outScale = lv.scale / PLAYER_SIZE;
    return stage;
}

/* The game's tick on a stage: AnimateBG, then LIME_UpdateEvents, then what
 * LIME_RenderEvents does to the events before drawing them (a following
 * event follows, or dies with what it follows). */
export class StageSim {
    constructor(stage) {
        this.stage = stage;
        this.pool = Array.from({ length: SLOTS }, () => ({ state: 0, world: new Float32Array(16), local: new Float32Array(16) }));
        this.reset();
    }
    reset() {
        this.frame = 0;
        this.ctl = -1;
        this.ticks = 0;
        this.serial = 0;
        for (const e of this.pool) e.state = 0;
    }
    trigger(scene, frame, m1) {
        let f = frame % scene.frames;
        if (f < 0) f = 0;
        for (const t of scene.tracks)
            for (const k of t.keys)
                if (k.frame === f) this.spawn(t, m1, k.m);
    }
    spawn(t, m1, m2) {
        const slot = this.pool.findIndex(e => e.state === 0);
        if (slot < 0 || !t.sceneNode) return;
        if (t.cap !== -1 && this.pool.filter(e => e.state > 0 && e.track === t).length >= t.cap) return;
        if (t.group !== 0) for (const e of this.pool) if (e.state > 0 && e.group === t.group) this.kill(e);
        const e = this.pool[slot];
        Object.assign(e, {
            state: 1, id: ++this.serial, track: t, scene: t.sceneNode, group: t.group, color: t.color,
            repeat2: t.repeat2, delay: t.delay, step: t.step, repeat: t.repeat, follow: t.follow,
            frameA: 0, frameB: -1, cursor: 0, followM: m1, off: t.offset,
        });
        mul(m2, m1, e.world);
        e.local.set(m2);
        if (t.shadow) { e.follow = 0; e.world[14] = 0; e.step = 1; }   /* ShadowOffset is 0 on a stage */
    }
    kill(e) { e.state = -2; e.world[15] = KILL; e.local[15] = KILL; }
    tick() {
        const root = this.stage.root, lv = this.stage.level;
        let go = true;
        if (!lv.loops) {
            const last = root.frames - 1;
            if (this.frame >= last) { this.frame = last; go = false; }
        }
        if (go) {
            const f = Math.trunc(this.frame);
            if (f !== this.ctl) { this.trigger(root, f, this.stage.bg); this.ctl = f; }
            this.frame += 1;
        }
        for (const e of this.pool) {
            if (e.state === 0) continue;
            if (e.state < 0) { e.state++; continue; }
            if (e.delay !== 0) {
                e.delay--;
                if (e.delay < 0) e.delay = 0;
                else if (e.delay !== 0) continue;
            }
            const f = Math.trunc(e.cursor);
            e.frameA = f;
            if (f !== e.frameB) this.trigger(e.scene, f, e.world);
            e.frameB = e.frameA;
            const n = e.scene.frames;
            if (e.frameA < n - 1) e.cursor += e.step;
            else if (e.repeat !== 0) {
                e.cursor = n - 1; e.frameA = e.frameB = n - 1;
                if (e.repeat !== -1) e.repeat--;
            } else if (e.repeat2 === 0) {
                this.kill(e);
                e.cursor += e.step;
            } else {
                e.cursor -= n; e.frameA -= n;
                if (e.repeat2 > 0) e.repeat2--;
                e.cursor += e.step;
            }
        }
        /* LIME_RenderEvents' own part */
        for (const e of this.pool) {
            if (e.state <= 0 || e.delay !== 0 || e.follow !== 1) continue;
            if (e.followM[15] === KILL) { this.kill(e); continue; }
            mul(e.local, e.followM, e.world);
        }
        this.ticks++;
    }
    seek(ticks) {
        if (ticks < this.ticks) this.reset();
        while (this.ticks < ticks) this.tick();
    }
    /* What is drawn now, in draw order: { key, scene, obj index, mesh, world
     * (row form, the game's world), alpha, mode, tint, lit } */
    draws() {
        const out = [], st = this.stage;
        const sceneDraws = (sc, frame, world, tint, owner) => {
            /* LIME_RenderScene: frame % n, below 0 taken as 0 (an effect
             * wrapping back one length sits on -1 for a tick) */
            const fa = Math.max(0, frame % sc.frames);
            sc.objects.forEach((o, i) => {
                if (o.mesh < 0 || o.marker) return;
                const pal = o.mats[Math.min(fa, o.mats.length - 1)];
                if (pal < 0) return;
                const k = o.keys[Math.min(fa, o.keys.length - 1)];
                const m = sc.set[o.mesh];
                const nm = m.name.toUpperCase();
                const mode = k.alpha >= OPAQUE_MIN ? (nm.startsWith('ALPHA') ? 'alpha' : nm.startsWith('ATST') ? 'atst' : 'opaque') : 'add';
                out.push({ key: owner + ':' + i, owner, scene: sc, obj: i, mesh: m, world: mul(sc.palM[pal], world), alpha: k.alpha, mode, tint,
                    lit: mode !== 'add' && sc.lit && !!m.light && !st.bright.has((m.texture || '').toUpperCase()) });
            });
        };
        sceneDraws(st.root, Math.trunc(this.frame), st.bg, [1, 1, 1], 'stage');
        st.extras.forEach((x, i) => out.push({ key: 'extra:' + i, owner: 'extra', extra: x, mesh: x.mesh, world: x.world, alpha: x.alpha, mode: 'glow', tint: [1, 1, 1], lit: false }));
        for (const e of this.pool) {
            if (e.state <= 0 || e.delay !== 0) continue;
            const p = e.scene.pos, o = e.off;
            const w = mul(mul(e.world, trans(o[0], o[1], o[2])), trans(p[0], p[1], p[2]));
            sceneDraws(e.scene, e.frameA, w, e.color, 'ev' + e.id);
        }
        return out;
    }
}

/* the vertices' colours for a lit mesh: CreateFadedRGBS at level 1.0 */
function litColours(m, amb) {
    const nv = m.pos.length / 3, c = new Float32Array(nv * 3);
    for (let v = 0; v < nv; v++)
        for (let k = 0; k < 3; k++) {
            const x = Math.min(255, Math.trunc(amb[k] * 255 + m.light[v]));
            c[v * 3 + k] = ((x * 255) >> 8) / 255;
        }
    return c;
}

/* -------------------------------------------------------------- three -- */

/* The textures as the game samples them: raw values (no colour management,
 * so blending happens on the same numbers GL blends), v as stored. */
export function stageTextures(src) {
    const cache = new Map();
    return stem => {
        if (!stem) return Promise.resolve(null);
        const k = stem.toUpperCase();
        if (!cache.has(k))
            cache.set(k, (async () => {
                const file = textureFile(src.files, stem);
                const b = file && await src.read(file);
                if (!b) return null;
                let t;
                if (isPng(b)) t = new THREE.Texture(await createImageBitmap(new Blob([b]), { imageOrientation: 'none', premultiplyAlpha: 'none', colorSpaceConversion: 'none' }));
                else {
                    const img = decodePvr(b);
                    if (!img) return null;
                    t = new THREE.DataTexture(img.rgba, img.width, img.height, THREE.RGBAFormat);
                }
                t.flipY = false;
                t.colorSpace = THREE.NoColorSpace;
                t.wrapS = t.wrapT = THREE.RepeatWrapping;
                t.magFilter = THREE.LinearFilter;
                t.minFilter = THREE.LinearMipmapLinearFilter;
                t.generateMipmaps = true;
                t.needsUpdate = true;
                return t;
            })());
        return cache.get(k);
    };
}

function geometryOf(m, lit, amb, cache) {
    const k = lit ? 'lit' : 'flat';
    m.geo = m.geo || {};
    if (!m.geo[k]) {
        const g = new THREE.BufferGeometry();
        g.setAttribute('position', new THREE.BufferAttribute(Float32Array.from(m.pos), 3));
        g.setAttribute('uv', new THREE.BufferAttribute(Float32Array.from(m.uv), 2));
        if (lit) g.setAttribute('color', new THREE.BufferAttribute(litColours(m, amb), 3));
        g.setIndex(new THREE.BufferAttribute(Uint32Array.from(m.faces), 1));
        g.computeBoundingSphere();
        m.geo[k] = g;
        cache.add(g);
    }
    return m.geo[k];
}

/* The material for one draw state, set the way GL is set for it. */
function style(mat, d) {
    const key = d.mode + (d.lit ? 'L' : '');
    if (mat.userData.key !== key) {
        mat.userData.key = key;
        mat.vertexColors = d.lit;
        mat.depthTest = d.mode !== 'glow';
        mat.alphaTest = d.mode === 'atst' ? 0.9 : 0;
        /* GL's default GL_LESS: of two surfaces at one depth the first drawn
         * stays (three's LessEqual let the later one through, and coplanar
         * pieces flickered) */
        mat.depthFunc = THREE.LessDepth;
        if (d.mode === 'opaque' || d.mode === 'atst') {
            mat.transparent = false; mat.blending = THREE.NormalBlending; mat.depthWrite = true; mat.side = THREE.FrontSide;
        } else if (d.mode === 'alpha') {
            mat.transparent = true; mat.blending = THREE.NormalBlending; mat.depthWrite = false; mat.side = THREE.FrontSide;
        } else {
            mat.transparent = true; mat.blending = THREE.AdditiveBlending; mat.depthWrite = false;
            mat.side = d.mode === 'glow' ? THREE.FrontSide : THREE.DoubleSide;
        }
        mat.needsUpdate = true;
    }
    if (d.lit) mat.color.setRGB(1, 1, 1);
    else mat.color.setRGB(d.tint[0], d.tint[1], d.tint[2]);
    mat.opacity = d.lit ? 1 : d.alpha;
}

/* The stage in a THREE.Group, kept on the sim's state: `update()` after
 * every seek or tick. Each drawn thing is a Mesh whose userData says what
 * it is ({ draw }), for picking. `solo` shows only the drawables whose key
 * is in it. */
export class StageView {
    constructor(stage, textures) {
        this.stage = stage;
        this.sim = new StageSim(stage);
        this.tex = textures;
        this.group = new THREE.Group();
        this.group.matrixAutoUpdate = false;
        this.live = new Map();              /* draw key -> mesh */
        this.geos = new Set();
        this.solo = null;
        this.ready = new Map();             /* texture stem -> its texture, loaded */
        this.hidden = new Set();            /* stage nodes switched off */
        this.effects = true;                /* the events and the glow drawn */
        this.wire = false;
        this.order = 0;
    }
    meshFor(d) {
        let mesh = this.live.get(d.key);
        if (mesh && mesh.userData.src !== d.mesh) { this.drop(d.key); mesh = null; }
        if (!mesh) {
            const mat = new THREE.MeshBasicMaterial({ color: 0xffffff });
            mesh = new THREE.Mesh(geometryOf(d.mesh, d.lit, this.stage.level.amb, this.geos), mat);
            mesh.matrixAutoUpdate = false;
            mesh.userData.src = d.mesh;
            const stem = (d.extra ? d.extra.texture : d.mesh.texture || '').toUpperCase();
            /* loaded before (preload): on at once, or a flipbook's new frame
             * flashes white for the frame it takes to arrive */
            if (this.ready.has(stem)) mat.map = this.ready.get(stem);
            else if (stem) this.tex(stem).then(t => { this.ready.set(stem, t); if (t && this.live.get(d.key) === mesh) { mat.map = t; mat.needsUpdate = true; } });
            this.live.set(d.key, mesh);
            this.group.add(mesh);
        }
        return mesh;
    }
    drop(key) {
        const m = this.live.get(key);
        if (!m) return;
        this.group.remove(m);
        m.material.dispose();
        this.live.delete(key);
    }
    /* every texture the stage and its effects can draw, before it plays */
    async preload() {
        const stems = new Set(this.stage.extras.map(x => x.texture.toUpperCase()));
        const walk = (sc, seen) => {
            if (!sc || seen.has(sc)) return;
            seen.add(sc);
            for (const m of sc.set) if (m.texture) stems.add(m.texture.toUpperCase());
            for (const t of sc.tracks) walk(t.sceneNode, seen);
        };
        walk(this.stage.root, new Set());
        await Promise.all([...stems].map(async st => { this.ready.set(st, await this.tex(st)); }));
    }
    update() {
        const draws = this.sim.draws(), seen = new Set(), to = this.stage.toOut;
        let order = 0;
        for (const d of draws) {
            seen.add(d.key);
            const mesh = this.meshFor(d);
            if (d.lit !== !!mesh.geometry.attributes.color) mesh.geometry = geometryOf(d.mesh, d.lit, this.stage.level.amb, this.geos);
            style(mesh.material, d);
            mesh.matrix.fromArray(mul(d.world, to));
            /* STAGE VIEWER's edits on top (stageedit.js) */
            if (this.edit && d.owner !== 'ev' && !d.key.startsWith('ev')) this.edit(d, mesh);
            mesh.matrixWorldNeedsUpdate = true;
            mesh.renderOrder = order++;
            mesh.userData.draw = d;
            mesh.visible = !(d.owner === 'stage' && this.hidden.has(d.obj)) && (this.effects || d.owner === 'stage') && (!this.solo || this.solo.has(d.key));
            mesh.material.wireframe = this.wire;
        }
        /* not drawn now: hidden, kept while its scene or effect is alive (a
         * flipbook's frames come back); an effect that died, dropped */
        const alive = new Set(this.sim.pool.filter(e => e.state > 0).map(e => 'ev' + e.id));
        for (const [k, m] of [...this.live]) {
            if (seen.has(k)) continue;
            if (k.startsWith('ev') && !alive.has(k.slice(0, k.indexOf(':')))) this.drop(k);
            else m.visible = false;
        }
        this.group.updateMatrixWorld(true);
    }
    seek(ticks) { this.sim.seek(ticks); this.update(); }
    tick() { this.sim.tick(); this.update(); }
    dispose() {
        for (const k of [...this.live.keys()]) this.drop(k);
        for (const g of this.geos) g.dispose();
        this.geos.clear();
    }
}

/* ------------------------------------------------------------- export --
 * The stage over its length as the export's description (export.js): every
 * thing ever drawn -- the scene's objects, each effect each time it is
 * spawned, the glow -- is a mesh bound whole to a bone of its own; a frame
 * where it is not drawn scales it to nothing. Its material says how the
 * game blends it (blend: opaque / alpha / clip / add), its tint and its
 * alpha where it holds one. One clip, one frame a tick, 30 a second.
 * `edits` (stageedit.js) on top: an element placed as edited, its vertices
 * with the Basis edited, its shape keys morph targets whose weights are
 * keyed a frame (meshes[].morphs, clips[].weights). */
export async function stageExport(src, st, ticks, edits) {
    const stage = await loadStage(src, st);
    const sim = new StageSim(stage);
    const n = ticks || Math.max(1, stage.root.frames);
    const things = new Map();               /* draw key -> { first draw, frames } */
    const frames = [];
    for (let f = 0; f < n; f++) {
        sim.tick();
        const now = new Map();
        for (const d of sim.draws()) {
            now.set(d.key, d);
            if (!things.has(d.key)) things.set(d.key, { d, alpha: [d.alpha], mode: new Set([d.mode]) });
            else { const t = things.get(d.key); t.alpha.push(d.alpha); t.mode.add(d.mode); }
        }
        frames.push(now);
    }
    const to = stage.toOut, length = Math.max(1, stage.root.frames);
    const els = (edits && edits.els) || {};
    /* the engine's placement with the edit on frame index f (tick f + 1) */
    const placed = (d, f) => {
        const M = mul(d.world, to), el = els[d.key];
        return el ? Float32Array.from(new THREE.Matrix4().fromArray(M).multiply(localMatrix(el, frameOf(f + 1, length))).elements) : M;
    };
    const first = new Map();
    frames.forEach((now, f) => { for (const k of now.keys()) if (!first.has(k)) first.set(k, f); });
    const meshes = [], weights = {}, materials = [], matOf = new Map(), names = [], rest = [], keys = [], used = new Map();
    for (const [key, t] of things) {
        const d = t.d, m = d.mesh;
        const blend = t.mode.has('add') || t.mode.has('glow') ? 'add' : t.mode.has('alpha') ? 'alpha' : t.mode.has('atst') ? 'clip' : 'opaque';
        const stem = d.extra ? d.extra.texture : m.texture || '';
        const tint = d.tint.map(x => +x.toFixed(4));
        const alpha = blend === 'opaque' || blend === 'clip' ? 1 : +Math.max(...t.alpha).toFixed(4);
        const mk = [stem, blend, tint.join(','), alpha].join('|');
        if (!matOf.has(mk)) {
            matOf.set(mk, materials.length);
            const nm0 = (stem || 'untextured') + (blend === 'opaque' ? '' : '_' + blend.toUpperCase());
            const nm = materials.some(x => x.name === nm0) ? nm0 + '_' + materials.length : nm0;
            materials.push({ name: nm, texture: stem ? stem + '.png' : '', stem, blend, color: tint, alpha, unlit: true });
        }
        const base = (d.extra ? d.extra.name : (d.owner === 'stage' ? '' : d.scene.file + '_') + d.scene.objects[d.obj].name).replace(/\W+/g, '_') || 'object';
        const c = (used.get(base) || 0) + 1;
        used.set(base, c);
        const name = c > 1 ? base + '_' + c : base;
        const el = els[key], M0 = placed(d, first.get(key)), r0 = split(M0);
        const nv = m.pos.length / 3, pos = new Float32Array(nv * 3), b = names.length;
        const src0 = el ? Float32Array.from(m.pos) : m.pos;
        if (el) for (const [v, x] of Object.entries(el.basis)) for (let k = 0; k < 3; k++) src0[v * 3 + k] += x[k];
        for (let i = 0; i < nv; i++) {
            const x = src0[i * 3], y = src0[i * 3 + 1], z = src0[i * 3 + 2];
            for (let k = 0; k < 3; k++) pos[i * 3 + k] = x * M0[k] + y * M0[4 + k] + z * M0[8 + k] + M0[12 + k];
        }
        const idx = new Int32Array(nv * 4), wt = new Float32Array(nv * 4);
        for (let v = 0; v < nv; v++) { idx[v * 4] = b; wt[v * 4] = 1; }
        const out = { name, pos, uv: m.uv, faces: m.faces, material: matOf.get(mk), skin: { idx, wt } };
        /* the shape keys: their deltas turned and scaled as the mesh is */
        const shapes = el ? el.shapes.filter(s => !s.mute && Object.keys(s.d).length) : [];
        if (shapes.length) {
            out.morphs = shapes.map(s => {
                const delta = new Float32Array(nv * 3);
                for (const [v, x] of Object.entries(s.d))
                    for (let k = 0; k < 3; k++) delta[v * 3 + k] = x[0] * M0[k] + x[1] * M0[4 + k] + x[2] * M0[8 + k];
                return { name: s.name.replace(/\W+/g, '_') || 'Key', delta, weight: shapeAt(s, 1) };
            });
            const w = new Float32Array(n * shapes.length);
            for (let f = 0; f < n; f++) shapes.forEach((s, j) => { w[f * shapes.length + j] = shapeAt(s, frameOf(f + 1, length)); });
            weights[meshes.length] = w;
        }
        meshes.push(out);
        names.push(name);
        rest.push({ R: r0.R, T: r0.T });
        keys.push({ key, S0: r0.S });
    }
    const poses = frames.map((now, f) => keys.map(({ key, S0 }, b) => {
        const d = now.get(key);
        if (!d) return { R: rest[b].R, T: rest[b].T, S: [0, 0, 0] };
        const r = split(placed(d, f));
        return { R: r.R, T: r.T, S: r.S.map((s, i) => (S0[i] ? s / S0[i] : 1)) };
    }));
    return {
        name: st.id + '_STAGE', meshes, materials, fps: TICKS_PER_SECOND,
        skeleton: { names, parent: names.map(() => -1), rest },
        clips: [{ name: 'Stage', poses, weights }],
    };
}

/* a row-form matrix as scale, rotation (orthonormal rows) and translation */
function split(M) {
    const S = [0, 1, 2].map(r => Math.hypot(M[r * 4], M[r * 4 + 1], M[r * 4 + 2]));
    const R = [];
    for (let r = 0; r < 3; r++) for (let c = 0; c < 3; c++) R.push(S[r] ? M[r * 4 + c] / S[r] : (r === c ? 1 : 0));
    /* a mirrored placement: keep a proper rotation, the sign on x */
    const det = R[0] * (R[4] * R[8] - R[5] * R[7]) - R[1] * (R[3] * R[8] - R[5] * R[6]) + R[2] * (R[3] * R[7] - R[4] * R[6]);
    if (det < 0) { S[0] = -S[0]; R[0] = -R[0]; R[1] = -R[1]; R[2] = -R[2]; }
    return { R, T: [M[12], M[13], M[14]], S };
}
