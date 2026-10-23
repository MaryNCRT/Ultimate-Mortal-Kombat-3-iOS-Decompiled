<script>
    /* MODEL VIEWER: the game's own files, by kind -- fighters (in the T-pose,
     * the pose they were modelled in, or any of their animations, played as
     * the game skins them), stages, every other model, the textures. A
     * texture can be copied into a mod as PNG to paint over: the game takes
     * it back by its name.
     *
     * Everything can be seen as the game alone, with the mods that are on, or
     * with one mod. A fighter's rig goes out to Blender (FBX) and comes back:
     * an animation over one of its clips, or a new model on the same bones
     * (mods/fbxin.js), written into a mod and shown at once. */
    import { onMount, onDestroy } from 'svelte';
    import * as THREE from 'three';
    import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
    import { app, tx } from '#lib/app.svelte.js';
    import SharpSliders from '#lib/components/SharpSliders.svelte';
    import { api } from '#lib/platform.js';
    import { hoverBlip, selectBlip } from '#lib/sound.js';
    import { ms, src, loadCatalog, fighter, refreshMods, reloadMods, exportThis, forgetMod, VIEW_KINDS } from '#lib/mods/store.svelte.js';
    import { stageDesc, meshsetDesc, textureFile, pngBytes, exportFiles, isPng, attachVerts } from '#lib/mods/assets.js';
    import { readFbx, fbxScene, fbxTakes, importAnim, importModel, rigDesc } from '#lib/mods/fbxin.js';
    import { framePose, framePoseBlend, posedVerts, fitRest } from '#lib/mods/rig.js';
    import { subsurfCreate } from '#lib/mods/subsurf.js';
    import { decodePvr } from '#lib/mods/formats.js';
    import { putTexture } from '#lib/mods/image.js';
    import { homeOf, textureDir, exportTexDir } from '#lib/mods/owners.js';

    let host = $state();
    let pic = $state();
    let info = $state('');
    let texInfo = $state(null);
    let target = $state('');
    let copied = $state('');
    let fx = $state(null);                  /* the fighter open, for its clips */
    let stems = $state([]);                 /* the textures of what is open */
    let imp = $state(null);                 /* an FBX being brought in */
    let done = $state('');

    let renderer, scene, camera, controls, root, raf, skel;
    let fighterView = null;                 /* { f, meshes: [{ mesh, map }], rest } */

    const items = $derived.by(() => {
        if (!ms.cat) return [];
        const q = ms.search.trim().toLowerCase();
        const all = ms.cat[ms.kind] || [];
        const fid = ms.kind === 'textures' && ms.texFighter;
        const own = fid ? new RegExp('^(IPAD)?' + fid + '(_|BABY|BABILITY|PORTRAIT|X_)', 'i') : null;
        const mine = own ? all.filter(x => own.test(x.name)) : all;
        const hit = q ? mine.filter(x => x.name.toLowerCase().includes(q) || String(x.id).toLowerCase().includes(q)) : mine;
        return hit.slice(0, 400);
    });

    /* ------------------------------------------------------------ 3D -- */

    function setup() {
        renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
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
        camera.aspect = w / Math.max(1, h);
        camera.updateProjectionMatrix();
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
        controls && controls.update();
        renderer && renderer.render(scene, camera);
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
        controls.target.copy(c);
        camera.near = size / 500;
        camera.far = size * 50;
        camera.position.set(c.x + size * 0.15, c.y + size * 0.1, c.z + size * 1.1);
        camera.updateProjectionMatrix();
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
        done = '';
        texPick = '';
        stems = [];
        ms.loading = true;
        ms.playing = false;
        try {
            clear();
            if (ms.kind === 'textures') {
                await showTexture(item);
            } else if (ms.kind === 'characters') {
                const f = await fighter(item);
                if (my !== token) return;
                fx = f;
                stems = [...new Set([...f.blockTex, ...f.variants])];
                if (!f.variants.includes(ms.outfit)) ms.outfit = f.variants[0];
                if (ms.clip >= f.clips.length) ms.clip = -1;
                await buildFighter(f);
                info = item.name + ' · ' + f.rig.num + ' bones · ' + f.topo.src.length + ' verts · ' + f.clips.length + ' clips';
            } else {
                const d = ms.kind === 'stages' ? await stageDesc(src, item) : await meshsetDesc(src, item.id);
                if (my !== token) return;
                root = new THREE.Group();
                for (const m of d.meshes) root.add(await meshFrom(m, d.materials[m.material].stem));
                stems = [...new Set(d.materials.map(m => m.stem).filter(Boolean))];
                scene.add(root);
                frame(root, ms.kind === 'stages');
                info = item.name + ' · ' + d.meshes.length + ' meshes';
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
        for (const [bi, blk] of f.topo.blocks.entries()) {
            /* the block's triangles over all of the fighter's vertices: the
             * positions are replaced every frame, so no compaction */
            const g = new THREE.BufferGeometry();
            g.setAttribute('position', new THREE.BufferAttribute(new Float32Array(f.topo.src.length * 3), 3));
            g.setAttribute('uv', new THREE.BufferAttribute(Float32Array.from(f.topo.uv), 2));
            g.setIndex(new THREE.BufferAttribute(f.topo.faces.slice(blk.first * 3, (blk.first + blk.count) * 3), 1));
            const stem = bi === 0 && ms.outfit ? shownOutfit() : f.blockTex[bi];
            const map = await texture(stem);
            const mat = new THREE.MeshLambertMaterial({ map, color: map ? 0xffffff : 0x9a96c8, side: THREE.DoubleSide, alphaTest: 0.35, wireframe: ms.wire });
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
        skel = new THREE.LineSegments(new THREE.BufferGeometry(), new THREE.LineBasicMaterial({ color: 0xff5a5a, depthTest: false }));
        skel.renderOrder = 10;
        root.add(skel);
        scene.add(root);
        fighterView = { f, meshes, extras, smooth: { level: 0, blocks: [] } };
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
        const covered = (q, i) => boxes.some(({ lo, hi }) => q[i] >= lo[0] && q[i] <= hi[0] && q[i + 1] >= lo[1] && q[i + 1] <= hi[1] && q[i + 2] >= lo[2] && q[i + 2] <= hi[2]);
        /* SMOOTH MODEL: a block drawn smoothed when it has been built */
        const sm = fighterView.smooth;
        const smooth = sm && sm.level > 0 && sm.blocks.length === meshes.length;
        const skin = smooth ? skinOf(f, pos) : null;
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
            m.visible = !sp;
            if (b && b.mesh) b.mesh.visible = !!sp;
            const g = sp ? b.mesh.geometry : m.geometry;
            g.attributes.position.array.set(sp || pos);
            g.attributes.position.needsUpdate = true;
            g.computeVertexNormals();
            g.computeBoundingSphere();
            g.computeBoundingBox();
            m.material.wireframe = ms.wire;
        });
        for (const [ei, { mesh }] of (fighterView.extras || []).entries()) {
            const v = extraPos[ei];
            mesh.visible = !!v;
            if (!v) continue;
            mesh.geometry.attributes.position.array.set(v);
            mesh.geometry.attributes.position.needsUpdate = true;
            mesh.geometry.computeVertexNormals();
            mesh.geometry.computeBoundingSphere();
            mesh.material.wireframe = ms.wire;
        }
        const seg = [];
        if (ms.bones)
            f.rig.bones.parent.forEach((par, b) => { if (par >= 0) seg.push(...p[par].T, ...p[b].T); });
        skel.geometry.setAttribute('position', new THREE.Float32BufferAttribute(seg, 3));
    }

    $effect(() => {
        ms.clip; ms.frame; ms.pose; ms.bones; ms.wire; ms.extras;
        pose();
    });
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
        fighterView.meshes[0].material.map = map;
        fighterView.meshes[0].material.color.set(map ? 0xffffff : 0x9a96c8);
        fighterView.meshes[0].material.needsUpdate = true;
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
            const files = await putTexture(api, src, name, stem, got.bytes, ms.autoSmall, await textureDir(src, ms.cat, stem));
            if (!files) { ms.error = tx('vwNotPicture'); return; }
            await refreshMods();
            forgetMod();
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
            if (ms.pick && ms.cat) open(ms.pick);
        }
    });

    /* --------------------------------------------------------- layers -- */

    async function setLayer(v) {
        selectBlip();
        ms.layer = v;
        texCache.clear();
        if (ms.pick) await open(ms.pick);
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

    /* the rig to model over: T-pose, no animation, FBX with its textures */
    async function exportRig() {
        selectBlip();
        const files = await exportFiles(src, rigDesc(fx), 'fbx');
        done = tx('exDone') + ' ' + await api.exportSave(homeOf('characters', fx.ch) + '/RIG', files);
    }

    /* ---------------------------------------- bringing Blender's work in -- */

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
            let file, bytes, what;
            if (imp.kind === 'anim') {
                /* over the mod's own copy, if it has one: clips replaced
                 * before stay replaced */
                file = fx.ch.file + '.skinanim';
                const base = (await api.resRead(file, name)) || (await api.resRead(file, ''));
                const r = importAnim(fx, imp.sc, imp.take, imp.clip, base);
                bytes = r.bytes;
                what = fx.clips[imp.clip].name + ' · ' + r.frames + ' → ' + r.into + ' · ' + r.matched + '/' + r.of;
            } else {
                file = fx.ch.file + '.skin';
                const r = importModel(fx, imp.sc);
                bytes = r.bytes;
                what = r.verts + ' verts · ' + r.tris + ' tris · ' + r.matched + '/' + r.of;
            }
            await api.modsPut(name, homeOf('characters', fx.ch) + '/' + file, bytes);
            await refreshMods();
            const clip = imp.kind === 'anim' ? imp.clip : ms.clip;
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
        /* the capture aid: UMK3_LAUNCHER_PICK=characters/KANO[@clip] */
        const raw = new URLSearchParams(location.search).get('pick') || '';
        const cut = raw.indexOf('/'), at = raw.lastIndexOf('@');
        const want = [raw.slice(0, cut), raw.slice(cut + 1, at > cut ? at : undefined), at > cut ? raw.slice(at + 1) : ''];
        if (want[1] && ms.cat && ms.cat[want[0]]) {
            ms.kind = want[0];
            const it = ms.cat[want[0]].find(x => String(x.id).toUpperCase() === want[1].toUpperCase());
            if (it) await open(it);
            if (want[2]) { ms.clip = Number(want[2]); ms.frame = 2; }
            setTimeout(() => api.shot(), 300);
        } else if (ms.pick && ms.cat) open(ms.pick);
    });
    onDestroy(() => {
        cancelAnimationFrame(raf);
        clear();
        renderer && renderer.dispose();
    });
</script>

<div class="viewer" class:full class:solo={ms.solo} bind:this={mainEl}>
    <aside class="side">
        <label class="layer"><span class="lbl">{tx('vwLayer')}</span>
            <select class="sel" value={ms.layer} onchange={e => setLayer(e.currentTarget.value)}>
                <option value="">{tx('vwLayerGame')}</option>
                <option value="*">{tx('vwLayerOn')}</option>
                {#each ms.list as m}<option value={m.folder}>{m.name}</option>{/each}
            </select></label>
        <button class="btn" onclick={refresh}>{tx('vwRefresh')}</button>
        <div class="kinds">
            {#each VIEW_KINDS as [k, key]}
                <button class="tab" class:active={ms.kind === k} onpointerenter={hoverBlip}
                        onclick={() => { selectBlip(); ms.kind = k; ms.search = ''; }}>{tx(key)}</button>
            {/each}
        </div>
        {#if ms.kind === 'textures' && ms.cat}
            <select class="sel" bind:value={ms.texFighter}>
                <option value="">{tx('vwTexAll')}</option>
                {#each ms.cat.characters as c}<option value={c.id}>{c.name}</option>{/each}
            </select>
        {/if}
        <input class="edt search" placeholder={tx('vwSearch')} spellcheck="false" bind:value={ms.search}>
        <ul class="list">
            {#each items as it (it.id)}
                <li><button class:on={ms.pick && ms.pick.id === it.id} onclick={() => open(it)}>{it.name}</button></li>
            {/each}
        </ul>
    </aside>

    <section class="main">
        <div class="view" class:hide={ms.kind === 'textures'} bind:this={host}></div>
        {#if ms.kind === 'textures'}
            <div class="texview"><canvas bind:this={pic} class:hide={!texInfo}></canvas></div>
        {/if}
        <div class="over">
            {#if ms.noGame}<span class="msg">{tx('vwNoGame')}</span>
            {:else if ms.loading}<span class="msg">{tx('vwLoading')}</span>
            {:else if ms.error}<span class="msg bad">{ms.error}</span>
            {:else if !ms.pick}<span class="msg">{tx('vwPick')}</span>{/if}
        </div>
        <div class="info">{info}</div>
        <div class="topbtns">
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
            {#if fx.variants.length > 1}
                <span class="lbl">{tx('vwOutfit')}</span>
                <select class="sel" value={ms.outfit} onchange={e => outfit(e.currentTarget.value)}>
                    {#each fx.variants as v}<option value={v}>{v}</option>{/each}
                </select>
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
            <div class="checks">
                <label class="chk"><input type="checkbox" bind:checked={ms.bones}><span>{tx('vwBones')}</span></label>
                <label class="chk"><input type="checkbox" bind:checked={ms.wire}><span>{tx('vwWire')}</span></label>
                {#if fx.attach && fx.attach.length}
                    <label class="chk"><input type="checkbox" bind:checked={ms.extras}><span>{tx('vwExtras')}</span></label>
                {/if}
            </div>
        {:else if ms.pick && ms.kind !== 'textures' && !ms.loading}
            <div class="checks">
                <label class="chk"><input type="checkbox" bind:checked={ms.wire}
                    onchange={() => root && root.traverse(o => { if (o.material) o.material.wireframe = ms.wire; })}><span>{tx('vwWire')}</span></label>
            </div>
        {/if}

        {#if ms.pick && (ms.kind === 'textures' ? texInfo : stems.length)}
            <span class="lbl">{tx('vwModName')}</span>
            <input class="edt" list="modnames" spellcheck="false" bind:value={target} placeholder={defaultTarget()}>
            <datalist id="modnames">{#each ms.list as m}<option value={m.folder}></option>{/each}</datalist>

            <span class="lbl head">{tx('vwTexHead')}</span>
            {#if ms.kind === 'textures'}
                <label class="chk"><input type="checkbox" bind:checked={ms.autoSmall}><span>{tx('vwAutoSmall')}</span></label>
                <button class="btn hot" onclick={() => replaceTexture(texInfo.name)}>{tx('vwImpTex')}</button>
                <button class="btn" onclick={toMod}>{tx('vwToMod')}</button>
            {:else}
                {#if fx}
                    <span class="lbl">{ms.autoSmall || !quality ? ms.outfit : shownOutfit()}</span>
                {:else}
                    <select class="sel" bind:value={texPick}>
                        {#each stems as s}<option value={s}>{s}</option>{/each}
                    </select>
                {/if}
                <label class="chk"><input type="checkbox" bind:checked={ms.autoSmall}><span>{tx('vwAutoSmall')}</span></label>
                <button class="btn hot" onclick={() => replaceTexture(fx ? (ms.autoSmall || !quality ? ms.outfit : shownOutfit()) : (texPick || stems[0]))}>{tx('vwImpTex')}</button>
                <button class="btn" onclick={exportTextures}>{tx('vwExpTex')}</button>
                {#if fx}<button class="btn" onclick={seeTextures}>{tx('vwSeeTex')}</button>{/if}
            {/if}

            {#if ms.kind === 'characters' && fx}
                <span class="lbl head">BLENDER</span>
                <button class="btn" onclick={exportRig}>{tx('vwExpRig')}</button>
                <button class="btn" onclick={() => pickImport('anim')}>{tx('vwImpAnim')}</button>
                <button class="btn" onclick={() => pickImport('model')}>{tx('vwImpModel')}</button>
                {#if imp}
                    <div class="imp">
                        <span class="lbl">{imp.file}</span>
                        {#if imp.kind === 'anim'}
                            <label class="lbl" for="itake">{tx('vwTake')}</label>
                            <select id="itake" class="sel" bind:value={imp.take}>
                                {#each imp.takes as t}<option value={t.id}>{t.name} ({t.frames})</option>{/each}
                            </select>
                            <label class="lbl" for="iclip">{tx('vwInto')}</label>
                            <select id="iclip" class="sel" bind:value={imp.clip}>
                                {#each fx.clips as c, i}<option value={i}>{c.name} ({c.to - c.from + 1})</option>{/each}
                            </select>
                        {/if}
                        <p class="note">{imp.kind === 'anim' ? tx('vwImpAnimNote') : tx('vwImpModelNote')}</p>
                        <div class="pair">
                            <button class="btn hot" onclick={applyImport}>{tx('vwApply')}</button>
                            <button class="btn" onclick={() => imp = null}>{tx('vwCancel')}</button>
                        </div>
                    </div>
                {/if}
            {/if}

            <span class="lbl head">{tx('vwExpHead')}</span>
            <button class="btn" onclick={exportThis}>{tx('vwExport')}</button>
        {/if}
        {#if done}<p class="note ok">{done}</p>{/if}
        {#if copied}<p class="note ok">{copied}</p>{/if}
    </aside>
</div>

<style>
    .viewer { display: grid; grid-template-columns: calc(var(--u) * 22) minmax(0, 1fr) calc(var(--u) * 24); gap: calc(var(--u) * 1.2); flex: 1; min-height: calc(var(--u) * 40); }
    .viewer.full { background: var(--bg); padding: calc(var(--u) * 1.2); }
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
    .list button:hover { color: #fff; background: rgba(150, 140, 255, .12); }
    .list button.on { color: #fff; background: rgba(140, 125, 255, .3); }
    .main { position: relative; display: flex; flex-direction: column; min-height: 0; }
    .view, .texview {
        flex: 1; min-height: 0; border-radius: var(--r-sm); overflow: hidden;
        background: radial-gradient(ellipse at 50% 35%, #2a2560, #0a0820 75%);
        border: 1px solid var(--line);
    }
    .view :global(canvas) { width: 100%; height: 100%; display: block; }
    .texview { position: relative;
        background: repeating-conic-gradient(#1b1840 0 25%, #14112f 0 50%) 0 0 / 24px 24px; }
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
    .viewer.solo .side, .viewer.solo .opts { display: none; }
    .play { display: flex; align-items: center; gap: calc(var(--u) * .9); margin-top: calc(var(--u) * .6); }
    .play input[type=range] { flex: 1; accent-color: var(--blue); }
    .play .speed { flex: 0 0 calc(var(--u) * 8); }
    .ok { color: var(--ok-text); word-break: break-all; }
</style>
