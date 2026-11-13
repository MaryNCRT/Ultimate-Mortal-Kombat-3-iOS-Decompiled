<script>
    import ModTarget from './ModTarget.svelte';
    /* MODEL VIEWER: the game's own files, by kind -- fighters (in the T-pose,
     * the pose they were modelled in, or any of their animations, played as
     * the game skins them), stages, every other model, the textures. A
     * texture can be copied into a mod as PNG to paint over: the game takes
     * it back by its name.
     *
     * Everything can be seen as the game alone, with the mods that are on, or
         * with one mod. A fighter's rig goes out to Blender (FBX) and comes back:
         * an animation over one of its clips (mods/fbxin.js), or a new model --
         * FBX, GLB or OBJ, seen on the fighter first in its own window
         * (ModelImport.svelte, mods/modelin.js) -- written into a mod and shown at
         * once.
         *
         * Under a fighter, the other models the game keeps of them (headless,
         * babality, cut in half ... mods/derived.js). On the right, by tabs: what
         * is seen, the model's layers (each mesh shown or hidden), its textures
         * with their pictures, bringing work in, exporting. */
    import { onMount, onDestroy, untrack } from 'svelte';
    import * as THREE from 'three';
    import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
    import { app, tx } from '#lib/app.svelte.js';
    import SharpSliders from '#lib/components/SharpSliders.svelte';
    import { api } from '#lib/platform.js';
    import { hoverBlip, selectBlip } from '#lib/sound.js';
    import { ms, src, loadCatalog, fighter, gameFighter, refreshMods, reloadMods, exportThis, forgetMod, VIEW_KINDS, modNames } from '#lib/mods/store.svelte.js';
    import { stageDesc, meshsetDesc, textureFile, pngBytes, exportFiles, isPng, attachVerts, fighterDesc } from '#lib/mods/assets.js';
    import { withLayers, bakeLayers, keyAt } from '#lib/mods/corrective.js';
    import { parseAnim } from '#lib/mods/formats.js';
    import { tuckUnder } from '#lib/mods/tuck.js';
    import { readFbx, fbxScene, fbxTakes, importAnim, rigDesc } from '#lib/mods/fbxin.js';
    import { sheetPng, matsText, solidPicture, bonesText, packSource, unpackSource } from '#lib/mods/modelin.js';
    import { decodePicture } from '#lib/mods/image.js';
    import { derivedModels, specialModels } from '#lib/mods/derived.js';
    import { parseMeshset } from '#lib/mods/formats.js';
    import { thumb, forgetThumbs, lazy } from '#lib/mods/thumbs.js';
    import ModelImport from './ModelImport.svelte';
    import { ViewNav } from '#lib/mods/viewnav.js';
    import { framePose, framePoseBlend, posedVerts, fitRest } from '#lib/mods/rig.js';
    import { subsurfCreate } from '#lib/mods/subsurf.js';
    import { decodePvr } from '#lib/mods/formats.js';
    import { putTexture } from '#lib/mods/image.js';
    import { homeOf, textureDir, exportTexDir } from '#lib/mods/owners.js';
    import { makeFloor, placeFloor, weightColors, applyLook, makeRigLines, poseRigLines } from '#lib/mods/viewkit.js';
    import { loadStage, StageView, stageTextures, PLAYER_SIZE } from '#lib/mods/stagerun.js';

    let host = $state();
    let pic = $state();
    let info = $state('');
    let texInfo = $state(null);
    let target = $state('');
    let copied = $state('');
    let fx = $state(null);                  /* the fighter open, for its clips */
    let stems = $state([]);                 /* the textures of what is open */
    let imp = $state(null);                 /* an FBX animation being brought in */
    let model = $state(null);               /* a model in REPLACE MODEL's window */
    let modelFresh = false;                 /* a file just picked, not one opened again */
    /* the window's fighter: the game's own, no mod on it, so a model put
     * over an earlier one is not fitted to that one's bones and weights;
     * saved / picks: a model opened again from its mod (EDIT SAVED MODEL) */
    let modelF = $state.raw(null), modelSaved = $state.raw(null), modelPicks = $state.raw(null);
    let hasSource = $state(false);          /* the mod open keeps the model it was made from */
    let done = $state('');
    let rtab = $state('view');              /* the right panel's tab */
    let layers = $state([]);                /* [{ key, name, on }]: the model's meshes */
    let pics = $state({});                  /* texture stem -> its small picture */

    let renderer, scene, camera, controls, root, raf, skel, floor, nav;
    let viewName = $state('');
    let topEl;                              /* the buttons the camera gizmo sits under */              /* Blender's view name, bottom left */
    let fighterView = null;                 /* { f, meshes: [{ mesh, map }], rest } */

    /* a fighter's textures, by name (KANO_..., KANOPORTRAIT, IPADKANO...) */
    const ownTex = id => new RegExp('^(IPAD)?' + id + '(_|BABY|BABILITY|PORTRAIT|X_)', 'i');

    /* ONLY WHAT THE MOD REPLACES (Mary, 2026-11-12): with a mod looked at
     * (LAYER: one mod, or every mod on), the lists cut to what it has. A
     * texture counts for its smaller copies too (the game finds X_LOW,
     * X_LITE by X: runtime/mods.c), a fighter for any file named for him
     * (his skin, bones, animations, textures), a model for its file or its
     * textures. */
    const modSet = $derived(ms.layer ? modNames(ms.layer) : null);
    const filtering = $derived(!!(ms.modOnly && modSet));
    const texIn = name => {
        const n = name.toUpperCase();
        return modSet.has(n) || modSet.has(n.replace(/_(LOW|LITE)$/, '')) || modSet.has(n.replace(/_(LOW|LITE)2$/, '2'));
    };
    const fighterIn = id => { for (const n of modSet) if (n.startsWith(id) || n.startsWith('IPAD' + id)) return true; return false; };
    const modelIn = it => {
        if (it.only) return (it.textures || []).some(texIn) || modSet.has(it.file.replace(/\.meshset$/i, '').toUpperCase());
        return modSet.has(String(it.file || it.id).replace(/^.*\//, '').replace(/\.[^.]+$/, '').toUpperCase());
    };
    const inMod = (kind, it) => !filtering || (kind === 'textures' ? texIn(it.name) : kind === 'characters' ? fighterIn(it.id) : modelIn(it));

    /* SPECIAL MODELS: read from the fighters' meshsets when first wanted */
    let specials = $state.raw(null);
    async function loadSpecials() {
        if (specials || !ms.cat) return;
        try { specials = await specialModels(ms.cat, f => src.read(f, ''), parseMeshset); } catch { specials = []; }
    }
    $effect(() => { if (ms.cat && (ms.kind === 'special' || ms.kind === 'characters')) untrack(loadSpecials); });

    const items = $derived.by(() => {
        if (!ms.cat) return [];
        const q = ms.search.trim().toLowerCase();
        const all = (ms.kind === 'special' ? specials : ms.cat[ms.kind]) || [];
        const fid = ms.kind === 'textures' && ms.texFighter;
        const own = fid ? ownTex(fid) : null;
        const mine = (own ? all.filter(x => own.test(x.name)) : all).filter(x => inMod(ms.kind, x));
        const hit = q ? mine.filter(x => x.name.toLowerCase().includes(q) || String(x.id).toLowerCase().includes(q)
            || (x.owner && typeof x.owner === 'string' && x.owner.toLowerCase().includes(q))) : mine;
        return hit.slice(0, 400);
    });

    /* the fighter open (or whose model or texture is): what belongs to him */
    const owner = $derived.by(() => {
        const p = ms.pick;
        if (ms.kind !== 'characters' || !p || !ms.cat) return null;
        const o = p.owner && typeof p.owner === 'object' ? p.owner : p;
        return ms.cat.characters.find(c => c.id === o.id && c.file === o.file) ? o : null;
    });
    const derived = $derived(owner && ms.cat ? (derivedModels(ms.cat).get(owner.id) || []).filter(d => inMod('models', d.item)) : []);
    const ownSpecials = $derived(owner && specials ? specials.filter(s => s.owner === owner.id && inMod('special', s)) : []);
    const ownTextures = $derived.by(() => {
        if (!owner || !ms.cat) return [];
        const re = ownTex(owner.id);
        /* one of each name: a .png beside the .pvr is the same texture */
        const seen = new Set();
        return ms.cat.textures.filter(t => re.test(t.name) && !/_LOW\d*$|_LITE\d*$/i.test(t.name) && inMod('textures', t)
            && !seen.has(t.name.toUpperCase()) && seen.add(t.name.toUpperCase()));
    });
    /* the roster's page: FIGHTERS opens on it, a fighter picked leaves it */
    const showRoster = $derived(ms.kind === 'characters' && (ms.roster || !owner));
    /* inside a fighter: only his models and textures on the left, the MODS
     * bar above hidden (Mary, 2026-11-13) */
    const inside = $derived(ms.kind === 'characters' && !showRoster && !!owner);
    $effect(() => { ms.inside = inside; });
    onDestroy(() => { ms.inside = false; });
    /* inside, two steps (Mary, 2026-11-13): first the fighter's models and
     * textures to choose from, then the one chosen with the viewer and its
     * options taking the room the list had; MODELS goes back */
    const choose = f => { ms.chosen = true; f(); };
    /* inside: the textures of the model open (its outfit's colours, its
     * parts), not every picture of the fighter; kept while one of them is
     * looked at */
    let modelTex = $state([]);
    $effect(() => {
        if (!ms.cat || !stems.length || (ms.pick && ms.pick.tex)) return;
        const want = new Set(stems.map(s => s.toUpperCase()));
        const seen = new Set();
        modelTex = ms.cat.textures.filter(t => want.has(t.name.toUpperCase())
            && !seen.has(t.name.toUpperCase()) && seen.add(t.name.toUpperCase()));
    });
    /* a texture shown in the fighter's own page */
    const texMode = $derived(ms.kind === 'textures' || !!(ms.pick && ms.pick.tex));
    const kindName = s => { const t = tx('dv'); return ((t && t[s.key]) || s.key).toUpperCase(); };
    const fighterName = id => { const c = ms.cat && ms.cat.characters.find(x => x.id === id); return c ? c.name : id; };

    /* THE ROSTER: each fighter by his portrait, the select screen's picture
     * (<F>PORTRAIT, from the mod looked at when it has one) */
    let portraits = $state({});
    function portrait(c) {
        const k = ms.layer + '|' + c.id;
        if (portraits[k] !== undefined) return;
        portraits[k] = '';
        const file = textureFile(src.files, c.id + 'PORTRAIT');
        if (file) thumb(src, file, ms.layer, 96).then(t => { portraits[k] = t ? t.url : ''; });
    }
    function openRelated(it, extra) {
        open({ ...it, ...extra, owner });
    }

    /* a special model whose meshes are the frames of one change (Raiden:
     * eleven meshes of the same size): shown one at a time, FRAME picks it */
    let seq = $state(0), seqAt = $state(0);
    function showSeq(k) {
        seqAt = k;
        layers.forEach((_, i) => { layers[i].on = i === k; });
        if (root) root.children.forEach((o, i) => { o.visible = i === k; });
    }
    const dvName = d => { const t = tx('dv'); return ((t && t[d.key]) || d.key).toUpperCase() + (d.arg ? ' ' + d.arg.toUpperCase() : ''); };
    const hasIce = $derived(!!(fx && fx.variants.find(v => /_ICE$/i.test(v))));
    function openDerived(d) {
        open({ id: d.item.id, name: d.item.name, derived: true, owner });
    }
    async function openFrozen() {
        if (!ms.pick || ms.pick.derived) await open(owner);
        const ice = fx && fx.variants.find(v => /_ICE$/i.test(v));
        if (ice) await outfit(ice);
    }

    /* ------------------------------------------------------------ 3D -- */

    function setup() {
        /* log depth: a stage behind (STAGE) reaches skies 1e7 units away */
        renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true, logarithmicDepthBuffer: true });
        renderer.setPixelRatio(window.devicePixelRatio);
        renderer.outputColorSpace = THREE.SRGBColorSpace;
        host.appendChild(renderer.domElement);
        scene = new THREE.Scene();
        camera = new THREE.PerspectiveCamera(35, 1, 0.5, 200000);
        controls = new OrbitControls(camera, renderer.domElement);
        controls.enableDamping = true;
        scene.add(new THREE.HemisphereLight(0xffffff, 0x445066, 2.2));
        const sun = new THREE.DirectionalLight(0xffffff, 1.4);
        sun.position.set(1, 2, 3);
        scene.add(sun);
        /* the floor (FLOOR): under a fighter at y 0, else at the model's feet */
        floor = makeFloor();
        scene.add(floor);
        /* the camera as Blender's (mods/viewnav.js) */
        nav = new ViewNav({ host, renderer, controls, persp: camera, onCamera: c => { camera = c; }, onLabel: t => { viewName = t; },
            names: { front: tx('nvFront'), back: tx('nvBack'), right: tx('nvRight'), left: tx('nvLeft'), top: tx('nvTop'), bottom: tx('nvBottom'),
                user: tx('nvUser'), persp: tx('nvPersp'), ortho: tx('nvOrtho') }, below: () => topEl });
        nav.gizmo.title = tx('nvKeys');
        nav.label();
        window.addEventListener('keydown', navKey);
        const ro = new ResizeObserver(fit);
        ro.observe(host);
        loop();
    }

    /* FULL SCREEN: the view and its controls over the whole screen (the
     * page's own fullscreen; Esc or the button leaves it) */
    let mainEl = $state(), full = $state(false);
    const SPEEDS = [0.1, 0.25, 0.5, 0.75, 1, 1.5, 2, 3];
    let texPick = $state('');               /* the texture REPLACE TEXTURE takes */
    const onFull = () => { full = document.fullscreenElement === mainEl; };
    function toggleFull() {
        if (document.fullscreenElement) document.exitFullscreen();
        else mainEl && mainEl.requestFullscreen().catch(() => {});
    }
    onMount(() => document.addEventListener('fullscreenchange', onFull));
    onDestroy(() => {
        document.removeEventListener('fullscreenchange', onFull);
        if (document.fullscreenElement) document.exitFullscreen();
    });

    function fit() {
        if (!renderer || !host) return;
        const w = host.clientWidth, h = host.clientHeight;
        renderer.setSize(w, h, false);
        if (nav) nav.resize();
        else { camera.aspect = w / Math.max(1, h); camera.updateProjectionMatrix(); }
    }
    /* the numpad's views, while REPLACE MODEL's window (its own) is shut */
    function navKey(e) {
        const tag = e.target && e.target.tagName;
        if (model || tag === 'INPUT' || tag === 'SELECT' || tag === 'TEXTAREA' || !nav) return;
        if (nav.key(e)) e.preventDefault();
    }

    let last = 0, acc = 0;
    let blendT = 0;                         /* SMOOTH ANIMATIONS: how far to the next frame */
    function loop(t = 0) {
        raf = requestAnimationFrame(loop);
        const dt = Math.min(0.1, (t - last) / 1000);
        last = t;
        if (ms.playing && fighterView && ms.clip >= 0) {
            acc += dt;
            if (acc >= 1 / (30 * ms.speed)) {          /* the game's 30 a second, times SPEED */
                acc = 0;
                const c = fighterView.f.clips[ms.clip];
                /* the last frame: back to the first (LOOP) or stop there */
                if (ms.frame < c.to - c.from) ms.frame++;
                else if (ms.loop) ms.frame = 0;
                else ms.playing = false;
            }
            /* between two frames: drawn blended, every screen refresh */
            if (ms.smoothAnims) {
                blendT = Math.min(1, acc * 30 * ms.speed);
                pose();
            }
        } else blendT = 0;
        /* the stage behind plays on its own, 30 a second, as in a fight */
        if (bgView) {
            bgAcc += dt;
            let n = 0;
            while (bgAcc >= 1 / 30 && n < 8) { bgAcc -= 1 / 30; bgView.sim.tick(); n++; }
            if (n) bgView.update();
        }
        controls && controls.update();
        renderer && renderer.render(scene, camera);
        nav && nav.draw();
    }

    function clear() {
        if (!root) return;
        root.traverse(o => {
            if (o.geometry) o.geometry.dispose();
            if (o.material) [].concat(o.material).forEach(m => { if (m.map) m.map.dispose(); m.dispose(); });
        });
        scene.remove(root);
        root = null;
        skel = null;
        fighterView = null;
        layers = [];
    }

    /* the camera on obj; on a stage, on its middle: the sky and the far
     * backdrop are many times the size of the floor the fight is on */
    function frame(obj, core) {
        let box = new THREE.Box3().setFromObject(obj);
        if (core) {
            const parts = obj.children.map(o => new THREE.Box3().setFromObject(o)).filter(b => !b.isEmpty());
            const sizes = parts.map(b => b.getSize(new THREE.Vector3()).length()).sort((a, b) => a - b);
            const cut = sizes[Math.floor(sizes.length * 0.7)] || Infinity;
            const near = new THREE.Box3();
            for (const b of parts) if (b.getSize(new THREE.Vector3()).length() <= cut && b.getCenter(new THREE.Vector3()).length() < cut * 3) near.union(b);
            if (!near.isEmpty()) box = near;
        }
        const c = box.getCenter(new THREE.Vector3()), size = box.getSize(new THREE.Vector3()).length() || 1;
        nav && nav.reframed();
        placeFloor(floor, box, !!fighterView);
        floor.visible = ms.floor && ms.kind !== 'stages' && !bgView;
        controls.target.copy(c);
        camera.near = size / 500;
        camera.far = bgView ? 1e5 / PLAYER_SIZE : size * 50;
        camera.position.set(c.x + size * 0.15, c.y + size * 0.1, c.z + size * 1.1);
        camera.updateProjectionMatrix();
        if (nav) { nav.reframed(); nav.axisView = ''; nav.label(); }
        if (bgView && !core) gameCamera();          /* on a stage: as the fight sees it */
    }

    /* a texture by its name, the way the game finds it; PNG through the
     * window's own decoder. The game's v runs down from the top row, which
     * is how both arrive here, so nothing is flipped. */
    const texCache = new Map();
    async function texture(stem) {
        if (!stem) return null;
        if (texCache.has(stem)) return texCache.get(stem);
        const p = (async () => {
            const file = textureFile(src.files, stem);
            if (!file) return null;
            const b = await src.read(file);
            if (!b) return null;
            let t;
            if (!isPng(b)) {
                const img = decodePvr(b);
                if (!img) return null;
                t = new THREE.DataTexture(img.rgba, img.width, img.height, THREE.RGBAFormat);
            } else {
                const bmp = await createImageBitmap(new Blob([b]), { imageOrientation: 'none' });
                t = new THREE.Texture(bmp);
            }
            t.flipY = false;
            t.colorSpace = THREE.SRGBColorSpace;
            t.wrapS = t.wrapT = THREE.RepeatWrapping;
            t.magFilter = THREE.LinearFilter;
            t.needsUpdate = true;
            return t;
        })();
        texCache.set(stem, p);
        return p;
    }

    async function meshFrom(m, stem) {
        const g = new THREE.BufferGeometry();
        g.setAttribute('position', new THREE.BufferAttribute(Float32Array.from(m.pos), 3));
        g.setAttribute('uv', new THREE.BufferAttribute(Float32Array.from(m.uv), 2));
        g.setIndex(new THREE.BufferAttribute(Uint32Array.from(m.faces), 1));
        g.computeVertexNormals();
        const map = await texture(stem);
        const mat = new THREE.MeshLambertMaterial({ map, color: map ? 0xffffff : 0x9a96c8, side: THREE.DoubleSide,
            alphaTest: 0.35, transparent: false, wireframe: ms.wire });
        return new THREE.Mesh(g, mat);
    }

    /* --------------------------------------------------------- opening -- */

    let token = 0;
    async function open(item) {
        const my = ++token;
        selectBlip();
        ms.pick = item;
        ms.error = '';
        copied = '';
        texInfo = null;
        fx = null;
        imp = null;
        hasSource = false;
        done = '';
        texPick = '';
        stems = [];
        seq = 0;
        ms.loading = true;
        ms.playing = false;
        try {
            clear();
            if (ms.kind === 'textures' || item.tex) {
                await showTexture(item);
            } else if (ms.kind === 'characters' && !item.derived) {
                ms.roster = false;
                const f = await fighter(item);
                if (my !== token) return;
                fx = f;
                await corrLoad(f);
                hasSource = !!(ms.layer && ms.layer !== '*' && await src.read(f.ch.file + '.skin.source'));
                /* with a replaced model, each material's own texture too
                 * (<skin>_M<n>), so REPLACE TEXTURE can change it */
                stems = [...new Set([...f.blockTex, ...f.variants, ...(f.parts || []).flat().map(p => p.stem).filter(Boolean)])];
                if (!f.variants.includes(ms.outfit)) ms.outfit = f.variants[0];
                if (ms.clip >= f.clips.length) ms.clip = -1;
                await buildFighter(f);
                info = item.name + ' · ' + f.rig.num + ' bones · ' + f.topo.src.length + ' verts · ' + f.clips.length + ' clips';
            } else {
                const d = ms.kind === 'stages' ? await stageDesc(src, item) : await meshsetDesc(src, item.file || item.id, item.only || undefined);
                if (my !== token) return;
                root = new THREE.Group();
                for (const m of d.meshes) root.add(await meshFrom(m, d.materials[m.material].stem));
                layers = d.meshes.map((m, i) => ({ key: 'm' + i, name: m.name + (d.materials[m.material].stem ? ' · ' + d.materials[m.material].stem : ''), on: true }));
                stems = [...new Set(d.materials.map(m => m.stem).filter(Boolean))];
                scene.add(root);
                frame(root, ms.kind === 'stages');
                info = item.name + ' · ' + d.meshes.length + ' meshes';
                /* the frames of one change: the first, FRAME for the rest */
                const n = d.meshes.length, tris = d.meshes.map(m => m.faces.length);
                seq = item.only && n > 2 && tris.every(t => t === tris[0]) ? n : 0;
                if (seq) showSeq(0);
            }
        } catch (e) {
            ms.error = String(e && e.message || e);
        } finally {
            if (my === token) ms.loading = false;
        }
    }

    async function buildFighter(f) {
        clear();
        root = new THREE.Group();
        const meshes = [];
        const colors = weightColors(f.weights, f.topo.src.length);
        for (const [bi, blk] of f.topo.blocks.entries()) {
            /* the block's triangles over all of the fighter's vertices: the
             * positions are replaced every frame, so no compaction */
            const g = new THREE.BufferGeometry();
            g.setAttribute('position', new THREE.BufferAttribute(new Float32Array(f.topo.src.length * 3), 3));
            g.setAttribute('uv', new THREE.BufferAttribute(Float32Array.from(f.topo.uv), 2));
            g.setIndex(new THREE.BufferAttribute(f.topo.faces.slice(blk.first * 3, (blk.first + blk.count) * 3), 1));
            g.setAttribute('color', new THREE.BufferAttribute(colors, 3));
            const stem = bi === 0 && ms.outfit ? shownOutfit() : f.blockTex[bi];
            const map = await texture(stem);
            let mat = new THREE.MeshLambertMaterial({ map, color: map ? 0xffffff : 0x9a96c8, side: THREE.DoubleSide, alphaTest: 0.35, wireframe: ms.wire });
            /* a model with parts (REPLACE MODEL): each its own texture, as
             * the game draws them (runtime/skin_mats.c) */
            const ps = (f.parts && f.parts[bi]) || [];
            if (ps.length) {
                const own = mat;
                mat = [];
                for (const [k, p] of ps.entries()) {
                    g.addGroup(p.first * 3, p.count * 3, k);
                    const m = p.stem ? await texture(p.stem) : map;
                    if (m && p.stem) m.wrapS = m.wrapT = THREE.RepeatWrapping;
                    mat.push(p.stem ? new THREE.MeshLambertMaterial({ map: m, color: m ? 0xffffff : 0x9a96c8, side: THREE.DoubleSide,
                        alphaTest: p.alpha ? 0.1 : 0.35, transparent: p.alpha, wireframe: ms.wire }) : own);
                }
            }
            const mesh = new THREE.Mesh(g, mat);
            root.add(mesh);
            meshes.push(mesh);
        }
        /* what rides his bones (Kung Lao's hat), placed per frame in pose() */
        const extras = [];
        for (const a of f.attach || []) {
            const g = new THREE.BufferGeometry();
            g.setAttribute('position', new THREE.BufferAttribute(new Float32Array(a.pos.length), 3));
            g.setAttribute('uv', new THREE.BufferAttribute(Float32Array.from(a.uv), 2));
            g.setIndex(new THREE.BufferAttribute(Uint32Array.from(a.faces), 1));
            const map = await texture(a.texture === f.blockTex[0] ? shownOutfit() : a.texture);
            const mesh = new THREE.Mesh(g, new THREE.MeshLambertMaterial({ map, color: map ? 0xffffff : 0x9a96c8,
                side: THREE.DoubleSide, alphaTest: 0.35, wireframe: ms.wire }));
            mesh.visible = false;
            root.add(mesh);
            extras.push({ a, mesh });
        }
        skel = makeRigLines();
        root.add(skel);
        scene.add(root);
        fighterView = { f, meshes, extras, smooth: { level: 0, blocks: [] } };
        layers = [...f.topo.blocks.flatMap((_, bi) => [{ key: 'b' + bi, name: (bi === 0 ? tx('vwBody') : tx('vwPart') + ' ' + (bi + 1)) + ' · ' + f.blockTex[bi], on: true },
            ...((f.parts && f.parts[bi]) || []).map((p, k) => ({ key: 'p' + bi + '_' + k, name: '  ' + tx('vwTexPart') + ' ' + (k + 1) + ' · ' + (p.stem || f.blockTex[bi]) + ' · ' + p.count + ' tris', on: true }))]),
            ...extras.map(({ a }, ei) => ({ key: 'e' + ei, name: a.name + ' · ' + (a.texture || ''), on: true }))];
        pose();
        frame(root);
        smoothModel();
    }

    /* SMOOTH MODEL: each block of the fighter through the game's weld +
     * Catmull-Clark (mods/subsurf.js), the weld measured in the bind pose.
     * Built when the level changes; pose() fills it every frame. */
    async function smoothModel() {
        const fv = fighterView;
        if (!fv || fv.smooth.level === ms.smoothModel) return;
        const level = ms.smoothModel;
        for (const b of fv.smooth.blocks) {
            if (b.ss) b.ss.free();
            if (b.mesh) { root && root.remove(b.mesh); b.mesh.geometry.dispose(); }
        }
        fv.smooth = { level, blocks: [] };
        if (level > 0) {
            const { f } = fv;
            const bind = skinOf(f, posedVerts(f.rig, f.topo, f.bind.pose));
            const blocks = [];
            for (const [bi, blk] of f.rig.skin.entries()) {
                const base = f.rig.blockBase[bi];
                /* a block with parts is not smoothed, as in the game */
                if (f.parts && f.parts[bi] && f.parts[bi].length) { blocks.push({ ss: null, mesh: null, base, n: blk.numVerts }); continue; }
                const ss = await subsurfCreate(level, bind.subarray(base * 3, (base + blk.numVerts) * 3), blk.tri, blk.triUv);
                if (ss) ss.setSharp(sharpAngle(), sharpStrength());
                if (fighterView !== fv || fv.smooth.level !== level) {
                    if (ss) ss.free();
                    for (const b of blocks) if (b.ss) b.ss.free();
                    return;
                }
                let mesh = null;
                if (ss) {
                    const g = new THREE.BufferGeometry();
                    g.setAttribute('position', new THREE.BufferAttribute(new Float32Array(ss.verts * 3), 3));
                    g.setAttribute('uv', new THREE.BufferAttribute(ss.uv, 2));
                    g.setIndex(new THREE.BufferAttribute(ss.index, 1));
                    mesh = new THREE.Mesh(g, fv.meshes[bi].material);
                    mesh.visible = false;
                    root.add(mesh);
                }
                blocks.push({ ss, mesh, base, n: blk.numVerts });
            }
            fv.smooth.blocks = blocks;
        }
        pose();
    }
    $effect(() => { ms.smoothModel; smoothModel(); });

    /* SHARP EDGES (components/SharpSliders.svelte): the game's two values
     * in umk3.ini, applied to the smoothed blocks as the sliders move -- at
     * most once a frame, since each change syncs the subdivision again */
    const sharpAngle = () => app.cfg.sharpAngle ?? 80;
    const sharpStrength = () => (app.cfg.sharpStrength ?? 100) / 100;
    let sharpQueued = false;
    $effect(() => {
        app.cfg.sharpAngle; app.cfg.sharpStrength;
        if (sharpQueued) return;
        sharpQueued = true;
        requestAnimationFrame(() => {
            sharpQueued = false;
            const fv = fighterView;
            if (!fv || !fv.smooth.level) return;
            for (const b of fv.smooth.blocks) if (b.ss) b.ss.setSharp(sharpAngle(), sharpStrength());
            pose();
        });
    });

    /* output-vertex positions (topology order) back to one per skin vertex */
    function skinOf(f, pos) {
        const out = new Float32Array(f.rig.numVerts * 3);
        f.topo.src.forEach((s, o) => { out[s * 3] = pos[o * 3]; out[s * 3 + 1] = pos[o * 3 + 1]; out[s * 3 + 2] = pos[o * 3 + 2]; });
        return out;
    }

    /* the fighter in what the controls say: a rest pose, or a frame of a clip */
    function pose() {
        if (!fighterView) return;
        const { f, meshes } = fighterView;
        let p, pos, at = -1;
        if (ms.clip >= 0) {
            const c = f.clips[ms.clip];
            at = c.from + Math.min(ms.frame, c.to - c.from);
            /* SMOOTH ANIMATIONS: on the way to the next frame (or the first,
             * looping) while it plays */
            const next = at < c.to ? at + 1 : ms.loop ? c.from : at;
            p = ms.smoothAnims && ms.playing && blendT > 0 ? framePoseBlend(f.rig, at, next, blendT) : framePose(f.rig, at);
            pos = posedVerts(f.rig, f.topo, p);
        } else if (ms.pose === 'tpose' && f.tpose) {
            p = f.tpose;
            pos = f.fitted ||= fitRest(f.rig, f.topo, f.weights, p);
        } else {
            p = f.bind.pose;
            pos = posedVerts(f.rig, f.topo, p);
        }
        /* what rides his bones this frame (Kung Lao's hat), and the box of
         * each piece, 2 cm wider: the skin inside gets no correction pass,
         * so the smoothed head does not come through the hat -- as the game
         * does (smooth_models.c port_smooth_cover_mesh) */
        const extraPos = (fighterView.extras || []).map(({ a }) => (ms.extras ? attachVerts(a, at, p) : null));
        const boxes = extraPos.filter(Boolean).map(v => {
            const lo = [1e30, 1e30, 1e30], hi = [-1e30, -1e30, -1e30];
            for (let i = 0; i < v.length; i += 3)
                for (let k = 0; k < 3; k++) { lo[k] = Math.min(lo[k], v[i + k] - 2); hi[k] = Math.max(hi[k], v[i + k] + 2); }
            return { lo, hi };
        });
        const pieces = (fighterView.extras || []).map(({ a }, ei) => ({ v: extraPos[ei], faces: a.faces })).filter(x => x.v);
        const covered = (q, i) => boxes.some(({ lo, hi }) => q[i] >= lo[0] && q[i] <= hi[0] && q[i + 1] >= lo[1] && q[i + 1] <= hi[1] && q[i + 2] >= lo[2] && q[i + 2] <= hi[2]);
        /* SMOOTH MODEL: a block drawn smoothed when it has been built */
        const sm = fighterView.smooth;
        const smooth = sm && sm.level > 0 && sm.blocks.length === meshes.length && !ms.weights;
        const skin = smooth ? skinOf(f, pos) : null;
        /* unsmoothed: the skin tucked under what his bones carry, as the
         * game does (mods/tuck.js); f.fitted is kept, so a copy */
        if (!smooth && pieces.length) { pos = Float32Array.from(pos); tuckUnder(pos, pieces); }
        meshes.forEach((m, bi) => {
            const b = smooth ? sm.blocks[bi] : null;
            if (b && b.ss) {
                const blk = skin.subarray(b.base * 3, (b.base + b.n) * 3);
                let fit = null;
                if (boxes.length) {
                    fit = new Float32Array(b.n);
                    for (let i = 0; i < b.n; i++) fit[i] = covered(blk, i * 3) ? 0 : 1;
                }
                b.ss.setFit(fit);
            }
            const sp = b && b.ss ? b.ss.eval(skin.subarray(b.base * 3, (b.base + b.n) * 3)) : null;
            m.visible = !sp && shown('b' + bi);
            if (Array.isArray(m.material)) m.material.forEach((mt, k) => { mt.visible = shown('p' + bi + '_' + k); });
            if (b && b.mesh) b.mesh.visible = !!sp && shown('b' + bi);
            const g = sp ? b.mesh.geometry : m.geometry;
            g.attributes.position.array.set(sp || pos);
            g.attributes.position.needsUpdate = true;
            g.computeVertexNormals();
            g.computeBoundingSphere();
            g.computeBoundingBox();
            for (const mt of [].concat(m.material)) applyLook(mt, lookOf(true));
        });
        for (const [ei, { mesh }] of (fighterView.extras || []).entries()) {
            const v = extraPos[ei];
            mesh.visible = !!v && shown('e' + ei);
            if (!v) continue;
            mesh.geometry.attributes.position.array.set(v);
            mesh.geometry.attributes.position.needsUpdate = true;
            mesh.geometry.computeVertexNormals();
            mesh.geometry.computeBoundingSphere();
            applyLook(mesh.material, lookOf(false));
        }
        poseRigLines(skel, p, f.rig.bones.parent, ms.bones || rtab === 'corr', rtab === 'corr' ? corr.bone : -1);
    }

    /* how the model draws: TEXTURES, SOLID, WEIGHTS (a fighter's, by its
     * bones), WIREFRAME */
    const lookOf = skinned => ({ wire: ms.wire, solid: ms.solid, weights: skinned && ms.weights });

    $effect(() => {
        ms.clip; ms.frame; ms.pose; ms.bones; ms.wire; ms.extras; ms.solid; ms.weights;
        pose();
        if (!fighterView && root) {
            const l = lookOf(false);
            root.traverse(o => { if (o.material) for (const mt of [].concat(o.material)) applyLook(mt, l); });
        }
    });
    $effect(() => { if (floor) floor.visible = ms.floor && ms.kind !== 'stages' && !ms.bgStage; });

    /* STAGE: a stage behind the fighter, as the fight has it -- drawn as the
     * engine draws it (mods/stagerun.js), playing, on black, the floor of
     * the stage under the fighter's feet (both in fighter units, y 0). Its
     * textures as sRGB, as the fighter's are here, so the colours are the
     * game's. */
    let bgView = null, bgAcc = 0, bgWant = null;
    async function setStage(id) {
        bgWant = id;
        if (bgView) { scene.remove(bgView.group); bgView.dispose(); bgView = null; }
        scene.background = null;
        if (!id || !ms.cat) return;
        const st = ms.cat.stages.find(x => x.id === id);
        if (!st) return;
        try {
            const raw = stageTextures(src);
            const v = new StageView(await loadStage(src, st), stem => raw(stem).then(t => {
                if (t && t.colorSpace !== THREE.SRGBColorSpace) { t.colorSpace = THREE.SRGBColorSpace; t.needsUpdate = true; }
                return t;
            }));
            await v.preload();
            if (bgWant !== id) { v.dispose(); return; }
            v.seek(1);
            bgView = v;
            scene.add(v.group);
            scene.background = new THREE.Color(0, 0, 0);
            if (floor) floor.visible = false;
            gameCamera();
        } catch (e) { console.warn('stage behind:', e); }
    }
    $effect(() => { const id = ms.kind === 'characters' ? ms.bgStage : ''; if (scene) untrack(() => setStage(id)); });

    /* GAME CAMERA: where the fight's camera stands as a round starts
     * (StageViewer.svelte gameCamera): 1.25 over the floor, the eye 1.5
     * over it and 8 back, 25 degrees, no far plane */
    function gameCamera() {
        const u = v => v / PLAYER_SIZE;
        nav && nav.reframed();
        controls.target.set(0, u(1.25), 0);
        camera.near = u(0.33);
        camera.far = u(1e5);
        camera.position.set(0, u(1.5), u(8));
        if (camera.isPerspectiveCamera) camera.fov = 25;
        camera.updateProjectionMatrix();
        if (nav) { nav.axisView = ''; nav.label(); }
    }

    /* LAYERS: a mesh of the model shown or hidden */
    const shown = key => { const l = layers.find(x => x.key === key); return !l || l.on; };
    function setLayerOn(i, on) {
        layers[i].on = on;
        if (fighterView) pose();
        else if (root) root.children.forEach((o, k) => { o.visible = shown('m' + k); });
    }
    function allLayers(on) {
        selectBlip();
        layers.forEach((_, i) => { layers[i].on = on; });
        if (fighterView) pose();
        else if (root) root.children.forEach(o => { o.visible = on; });
    }

    /* TEXTURES: each texture's small picture, made when it comes into sight */
    function texPic(stem) {
        const file = textureFile(src.files, stem);
        if (!file || pics[ms.layer + '|' + stem] !== undefined) return;
        pics[ms.layer + '|' + stem] = '';
        thumb(src, file, ms.layer).then(t => { pics[ms.layer + '|' + stem] = t ? t.url : ''; });
    }
    function pickTexStem(s) {
        selectBlip();
        texPick = s;
        if (fx && fx.variants.includes(s) && s !== ms.outfit) outfit(s);
    }
    /* a clip moves the fighter about (its root travels): framed again */
    let framedFor = null;
    $effect(() => {
        const k = ms.clip + ':' + ms.pose;
        if (fighterView && root && framedFor !== k) {
            framedFor = k;
            frame(root);
        }
    });

    /* the outfit at a quality: '' its own texture, LOW or LITE the smaller
     * copy the game takes at IPHONE quality (X_LOW, X_LITE; for a second
     * outfit X2 also X_LOW2, X_LITE2), null when the game has none */
    let quality = $state('');
    function copyOf(stem, q) {
        if (!q) return stem;
        const s = String(stem).toUpperCase(), names = [s + '_' + q];
        if (/2$/.test(s)) names.push(s.slice(0, -1) + '_' + q + '2');
        return names.find(n => textureFile(src.files, n)) || null;
    }
    const qualities = $derived(fx && ms.outfit ? ['', 'LOW', 'LITE'].filter(q => copyOf(ms.outfit, q)) : []);
    const shownOutfit = () => copyOf(ms.outfit, quality) || ms.outfit;

    async function outfit(v) {
        ms.outfit = v;
        await showOutfit();
    }
    async function showOutfit() {
        if (!fighterView) return;
        const map = await texture(shownOutfit());
        const m0 = fighterView.meshes[0].material;
        /* with parts: the ones on the block's own texture */
        const own = Array.isArray(m0) ? m0.filter((_, k) => !fighterView.f.parts[0][k].stem) : [m0];
        for (const mt of own) {
            mt.map = map;
            mt.color.set(map ? 0xffffff : 0x9a96c8);
            mt.needsUpdate = true;
        }
        for (const { a, mesh } of fighterView.extras || [])
            if (a.texture === fighterView.f.blockTex[0]) {
                mesh.material.map = map;
                mesh.material.needsUpdate = true;
            }
    }
    $effect(() => { quality; showOutfit(); });

    /* ------------------------------------------------------- textures -- */

    async function showTexture(item) {
        const b = await src.read(item.id);
        let w, h, data;
        if (!isPng(b)) {
            const img = decodePvr(b);
            if (!img) throw new Error('?');
            ({ width: w, height: h } = img);
            data = new ImageData(img.rgba, w, h);
        } else {
            const bmp = await createImageBitmap(new Blob([b]));
            w = bmp.width; h = bmp.height;
            const c = new OffscreenCanvas(w, h);
            c.getContext('2d').drawImage(bmp, 0, 0);
            data = c.getContext('2d').getImageData(0, 0, w, h);
        }
        texInfo = { w, h, name: item.name };
        await Promise.resolve();
        pic.width = w;
        pic.height = h;
        pic.getContext('2d').putImageData(data, 0, 0);
        info = item.name + ' · ' + w + ' × ' + h;
        if (!target) {
            await refreshMods();
            target = (ms.list[0] && ms.list[0].folder) || 'My textures';
        }
    }

    async function toMod() {
        const png = await pngBytes(src, ms.pick.id);
        const name = target.trim();
        if (!png || !name) return;
        if (!ms.list.some(m => m.folder === name))
            await api.modsCreate({ name, category: 'textures' });
        const dir = await textureDir(src, ms.cat, texInfo.name);
        const file = (dir ? dir + '/' : '') + texInfo.name + '.png';
        await api.modsPut(name, file, png);
        await refreshMods();
        selectBlip();
        copied = tx('vwCopied') + name + '\\' + file.replace(/\//g, '\\');
    }

    /* the mod a change goes to when its box is left empty */
    function defaultTarget() {
        if (ms.layer && ms.layer !== '*') return ms.layer;
        if (fx) return fx.ch.name + ' mod';
        return (ms.list[0] && ms.list[0].folder) || 'My textures';
    }

    /* REPLACE TEXTURE: a picture, saved in the mod as the texture's PNG
     * (the game takes it in place of its own) with, when AUTO is on, the
     * game's smaller copies of it; shown at once */
    async function replaceTexture(stem) {
        if (!stem) return;
        selectBlip();
        done = copied = '';
        ms.error = '';
        const got = await api.pickFile('picture');
        if (!got) return;
        const name = target.trim() || defaultTarget();
        try {
            await refreshMods();
            if (!ms.list.some(m => m.folder === name)) await api.modsCreate({ name, category: 'textures' });
            const files = await putTexture(api, src, name, stem, got.bytes, ms.autoSmall, await textureDir(src, ms.cat, stem), ms.autoIce);
            if (!files) { ms.error = tx('vwNotPicture'); return; }
            await refreshMods();
            forgetMod();
            forgetThumbs();
            pics = {};
            ms.layer = name;
            target = name;
            texCache.clear();
            await open(ms.pick);
            texPick = stem;
            done = tx('vwSaved') + name + '\\' + files.join(', ');
        } catch (e) {
            ms.error = String(e && e.message || e);
        }
    }

    /* REPLACE TEXTURE's list starts on the outfit shown */
    $effect(() => {
        if (stems.length && !stems.includes(texPick)) texPick = stems.includes(ms.outfit) ? ms.outfit : stems[0];
    });

    /* the open fighter's textures, in TEXTURES */
    function seeTextures() {
        selectBlip();
        ms.texFighter = fx.ch.id;
        ms.kind = 'textures';
        ms.search = '';
    }

    /* REFRESH: the mods' folders read again (files changed outside) */
    async function refresh() {
        selectBlip();
        await reloadMods();
    }
    let seenRev = ms.rev;
    $effect(() => {
        if (ms.rev !== seenRev) {
            seenRev = ms.rev;
            texCache.clear();
            forgetThumbs();
            pics = {};
            if (ms.pick && ms.cat) open(ms.pick);
        }
    });

    /* --------------------------------------------------------- layers -- */

    async function setLayer(v) {
        selectBlip();
        if (!skinMod(v)) baseLayer = v;
        ms.layer = v;
        texCache.clear();
        if (ms.pick) await open(ms.pick);
    }

    /* ---------------------------------------------- OUTFITS and COLOURS --
     * A fighter's OUTFITS: 1 the original (the game's, with the mods that
     * replace it), then each outfit that adds (a mod of type=skin, lib/
     * mods.js, runtime/mods.c) -- its own model and its own COLOR 1, COLOR 2
     * and frozen sheets, beside the original, chosen in the game's SKIN
     * box. Choosing an outfit shows it and sends every change (a texture, a
     * model) to it; the original is left as it is. A new outfit starts as a
     * copy of the original. */
    let baseLayer = $state(ms.layer && !ms.list.some(m => m.folder === ms.layer && m.skin) ? ms.layer : '*');
    let newOutfit = $state(null);           /* the name being typed for a new one */
    let outfitName = $state('');
    const skinMod = f => f && ms.list.find(m => m.folder === f && m.skin) || null;
    const outfits = $derived(fx ? ms.list.filter(m => m.skin && m.skin.character === String(fx.ch.id).toLowerCase()) : []);
    const curOutfit = $derived(skinMod(ms.layer));
    $effect(() => { outfitName = curOutfit ? curOutfit.skin.title : ''; });
    /* the edits go where the outfit shown is */
    $effect(() => { if (curOutfit) untrack(() => { target = curOutfit.folder; }); });

    async function pickOutfit(m) {
        newOutfit = null;
        if (m) { await setLayer(m.folder); target = m.folder; }
        else { await setLayer(baseLayer); if (skinMod(target)) target = ''; }
    }
    async function createOutfit() {
        const title = (newOutfit || '').trim();
        if (!title || !fx) return;
        selectBlip();
        const r = await api.modsSkinCreate({ texBase: fx.ch.id, skinFile: fx.ch.file + '.skin', title });
        if (!r || !r.ok) { ms.error = tx('vwOutfitBad'); return; }
        newOutfit = null;
        await refreshMods();
        forgetMod();
        await pickOutfit(ms.list.find(m => m.folder === r.folder));
    }
    async function renameOutfit() {
        const v = outfitName.replace(/[\r\n=]+/g, ' ').trim();
        if (!curOutfit || !v || v === curOutfit.skin.title) return;
        await api.modsInfo(curOutfit.folder, { skin: v });
        await refreshMods();
    }
    /* a sheet's colour by its name: COLOR 1, COLOR 2, frozen, or its own */
    function colorName(v) {
        if (/_DIFFUSE$/i.test(v)) return tx('vwColor1');
        if (/_DIFFUSE2$/i.test(v)) return tx('vwColor2');
        if (/_ICE$/i.test(v)) return tx('vwFrozen');
        return v.replace(/^[A-Z]+_/, '');
    }

    /* the textures of what is on screen, as PNG, to exports\<name>_TEXTURES */
    async function exportTextures() {
        selectBlip();
        const files = {};
        for (const stem of stems) {
            const file = textureFile(src.files, stem);
            const png = file && await pngBytes(src, file);
            if (png) files[stem + '.png'] = png;
        }
        const home = fx ? homeOf('characters', fx.ch)
            : homeOf(ms.kind, ms.pick) || await textureDir(src, ms.cat, ms.pick.name);
        done = tx('exDone') + ' ' + await api.exportSave(exportTexDir(home), files);
    }

    /* EXPORT ONLY THE LAYERS SHOWN: the model as the exporter writes it
     * (assets.js), its meshes cut to those LAYERS has on. A fighter's
     * meshes are his blocks, or each part of a block that has parts (a
     * model brought in), in that order; what rides his bones (e*) is not
     * part of his export, as in EXPORT. A special model keeps only its own
     * meshes. Materials no mesh uses are dropped. */
    const LAY_FORMATS = [['fbx', 'FBX'], ['glb', 'GLB'], ['usd', 'USDA'], ['usdc', 'USDC'], ['usdz', 'USDZ'], ['obj', 'OBJ']];
    let layFormat = $state('fbx');
    async function exportLayers() {
        selectBlip();
        done = copied = '';
        ms.error = '';
        try {
            let desc, home;
            if (fx) {
                desc = fighterDesc(fx, { rest: ms.pose === 'tpose' && fx.tpose ? 'tpose' : 'bind', variant: ms.outfit, parts: true,
                    clips: layFormat === 'obj' || ms.clip < 0 ? [] : [ms.clip] });
                const keep = [];
                fx.topo.blocks.forEach((_, bi) => {
                    const ps = fx.parts && fx.parts[bi] && fx.parts[bi].length ? fx.parts[bi] : null;
                    if (ps) ps.forEach((_, k) => keep.push(shown('b' + bi) && shown('p' + bi + '_' + k)));
                    else keep.push(shown('b' + bi));
                });
                desc.meshes = desc.meshes.filter((_, i) => keep[i]);
                home = homeOf('characters', fx.ch);
            } else {
                desc = await meshsetDesc(src, ms.pick.file || ms.pick.id, ms.pick.only || undefined);
                desc.meshes = desc.meshes.filter((_, i) => shown('m' + i));
                home = homeOf('models', { name: ms.pick.owner && ms.pick.only ? fighterName(typeof ms.pick.owner === 'object' ? ms.pick.owner.id : ms.pick.owner) + ' - ' + ms.pick.name : ms.pick.name });
            }
            const used = [...new Set(desc.meshes.map(m => m.material))];
            desc.materials = used.map(i => desc.materials[i]);
            desc.meshes = desc.meshes.map(m => ({ ...m, material: used.indexOf(m.material) }));
            desc.name = (ms.pick.only ? ms.pick.name.replace(/\W+/g, '_').toUpperCase() : desc.name) + '_LAYERS';
            const files = await exportFiles(src, desc, layFormat);
            done = tx('exDone') + ' ' + await api.exportSave(home + '/' + layFormat.toUpperCase() + ' LAYERS', files);
        } catch (e) {
            ms.error = String(e && e.message || e);
        }
    }

    /* the rig to model over: T-pose, no animation, FBX with its textures */
    /* EXPORT RIG / ANIMATIONS: the skeleton and skin alone, or with every
     * clip, as FBX or GLB */
    async function exportRig(format = 'fbx', anims = false) {
        selectBlip();
        const desc = anims ? fighterDesc(fx, { rest: fx.tpose ? 'tpose' : 'bind', clips: 'all', variant: ms.outfit }) : rigDesc(fx);
        const files = await exportFiles(src, desc, format);
        done = tx('exDone') + ' ' + await api.exportSave(homeOf('characters', fx.ch) + '/' + (anims ? 'ANIMATIONS' : 'RIG') + '/' + format.toUpperCase(), files);
    }

    /* ------------------------------------ CORRECTIVE ANIMATION LAYERS --
     * Turns on the game's animations, a bone at a time, keyed on frames
     * (mods/corrective.js). The viewer shows them on the animation they were
     * put on (fx.anim0: the mod's <fighter>.skinanim.orig when it has
     * layers, else the animation loaded); SAVE bakes them into the mod's
     * .skinanim and keeps the layers and that animation beside it. With REC
     * on, a turn set is keyed at the frame shown; without, KEY keys it. */
    let corr = $state({ layers: [], cur: -1, bone: -1, rec: false, pend: null });
    const dec = new TextDecoder(), enc = new TextEncoder();
    async function corrLoad(f) {
        f.anim0 = f.rig.anim;
        corr.layers = [];
        corr.cur = -1;
        corr.pend = null;
        const file = f.ch.file;
        try {
            const lj = await api.resRead(file + '.layers.json', ms.layer || '');
            if (!lj) return;
            const saved = JSON.parse(dec.decode(lj));
            const ob = await api.resRead(file + '.skinanim.orig', ms.layer || '');
            /* the animation before the layers: its turns; the root's
             * positions are the loaded ones (the bake leaves them) */
            if (ob) f.anim0 = { ...f.rig.anim, quat: parseAnim(ob).quat };
            corr.layers = (saved.layers || []).map(L => ({ name: L.name, clip: L.clip, on: L.on !== false, weight: L.weight ?? 1, keys: L.keys || {} }));
            corr.cur = corr.layers.length ? 0 : -1;
            applyCorr();
        } catch { /* no layers */ }
    }
    function applyCorr() {
        if (!fx || !fx.anim0) return;
        const pend = corr.pend && corr.bone >= 0 && ms.clip >= 0 ? { bone: corr.bone, r: corr.pend, clip: ms.clip, frame: ms.frame } : null;
        fx.rig.anim = withLayers(fx.anim0, fx.rig.bones, $state.snapshot(corr.layers), fx.clips, pend);
        pose();
    }
    const curLayer = () => (corr.cur >= 0 ? corr.layers[corr.cur] : null);
    /* the turn shown for the bone picked: the one being set, else its keys */
    function shownTurn() {
        const L = curLayer();
        if (corr.pend) return corr.pend;
        if (!L || corr.bone < 0) return [0, 0, 0];
        return keyAt(L.keys[corr.bone], ms.frame, L.clip < 0) || [0, 0, 0];
    }
    function addLayer() {
        selectBlip();
        corr.layers.push({ name: tx('crLayer') + ' ' + (corr.layers.length + 1), clip: ms.clip, on: true, weight: 1, keys: {} });
        corr.cur = corr.layers.length - 1;
    }
    function delLayer(i) {
        selectBlip();
        corr.layers.splice(i, 1);
        corr.cur = Math.min(corr.cur, corr.layers.length - 1);
        applyCorr();
    }
    function writeKey(r) {
        const L = curLayer();
        if (!L || corr.bone < 0) return;
        const fr = L.clip < 0 ? 0 : ms.frame;
        const keys = (L.keys[corr.bone] || []).filter(k => k.f !== fr);
        keys.push({ f: fr, r: r.map(v => Math.round(v * 10) / 10) });
        keys.sort((a, b) => a.f - b.f);
        L.keys[corr.bone] = keys;
        corr.pend = null;
        applyCorr();
    }
    function setTurn(c, v) {
        const r = [...shownTurn()];
        r[c] = Math.min(180, Math.max(-180, Number(v) || 0));
        if (corr.rec) writeKey(r);
        else { corr.pend = r; applyCorr(); }
    }
    function delKey() {
        const L = curLayer();
        if (!L || corr.bone < 0) return;
        selectBlip();
        const fr = L.clip < 0 ? 0 : ms.frame;
        L.keys[corr.bone] = (L.keys[corr.bone] || []).filter(k => k.f !== fr);
        if (!L.keys[corr.bone].length) delete L.keys[corr.bone];
        corr.pend = null;
        applyCorr();
    }
    /* another frame, bone or layer: what was being set and not keyed goes */
    $effect(() => {
        ms.frame; ms.clip; corr.bone; corr.cur;
        untrack(() => { if (corr.pend) { corr.pend = null; applyCorr(); } });
    });
    async function saveCorr() {
        if (!fx) return;
        selectBlip();
        const name = target.trim() || defaultTarget();
        ms.error = '';
        try {
            await refreshMods();
            if (!ms.list.some(m => m.folder === name)) await api.modsCreate({ name, category: 'models' });
            const dir = homeOf('characters', fx.ch) + '/', file = fx.ch.file + '.skinanim';
            const kept = await api.resRead(file + '.orig', name);
            const orig = kept || (await api.resRead(file, name)) || (await api.resRead(file, ''));
            if (!kept) await api.modsPut(name, dir + file + '.orig', orig);
            const layers = $state.snapshot(corr.layers);
            await api.modsPut(name, dir + file, bakeLayers(fx, layers, orig));
            await api.modsPut(name, dir + fx.ch.file + '.layers.json', enc.encode(JSON.stringify({ fighter: fx.ch.file, layers }, null, 1)));
            await refreshMods();
            forgetMod();
            ms.layer = name;
            target = name;
            const clip = ms.clip, frame = ms.frame;
            await open(ms.pick);
            ms.clip = clip;
            ms.frame = frame;
            rtab = 'corr';
            done = tx('vwSaved') + name + '\\' + file + ' (' + layers.length + ' ' + tx('crLayers') + ')';
        } catch (e) {
            ms.error = String(e && e.message || e);
        }
    }

    /* ---------------------------------------- bringing Blender's work in -- */

    /* REPLACE MODEL: FBX, GLB or OBJ, into its window */
    async function pickModel() {
        selectBlip();
        done = '';
        ms.error = '';
        const got = await api.pickFile('model');
        if (!got) return;
        if (!target) target = (ms.layer && ms.layer !== '*' ? ms.layer : '') || fx.ch.name + ' mod';
        ms.playing = false;
        try { modelF = await gameFighter(fx.ch); } catch (e) { ms.error = String(e && e.message || e); return; }
        modelSaved = null;
        modelPicks = null;
        modelFresh = true;
        model = got;
    }
    /* EDIT: the model the fighter has now -- the mod's own file when it
     * kept it (EDIT SAVED MODEL below), else the skin shown, as the exporter
     * writes it (GLB: the fighter's bones by name, its weights, each part
     * with its texture) -- in REPLACE MODEL's window, without a file */
    async function editCurrent() {
        if (hasSource) return editSaved();
        selectBlip();
        done = '';
        ms.error = '';
        try {
            const desc = fighterDesc(fx, { rest: 'bind', clips: [], variant: ms.outfit, parts: true });
            const files = await exportFiles(src, desc, 'glb');
            const name = Object.keys(files).find(n => /\.glb$/i.test(n));
            modelF = await gameFighter(fx.ch);
            modelSaved = null;
            modelPicks = null;
            if (!target) target = (ms.layer && ms.layer !== '*' ? ms.layer : '') || fx.ch.name + ' mod';
            ms.playing = false;
            modelFresh = false;
            model = { name, bytes: files[name], beside: {} };
        } catch (e) {
            ms.error = String(e && e.message || e);
        }
    }

    /* EDIT SAVED MODEL: the mod's model (<skin>.source) in REPLACE MODEL's
     * window again, as it was set (<skin>.import.json), saved back to it */
    async function editSaved() {
        selectBlip();
        done = '';
        ms.error = '';
        try {
            const b = await src.read(fx.ch.file + '.skin.source');
            if (!b) return;
            const { file: got, picks } = unpackSource(b);
            const j = await src.read(fx.ch.file + '.skin.import.json');
            modelSaved = j ? JSON.parse(new TextDecoder().decode(j)) : null;
            modelPicks = picks;
            modelF = await gameFighter(fx.ch);
            target = ms.layer;
            ms.playing = false;
            modelFresh = false;
        model = got;
        } catch (e) {
            ms.error = String(e && e.message || e);
        }
    }

    /* the window's SAVE: the .skin, and each block's sheet of pictures as
     * its texture (the outfit's in every outfit when asked; the frozen one
     * tinted as ice) */
    /* REPLACE MODEL saved: the .skin, each material's picture as its own
     * texture (<skin>_M<n>, and _M<n>_ICE for frozen), and the .mats that
     * says which triangles each one covers (runtime/skin_mats.c). Every
     * part gets a texture of its own, so the model's UVs are kept and no
     * texture the game shares with other models (derived models, other
     * fighters) is touched: a material without a picture is its colour, one
     * left on THE GAME'S a copy of the outfit's. */
    async function applyModel({ r, mdl, settings, file: from, picks }) {
        const name = target.trim() || defaultTarget();
        model = null;
        ms.error = '';
        try {
            await refreshMods();
            if (!ms.list.some(m => m.folder === name)) await api.modsCreate({ name, category: 'models' });
            const file = homeOf('characters', fx.ch) + '/' + fx.ch.file + '.skin';
            await api.modsPut(name, file, r.bytes);
            const files = [file];
            const dir = await textureDir(src, ms.cat, fx.blockTex[0]);
            const texOf = new Map();
            for (const [i, m] of mdl.materials.entries()) {
                if (!r.parts.some(ps => ps.some(p => p.mat === i))) continue;
                /* a game texture replaced: the picture saved as it, and the
                 * part drawn with the block's own texture */
                if (m.rep) {
                    if (m.image) {
                        const w = await putTexture(api, src, name, m.rep, await sheetPng(m.image), false, dir, true);
                        if (w) files.push(...w);
                    }
                    continue;
                }
                const own = m.image || (m.game && await gamePicture(shownOutfit())) || solidPicture(m.color);
                const st = (fx.ch.file + '_M' + (i + 1)).toUpperCase();
                const w = await putTexture(api, src, name, st, await sheetPng(own), false, dir, true);
                if (w) files.push(...w);
                texOf.set(i, st + '.png');
            }
            const mats = file + '.mats';
            await api.modsPut(name, mats, matsText(r.parts, mdl, i => texOf.get(i) || '-'));
            files.push(mats);
            /* the proportion trick's bones; without it one that changes
             * nothing, so an earlier import's does not stay */
            await api.modsPut(name, file + '.bones', bonesText(r.bones || { offsets: new Map(), rootY: 1 }));
            files.push(file + '.bones');
            /* what the window was set to, to see what was done (rig, joints,
             * angles, scale, materials) */
            if (settings) {
                await api.modsPut(name, file + '.import.json', new TextEncoder().encode(JSON.stringify(settings, null, 1)));
                files.push(file + '.import.json');
            }
            /* the model itself, to open again (EDIT SAVED MODEL) */
            if (from) {
                await api.modsPut(name, file + '.source', packSource(from, picks));
                files.push(file + '.source');
            }
            await refreshMods();
            forgetMod();
            forgetThumbs();
            pics = {};
            ms.layer = name;
            target = name;
            texCache.clear();
            await open(ms.pick);
            done = tx('vwSaved') + name + '\\' + files.join(', ').replace(/\//g, '\\') + ' (' + r.verts + ' verts · ' + r.tris + ' tris)';
        } catch (e) {
            ms.error = String(e && e.message || e);
        }
    }
    /* a texture of the game's as a picture (PNG or PVR), or null */
    async function gamePicture(stem) {
        const file = stem && textureFile(src.files, stem);
        const b = file && await src.read(file);
        if (!b) return null;
        if (isPng(b)) return decodePicture(b).catch(() => null);
        const img = decodePvr(b);
        if (!img) return null;
        const c = new OffscreenCanvas(img.width, img.height);
        c.getContext('2d').putImageData(new ImageData(new Uint8ClampedArray(img.rgba.buffer, img.rgba.byteOffset, img.rgba.length), img.width, img.height), 0, 0);
        return c;
    }

    async function pickImport(kind) {
        selectBlip();
        done = '';
        const got = await api.pickFile('fbx');
        if (!got) return;
        try {
            const sc = fbxScene(await readFbx(got.bytes));
            const takes = kind === 'anim' ? fbxTakes(sc) : [];
            if (kind === 'anim' && !takes.length) throw new Error(tx('vwNoTakes'));
            const clipName = ms.clip >= 0 ? fx.clips[ms.clip].name.toLowerCase() : '';
            const take = takes.find(t => clipName && t.name.toLowerCase().includes(clipName)) || takes[0];
            if (!target) target = (ms.layer && ms.layer !== '*' ? ms.layer : '') || fx.ch.name + ' mod';
            imp = { kind, sc, file: got.name, takes, take: take ? take.id : null, clip: Math.max(0, ms.clip) };
        } catch (e) {
            ms.error = String(e && e.message || e);
        }
    }

    async function applyImport() {
        const name = target.trim();
        if (!imp || !fx || !name) return;
        selectBlip();
        ms.error = '';
        try {
            await refreshMods();
            if (!ms.list.some(m => m.folder === name)) await api.modsCreate({ name, category: 'models' });
            /* over the mod's own copy, if it has one: clips replaced
             * before stay replaced */
            const file = fx.ch.file + '.skinanim';
            /* with corrective layers: into the animation before them, which
             * is kept, and the layers baked on it again */
            const orig = await api.resRead(file + '.orig', name);
            const base = orig || (await api.resRead(file, name)) || (await api.resRead(file, ''));
            const r = importAnim(fx, imp.sc, imp.take, imp.clip, base);
            let bytes = r.bytes;
            if (orig) {
                await api.modsPut(name, homeOf('characters', fx.ch) + '/' + file + '.orig', bytes);
                const lj = await api.resRead(fx.ch.file + '.layers.json', name);
                if (lj) bytes = bakeLayers(fx, JSON.parse(dec.decode(lj)).layers || [], bytes);
            }
            const what = fx.clips[imp.clip].name + ' · ' + r.frames + ' → ' + r.into + ' · ' + r.matched + '/' + r.of;
            await api.modsPut(name, homeOf('characters', fx.ch) + '/' + file, bytes);
            await refreshMods();
            const clip = imp.clip;
            imp = null;
            forgetMod();
            ms.layer = name;
            texCache.clear();
            await open(ms.pick);
            ms.clip = clip;
            ms.frame = 0;
            ms.playing = clip >= 0;
            done = tx('vwSaved') + name + '\\' + file + ' (' + what + ')';
        } catch (e) {
            ms.error = String(e && e.message || e);
        }
    }

    onMount(async () => {
        setup();
        refreshMods();
        await loadCatalog();
        /* the capture aid: UMK3_LAUNCHER_PICK=characters/KANO[@clip];
         * characters/ alone is the roster's page, special/<id>[@frame] a
         * special model (special:NIGHTWOLF:Raiden@10), and a leading
         * ~<mod folder>~ looks at that mod with ONLY WHAT THE MOD REPLACES
         * on (~Sonya Blade MKX~textures/) */
        let raw = new URLSearchParams(location.search).get('pick') || '';
        const lm = /^~([^~]+)~/.exec(raw);
        if (lm) { raw = raw.slice(lm[0].length); ms.layer = lm[1]; ms.modOnly = true; }
        const cut = raw.indexOf('/'), at = raw.lastIndexOf('@');
        const want = [raw.slice(0, cut), raw.slice(cut + 1, at > cut ? at : undefined), at > cut ? raw.slice(at + 1) : ''];
        if (want[0] === 'special') await loadSpecials();
        const list = want[0] === 'special' ? specials : ms.cat && ms.cat[want[0]];
        if (cut > 0 && list) {
            ms.kind = want[0];
            ms.roster = true;
            const it = want[1] && list.find(x => String(x.id).toUpperCase() === want[1].toUpperCase());
            if (it) await open(it);
            if (want[2] && seq) showSeq(Math.min(seq - 1, Number(want[2])));
            else if (want[2]) { ms.clip = Number(want[2]); ms.frame = 2; }
            setTimeout(() => api.shot(), want[1] ? 300 : 2500);
        } else if (ms.pick && ms.cat) open(ms.pick);
    });
    onDestroy(() => {
        cancelAnimationFrame(raf);
        if (bgView) { bgView.dispose(); bgView = null; }
        clear();
        window.removeEventListener('keydown', navKey);
        nav && nav.dispose();
        renderer && renderer.dispose();
    });
</script>

{#snippet sideHead()}
        <label class="layer"><span class="lbl">{tx('vwLayer')}</span>
            <select class="sel" value={ms.layer} onchange={e => setLayer(e.currentTarget.value)}>
                <option value="">{tx('vwLayerGame')}</option>
                <option value="*">{tx('vwLayerOn')}</option>
                {#each ms.list.filter(m => !m.skin || m.folder === ms.layer) as m}<option value={m.folder}>{m.skin ? tx('vwOutfit') + ': ' + m.skin.title : m.name}</option>{/each}
            </select></label>
        <button class="btn" onclick={refresh}>{tx('vwRefresh')}</button>
        {#if ms.layer}
            <label class="chk modonly" title={tx('vwModOnlyNote')}><input type="checkbox" bind:checked={ms.modOnly}><span>{tx('vwModOnly')}</span></label>
        {/if}
        <div class="kinds">
            {#each VIEW_KINDS as [k, key]}
                <button class="tab" class:active={ms.kind === k} onpointerenter={hoverBlip}
                        onclick={() => { selectBlip(); ms.kind = k; ms.search = ''; if (k === 'characters') ms.roster = true; }}>{tx(key)}</button>
            {/each}
        </div>
{/snippet}

<div class="viewer" class:full class:solo={ms.solo} class:picking={inside && !ms.chosen} class:chosen={inside && ms.chosen} bind:this={mainEl}>
    {#if showRoster}
        <!-- THE ROSTER (Mary, 2026-11-12): a page of its own, every fighter
             by his select screen portrait; a click opens him in the viewer,
             and BACK TO THE ROSTER returns here -->
        <div class="rosterpage">
            <aside class="side">{@render sideHead()}
                <input class="edt search" placeholder={tx('vwSearch')} spellcheck="false" bind:value={ms.search}>
            </aside>
            <div class="grid">
                {#each items as it (it.id)}
                    {@const p = portraits[ms.layer + '|' + it.id]}
                    <button class="cell" class:on={owner && owner.id === it.id} title={it.name} use:lazy={() => portrait(it)}
                            onpointerenter={hoverBlip} onclick={() => { ms.chosen = false; ms.leftTab = 'models'; open(it); }}>
                        <span class="face">{#if p}<img src={p} alt="">{:else}<i>{it.name.slice(0, 2)}</i>{/if}</span>
                        <span class="nm">{it.name}</span>
                    </button>
                {/each}
                {#if !items.length}<p class="note">{tx(filtering ? 'vwModNone' : 'vwPick')}</p>{/if}
            </div>
        </div>
    {/if}
    <aside class="side" class:inside>
        {#if !inside}{@render sideHead()}{/if}
        {#if ms.kind === 'textures' && ms.cat}
            <select class="sel" bind:value={ms.texFighter}>
                <option value="">{tx('vwTexAll')}</option>
                {#each ms.cat.characters as c}<option value={c.id}>{c.name}</option>{/each}
            </select>
        {/if}
        {#if ms.kind !== 'characters'}<input class="edt search" placeholder={tx('vwSearch')} spellcheck="false" bind:value={ms.search}>{/if}
        {#if ms.kind === 'characters'}
            <button class="btn back" onpointerenter={hoverBlip} onclick={() => { selectBlip(); ms.roster = true; }}>&#8592; {tx('vwBackRoster')}</button>
            {#if owner}
                {@const p = portraits[ms.layer + '|' + owner.id]}
                <div class="who" use:lazy={() => portrait(owner)}>
                    <span class="face">{#if p}<img src={p} alt="">{/if}</span>
                    <span class="wn">{owner.name.toUpperCase()}</span>
                </div>
                {#if ms.leftTab === 'models'}
                <ul class="list related">
                    <li class="sub"><button class:on={ms.pick === owner || (ms.pick.id === owner.id && !ms.pick.tex)} onclick={() => choose(() => open(owner))}>{tx('vwRoster')}</button></li>
                    {#if hasIce}
                        <li class="sub"><button class:on={!ms.pick.derived && !ms.pick.tex && /_ICE$/i.test(ms.outfit)} onclick={() => choose(openFrozen)}>{tx('vwFrozen')}</button></li>
                    {/if}
                    {#each ownSpecials as d (d.id)}
                        <li class="sub"><button class="special" class:on={ms.pick.id === d.id} title={d.name}
                                onclick={() => choose(() => openRelated(d, { derived: true }))}>&#9733; {d.name.toUpperCase()} <small>{kindName(d)}</small></button></li>
                    {/each}
                    {#each derived as d (d.item.id)}
                        <li class="sub"><button class:on={ms.pick.derived && ms.pick.id === d.item.id} title={d.item.name}
                                onclick={() => choose(() => openDerived(d))}>{dvName(d)}</button></li>
                    {/each}
                </ul>
                {:else}
                <!-- the fighter's textures, as pictures -->
                <div class="texgrid">
                    {#each modelTex as t (t.id)}
                        <button class="tcell" class:on={ms.pick.tex && ms.pick.id === t.id} title={t.name}
                                use:lazy={() => texPic(t.name)} onpointerenter={hoverBlip} onclick={() => choose(() => openRelated(t, { tex: true }))}>
                            <span class="pic">{#if pics[ms.layer + '|' + t.name]}<img src={pics[ms.layer + '|' + t.name]} alt="">{/if}</span>
                            <span class="tn">{t.name.replace(new RegExp('^' + owner.id + '_?', 'i'), '') || t.name}</span>
                        </button>
                    {/each}
                </div>
                {/if}
            {/if}
        {:else}
            <ul class="list">
                {#if ms.kind === 'special' && !specials}<li class="subhead">{tx('vwLoading')}</li>{/if}
                {#each items as it (it.id)}
                    <li><button class:on={ms.pick && ms.pick.id === it.id} title={it.name}
                                onclick={() => open(it)}>{#if ms.kind === 'special'}<span class="spn">{it.name.toUpperCase()}</span>
                                <small class="spk">{it.owner ? fighterName(it.owner).toUpperCase() + ' · ' : ''}{kindName(it)}</small>{:else}{it.name}{/if}</button></li>
                {/each}
                {#if filtering && !items.length}<li class="subhead">{tx('vwModNone')}</li>{/if}
            </ul>
        {/if}
    </aside>

    <section class="main">
        <div class="view" class:hide={texMode} bind:this={host}><div class="vname">{viewName}</div></div>
        {#if texMode}
            <div class="texview"><canvas bind:this={pic} class:hide={!texInfo}></canvas></div>
        {/if}
        <div class="over">
            {#if ms.noGame}<span class="msg">{tx('vwNoGame')}</span>
            {:else if ms.loading}<span class="msg">{tx('vwLoading')}</span>
            {:else if ms.error}<span class="msg bad">{ms.error}</span>
            {:else if !ms.pick}<span class="msg">{tx('vwPick')}</span>{/if}
        </div>
        <div class="info">{info}</div>
        <div class="topbtns" bind:this={topEl}>
            <button class="btn" onclick={() => { selectBlip(); ms.solo = !ms.solo; }}>{ms.solo ? tx('vwPanels') : tx('vwSolo')}</button>
            <button class="btn" onclick={toggleFull}>{full ? tx('vwFullExit') : tx('vwFull')}</button>
        </div>
        {#if ms.kind === 'characters' && fx && ms.clip >= 0}
            <div class="play">
                <button class="btn" onclick={() => { if (!ms.playing && ms.frame >= fx.clips[ms.clip].to - fx.clips[ms.clip].from) ms.frame = 0; ms.playing = !ms.playing; }}>{ms.playing ? tx('vwPause') : tx('vwPlay')}</button>
                <label class="chk"><input type="checkbox" bind:checked={ms.loop}><span>{tx('vwLoop')}</span></label>
                <label class="chk"><input type="checkbox" bind:checked={ms.smoothAnims}><span>{tx('vwSmoothAnims')}</span></label>
                <input type="range" min="0" max={fx.clips[ms.clip].to - fx.clips[ms.clip].from} bind:value={ms.frame}>
                <span class="lbl">{tx('vwFrame')} {ms.frame + 1}</span>
                <select class="sel speed" bind:value={ms.speed} title={tx('vwSpeed')}>
                    {#each SPEEDS as v}<option value={v}>{v}x</option>{/each}
                </select>
            </div>
        {/if}
    </section>

    <aside class="opts">
        {#if ms.pick && !ms.loading && (texMode ? texInfo : true)}
            <div class="rtabs">
                {#each [['view', 'vwTabView'], ...(!texMode ? [['layers', 'vwTabLayers']] : []), ['tex', 'vwTabTex'], ...(fx ? [['import', 'vwTabImport'], ['corr', 'vwTabCorr']] : []), ['export', 'vwTabExport']] as [k, key]}
                    <button class="tab" class:active={rtab === k} onpointerenter={hoverBlip}
                            onclick={() => { selectBlip(); rtab = k; }}>{tx(key)}</button>
                {/each}
            </div>
        {/if}

        {#if fx && ms.kind === 'characters' && (rtab === 'view' || rtab === 'tex' || rtab === 'import')}
            <div class="look">
                <span class="step"><b>1</b>{tx('vwOutfit')}</span>
                <div class="slots">
                    <button class="slot" class:on={!curOutfit} onpointerenter={hoverBlip} onclick={() => pickOutfit(null)}>
                        <span class="n">1</span><span class="t">{tx('vwOriginal')}</span></button>
                    {#each outfits as m, i (m.folder)}
                        <button class="slot" class:on={curOutfit && curOutfit.folder === m.folder} class:off={!m.on}
                                title={m.folder} onpointerenter={hoverBlip} onclick={() => pickOutfit(m)}>
                            <span class="n">{i + 2}</span><span class="t">{m.skin.title}</span></button>
                    {/each}
                    <button class="slot add" class:on={newOutfit !== null} onpointerenter={hoverBlip}
                            onclick={() => { selectBlip(); newOutfit = newOutfit === null ? '' : null; }}>
                        <span class="n">+</span><span class="t">{tx('vwNewOutfit')}</span></button>
                </div>
                {#if newOutfit !== null}
                    <p class="note">{tx('vwNewOutfitNote')}</p>
                    <div class="pair">
                        <input class="edt" spellcheck="false" placeholder={tx('vwOutfitName')} bind:value={newOutfit}
                               onkeydown={e => { if (e.key === 'Enter') createOutfit(); }}>
                        <button class="btn hot" disabled={!newOutfit.trim()} onclick={createOutfit}>{tx('vwCreate')}</button>
                    </div>
                {:else if curOutfit}
                    <label class="named"><span class="lbl">{tx('vwOutfitName')}</span>
                        <input class="edt" spellcheck="false" bind:value={outfitName} onchange={renameOutfit}
                               onkeydown={e => { if (e.key === 'Enter') e.currentTarget.blur(); }}></label>
                    {#if !curOutfit.on}<p class="note warn">{tx('vwOutfitOff')}</p>{/if}
                {/if}
                {#if fx.variants.length > 1}
                    <span class="step"><b>2</b>{tx('vwColor')}</span>
                    <div class="slots colors">
                        {#each fx.variants as v (v)}
                            <button class="slot" class:on={ms.outfit === v} title={v} onpointerenter={hoverBlip}
                                    onclick={() => { selectBlip(); outfit(v); }}><span class="t">{colorName(v)}</span></button>
                        {/each}
                    </div>
                {/if}
                {#if ms.outfit}
                    <!-- the chosen colour's sheet: its picture, its name, REPLACE -->
                    {#key ms.layer + '|' + ms.outfit}
                        <div class="texcard" use:lazy={() => texPic(ms.outfit)}>
                            <span class="pic">{#if pics[ms.layer + '|' + ms.outfit]}<img src={pics[ms.layer + '|' + ms.outfit]} alt="">{/if}</span>
                            <span class="tinfo">
                                <span class="tfile" title={ms.outfit}>{ms.outfit}</span>
                                <span class="tfrom">{curOutfit ? tx('vwOutfit') + ' ' + curOutfit.skin.title : tx('vwOriginal')}</span>
                                <button class="btn hot" onclick={() => replaceTexture(ms.outfit)}>{tx('vwImpTex')}</button>
                            </span>
                        </div>
                    {/key}
                {/if}
                <p class="dest" class:skin={curOutfit}>{curOutfit ? tx('vwDestOutfit') + ' ' + curOutfit.skin.title : tx('vwDestOriginal')}</p>
            </div>
        {/if}

        {#if rtab === 'view' || !ms.pick}
            {#if ms.kind === 'characters' && fx}
                <span class="lbl">{tx('vwAnim')}</span>
                <select class="sel" value={ms.clip} onchange={e => { ms.clip = Number(e.currentTarget.value); ms.frame = 0; }}>
                    <option value={-1}>{tx('vwPose')}</option>
                    {#each fx.clips as c, i}<option value={i}>{c.name} ({c.to - c.from + 1})</option>{/each}
                </select>
                {#if ms.clip < 0}
                    <span class="tabs sub">
                        <button class="tab" class:active={ms.pose === 'tpose'} disabled={!fx.tpose} onclick={() => ms.pose = 'tpose'}>{tx('vwTpose')}</button>
                        <button class="tab" class:active={ms.pose === 'bind'} onclick={() => ms.pose = 'bind'}>{tx('vwBind')}</button>
                    </span>
                {/if}

                {#if qualities.length > 1}
                    <span class="lbl">{tx('vwQuality')}</span>
                    <span class="tabs sub q{qualities.length}">
                        {#each qualities as q}
                            <button class="tab" class:active={quality === q} onclick={() => { selectBlip(); quality = q; }}>{q || 'NORMAL'}</button>
                        {/each}
                    </span>
                {/if}
                <span class="lbl">{tx('vwSmoothModel')}</span>
                <span class="tabs sub q3">
                    {#each tx('smodels') as m, i}
                        <button class="tab" class:active={ms.smoothModel === i} onclick={() => { selectBlip(); ms.smoothModel = i; }}>{m}</button>
                    {/each}
                </span>
                {#if fx}<SharpSliders disabled={!ms.smoothModel} />{/if}
                {#if ms.cat && ms.cat.stages}
                    <span class="lbl">{tx('vwStage')}</span>
                    <select class="sel" bind:value={ms.bgStage}>
                        <option value="">{tx('vwNoStage')}</option>
                        {#each ms.cat.stages as st}<option value={st.id}>{st.name}</option>{/each}
                    </select>
                    {#if ms.bgStage}<button class="btn" onclick={() => { selectBlip(); gameCamera(); }}>{tx('stGameCam')}</button>{/if}
                {/if}
                <div class="checks">
                    <label class="chk"><input type="checkbox" bind:checked={ms.bones}><span>{tx('vwBones')}</span></label>
                    <label class="chk"><input type="checkbox" bind:checked={ms.wire}><span>{tx('vwWire')}</span></label>
                    <label class="chk"><input type="checkbox" bind:checked={ms.solid} onchange={() => { if (ms.solid) ms.weights = false; }}><span>{tx('vwSolid')}</span></label>
                    <label class="chk"><input type="checkbox" bind:checked={ms.weights} onchange={() => { if (ms.weights) ms.solid = false; }}><span>{tx('vwWeights')}</span></label>
                    {#if !ms.bgStage}<label class="chk"><input type="checkbox" bind:checked={ms.floor}><span>{tx('vwFloor')}</span></label>{/if}
                    {#if fx.attach && fx.attach.length}
                        <label class="chk"><input type="checkbox" bind:checked={ms.extras}><span>{tx('vwExtras')}</span></label>
                    {/if}
                </div>
            {:else if ms.pick && !texMode && !ms.loading}
                {#if seq}
                    <span class="lbl">{tx('vwFrame')} {seqAt + 1} / {seq}</span>
                    <input class="seq" type="range" min="0" max={seq - 1} value={seqAt} oninput={e => showSeq(Number(e.currentTarget.value))}>
                    <p class="note">{tx('vwSeqNote')}</p>
                {/if}
                <div class="checks">
                    <label class="chk"><input type="checkbox" bind:checked={ms.wire}><span>{tx('vwWire')}</span></label>
                    <label class="chk"><input type="checkbox" bind:checked={ms.solid}><span>{tx('vwSolid')}</span></label>
                    {#if ms.kind !== 'stages'}<label class="chk"><input type="checkbox" bind:checked={ms.floor}><span>{tx('vwFloor')}</span></label>{/if}
                </div>
            {:else if texMode && texInfo}
                <span class="lbl">{texInfo.name}</span>
                <span class="lbl">{texInfo.w} × {texInfo.h}</span>
            {/if}

        {:else if rtab === 'layers'}
            <span class="lbl head">{tx('vwTabLayers')} ({layers.filter(l => l.on).length}/{layers.length})</span>
            <div class="pair">
                <button class="btn" onclick={() => allLayers(true)}>{tx('vwAllOn')}</button>
                <button class="btn" onclick={() => allLayers(false)}>{tx('vwAllOff')}</button>
            </div>
            <ul class="layers">
                {#each layers as l, i (l.key)}
                    <li><label class="chk"><input type="checkbox" checked={l.on} onchange={e => setLayerOn(i, e.currentTarget.checked)}><span title={l.name}>{l.name}</span></label></li>
                {/each}
            </ul>
            {#if !layers.length}<p class="note">{tx('vwNoLayers')}</p>{/if}

        {:else if rtab === 'tex'}
            <span class="lbl">{tx('vwModName')}</span>
            <ModTarget bind:value={target} placeholder={defaultTarget()} />
            <label class="chk"><input type="checkbox" bind:checked={ms.autoSmall}><span>{tx('vwAutoSmall')}</span></label>
            <label class="chk"><input type="checkbox" bind:checked={ms.autoIce}><span>{tx('vwAutoIce')}</span></label>
            {#if texMode}
                <button class="btn hot" onclick={() => replaceTexture(texInfo.name)}>{tx('vwImpTex')}</button>
                <button class="btn" onclick={toMod}>{tx('vwToMod')}</button>
            {:else}
                <ul class="texlist">
                    {#each stems.filter(st => inMod('textures', { name: st })) as st (st)}
                        <li use:lazy={() => texPic(st)}>
                            <button class:on={texPick === st} onclick={() => pickTexStem(st)} title={st}>
                                <span class="pic">{#if pics[ms.layer + '|' + st]}<img src={pics[ms.layer + '|' + st]} alt="">{/if}</span>
                                <span class="nm">{st}</span>
                            </button>
                        </li>
                    {/each}
                </ul>
                <button class="btn hot" disabled={!texPick} onclick={() => replaceTexture(fx && texPick === ms.outfit && !ms.autoSmall && quality ? shownOutfit() : texPick)}>{tx('vwImpTex')}</button>
                <button class="btn" onclick={exportTextures}>{tx('vwExpTex')}</button>
                {#if fx}<button class="btn" onclick={seeTextures}>{tx('vwSeeTex')}</button>{/if}
            {/if}

        {:else if rtab === 'import' && fx}
            <span class="lbl">{tx('vwModName')}</span>
            <ModTarget bind:value={target} placeholder={defaultTarget()} />
            <span class="lbl head">BLENDER</span>
            <div class="pair">
                <button class="btn" onclick={() => exportRig('fbx')}>{tx('vwExpRig')}</button>
                <button class="btn" onclick={() => exportRig('glb')}>{tx('vwExpRigGlb')}</button>
            </div>
            <div class="pair">
                <button class="btn" onclick={() => exportRig('fbx', true)}>{tx('vwExpAnims')}</button>
                <button class="btn" onclick={() => exportRig('glb', true)}>{tx('vwExpAnimsGlb')}</button>
            </div>
            <button class="btn hot" onclick={pickModel}>{tx('vwImpModel')}</button>
            <button class="btn" onclick={editCurrent} title={tx('vwEditNote')}>{tx(hasSource ? 'vwEditSaved' : 'vwEdit')}</button>
            <p class="note">{tx('vwImpModelNote')}</p>
            <button class="btn" onclick={() => pickImport('anim')}>{tx('vwImpAnim')}</button>
            {#if imp}
                <div class="imp">
                    <span class="lbl">{imp.file}</span>
                    <label class="lbl" for="itake">{tx('vwTake')}</label>
                    <select id="itake" class="sel" bind:value={imp.take}>
                        {#each imp.takes as t}<option value={t.id}>{t.name} ({t.frames})</option>{/each}
                    </select>
                    <label class="lbl" for="iclip">{tx('vwInto')}</label>
                    <select id="iclip" class="sel" bind:value={imp.clip}>
                        {#each fx.clips as c, i}<option value={i}>{c.name} ({c.to - c.from + 1})</option>{/each}
                    </select>
                    <p class="note">{tx('vwImpAnimNote')}</p>
                    <div class="pair">
                        <button class="btn hot" onclick={applyImport}>{tx('vwApply')}</button>
                        <button class="btn" onclick={() => imp = null}>{tx('vwCancel')}</button>
                    </div>
                </div>
            {/if}

        {:else if rtab === 'corr' && fx}
            <!-- CORRECTIVE ANIMATION LAYERS and the keyframe recorder -->
            <span class="lbl head">{tx('crTitle')}</span>
            <p class="note">{tx('crNote')}</p>
            <button class="btn" onclick={addLayer}>{tx('crAdd')}</button>
            {#each corr.layers as L, i}
                <div class="crl" class:hot={corr.cur === i}>
                    <button class="btn nm" onclick={() => { selectBlip(); corr.cur = i; }}>{L.name}</button>
                    <select class="sel" value={L.clip} onchange={e => { L.clip = Number(e.currentTarget.value); applyCorr(); }}>
                        <option value={-1}>{tx('crAll')}</option>
                        {#each fx.clips as c, ci}<option value={ci}>{c.name}</option>{/each}
                    </select>
                    <label class="chk"><input type="checkbox" checked={L.on} onchange={e => { L.on = e.currentTarget.checked; applyCorr(); }}><span>{tx('crOn')}</span></label>
                    <input type="range" min="0" max="1" step="0.05" value={L.weight} title={tx('crWeight')} oninput={e => { L.weight = Number(e.currentTarget.value); applyCorr(); }}>
                    <button class="eye" onclick={() => delLayer(i)} title={tx('crDel')}>✕</button>
                </div>
            {/each}
            {#if curLayer()}
                <span class="lbl">{tx('crBone')}</span>
                <select class="sel" value={corr.bone} onchange={e => { corr.bone = Number(e.currentTarget.value); }}>
                    <option value={-1}>-</option>
                    {#each fx.names as n, i}<option value={i}>{n}{curLayer().keys[i] ? ' · ' + curLayer().keys[i].length + ' ' + tx('crKeys') : ''}</option>{/each}
                </select>
                {#if corr.bone >= 0}
                    {#if curLayer().clip >= 0 && ms.clip !== curLayer().clip}
                        <p class="note">{tx('crPlayClip')}: {fx.clips[curLayer().clip].name}</p>
                    {/if}
                    {#each ['X', 'Y', 'Z'] as ax, c}
                        <label class="scale">
                            <span class="lbl">{ax} {shownTurn()[c].toFixed(1)}°</span>
                            <input type="range" min="-180" max="180" step="0.5" value={shownTurn()[c]} oninput={e => setTurn(c, e.currentTarget.value)}>
                        </label>
                    {/each}
                    <div class="pair">
                        <button class="btn" class:hot={corr.rec} onclick={() => { selectBlip(); corr.rec = !corr.rec; }}>{corr.rec ? '● REC' : '○ REC'}</button>
                        <button class="btn" onclick={() => { selectBlip(); writeKey(shownTurn()); }}>{tx('crKey')}</button>
                        <button class="btn" onclick={delKey}>{tx('crDelKey')}</button>
                    </div>
                    {#if curLayer().keys[corr.bone]}
                        <div class="keys">
                            {#each curLayer().keys[corr.bone] as k}
                                <button class="eye wide" class:hot={curLayer().clip >= 0 && k.f === ms.frame}
                                        onclick={() => { if (curLayer().clip >= 0) { ms.clip = curLayer().clip; ms.playing = false; ms.frame = k.f; } }}>{curLayer().clip < 0 ? tx('crAll') : k.f}</button>
                            {/each}
                        </div>
                    {/if}
                {/if}
            {/if}
            <span class="lbl">{tx('vwModName')}</span>
            <ModTarget bind:value={target} placeholder={defaultTarget()} />
            <button class="btn hot" onclick={saveCorr}>{tx('crSave')}</button>

        {:else if rtab === 'export'}
            <button class="btn" onclick={exportThis}>{tx('vwExport')}</button>
            {#if !texMode && layers.length}
                <!-- EXPORT ONLY THE LAYERS SHOWN (Mary, 2026-11-12): what LAYERS has on -->
                <span class="lbl head">{tx('vwExpLayers')} ({layers.filter(l => l.on && l.key[0] !== 'e').length}/{layers.filter(l => l.key[0] !== 'e').length})</span>
                <select class="sel" bind:value={layFormat}>
                    {#each LAY_FORMATS as [v, n]}<option value={v}>{n}</option>{/each}
                </select>
                <button class="btn hot" disabled={!layers.some(l => l.on && l.key[0] !== 'e')} onclick={exportLayers}>{tx('vwExpLayersGo')}</button>
                <p class="note">{tx('vwExpLayersNote')}</p>
            {/if}
            {#if !texMode && stems.length}<button class="btn" onclick={exportTextures}>{tx('vwExpTex')}</button>{/if}
        {/if}
        {#if done}<p class="note ok">{done}</p>{/if}
        {#if copied}<p class="note ok">{copied}</p>{/if}
    </aside>
</div>

{#if model && fx}
    <ModelImport f={modelF || fx} file={model} {texture} outfit={shownOutfit()} saved={modelSaved} picks={modelPicks} fresh={modelFresh} onApply={applyModel} onClose={() => { model = null; }} />
{/if}

<style>
    .viewer { display: grid; grid-template-columns: calc(var(--u) * 22) minmax(0, 1fr) calc(var(--u) * 24); gap: calc(var(--u) * 1.2); flex: 1; min-height: calc(var(--u) * 40); }
    .viewer.full { background: var(--bg); padding: calc(var(--u) * 1.2); }
    /* THE ROSTER's page, over the viewer (which stays, its 3D kept) */
    .viewer { position: relative; }
    .rosterpage { position: absolute; inset: 0; z-index: 5; display: grid; grid-template-columns: calc(var(--u) * 22) minmax(0, 1fr);
        gap: calc(var(--u) * 1.2); background: var(--bg, #0b0e14); }
    .viewer.full .rosterpage { inset: calc(var(--u) * 1.2); }
    .grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(calc(var(--u) * 10), 1fr)); gap: calc(var(--u) * .7); align-content: start;
        overflow-y: auto; min-height: 0; background: var(--well); border-radius: var(--r-sm); padding: calc(var(--u) * .8); }
    .cell { display: flex; flex-direction: column; align-items: stretch; gap: calc(var(--u) * .3); padding: calc(var(--u) * .3); background: none;
        border: 1px solid transparent; border-radius: var(--r-sm); cursor: pointer; color: var(--lav); min-width: 0; }
    .cell:hover { border-color: rgba(120, 140, 170, .6); color: #fff; }
    .cell.on { border-color: #e0463c; background: rgba(163, 32, 26, .45); color: #fff; }
    .cell .face, .who .face { aspect-ratio: 1; display: grid; place-items: center; background: #05070b; border-radius: 3px; overflow: hidden; }
    .cell .face img, .who .face img { width: 100%; height: 100%; object-fit: cover; }
    .cell .face i { font-style: normal; color: var(--dim); font-size: calc(var(--t) * 1.6); }
    .cell .nm { font-size: calc(var(--t) * 1.2); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; text-align: center; }
    .back { flex: none; }
    .who { display: flex; align-items: center; gap: calc(var(--u) * .8); flex: none; }
    .who .face { flex: 0 0 calc(var(--u) * 5); }
    .wn { color: #fff; font-size: calc(var(--t) * 1.5); letter-spacing: .06em; }
    .related { flex: 1; }
    .related .tex button { display: flex; align-items: center; gap: calc(var(--u) * .5); }
    .related .tex .pic { flex: 0 0 calc(var(--u) * 2.6); height: calc(var(--u) * 2.6); display: grid; place-items: center; border-radius: 2px;
        background: repeating-conic-gradient(#161c26 0 25%, #0b0e14 0 50%) 0 0 / 8px 8px; overflow: hidden; }
    .related .tex .pic img { max-width: 100%; max-height: 100%; }
    .related .tn { overflow: hidden; text-overflow: ellipsis; }
    .related .special { color: #ffd77a; }
    .related small, .spk { color: var(--dim); font-size: calc(var(--t) * 1); margin-left: calc(var(--u) * .4); }
    .spk { display: block; margin-left: 0; }
    .modonly { flex: none; }
    .seq { width: 100%; accent-color: var(--blue); }
    .side, .opts { display: flex; flex-direction: column; gap: calc(var(--u) * .6); min-height: 0; }
    .side .sel, .side .btn { flex: none; }
    .opts { overflow-x: hidden; overflow-y: auto; padding-right: calc(var(--u) * .3); }
    .opts .btn { white-space: normal; height: auto; min-height: var(--ctl-h); padding-top: calc(var(--u) * .4); padding-bottom: calc(var(--u) * .4); }
    .opts .sub.q3 { grid-template-columns: 1fr 1fr 1fr; }
    .opts .sub { display: grid; grid-template-columns: 1fr 1fr; gap: calc(var(--u) * .4); }
    .opts .sub .tab { font-size: calc(var(--t) * 1.15); padding: calc(var(--u) * .5) calc(var(--u) * .3); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
    .opts .btn, .opts .sel, .opts .edt { width: 100%; flex: none; }
    .opts .head { margin-top: calc(var(--u) * .8); border-top: 1px solid var(--line); padding-top: calc(var(--u) * .6); }
    .checks { display: flex; flex-wrap: wrap; gap: calc(var(--u) * .9); }
    .pair { display: flex; gap: calc(var(--u) * .5); }
    .look .pair .edt { flex: 1; width: auto; min-width: 0; }
    .look .pair .btn { flex: none; width: auto; }
    /* inside a fighter: MODELS / TEXTURES, the textures as a grid of pictures */
    .ltabs { margin-top: calc(var(--u) * .3); }
    .side.inside .related { flex: 1; min-height: 0; }
    .texgrid { flex: 1; min-height: 0; overflow-y: auto; display: grid; grid-template-columns: repeat(2, minmax(0, 1fr));
        gap: calc(var(--u) * .4); align-content: start; padding-right: 2px; }
    .tcell { display: flex; flex-direction: column; gap: 3px; padding: calc(var(--u) * .3); background: rgba(120, 140, 170, .1);
        border: 1px solid var(--line); border-radius: 4px; color: var(--lav); cursor: pointer; font: inherit; min-width: 0; }
    .tcell:hover { background: rgba(120, 140, 170, .26); color: #fff; }
    .tcell.on { border-color: #ffc890; background: rgba(170, 30, 40, .45); color: #fff; }
    .tcell .pic { display: block; aspect-ratio: 1; background: #0c0a1e; border-radius: 3px; overflow: hidden; }
    .tcell .pic img { width: 100%; height: 100%; object-fit: contain; display: block; }
    .tcell .tn { font-size: calc(var(--t) * .85); overflow: hidden; text-overflow: ellipsis; white-space: nowrap; text-align: center; }
    /* OUTFIT and COLOR: big numbered slots, the chosen one lit */
    .look { display: flex; flex-direction: column; gap: calc(var(--u) * .5); padding-bottom: calc(var(--u) * .7);
        margin-bottom: calc(var(--u) * .5); border-bottom: 1px solid var(--line); }
    .step { display: flex; align-items: center; gap: calc(var(--u) * .5); color: var(--lav); font-size: calc(var(--t) * 1.1); letter-spacing: .06em; }
    .step b { display: inline-grid; place-items: center; width: calc(var(--u) * 1.5); height: calc(var(--u) * 1.5); border-radius: 50%;
        background: rgba(255, 200, 144, .18); color: #ffc890; font-weight: 400; font-size: calc(var(--t) * .95); }
    .slots { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: calc(var(--u) * .4); }
    .slot { display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 2px; min-height: calc(var(--u) * 3.2);
        padding: calc(var(--u) * .35); background: rgba(120, 140, 170, .12); border: 1px solid var(--line); border-radius: 4px;
        color: var(--lav); cursor: pointer; font: inherit; min-width: 0; }
    .slot:hover { background: rgba(120, 140, 170, .28); color: #fff; }
    .slot.on { background: rgba(170, 30, 40, .55); border-color: #ffc890; color: #fff; }
    .slot.off { opacity: .6; }
    .slot.add { border-style: dashed; }
    .slot .n { font-size: calc(var(--t) * 1.5); line-height: 1; color: #ffc890; }
    .slot .t { font-size: calc(var(--t) * .95); max-width: 100%; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
    .slots.colors .slot { min-height: calc(var(--u) * 2.2); }
    .named { display: flex; flex-direction: column; gap: 2px; }
    .texcard { display: grid; grid-template-columns: calc(var(--u) * 6) minmax(0, 1fr); gap: calc(var(--u) * .5); align-items: center;
        padding: calc(var(--u) * .4); background: rgba(120, 140, 170, .1); border: 1px solid var(--line); border-radius: 4px; }
    .texcard .pic { display: block; aspect-ratio: 1; background: #0c0a1e; border-radius: 3px; overflow: hidden; }
    .texcard .pic img { width: 100%; height: 100%; object-fit: contain; display: block; }
    .texcard .tinfo { display: flex; flex-direction: column; gap: 3px; min-width: 0; }
    .texcard .tfile { color: #fff; font-size: calc(var(--t) * .95); overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
    .texcard .tfrom { color: var(--dim); font-size: calc(var(--t) * .85); }
    .dest { margin: 0; font-size: calc(var(--t) * .95); color: var(--dim); line-height: 1.4; }
    .dest.skin { color: #ffc890; }
    .note.warn { color: #ffb070; }
    .opts .pair .btn { flex: 1; width: auto; }
    .kinds { display: grid; grid-template-columns: 1fr 1fr; gap: calc(var(--u) * .4); }
    .kinds .tab { font-size: calc(var(--t) * 1.15); padding: calc(var(--u) * .5) calc(var(--u) * .3); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
    .search { flex: none; }
    .layer { display: flex; flex-direction: column; gap: calc(var(--u) * .3); }
    .imp { display: flex; flex-direction: column; gap: calc(var(--u) * .5); background: var(--well); border-radius: var(--r-sm); padding: calc(var(--u) * .6) calc(var(--u) * .8); }
    .list { list-style: none; margin: 0; padding: 0; overflow-y: auto; flex: 1; min-height: 0; background: var(--well); border-radius: var(--r-sm); }
    .list button {
        width: 100%; text-align: left; background: none; border: 0; cursor: pointer;
        padding: calc(var(--u) * .45) calc(var(--u) * .9); font-size: calc(var(--t) * 1.4); color: var(--lav);
        white-space: nowrap; overflow: hidden; text-overflow: ellipsis;
    }
    .list button:hover { color: #fff; background: rgba(120, 140, 170, .12); }
    .list button.on { color: #fff; background: rgba(163, 32, 26, .5); }
    .main { position: relative; display: flex; flex-direction: column; min-height: 0; }
    .view, .texview {
        flex: 1; min-height: 0; border-radius: var(--r-sm); overflow: hidden;
        background: radial-gradient(ellipse at 50% 35%, #1c2430, #05070b 75%);
        border: 1px solid var(--line);
    }
    .view :global(canvas) { width: 100%; height: 100%; display: block; }
    .view :global(canvas.navgizmo) { width: 96px; height: 96px; }
    .vname { position: absolute; left: calc(var(--u) * 1); bottom: calc(var(--u) * 1); font-size: calc(var(--t) * 1.1); color: var(--dim); pointer-events: none; letter-spacing: .06em; z-index: 2; }
    .texview { position: relative;
        background: repeating-conic-gradient(#161c26 0 25%, #0b0e14 0 50%) 0 0 / 24px 24px; }
    .texview canvas { position: absolute; inset: 0; margin: auto; max-width: 100%; max-height: 100%; }
    .hide { display: none; }
    .over { position: absolute; inset: 0 0 auto 0; height: 60%; display: grid; place-items: center; pointer-events: none; }
    .msg { color: var(--dim); letter-spacing: .08em; }
    .msg.bad { color: #ff9aa0; }
    .info { position: absolute; left: calc(var(--u) * 1); top: calc(var(--u) * .6); font-size: calc(var(--t) * 1.3); color: var(--dim); pointer-events: none; }
    .topbtns { position: absolute; right: calc(var(--u) * .8); top: calc(var(--u) * .6); display: flex; gap: calc(var(--u) * .5); }
    .topbtns .btn { height: auto; padding: calc(var(--u) * .35) calc(var(--u) * .9); font-size: calc(var(--t) * 1.2); opacity: .85; }
    /* SOLO MODEL: the side panels away, the view across */
    .viewer.solo { grid-template-columns: minmax(0, 1fr); }
    /* inside a fighter: choosing (the list and the viewer), then chosen
     * (the viewer and its options, the list gone) */
    .viewer.picking:not(.solo) { grid-template-columns: calc(var(--u) * 26) minmax(0, 1fr); }
    .viewer.picking .opts { display: none; }
    .viewer.chosen:not(.solo) { grid-template-columns: minmax(0, 1fr) calc(var(--u) * 26); }
    .viewer.chosen .side { display: none; }
    .viewer.solo .side, .viewer.solo .opts { display: none; }
    .play { display: flex; align-items: center; gap: calc(var(--u) * .9); margin-top: calc(var(--u) * .6); }
    .play input[type=range] { flex: 1; accent-color: var(--blue); }
    .play .speed { flex: 0 0 calc(var(--u) * 8); }
    .ok { color: var(--ok-text); word-break: break-all; }
    .rtabs { display: grid; grid-template-columns: repeat(3, 1fr); gap: calc(var(--u) * .3); margin-bottom: calc(var(--u) * .4); }
    .rtabs .tab { font-size: calc(var(--t) * 1.05); padding: calc(var(--u) * .45) calc(var(--u) * .2); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
    .layers, .texlist { list-style: none; margin: 0; padding: calc(var(--u) * .3); background: var(--well); border-radius: var(--r-sm); flex: none; max-height: calc(var(--u) * 36); overflow-y: auto; }
    .layers li { padding: calc(var(--u) * .2) 0; }
    .layers .chk span { white-space: nowrap; overflow: hidden; text-overflow: ellipsis; font-size: calc(var(--t) * 1.15); }
    .texlist button { display: flex; align-items: center; gap: calc(var(--u) * .6); width: 100%; background: none; border: 0; cursor: pointer; padding: calc(var(--u) * .3); border-radius: var(--r-sm); color: var(--lav); text-align: left; }
    .texlist button:hover { background: rgba(120, 140, 170, .12); color: #fff; }
    .texlist button.on { background: rgba(163, 32, 26, .5); color: #fff; }
    .texlist .pic { flex: 0 0 calc(var(--u) * 5); height: calc(var(--u) * 5); display: grid; place-items: center; border-radius: 3px;
        background: repeating-conic-gradient(#161c26 0 25%, #0b0e14 0 50%) 0 0 / 10px 10px; }
    .texlist .pic img { max-width: 100%; max-height: 100%; }
    .texlist .nm { flex: 1; min-width: 0; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; font-size: calc(var(--t) * 1.15); }
    .list .subhead { padding: calc(var(--u) * .4) calc(var(--u) * 1.6) calc(var(--u) * .1); font-size: calc(var(--t) * 1.1); color: var(--dim); letter-spacing: .08em; }
    .list .sub button { padding-left: calc(var(--u) * 2.2); font-size: calc(var(--t) * 1.25); }
    .list button.own { color: #fff; background: rgba(140, 125, 255, .15); }
    .crl { display: flex; flex-wrap: wrap; align-items: center; gap: 6px; padding: 6px; border-radius: var(--r-sm); background: var(--well); }
    .crl.hot { outline: 1px solid var(--blue); }
    .crl .nm { flex: 1 1 100%; }
    .crl .sel { flex: 1; min-width: 0; }
    .crl input[type=range] { flex: 1; min-width: 60px; }
    .keys { display: flex; flex-wrap: wrap; gap: 4px; }
    .keys .eye.wide { width: auto; padding: 0 8px; }
    .keys .eye.hot { border-color: var(--blue); }
</style>
