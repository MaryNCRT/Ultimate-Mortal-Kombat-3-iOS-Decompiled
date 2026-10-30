<script>
    /* REPLACE MODEL's window: the model brought in (mods/modelin.js), made
     * the fighter's .skin and shown on the fighter's own bones -- as the game
     * will draw it -- before anything is saved. Its animations play on it;
     * on the right, the model's textures, each kept on its own: its picture
     * (seen large on a click), the fighter's block it goes to and the game
     * texture it replaces (its picture is saved as that texture, and the
     * game draws it on the model's own UVs) or a new texture of its own, a
     * picture to put in its place, and an eye to hide its part or see it
     * alone. Over the view: how it is drawn (textures, plain, wire, its
     * bones' weights), the rig it will have (bones and joints, as they move
     * in the animation chosen) and the floor. At the side, two tabs:
     * MATERIALS, and RIG -- fitted or kept proportions, the file's pose (T
     * or A), the arms' and legs' angle and roll, the size about 0,0,0, and
     * each joint moved by hand (picked on the view or from the list, its
     * other side mirrored), as gtastuff.com's SA viewer does, with the
     * weights kept or worked out again. */
    import { onMount, onDestroy } from 'svelte';
    import * as THREE from 'three';
    import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
    import { app, tx } from '#lib/app.svelte.js';
    import { api } from '#lib/platform.js';
    import { selectBlip } from '#lib/sound.js';
    import { readModel, buildSkin, withSkin, solidPicture, poseName } from '#lib/mods/modelin.js';
    import { makeFloor, placeFloor, weightColors, applyLook, makeRigLines, poseRigLines } from '#lib/mods/viewkit.js';
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
    let keep = $state(false);               /* PROPORTION TRICK (a humanoid rig) */
    let scale = $state(1);                  /* about the pivot (buildSkin o.scale) */
    let blockOf = $state([]);
    let thumbs = $state([]);
    let clip = $state(-1), frame = $state(0), playing = $state(false), loop = $state(true);
    let hidden = $state({});                /* material -> hidden */
    let big = $state(-1);                   /* the material seen large */
    /* ARM / LEG ANGLE and ROLL (buildSkin o.adjust), degrees */
    let adjust = $state({ armAngle: 0, armRoll: 0, legAngle: 0, legRoll: 0, auto: false });
    let tab = $state('rig');                 /* the side's tab: 'mats' or 'rig' */
    let joints = $state({});                /* bone -> [dx, dy, dz] (buildSkin o.joints) */
    let sel = $state(-1);                   /* the joint being moved */
    let mirror = $state(true), reweight = $state(false);
    let look = $state({ wire: false, solid: false, weights: false });
    let showRig = $state(true), showFloor = $state(true);

    let renderer, scene, camera, controls, root, raf, meshes = [], floor, rigLines, lastPose = null;
    /* the fighter with the model, its clips under the view (state: the bar
     * shows once it is built) */
    let pf = $state.raw(null);

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
        floor = makeFloor();
        rigLines = makeRigLines();
        scene.add(floor, rigLines);
        new ResizeObserver(fit).observe(host);
        renderer.domElement.addEventListener('pointerdown', pickStart);
        renderer.domElement.addEventListener('pointerup', pickJoint);
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
            res = buildSkin(f, mdl, { turn, blockOf, keep, scale, adjust: { ...adjust }, joints: $state.snapshot(joints), reweight });
            pf = withSkin(f, res.bytes, res.bones);
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
        const colors = weightColors(pf.weights, pf.topo.src.length);
        const pic = (key, im) => {
            if (!im) return null;
            if (!maps.has(key)) {
                const t = new THREE.Texture(im);
                t.flipY = false;
                if (!key.startsWith('n')) t.colorSpace = THREE.SRGBColorSpace;
                t.wrapS = t.wrapT = THREE.RepeatWrapping;
                t.needsUpdate = true;
                maps.set(key, t);
            }
            return maps.get(key);
        };
        for (const [bi, blk] of pf.topo.blocks.entries()) {
            const ps = res.parts[bi].length ? res.parts[bi] : [{ mat: -1, first: 0, count: blk.count }];
            for (const p of ps) {
                const g = new THREE.BufferGeometry();
                g.setAttribute('position', new THREE.BufferAttribute(new Float32Array(pf.topo.src.length * 3), 3));
                g.setAttribute('uv', new THREE.BufferAttribute(Float32Array.from(pf.topo.uv), 2));
                g.setAttribute('color', new THREE.BufferAttribute(colors, 3));
                const a = blk.first + p.first;
                g.setIndex(new THREE.BufferAttribute(pf.topo.faces.slice(a * 3, (a + p.count) * 3), 1));
                const M = p.mat >= 0 ? mdl.materials[p.mat] : null;
                /* the material as the file has it: its picture, else its
                 * colour; the game's texture only when chosen */
                let map = null, color = 0xffffff;
                if (M && M.image) map = pic('d' + p.mat, M.image);
                else if (!M || M.game || M.rep) map = await texture((M && M.rep) || (bi === 0 && outfit ? outfit : f.blockTex[bi]));
                else color = M.color ? new THREE.Color().setRGB(M.color[0] / 255, M.color[1] / 255, M.color[2] / 255, THREE.SRGBColorSpace) : 0xb4b4b4;
                const see = !!(M && M.alpha);
                const mesh = new THREE.Mesh(g, new THREE.MeshPhongMaterial({ map, color,
                    normalMap: M ? pic('n' + p.mat, M.normal) : null, specularMap: M ? pic('s' + p.mat, M.specular) : null,
                    specular: M && M.specular ? 0xffffff : 0x111111, shininess: 30,
                    side: THREE.DoubleSide, alphaTest: see ? 0.1 : 0.35, transparent: see }));
                applyLook(mesh.material, look);
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
        placeFloor(floor, box, true);
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
        } else p = res.rest;
        const pos = posedVerts(pf.rig, pf.topo, p);
        for (const m of meshes) {
            m.geometry.attributes.position.array.set(pos);
            m.geometry.attributes.position.needsUpdate = true;
            m.geometry.computeVertexNormals();
            m.geometry.computeBoundingSphere();
        }
        poseRigLines(rigLines, p, pf.rig.bones.parent, showRig, sel);
        lastPose = p;
    }
    $effect(() => { clip; frame; showRig; sel; pose(); });
    $effect(() => { const l = { ...look }; for (const m of meshes) applyLook(m.material, l); });
    $effect(() => { if (floor) floor.visible = showFloor; });

    async function makeThumbs() {
        thumbs = await Promise.all(mdl.materials.map(async m => {
            /* its picture, else its colour (a game texture: no thumb) */
            const im = m.image || (m.game || m.rep ? null : solidPicture(m.color));
            if (!im) return null;
            const w = im.width || im.naturalWidth, h = im.height || im.naturalHeight;
            const k = Math.min(1, 96 / Math.max(w, h));
            const c = new OffscreenCanvas(Math.max(1, Math.round(w * k)), Math.max(1, Math.round(h * k)));
            c.getContext('2d').drawImage(im, 0, 0, c.width, c.height);
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
        mdl.materials[i].game = false;
        await makeThumbs();
        await rebuild();
    }
    async function gameTex(i) {
        selectBlip();
        mdl.materials[i].image = null;
        mdl.materials[i].game = true;    /* a copy of the outfit's, saved as its own */
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

    /* the block a material goes to and what its picture becomes: "b|STEM"
     * replaces that game texture, "b|" a new texture of its own */
    const choices = () => (f ? f.rig.skin.flatMap((_, b) => [
        ...(b === 0 ? texOutfits() : [f.blockTex[b]]).map(t => ({ v: b + '|' + t, b, t })),
        { v: b + '|', b, t: '' },
    ]) : []);
    const texOutfits = () => {
        const v = (f.variants || []).filter(t => !/_ICE$/i.test(t));
        return v.length ? v : [outfit || f.blockTex[0]];
    };
    const choiceOf = i => blockOf[i] + '|' + (mdl.materials[i].rep || '');
    async function setChoice(i, v) {
        const [b, t] = v.split('|');
        blockOf[i] = Number(b);
        mdl.materials[i].rep = t;
        await makeThumbs();
        await rebuild();
    }

    let scaleWait;
    function setScale(v) {
        scale = Math.min(4, Math.max(0.25, Number(v) || 1));
        clearTimeout(scaleWait);
        scaleWait = setTimeout(rebuild, 150);
    }

    /* a click (not a drag of the camera) on a joint picks it */
    let downAt = null;
    function pickStart(e) { downAt = [e.clientX, e.clientY]; }
    function pickJoint(e) {
        if (!downAt || !lastPose || !showRig || Math.hypot(e.clientX - downAt[0], e.clientY - downAt[1]) > 4) return;
        const r = renderer.domElement.getBoundingClientRect();
        const x = (e.clientX - r.left) / r.width * 2 - 1, y = -(e.clientY - r.top) / r.height * 2 + 1;
        let best = -1, bd = 0.03;
        lastPose.forEach((b, i) => {
            if (!b || i === f.rig.bones.root) return;
            const v = new THREE.Vector3(...b.T).project(camera);
            const d = Math.hypot(v.x - x, (v.y - y) * r.height / r.width);
            if (v.z < 1 && d < bd) { bd = d; best = i; }
        });
        if (best >= 0) { selectBlip(); sel = best; tab = 'rig'; }
    }

    /* BONE POSITIONS: the joint picked, moved; its other side mirrored */
    const twin = i => {
        const n = f.names[i], t = n.endsWith('.L') ? n.slice(0, -2) + '.R' : n.endsWith('.R') ? n.slice(0, -2) + '.L' : null;
        return t ? f.names.indexOf(t) : -1;
    };
    let jointWait;
    function setJoint(k, v) {
        if (sel < 0) return;
        const d = [...(joints[sel] || [0, 0, 0])];
        d[k] = Math.min(60, Math.max(-60, Number(v) || 0));
        const next = { ...joints, [sel]: d };
        const o = mirror ? twin(sel) : -1;
        if (o >= 0) next[o] = [-d[0], d[1], d[2]];
        joints = next;
        clearTimeout(jointWait);
        jointWait = setTimeout(rebuild, 120);
    }
    async function resetJoints(all) {
        selectBlip();
        if (all) joints = {};
        else if (sel >= 0) {
            const next = { ...joints };
            delete next[sel];
            if (mirror && twin(sel) >= 0) delete next[twin(sel)];
            joints = next;
        }
        await rebuild();
    }
    async function setReweight(v) {
        selectBlip();
        reweight = v;
        await rebuild();
    }

    let adjWait;
    function setAdjust(key, v) {
        adjust[key] = typeof v === 'boolean' ? v : Math.min(90, Math.max(-90, Number(v) || 0));
        clearTimeout(adjWait);
        adjWait = setTimeout(rebuild, 150);
    }
    const poseLine = (deg, leg) => (deg === null || deg === undefined ? '-'
        : (leg ? '' : poseName(deg) + ' · ') + Math.round(deg) + '°');

    async function setKeep(v) {
        selectBlip();
        keep = v;
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
        /* what this window was set to, kept beside the skin in the mod */
        const settings = { file: file.name, keep, scale, turn, adjust: $state.snapshot(adjust), joints: $state.snapshot(joints), reweight,
            blockOf: $state.snapshot(blockOf), replaces: mdl.materials.map(m => m.rep || '') };
        onApply({ r: res, mdl, settings });
    }

    onMount(async () => {
        app.overlay++;
        setup();
        try {
            mdl = await readModel(file);
            blockOf = mdl.materials.map((_, i) => Math.min(i, f.rig.skin.length - 1));
            /* every material its own texture, as the model has them; a game
             * texture is replaced only when chosen */
            for (const m of mdl.materials) m.rep = '';
            await makeThumbs();
            await rebuild();
        } catch (e) {
            err = String(e && e.message || e);
        } finally {
            busy = false;
        }
    });
    onDestroy(() => {
        app.overlay--;
        cancelAnimationFrame(raf);
        if (root) root.traverse(o => { if (o.geometry) o.geometry.dispose(); if (o.material) o.material.dispose(); });
        renderer && renderer.dispose();
    });
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
                <div class="looks">
                    <button class="eye wide" class:hot={!look.solid && !look.weights} onclick={() => { look.solid = false; look.weights = false; }}>{tx('imLookTex')}</button>
                    <button class="eye wide" class:hot={look.solid} onclick={() => { look.solid = !look.solid; look.weights = false; }}>{tx('imLookSolid')}</button>
                    <button class="eye wide" class:hot={look.weights} onclick={() => { look.weights = !look.weights; look.solid = false; }}>{tx('imLookWeights')}</button>
                    <button class="eye wide" class:hot={look.wire} onclick={() => look.wire = !look.wire}>{tx('vwWire')}</button>
                    <button class="eye wide" class:hot={showRig} onclick={() => showRig = !showRig}>{tx('imLookRig')}</button>
                    <button class="eye wide" class:hot={showFloor} onclick={() => showFloor = !showFloor}>{tx('imLookFloor')}</button>
                </div>
                <div class="over">
                    {#if busy}<span class="msg">{tx('vwLoading')}</span>
                    {:else if err}<span class="msg bad">{err}</span>{/if}
                </div>
                {#if res}
                    <div class="info">{res.verts} verts · {res.tris} tris · {res.matched ? res.matched + '/' + res.of + ' ' + tx(res.rig === 'humanoid' ? 'imHumanoid' : 'imBones') : tx('imNoBones')}{res.auto ? ' · ' + res.auto + ' ' + tx('imAuto') : ''}</div>
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
                <div class="pair tabs2">
                    <button class="btn" class:hot={tab === 'rig'} onclick={() => { selectBlip(); tab = 'rig'; }}>{tx('imTabRig')}</button>
                    <button class="btn" class:hot={tab === 'mats'} onclick={() => { selectBlip(); tab = 'mats'; }}>{tx('imTextures')}</button>
                </div>
                {#if tab === 'mats'}
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
                                <select class="sel" value={choiceOf(i)} onchange={e => setChoice(i, e.currentTarget.value)} title={tx('imReplaces')}>
                                    {#each choices() as c}<option value={c.v}>{tx('imBlock')} {c.b + 1} · {c.t ? tx('imReplaces') + ' ' + c.t : tx('imNewTex')}</option>{/each}
                                </select>
                                <div class="pair">
                                    <button class="btn" onclick={() => pickTex(i)}>{tx('imPickTex')}</button>
                                    {#if m.image}<button class="btn" onclick={() => gameTex(i)}>{tx('imGameTex')}</button>{/if}
                                </div>
                            </div>
                        </li>
                    {/each}
                </ul>
                {:else}
                <div class="rigpane">
                    {#if res && !res.matched}
                        <button class="btn" onclick={turnIt}>{tx('imTurn')}</button>
                    {/if}
                    {#if res && res.rig === 'humanoid'}
                        <!-- a humanoid rig (Mixamo, AccuRig, ValveBiped...): fitted
                             to the fighter's bones, or the proportion trick -->
                        <div class="pair">
                            <button class="btn" class:hot={!keep} onclick={() => setKeep(false)}>{tx('imFit')}</button>
                            <button class="btn" class:hot={keep} onclick={() => setKeep(true)}>{tx('imKeep')}</button>
                        </div>
                        <p class="note">{tx(keep ? 'imKeepNote' : 'imFitNote')}</p>
                    {/if}
                    {#if res && res.pose}
                        <!-- the pose the file is in, beside the fighter's -->
                        <p class="note">{tx('imPoseFile')}: {poseLine(res.pose.file.arm)} · {tx('imPoseFighter')}: {poseLine(res.pose.fighter.arm)}</p>
                        <p class="note">{tx('imLegSpread')}: {poseLine(res.pose.file.leg, true)} · {poseLine(res.pose.fighter.leg, true)}</p>
                        {#if keep}
                            <label class="chk" title={tx('imAutoPoseNote')}><input type="checkbox" checked={adjust.auto} onchange={e => { setAdjust('auto', e.currentTarget.checked); }}><span>{tx('imAutoPose')}</span></label>
                        {/if}
                        {#each [['armAngle', 'imArmAngle'], ['armRoll', 'imArmRoll'], ['legAngle', 'imLegAngle'], ['legRoll', 'imLegRoll']] as [key, label]}
                            <label class="scale">
                                <span class="lbl">{tx(label)} {adjust[key] > 0 ? '+' : ''}{adjust[key]}°</span>
                                <input type="range" min="-90" max="90" step="1" value={adjust[key]} oninput={e => setAdjust(key, e.currentTarget.value)}>
                                <button class="eye" onclick={() => setAdjust(key, 0)} title="0">0</button>
                            </label>
                        {/each}
                    {/if}
                    <label class="scale" title={tx('imScaleNote')}>
                        <span class="lbl">{tx('imScale')} ×{scale.toFixed(2)}</span>
                        <input type="range" min="0.25" max="4" step="0.01" value={scale} oninput={e => setScale(e.currentTarget.value)}>
                        <button class="eye" onclick={() => setScale(1)} title="1">1</button>
                    </label>
                    <!-- BONE POSITIONS: a joint picked on the view or here -->
                    <span class="lbl head">{tx('imJoints')}</span>
                    <select class="sel" value={sel} onchange={e => { sel = Number(e.currentTarget.value); }}>
                        <option value={-1}>{tx('imJointPick')}</option>
                        {#each f.names as n, i}{#if i !== f.rig.bones.root}<option value={i}>{n}{joints[i] && joints[i].some(x => x) ? ' *' : ''}</option>{/if}{/each}
                    </select>
                    {#if sel >= 0}
                        {#each ['X', 'Y', 'Z'] as ax, k}
                            <label class="scale">
                                <span class="lbl">{ax} {((joints[sel] || [0, 0, 0])[k]).toFixed(1)}</span>
                                <input type="range" min="-60" max="60" step="0.5" value={(joints[sel] || [0, 0, 0])[k]} oninput={e => setJoint(k, e.currentTarget.value)}>
                                <button class="eye" onclick={() => setJoint(k, 0)} title="0">0</button>
                            </label>
                        {/each}
                    {/if}
                    <label class="chk"><input type="checkbox" bind:checked={mirror}><span>{tx('imMirror')}</span></label>
                    <label class="chk" title={tx('imReweightNote')}><input type="checkbox" checked={reweight} onchange={e => setReweight(e.currentTarget.checked)}><span>{tx('imReweight')}</span></label>
                    <div class="pair">
                        <button class="btn" disabled={sel < 0} onclick={() => resetJoints(false)}>{tx('imJointReset')}</button>
                        <button class="btn" onclick={() => resetJoints(true)}>{tx('imJointResetAll')}</button>
                    </div>
                    <p class="note">{tx('imJointNote')}</p>
                </div>
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
    .scale { display: flex; align-items: center; gap: 8px; }
    .scale input { flex: 1; min-width: 0; }
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
    .looks { position: absolute; right: calc(var(--u) * .6); top: calc(var(--u) * .6); display: flex; flex-wrap: wrap; justify-content: flex-end;
        gap: calc(var(--u) * .3); max-width: 70%; }
    .eye.wide { width: auto; padding: 0 calc(var(--u) * .6); font-size: calc(var(--t) * 1); letter-spacing: .06em; }
    .eye.hot { border-color: var(--blue); color: #fff; background: rgba(90, 80, 255, .35); }
    .scale .lbl { flex: 0 0 calc(var(--u) * 11); font-size: calc(var(--t) * 1.1); }
    .rigpane { flex: 1; min-height: 0; overflow-y: auto; display: flex; flex-direction: column; gap: calc(var(--u) * .5); }
    .rigpane .sel { width: 100%; }
    .tabs2 .btn.hot, .rigpane .btn.hot { border-color: var(--blue); background: rgba(90, 80, 255, .35); }
</style>
