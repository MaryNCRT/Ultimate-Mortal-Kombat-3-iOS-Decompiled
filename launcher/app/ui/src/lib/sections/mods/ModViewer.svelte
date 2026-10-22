<script>
    /* MODEL VIEWER: the game's own files, by kind -- fighters (in the T-pose,
     * the pose they were modelled in, or any of their animations, played as
     * the game skins them), stages, every other model, the textures. A
     * texture can be copied into a mod as PNG to paint over: the game takes
     * it back by its name. */
    import { onMount, onDestroy } from 'svelte';
    import * as THREE from 'three';
    import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
    import { tx } from '#lib/app.svelte.js';
    import { api } from '#lib/platform.js';
    import { hoverBlip, selectBlip } from '#lib/sound.js';
    import { ms, src, loadCatalog, fighter, refreshMods, exportThis, VIEW_KINDS } from '#lib/mods/store.svelte.js';
    import { stageDesc, meshsetDesc, textureFile, pngBytes } from '#lib/mods/assets.js';
    import { framePose, posedVerts, fitRest } from '#lib/mods/rig.js';
    import { decodePvr } from '#lib/mods/formats.js';

    let host = $state();
    let pic = $state();
    let info = $state('');
    let texInfo = $state(null);
    let target = $state('');
    let copied = $state('');
    let fx = $state(null);                  /* the fighter open, for its clips */

    let renderer, scene, camera, controls, root, raf, skel;
    let fighterView = null;                 /* { f, meshes: [{ mesh, map }], rest } */

    const items = $derived.by(() => {
        if (!ms.cat) return [];
        const q = ms.search.trim().toLowerCase();
        const all = ms.cat[ms.kind] || [];
        const hit = q ? all.filter(x => x.name.toLowerCase().includes(q) || String(x.id).toLowerCase().includes(q)) : all;
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

    function fit() {
        if (!renderer || !host) return;
        const w = host.clientWidth, h = host.clientHeight;
        renderer.setSize(w, h, false);
        camera.aspect = w / Math.max(1, h);
        camera.updateProjectionMatrix();
    }

    let last = 0, acc = 0;
    function loop(t = 0) {
        raf = requestAnimationFrame(loop);
        const dt = Math.min(0.1, (t - last) / 1000);
        last = t;
        if (ms.playing && fighterView && ms.clip >= 0) {
            acc += dt;
            if (acc >= 1 / 30) {
                acc = 0;
                const c = fighterView.f.clips[ms.clip];
                ms.frame = ms.frame >= c.to - c.from ? 0 : ms.frame + 1;
            }
        }
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
            if (/\.pvr$/i.test(file)) {
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
                if (!f.variants.includes(ms.outfit)) ms.outfit = f.variants[0];
                if (ms.clip >= f.clips.length) ms.clip = -1;
                await buildFighter(f);
                info = item.name + ' · ' + f.rig.num + ' bones · ' + f.topo.src.length + ' verts · ' + f.clips.length + ' clips';
            } else {
                const d = ms.kind === 'stages' ? await stageDesc(src, item) : await meshsetDesc(src, item.id);
                if (my !== token) return;
                root = new THREE.Group();
                for (const m of d.meshes) root.add(await meshFrom(m, d.materials[m.material].stem));
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
            const stem = bi === 0 && ms.outfit ? ms.outfit : f.blockTex[bi];
            const map = await texture(stem);
            const mat = new THREE.MeshLambertMaterial({ map, color: map ? 0xffffff : 0x9a96c8, side: THREE.DoubleSide, alphaTest: 0.35, wireframe: ms.wire });
            const mesh = new THREE.Mesh(g, mat);
            root.add(mesh);
            meshes.push(mesh);
        }
        skel = new THREE.LineSegments(new THREE.BufferGeometry(), new THREE.LineBasicMaterial({ color: 0xff5a5a, depthTest: false }));
        skel.renderOrder = 10;
        root.add(skel);
        scene.add(root);
        fighterView = { f, meshes };
        pose();
        frame(root);
    }

    /* the fighter in what the controls say: a rest pose, or a frame of a clip */
    function pose() {
        if (!fighterView) return;
        const { f, meshes } = fighterView;
        let p, pos;
        if (ms.clip >= 0) {
            const c = f.clips[ms.clip];
            p = framePose(f.rig, c.from + Math.min(ms.frame, c.to - c.from));
            pos = posedVerts(f.rig, f.topo, p);
        } else if (ms.pose === 'tpose' && f.tpose) {
            p = f.tpose;
            pos = f.fitted ||= fitRest(f.rig, f.topo, f.weights, p);
        } else {
            p = f.bind.pose;
            pos = posedVerts(f.rig, f.topo, p);
        }
        for (const m of meshes) {
            m.geometry.attributes.position.array.set(pos);
            m.geometry.attributes.position.needsUpdate = true;
            m.geometry.computeVertexNormals();
            m.geometry.computeBoundingSphere();
            m.geometry.computeBoundingBox();
            m.material.wireframe = ms.wire;
        }
        const seg = [];
        if (ms.bones)
            f.rig.bones.parent.forEach((par, b) => { if (par >= 0) seg.push(...p[par].T, ...p[b].T); });
        skel.geometry.setAttribute('position', new THREE.Float32BufferAttribute(seg, 3));
    }

    $effect(() => {
        ms.clip; ms.frame; ms.pose; ms.bones; ms.wire;
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

    async function outfit(v) {
        ms.outfit = v;
        if (fighterView) {
            const map = await texture(v);
            fighterView.meshes[0].material.map = map;
            fighterView.meshes[0].material.needsUpdate = true;
        }
    }

    /* ------------------------------------------------------- textures -- */

    async function showTexture(item) {
        const b = await src.read(item.id);
        let w, h, data;
        if (/\.pvr$/i.test(item.id)) {
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
        await api.modsPut(name, texInfo.name + '.png', png);
        await refreshMods();
        selectBlip();
        copied = tx('vwCopied') + name + '\\' + texInfo.name + '.png';
    }

    onMount(async () => {
        setup();
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

<div class="viewer">
    <aside class="side">
        <div class="kinds">
            {#each VIEW_KINDS as [k, key]}
                <button class="tab" class:active={ms.kind === k} onpointerenter={hoverBlip}
                        onclick={() => { selectBlip(); ms.kind = k; ms.search = ''; }}>{tx(key)}</button>
            {/each}
        </div>
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

        {#if ms.kind === 'characters' && fx}
            <div class="ctl">
                <select class="sel" value={ms.clip} onchange={e => { ms.clip = Number(e.currentTarget.value); ms.frame = 0; }}>
                    <option value={-1}>{tx('vwPose')}</option>
                    {#each fx.clips as c, i}<option value={i}>{c.name} ({c.to - c.from + 1})</option>{/each}
                </select>
                {#if ms.clip < 0}
                    <span class="tabs sub">
                        <button class="tab" class:active={ms.pose === 'tpose'} disabled={!fx.tpose} onclick={() => ms.pose = 'tpose'}>{tx('vwTpose')}</button>
                        <button class="tab" class:active={ms.pose === 'bind'} onclick={() => ms.pose = 'bind'}>{tx('vwBind')}</button>
                    </span>
                {:else}
                    <button class="btn" onclick={() => ms.playing = !ms.playing}>{ms.playing ? tx('vwPause') : tx('vwPlay')}</button>
                    <input type="range" min="0" max={fx.clips[ms.clip].to - fx.clips[ms.clip].from} bind:value={ms.frame}>
                    <span class="lbl">{tx('vwFrame')} {ms.frame + 1}</span>
                {/if}
                {#if fx.variants.length > 1}
                    <select class="sel" value={ms.outfit} onchange={e => outfit(e.currentTarget.value)}>
                        {#each fx.variants as v}<option value={v}>{v}</option>{/each}
                    </select>
                {/if}
                <label class="chk"><input type="checkbox" bind:checked={ms.bones}><span>{tx('vwBones')}</span></label>
                <label class="chk"><input type="checkbox" bind:checked={ms.wire}><span>{tx('vwWire')}</span></label>
                <button class="btn hot" onclick={exportThis}>{tx('vwExport')}</button>
            </div>
        {:else if ms.kind === 'textures' && texInfo}
            <div class="ctl">
                <label class="lbl" for="tmod">{tx('vwModName')}</label>
                <input id="tmod" class="edt" list="modnames" spellcheck="false" bind:value={target}>
                <datalist id="modnames">{#each ms.list as m}<option value={m.folder}></option>{/each}</datalist>
                <button class="btn hot" onclick={toMod}>{tx('vwToMod')}</button>
                <button class="btn" onclick={exportThis}>{tx('vwExport')}</button>
            </div>
            {#if copied}<p class="note ok">{copied}</p>{/if}
        {:else if ms.pick && ms.kind !== 'characters'}
            <div class="ctl">
                <label class="chk"><input type="checkbox" bind:checked={ms.wire}
                    onchange={() => root && root.traverse(o => { if (o.material) o.material.wireframe = ms.wire; })}><span>{tx('vwWire')}</span></label>
                <button class="btn hot" onclick={exportThis}>{tx('vwExport')}</button>
            </div>
        {/if}
    </section>
</div>

<style>
    .viewer { display: grid; grid-template-columns: calc(var(--u) * 26) minmax(0, 1fr); gap: calc(var(--u) * 1.4); flex: 1; min-height: calc(var(--u) * 40); }
    .side { display: flex; flex-direction: column; gap: calc(var(--u) * .6); min-height: 0; }
    .kinds { display: grid; grid-template-columns: 1fr 1fr; gap: calc(var(--u) * .4); }
    .kinds .tab { font-size: calc(var(--u) * 1.25); padding: calc(var(--u) * .5) calc(var(--u) * .4); }
    .search { flex: none; }
    .list { list-style: none; margin: 0; padding: 0; overflow-y: auto; flex: 1; min-height: 0; background: var(--well); border-radius: var(--r-sm); }
    .list button {
        width: 100%; text-align: left; background: none; border: 0; cursor: pointer;
        padding: calc(var(--u) * .45) calc(var(--u) * .9); font-size: calc(var(--u) * 1.4); color: var(--lav);
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
    .info { position: absolute; left: calc(var(--u) * 1); top: calc(var(--u) * .6); font-size: calc(var(--u) * 1.3); color: var(--dim); pointer-events: none; }
    .ctl { display: flex; align-items: center; gap: calc(var(--u) * .9); margin-top: calc(var(--u) * .8); flex-wrap: wrap; }
    .ctl .sel { flex: 0 1 calc(var(--u) * 22); }
    .ctl input[type=range] { flex: 1; min-width: calc(var(--u) * 10); accent-color: var(--blue); }
    .ctl .edt { flex: 0 1 calc(var(--u) * 20); }
    .ok { color: var(--ok-text); }
</style>
