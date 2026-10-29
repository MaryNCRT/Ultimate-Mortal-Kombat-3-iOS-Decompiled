<script>
    /* REPLACE MODEL's window: the model brought in (mods/modelin.js), made
     * the fighter's .skin and shown on the fighter's own bones -- as the game
     * will draw it -- before anything is saved. Its animations play on it;
     * on the right, the model's textures, each kept on its own: its picture
     * (seen large on a click), the fighter's block it goes to, a picture to
     * put in its place, and an eye to hide its part or see it alone. */
    import { onMount, onDestroy } from 'svelte';
    import * as THREE from 'three';
    import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
    import { tx } from '#lib/app.svelte.js';
    import { api } from '#lib/platform.js';
    import { selectBlip } from '#lib/sound.js';
    import { readModel, buildSkin, withSkin } from '#lib/mods/modelin.js';
    import { framePose, posedVerts } from '#lib/mods/rig.js';
    import { decodePicture } from '#lib/mods/image.js';
    import { dataUrl } from '#lib/mods/thumbs.js';

    /* f the fighter; file what main.js file:pick gave; texture(stem) the
     * viewer's game textures; outfit the outfit shown; onApply({ r, mdl }),
     * onClose() */
    let { f, file, texture, outfit, onApply, onClose } = $props();

    let host = $state();
    let mdl = $state(null);
    let res = $state(null);                 /* buildSkin's answer */
    let err = $state('');
    let busy = $state(true);
    let turn = $state(0);
    let blockOf = $state([]);
    let thumbs = $state([]);
    let clip = $state(-1), frame = $state(0), playing = $state(false), loop = $state(true);
    let hidden = $state({});                /* material -> hidden */
    let big = $state(-1);                   /* the material seen large */

    let renderer, scene, camera, controls, root, raf, pf = null, meshes = [];

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
        new ResizeObserver(fit).observe(host);
        loop_();
    }
    function fit() {
        if (!renderer || !host) return;
        renderer.setSize(host.clientWidth, host.clientHeight, false);
        camera.aspect = host.clientWidth / Math.max(1, host.clientHeight);
        camera.updateProjectionMatrix();
    }
    let last = 0, acc = 0;
    function loop_(t = 0) {
        raf = requestAnimationFrame(loop_);
        const dt = Math.min(0.1, (t - last) / 1000);
        last = t;
        if (playing && pf && clip >= 0) {
            acc += dt;
            if (acc >= 1 / 30) {
                acc = 0;
                const c = pf.clips[clip];
                if (frame < c.to - c.from) frame++;
                else if (loop) frame = 0;
                else playing = false;
            }
        }
        controls && controls.update();
        renderer && renderer.render(scene, camera);
    }

    /* the model as the game will have it, built again after each change */
    async function rebuild() {
        if (!mdl) return;
        err = '';
        try {
            res = buildSkin(f, mdl, { turn, blockOf });
            pf = withSkin(f, res.bytes);
            await show();
        } catch (e) {
            err = String(e && e.message || e);
        }
    }

    async function show() {
        if (root) {
            root.traverse(o => { if (o.geometry) o.geometry.dispose(); if (o.material) o.material.dispose(); });
            scene.remove(root);
        }
        root = new THREE.Group();
        meshes = [];
        /* a mesh a part: each material with its own picture, as the game
         * draws them (runtime/skin_mats.c) */
        const maps = new Map();
        for (const [bi, blk] of pf.topo.blocks.entries()) {
            const ps = res.parts[bi].length ? res.parts[bi] : [{ mat: -1, first: 0, count: blk.count }];
            for (const p of ps) {
                const g = new THREE.BufferGeometry();
                g.setAttribute('position', new THREE.BufferAttribute(new Float32Array(pf.topo.src.length * 3), 3));
                g.setAttribute('uv', new THREE.BufferAttribute(Float32Array.from(pf.topo.uv), 2));
                const a = blk.first + p.first;
                g.setIndex(new THREE.BufferAttribute(pf.topo.faces.slice(a * 3, (a + p.count) * 3), 1));
                const M = p.mat >= 0 ? mdl.materials[p.mat] : null;
                let map = null;
                if (M && M.image) {
                    if (!maps.has(p.mat)) {
                        const t = new THREE.Texture(M.image);
                        t.flipY = false;
                        t.colorSpace = THREE.SRGBColorSpace;
                        t.wrapS = t.wrapT = THREE.RepeatWrapping;
                        t.needsUpdate = true;
                        maps.set(p.mat, t);
                    }
                    map = maps.get(p.mat);
                } else map = await texture(bi === 0 && outfit ? outfit : f.blockTex[bi]);
                const see = !!(M && M.alpha);
                const mesh = new THREE.Mesh(g, new THREE.MeshLambertMaterial({ map, color: map ? 0xffffff : 0x9a96c8,
                    side: THREE.DoubleSide, alphaTest: see ? 0.1 : 0.35, transparent: see }));
                mesh.userData.mat = p.mat;
                mesh.renderOrder = see ? 1 : 0;
                mesh.visible = !hidden[p.mat];
                root.add(mesh);
                meshes.push(mesh);
            }
        }
        scene.add(root);
        pose();
        const box = new THREE.Box3().setFromObject(root);
        const c = box.getCenter(new THREE.Vector3()), size = box.getSize(new THREE.Vector3()).length() || 1;
        controls.target.copy(c);
        camera.near = size / 500;
        camera.far = size * 50;
        camera.position.set(c.x + size * 0.15, c.y + size * 0.1, c.z + size * 1.1);
        camera.updateProjectionMatrix();
    }

    function pose() {
        if (!pf || !meshes.length) return;
        let p;
        if (clip >= 0) {
            const c = pf.clips[clip];
            p = framePose(pf.rig, c.from + Math.min(frame, c.to - c.from));
        } else p = pf.tpose || pf.bind.pose;
        const pos = posedVerts(pf.rig, pf.topo, p);
        for (const m of meshes) {
            m.geometry.attributes.position.array.set(pos);
            m.geometry.attributes.position.needsUpdate = true;
            m.geometry.computeVertexNormals();
            m.geometry.computeBoundingSphere();
        }
    }
    $effect(() => { clip; frame; pose(); });

    async function makeThumbs() {
        thumbs = await Promise.all(mdl.materials.map(async m => {
            if (!m.image) return null;
            const w = m.image.width || m.image.naturalWidth, h = m.image.height || m.image.naturalHeight;
            const k = Math.min(1, 96 / Math.max(w, h));
            const c = new OffscreenCanvas(Math.max(1, Math.round(w * k)), Math.max(1, Math.round(h * k)));
            c.getContext('2d').drawImage(m.image, 0, 0, c.width, c.height);
            return dataUrl(await c.convertToBlob({ type: 'image/png' }));
        }));
    }

    /* a picture for one of the model's materials */
    async function pickTex(i) {
        selectBlip();
        const got = await api.pickFile('picture');
        if (!got) return;
        const pic = await decodePicture(got.bytes).catch(() => null);
        if (!pic) { err = tx('vwNotPicture'); return; }
        mdl.materials[i].image = pic;
        mdl.materials[i].file = got.name;
        await makeThumbs();
        await rebuild();
    }
    async function gameTex(i) {
        selectBlip();
        mdl.materials[i].image = null;
        await makeThumbs();
        await rebuild();
    }

    /* the eye: hide a material's part; 1: it alone (again: all) */
    function toggle(i) {
        selectBlip();
        hidden = { ...hidden, [i]: !hidden[i] };
        for (const m of meshes) m.visible = !hidden[m.userData.mat];
    }
    function alone(i) {
        selectBlip();
        const only = !hidden[i] && mdl.materials.every((_, k) => k === i || hidden[k]);
        hidden = Object.fromEntries(mdl.materials.map((_, k) => [k, only ? false : k !== i]));
        for (const m of meshes) m.visible = !hidden[m.userData.mat];
    }
    async function bigUrl(i) {
        const im = mdl.materials[i].image;
        const c = new OffscreenCanvas(im.width || im.naturalWidth, im.height || im.naturalHeight);
        c.getContext('2d').drawImage(im, 0, 0);
        return dataUrl(await c.convertToBlob({ type: 'image/png' }));
    }

    async function setBlock(i, b) {
        blockOf[i] = Number(b);
        await rebuild();
    }

    async function turnIt() {
        selectBlip();
        turn = (turn + 1) % 4;
        await rebuild();
    }

    function apply() {
        if (!res) return;
        selectBlip();
        onApply({ r: res, mdl });
    }

    onMount(async () => {
        setup();
        try {
            mdl = await readModel(file);
            blockOf = mdl.materials.map((_, i) => Math.min(i, f.rig.skin.length - 1));
            await makeThumbs();
            await rebuild();
        } catch (e) {
            err = String(e && e.message || e);
        } finally {
            busy = false;
        }
    });
    onDestroy(() => {
        cancelAnimationFrame(raf);
        if (root) root.traverse(o => { if (o.geometry) o.geometry.dispose(); if (o.material) o.material.dispose(); });
        renderer && renderer.dispose();
    });
    const blockName = b => (b === 0 ? (outfit || f.blockTex[0]) : f.blockTex[b]);
</script>

<div class="veil" role="presentation">
    <div class="win" role="dialog" aria-label={tx('imTitle')}>
        <header>
            <span class="ttl">{tx('imTitle')} · {file.name}</span>
            <button class="btn x" onclick={onClose} aria-label={tx('vwCancel')}>✕</button>
        </header>
        <div class="body">
            <section class="main">
                <div class="view" bind:this={host}></div>
                <div class="over">
                    {#if busy}<span class="msg">{tx('vwLoading')}</span>
                    {:else if err}<span class="msg bad">{err}</span>{/if}
                </div>
                {#if res}
                    <div class="info">{res.verts} verts · {res.tris} tris · {res.matched ? res.matched + '/' + res.of + ' ' + tx('imBones') : tx('imNoBones')}{res.auto ? ' · ' + res.auto + ' ' + tx('imAuto') : ''}</div>
                {/if}
                {#if pf}
                    <div class="play">
                        <select class="sel" value={clip} onchange={e => { clip = Number(e.currentTarget.value); frame = 0; playing = clip >= 0; }}>
                            <option value={-1}>{tx('vwPose')}</option>
                            {#each pf.clips as c, i}<option value={i}>{c.name} ({c.to - c.from + 1})</option>{/each}
                        </select>
                        {#if clip >= 0}
                            <button class="btn" onclick={() => playing = !playing}>{playing ? tx('vwPause') : tx('vwPlay')}</button>
                            <label class="chk"><input type="checkbox" bind:checked={loop}><span>{tx('vwLoop')}</span></label>
                            <input type="range" min="0" max={pf.clips[clip].to - pf.clips[clip].from} bind:value={frame}>
                        {/if}
                    </div>
                {/if}
            </section>
            <aside class="side">
                <span class="lbl head">{tx('imTextures')}</span>
                <ul class="mats">
                    {#each mdl ? mdl.materials : [] as m, i}
                        <li>
                            <button class="pic" class:off={hidden[i]} onclick={() => { if (m.image) big = i; }} title={m.file || m.name}>{#if thumbs[i]}<img src={thumbs[i]} alt="">{:else}<span>{tx('imGameTex')}</span>{/if}</button>
                            <div class="what">
                                <div class="row">
                                    <span class="nm" title={m.file || m.name}>{m.name}{m.alpha ? ' · ' + tx('imAlpha') : ''}</span>
                                    <button class="eye" class:off={hidden[i]} onclick={() => toggle(i)} title={tx('imHide')}>{hidden[i] ? '◌' : '◉'}</button>
                                    <button class="eye" onclick={() => alone(i)} title={tx('imAlone')}>1</button>
                                </div>
                                <select class="sel" value={blockOf[i]} onchange={e => setBlock(i, e.currentTarget.value)} title={tx('imBlock')}>
                                    {#each f.rig.skin as _, b}<option value={b}>{tx('imBlock')} {b + 1}: {blockName(b)}</option>{/each}
                                </select>
                                <div class="pair">
                                    <button class="btn" onclick={() => pickTex(i)}>{tx('imPickTex')}</button>
                                    {#if m.image}<button class="btn" onclick={() => gameTex(i)}>{tx('imGameTex')}</button>{/if}
                                </div>
                            </div>
                        </li>
                    {/each}
                </ul>
                {#if res && !res.matched}
                    <button class="btn" onclick={turnIt}>{tx('imTurn')}</button>
                {/if}
                <p class="note">{tx('imNote')}</p>
                <div class="pair">
                    <button class="btn hot" disabled={!res || !!err} onclick={apply}>{tx('imApply')}</button>
                    <button class="btn" onclick={onClose}>{tx('vwCancel')}</button>
                </div>
            </aside>
        </div>
    </div>
    {#if big >= 0 && mdl && mdl.materials[big] && mdl.materials[big].image}
        <div class="bigpic" role="presentation" onclick={() => big = -1}>
            <span class="nm">{mdl.materials[big].name} · {mdl.materials[big].image.width}×{mdl.materials[big].image.height}{mdl.materials[big].alpha ? ' · ' + tx('imAlpha') : ''}</span>
            {#await bigUrl(big) then u}<img src={u} alt="">{/await}
        </div>
    {/if}
</div>

<style>
    .veil { position: fixed; inset: 0; z-index: 50; background: rgba(5, 4, 20, .72); display: grid; place-items: center; }
    .win {
        width: min(96vw, calc(var(--u) * 150)); height: min(92vh, calc(var(--u) * 90));
        display: flex; flex-direction: column; background: var(--bg); border: 1px solid var(--line);
        border-radius: var(--r-sm); box-shadow: 0 0 calc(var(--u) * 4) rgba(120, 100, 255, .35); padding: calc(var(--u) * 1);
    }
    header { display: flex; align-items: center; gap: calc(var(--u) * 1); margin-bottom: calc(var(--u) * .8); }
    .ttl { flex: 1; letter-spacing: .08em; color: #fff; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
    .x { height: auto; padding: calc(var(--u) * .3) calc(var(--u) * .9); }
    .body { flex: 1; min-height: 0; display: grid; grid-template-columns: minmax(0, 1fr) calc(var(--u) * 30); gap: calc(var(--u) * 1.2); }
    .main { position: relative; display: flex; flex-direction: column; min-height: 0; }
    .view { flex: 1; min-height: 0; border-radius: var(--r-sm); overflow: hidden; border: 1px solid var(--line);
        background: radial-gradient(ellipse at 50% 35%, #2a2560, #0a0820 75%); }
    .view :global(canvas) { width: 100%; height: 100%; display: block; }
    .over { position: absolute; inset: 0 0 auto 0; height: 60%; display: grid; place-items: center; pointer-events: none; }
    .msg { color: var(--dim); letter-spacing: .08em; }
    .msg.bad { color: #ff9aa0; max-width: 80%; text-align: center; }
    .info { position: absolute; left: calc(var(--u) * 1); top: calc(var(--u) * .6); font-size: calc(var(--t) * 1.3); color: var(--dim); pointer-events: none; }
    .play { display: flex; align-items: center; gap: calc(var(--u) * .9); margin-top: calc(var(--u) * .6); }
    .play .sel { flex: 0 0 calc(var(--u) * 22); }
    .play input[type=range] { flex: 1; accent-color: var(--blue); }
    .side { display: flex; flex-direction: column; gap: calc(var(--u) * .6); min-height: 0; }
    .side .btn { white-space: normal; height: auto; min-height: var(--ctl-h); }
    .mats { list-style: none; margin: 0; padding: 0; flex: 1; min-height: 0; overflow-y: auto; display: flex; flex-direction: column; gap: calc(var(--u) * .5); }
    .mats li { display: flex; gap: calc(var(--u) * .6); background: var(--well); border-radius: var(--r-sm); padding: calc(var(--u) * .5); }
    .pic { flex: 0 0 calc(var(--u) * 7); height: calc(var(--u) * 7); display: grid; place-items: center; border-radius: var(--r-sm);
        background: repeating-conic-gradient(#1b1840 0 25%, #14112f 0 50%) 0 0 / 12px 12px; font-size: calc(var(--t) * 1); color: var(--dim); text-align: center; }
    .pic img { max-width: 100%; max-height: 100%; }
    .pic { border: 0; padding: 0; cursor: zoom-in; }
    .pic.off { opacity: .35; }
    .row { display: flex; align-items: center; gap: calc(var(--u) * .4); }
    .row .nm { flex: 1; min-width: 0; }
    .eye { flex: none; width: calc(var(--u) * 2.6); height: calc(var(--u) * 2.6); border: 1px solid var(--line); border-radius: var(--r-sm);
        background: var(--bg); color: #fff; cursor: pointer; font-size: calc(var(--t) * 1.2); padding: 0; }
    .eye.off { color: var(--dim); }
    .bigpic { position: absolute; inset: 0; z-index: 2; display: flex; flex-direction: column; align-items: center; justify-content: center;
        gap: calc(var(--u) * .8); background: rgba(5, 4, 20, .9); cursor: zoom-out; }
    .bigpic img { max-width: 90%; max-height: 82%;
        background: repeating-conic-gradient(#1b1840 0 25%, #14112f 0 50%) 0 0 / 16px 16px; }
    .what { flex: 1; min-width: 0; display: flex; flex-direction: column; gap: calc(var(--u) * .35); }
    .nm { color: #fff; font-size: calc(var(--t) * 1.3); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
    .what .sel { width: 100%; font-size: calc(var(--t) * 1.15); }
    .pair { display: flex; gap: calc(var(--u) * .5); }
    .pair .btn { flex: 1; font-size: calc(var(--t) * 1.15); }
    .head { color: #fff; }
</style>
