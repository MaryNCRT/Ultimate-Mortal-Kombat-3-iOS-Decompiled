<script>
    /* STAGE VIEWER: a stage as the engine plays it (mods/stagerun.js) -- its
     * objects on the frame the fight has reached, drawn opaque, blended or
     * additive as LIME_RenderScene draws them, the effects its .events fire
     * (skies, fire, smoke, flares), the Temple's window glow -- seen first
     * from where the fight's camera stands, on black as the game clears it.
     *
     * A click on the stage picks the element under the pointer: its
     * properties on the right, ISOLATE shows it alone (Blender's local view,
     * numpad / too), F or numpad . frames it. On the right, by tabs: what is
     * seen, the elements (each shown or hidden), the one picked, exporting
     * with the animation and the effects. */
    import { onMount, onDestroy } from 'svelte';
    import * as THREE from 'three';
    import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
    import { tx } from '#lib/app.svelte.js';
    import { api } from '#lib/platform.js';
    import { hoverBlip, selectBlip, doneChord, failChord } from '#lib/sound.js';
    import { ms, src, loadCatalog, reloadMods } from '#lib/mods/store.svelte.js';
    import { loadStage, StageView, stageTextures, stageExport, TICKS_PER_SECOND, PLAYER_SIZE } from '#lib/mods/stagerun.js';
    import { exportFiles } from '#lib/mods/assets.js';
    import { homeOf } from '#lib/mods/owners.js';
    import { ViewNav } from '#lib/mods/viewnav.js';

    let host = $state(), mainEl = $state(), topEl;
    let pick = $state(null);                /* the stage open */
    let loading = $state(false), error = $state('');
    let search = $state('');
    let rtab = $state('view');
    let playing = $state(true), speed = $state(1), tick = $state(0);
    let black = $state(true);               /* the game's clear colour */
    let effects = $state(true);
    let wire = $state(false);
    let elements = $state([]);              /* [{ i, name, on }] the scene's objects */
    let sel = $state(null);                 /* the draw picked (a snapshot for the panel) */
    let selKey = null;
    let isolated = $state(false);
    let viewName = $state('');
    let full = $state(false), solo = $state(false);
    let format = $state('glb'), done = $state(''), busy = $state(false);
    let length = $state(1);

    let renderer, scene, camera, controls, nav, raf, view = null, overlay = null;
    const SPEEDS = [0.1, 0.25, 0.5, 0.75, 1, 1.5, 2, 3];
    const FORMATS = ['glb', 'fbx', 'usdc', 'usdz', 'usda'];
    const MODES = { opaque: 'stOpaque', alpha: 'stAlpha', atst: 'stClip', add: 'stAdd', glow: 'stGlow' };

    const items = $derived.by(() => {
        if (!ms.cat) return [];
        const q = search.trim().toLowerCase();
        return ms.cat.stages.filter(x => !q || x.name.toLowerCase().includes(q) || x.id.toLowerCase().includes(q));
    });

    function setup() {
        renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
        renderer.setPixelRatio(window.devicePixelRatio);
        /* the game's numbers as they are: blended as GL blends them */
        renderer.outputColorSpace = THREE.LinearSRGBColorSpace;
        host.appendChild(renderer.domElement);
        scene = new THREE.Scene();
        camera = new THREE.PerspectiveCamera(25, 1, 1, 100000);
        controls = new OrbitControls(camera, renderer.domElement);
        controls.enableDamping = true;
        nav = new ViewNav({ host, renderer, controls, persp: camera, onCamera: c => { camera = c; }, onLabel: t => { viewName = t; },
            names: { front: tx('nvFront'), back: tx('nvBack'), right: tx('nvRight'), left: tx('nvLeft'), top: tx('nvTop'), bottom: tx('nvBottom'),
                user: tx('nvUser'), persp: tx('nvPersp'), ortho: tx('nvOrtho') }, below: () => topEl });
        nav.gizmo.title = tx('nvKeys');
        nav.label();
        renderer.domElement.addEventListener('pointerdown', onDown);
        renderer.domElement.addEventListener('pointerup', onUp);
        window.addEventListener('keydown', onKey);
        new ResizeObserver(fit).observe(host);
        loop();
    }
    function fit() {
        if (!renderer || !host) return;
        renderer.setSize(host.clientWidth, host.clientHeight, false);
        nav.resize();
    }

    let last = 0, acc = 0;
    function loop(t = 0) {
        raf = requestAnimationFrame(loop);
        const dt = Math.min(0.1, (t - last) / 1000);
        last = t;
        if (playing && view) {
            acc += dt;
            const step = 1 / (TICKS_PER_SECOND * speed);
            let n = 0;
            while (acc >= step && n < 8) { acc -= step; view.sim.tick(); n++; }
            if (n) { view.update(); tick = view.sim.ticks; follow(); }
        }
        scene.background = black ? new THREE.Color(0, 0, 0) : null;
        controls.update();
        renderer.render(scene, camera);
        nav.draw();
    }

    /* The fight's camera as a round starts (GameCode.c, SetToUseCamera): the
     * look-at 1.25 over the floor between the fighters, the eye 1.5 over it
     * and 8 back, 25 degrees; here in fighter units, y up, the front +z. */
    function gameCamera() {
        const u = v => v / PLAYER_SIZE;
        nav.reframed();
        controls.target.set(0, u(1.25), 0);
        camera.near = u(0.5);
        camera.far = u(400);
        camera.position.set(0, u(1.5), u(8));
        camera.fov = 25;
        camera.updateProjectionMatrix();
        nav.axisView = '';
        nav.label();
    }

    function close() {
        if (view) { scene.remove(view.group); view.dispose(); view = null; }
        clearSel();
    }
    async function open(st) {
        selectBlip();
        pick = st;
        loading = true;
        error = '';
        done = '';
        close();
        try {
            const stage = await loadStage(src, st);
            if (pick !== st) return;
            view = new StageView(stage, stageTextures(src));
            view.group.visible = true;
            scene.add(view.group);
            length = Math.max(1, stage.root.frames);
            elements = stage.root.objects.map((o, i) => ({ i, name: o.name, on: true, drawn: o.mesh >= 0 && !o.marker }))
                .filter(e => e.drawn);
            view.seek(1);
            tick = view.sim.ticks;
            applyEffects();
            gameCamera();
        } catch (e) {
            error = String(e && e.message || e);
        } finally {
            loading = false;
        }
    }

    function seek(t) {
        if (!view) return;
        view.seek(Math.max(1, t));
        tick = view.sim.ticks;
        follow();
    }
    function applyEffects() {
        if (!view) return;
        view.effects = effects;
        refreshShown();
    }
    function refreshShown() {
        if (!view) return;
        view.hidden = new Set(elements.filter(e => !e.on).map(e => e.i));
        view.wire = wire;
        view.update();
        follow();
    }
    $effect(() => { effects; applyEffects(); });
    $effect(() => { wire; refreshShown(); });

    /* ------------------------------------------------------------ picking -- */

    const ray = new THREE.Raycaster();
    let downAt = null;
    function ndc(e) {
        const r = renderer.domElement.getBoundingClientRect();
        return new THREE.Vector2(((e.clientX - r.left) / r.width) * 2 - 1, -((e.clientY - r.top) / r.height) * 2 + 1);
    }
    function onDown(e) { downAt = e.button === 0 ? [e.clientX, e.clientY] : null; }
    function onUp(e) {
        if (!downAt || !view || Math.hypot(e.clientX - downAt[0], e.clientY - downAt[1]) > 4) return;
        downAt = null;
        ray.setFromCamera(ndc(e), camera);
        const hits = ray.intersectObjects([...view.live.values()].filter(m => m.visible), false);
        if (!hits.length) { clearSel(); return; }
        selectBlip();
        choose(hits[0].object.userData.draw.key);
    }
    function choose(key) {
        selKey = key;
        follow();
    }
    function clearSel() {
        selKey = null;
        sel = null;
        if (overlay) { scene.remove(overlay); overlay.material.dispose(); overlay = null; }
        if (isolated && view) { view.solo = null; isolated = false; refreshShown(); }
    }
    /* the panel and the outline on what is picked, frame by frame */
    function follow() {
        if (!view || !selKey) return;
        const mesh = view.live.get(selKey);
        if (!mesh) {
            if (overlay) overlay.visible = false;
            if (sel) sel = { ...sel, gone: true };
            return;
        }
        const d = mesh.userData.draw;
        if (!overlay) {
            overlay = new THREE.Mesh(mesh.geometry, new THREE.MeshBasicMaterial({ color: 0xffa040, wireframe: true, depthTest: false, transparent: true, opacity: 0.55 }));
            overlay.matrixAutoUpdate = false;
            overlay.renderOrder = 1e6;
            scene.add(overlay);
        }
        overlay.geometry = mesh.geometry;
        overlay.matrix.copy(mesh.matrix);
        overlay.matrixWorldNeedsUpdate = true;
        overlay.visible = true;
        const p = new THREE.Vector3(), q = new THREE.Quaternion(), s = new THREE.Vector3();
        mesh.matrix.decompose(p, q, s);
        const r = new THREE.Euler().setFromQuaternion(q);
        const o = d.extra ? null : d.scene.objects[d.obj];
        const shownAt = o ? o.mats.map((m, f) => (m >= 0 ? f : -1)).filter(f => f >= 0) : [];
        sel = {
            key: d.key,
            name: d.extra ? d.extra.name : o.name,
            owner: d.owner === 'stage' ? '' : d.owner === 'extra' ? 'RenderLevelBG' : d.scene.file,
            mesh: d.mesh.name || d.extra.name,
            texture: (d.extra ? d.extra.texture : d.mesh.texture) || '—',
            verts: d.mesh.pos.length / 3,
            tris: d.mesh.faces.length / 3,
            mode: d.mode, alpha: d.alpha, tint: d.tint, lit: d.lit,
            pos: [p.x, p.y, p.z], rot: [r.x, r.y, r.z].map(v => v * 180 / Math.PI), scl: [s.x, s.y, s.z],
            frames: o ? o.keys.length : 0,
            shown: shownAt.length ? (shownAt[0] + 1) + '–' + (shownAt[shownAt.length - 1] + 1) + ' (' + shownAt.length + ')' : '',
            animated: o ? new Set(o.mats.filter(m => m >= 0)).size > 1 : false,
            bone: o && o.keys[0] ? o.keys[0].bone : 0,
        };
    }
    function isolate() {
        if (!view || !selKey) return;
        selectBlip();
        isolated = !isolated;
        view.solo = isolated ? new Set([selKey]) : null;
        refreshShown();
        if (isolated) frameSel();
    }
    function frameSel() {
        const mesh = view && selKey && view.live.get(selKey);
        if (!mesh) return;
        const box = new THREE.Box3().setFromObject(mesh);
        const c = box.getCenter(new THREE.Vector3()), size = box.getSize(new THREE.Vector3()).length() || 1;
        nav.reframed();
        controls.target.copy(c);
        const dir = camera.position.clone().sub(controls.target).normalize();
        camera.position.copy(c).addScaledVector(dir, size * 1.6);
        camera.near = size / 500;
        camera.updateProjectionMatrix();
        nav.axisView = '';
        nav.label();
    }
    function onKey(e) {
        const tag = e.target && e.target.tagName;
        if (tag === 'INPUT' || tag === 'SELECT' || tag === 'TEXTAREA' || !nav) return;
        if (e.code === 'NumpadDivide' && selKey) { isolate(); e.preventDefault(); return; }
        if ((e.code === 'NumpadDecimal' || e.key === 'f' || e.key === 'F') && selKey) { frameSel(); e.preventDefault(); return; }
        if (e.key === 'Escape' && selKey) { clearSel(); e.preventDefault(); return; }
        if (e.key === ' ' && view) { playing = !playing; e.preventDefault(); return; }
        if (nav.key(e)) e.preventDefault();
    }

    /* ------------------------------------------------------------- export -- */

    async function exportStage() {
        if (!pick || busy) return;
        selectBlip();
        busy = true;
        done = '';
        try {
            const desc = await stageExport(src, pick);
            const files = await exportFiles(src, desc, format);
            const at = await api.exportSave(homeOf('stages', pick) + '/' + format.toUpperCase(), files);
            doneChord();
            done = tx('exDone') + ' ' + at;
        } catch (e) {
            failChord();
            done = String(e && e.message || e);
        } finally {
            busy = false;
        }
    }

    const onFull = () => { full = document.fullscreenElement === mainEl; };
    function toggleFull() {
        if (document.fullscreenElement) document.exitFullscreen();
        else mainEl && mainEl.requestFullscreen().catch(() => {});
    }
    async function setLayer(v) {
        selectBlip();
        ms.layer = v;
        if (pick) await open(pick);
    }

    onMount(async () => {
        document.addEventListener('fullscreenchange', onFull);
        setup();
        await loadCatalog();
    });
    onDestroy(() => {
        document.removeEventListener('fullscreenchange', onFull);
        if (document.fullscreenElement) document.exitFullscreen();
        cancelAnimationFrame(raf);
        close();
        window.removeEventListener('keydown', onKey);
        nav && nav.dispose();
        renderer && renderer.dispose();
    });

    const f2 = v => (Math.abs(v) < 0.005 ? '0' : v.toFixed(2));
</script>

<div class="viewer" class:full class:solo bind:this={mainEl}>
    <aside class="side">
        <label class="layer"><span class="lbl">{tx('vwLayer')}</span>
            <select class="sel" value={ms.layer} onchange={e => setLayer(e.currentTarget.value)}>
                <option value="">{tx('vwLayerGame')}</option>
                <option value="*">{tx('vwLayerOn')}</option>
                {#each ms.list as m}<option value={m.folder}>{m.name}</option>{/each}
            </select></label>
        <button class="btn" onclick={async () => { selectBlip(); await reloadMods(); if (pick) open(pick); }}>{tx('vwRefresh')}</button>
        <input class="edt search" placeholder={tx('vwSearch')} spellcheck="false" bind:value={search}>
        <ul class="list">
            {#each items as it (it.id)}
                <li><button class:on={pick && pick.id === it.id} onpointerenter={hoverBlip} onclick={() => open(it)}>{it.name}</button></li>
            {/each}
        </ul>
    </aside>

    <section class="main">
        <div class="view" bind:this={host}><div class="vname">{viewName}</div></div>
        <div class="over">
            {#if ms.noGame}<span class="msg">{tx('vwNoGame')}</span>
            {:else if loading}<span class="msg">{tx('vwLoading')}</span>
            {:else if error}<span class="msg bad">{error}</span>
            {:else if !pick}<span class="msg">{tx('vwPick')}</span>{/if}
        </div>
        {#if sel && !sel.gone}<div class="info">{sel.name}</div>{/if}
        <div class="topbtns" bind:this={topEl}>
            {#if pick}<button class="btn" onclick={() => { selectBlip(); gameCamera(); }}>{tx('stGameCam')}</button>{/if}
            <button class="btn" onclick={() => { selectBlip(); solo = !solo; }}>{solo ? tx('vwPanels') : tx('vwSolo')}</button>
            <button class="btn" onclick={toggleFull}>{full ? tx('vwFullExit') : tx('vwFull')}</button>
        </div>
        {#if view && pick}
            <div class="play">
                <button class="btn" onclick={() => { selectBlip(); playing = !playing; }}>{playing ? tx('vwPause') : tx('vwPlay')}</button>
                <button class="btn" onclick={() => { selectBlip(); seek(1); }}>{tx('stRestart')}</button>
                <input type="range" min="1" max={length} value={Math.min(tick, length)} oninput={e => { playing = false; seek(Number(e.currentTarget.value)); }}>
                <span class="lbl">{tx('vwFrame')} {tick}</span>
                <select class="sel speed" bind:value={speed} title={tx('vwSpeed')}>
                    {#each SPEEDS as v}<option value={v}>{v}x</option>{/each}
                </select>
            </div>
        {/if}
    </section>

    <aside class="opts">
        {#if pick && !loading}
            <div class="rtabs">
                {#each [['view', 'vwTabView'], ['elements', 'stTabElements'], ['props', 'stTabProps'], ['export', 'vwTabExport']] as [k, key]}
                    <button class="tab" class:active={rtab === k} onpointerenter={hoverBlip} onclick={() => { selectBlip(); rtab = k; }}>{tx(key)}</button>
                {/each}
            </div>
        {/if}

        {#if rtab === 'view' || !pick}
            <div class="checks">
                <label class="chk"><input type="checkbox" bind:checked={black}><span>{tx('stBlack')}</span></label>
                <label class="chk"><input type="checkbox" bind:checked={effects}><span>{tx('stEffects')}</span></label>
                <label class="chk"><input type="checkbox" bind:checked={wire}><span>{tx('vwWire')}</span></label>
            </div>
            <p class="note">{tx('stNote')}</p>
            <p class="note">{tx('stKeys')}</p>

        {:else if rtab === 'elements'}
            <span class="lbl head">{tx('stTabElements')} ({elements.filter(l => l.on).length}/{elements.length})</span>
            <div class="pair">
                <button class="btn" onclick={() => { selectBlip(); elements.forEach(l => l.on = true); refreshShown(); }}>{tx('vwAllOn')}</button>
                <button class="btn" onclick={() => { selectBlip(); elements.forEach(l => l.on = false); refreshShown(); }}>{tx('vwAllOff')}</button>
            </div>
            <ul class="layers">
                {#each elements as l (l.i)}
                    <li class:on={sel && sel.key === 'stage:' + l.i}>
                        <input type="checkbox" bind:checked={l.on} onchange={refreshShown}>
                        <button onclick={() => { selectBlip(); choose('stage:' + l.i); rtab = 'props'; }} title={l.name}>{l.name}</button>
                    </li>
                {/each}
            </ul>

        {:else if rtab === 'props'}
            {#if sel}
                <span class="lbl head">{sel.name}</span>
                {#if sel.gone}<p class="note">{tx('stGone')}</p>{/if}
                <dl class="props">
                    {#if sel.owner}<dt>{tx('stOwner')}</dt><dd>{sel.owner}</dd>{/if}
                    <dt>{tx('stMesh')}</dt><dd>{sel.mesh}</dd>
                    <dt>{tx('stTexture')}</dt><dd>{sel.texture}</dd>
                    <dt>{tx('stGeo')}</dt><dd>{sel.verts} / {sel.tris}</dd>
                    <dt>{tx('stDrawn')}</dt><dd>{tx(MODES[sel.mode])}{sel.mode !== 'opaque' && sel.mode !== 'atst' ? ' · ' + Math.round(sel.alpha * 100) + '%' : ''}</dd>
                    <dt>{tx('stTint')}</dt><dd>{sel.tint.map(f2).join(' ')}</dd>
                    <dt>{tx('stLit')}</dt><dd>{sel.lit ? tx('stYes') : tx('stNo')}</dd>
                    <dt>{tx('stPos')}</dt><dd>{sel.pos.map(f2).join(' ')}</dd>
                    <dt>{tx('stRot')}</dt><dd>{sel.rot.map(v => v.toFixed(1)).join('° ')}°</dd>
                    <dt>{tx('stScale')}</dt><dd>{sel.scl.map(f2).join(' ')}</dd>
                    {#if sel.frames}
                        <dt>{tx('stShown')}</dt><dd>{sel.shown} / {sel.frames}</dd>
                        <dt>{tx('stAnimated')}</dt><dd>{sel.animated ? tx('stYes') : tx('stNo')}</dd>
                        {#if sel.bone}<dt>{tx('stBone')}</dt><dd>{sel.bone}</dd>{/if}
                    {/if}
                </dl>
                <div class="pair">
                    <button class="btn" class:hot={isolated} onclick={isolate}>{isolated ? tx('stShowAll') : tx('stIsolate')}</button>
                    <button class="btn" onclick={() => { selectBlip(); frameSel(); }}>{tx('stFrame')}</button>
                </div>
                <button class="btn" onclick={() => { selectBlip(); clearSel(); }}>{tx('stDeselect')}</button>
            {:else}
                <p class="note">{tx('stPickNote')}</p>
            {/if}

        {:else if rtab === 'export'}
            <span class="lbl">{tx('exFormat')}</span>
            <span class="tabs sub q5">
                {#each FORMATS as f}
                    <button class="tab" class:active={format === f} onclick={() => { selectBlip(); format = f; }}>{f.toUpperCase()}</button>
                {/each}
            </span>
            <p class="note">{tx('stExportNote')}</p>
            <button class="btn hot" disabled={busy} onclick={exportStage}>{busy ? tx('vwLoading') : tx('stExport')}</button>
            {#if done}<p class="ok">{done}</p>{/if}
        {/if}
    </aside>
</div>

<style>
    .viewer { display: grid; grid-template-columns: calc(var(--u) * 22) minmax(0, 1fr) calc(var(--u) * 24); gap: calc(var(--u) * 1.2); flex: 1; min-height: calc(var(--u) * 40); }
    .viewer.full { background: var(--bg); padding: calc(var(--u) * 1.2); }
    .side, .opts { display: flex; flex-direction: column; gap: calc(var(--u) * .6); min-height: 0; }
    .side .sel, .side .btn { flex: none; }
    .opts { overflow-x: hidden; overflow-y: auto; padding-right: calc(var(--u) * .3); }
    .opts .btn { white-space: normal; height: auto; min-height: var(--ctl-h); padding-top: calc(var(--u) * .4); padding-bottom: calc(var(--u) * .4); }
    .opts .btn, .opts .sel, .opts .edt { width: 100%; flex: none; }
    .opts .head { margin-top: calc(var(--u) * .8); border-top: 1px solid var(--line); padding-top: calc(var(--u) * .6); word-break: break-all; }
    .opts .sub { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: calc(var(--u) * .4); }
    .opts .sub .tab { font-size: calc(var(--t) * 1.15); padding: calc(var(--u) * .5) calc(var(--u) * .3); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
    .checks { display: flex; flex-wrap: wrap; gap: calc(var(--u) * .9); }
    .pair { display: flex; gap: calc(var(--u) * .5); }
    .opts .pair .btn { flex: 1; width: auto; }
    .search { flex: none; }
    .layer { display: flex; flex-direction: column; gap: calc(var(--u) * .3); }
    .list { list-style: none; margin: 0; padding: 0; overflow-y: auto; flex: 1; min-height: 0; background: var(--well); border-radius: var(--r-sm); }
    .list button {
        width: 100%; text-align: left; background: none; border: 0; cursor: pointer;
        padding: calc(var(--u) * .45) calc(var(--u) * .9); font-size: calc(var(--t) * 1.4); color: var(--lav);
        white-space: nowrap; overflow: hidden; text-overflow: ellipsis;
    }
    .list button:hover { color: #fff; background: rgba(150, 140, 255, .12); }
    .list button.on { color: #fff; background: rgba(140, 125, 255, .3); }
    .main { position: relative; display: flex; flex-direction: column; min-height: 0; }
    .view {
        flex: 1; min-height: 0; border-radius: var(--r-sm); overflow: hidden;
        background: radial-gradient(ellipse at 50% 35%, #2a2560, #0a0820 75%);
        border: 1px solid var(--line);
    }
    .view :global(canvas) { width: 100%; height: 100%; display: block; }
    .view :global(canvas.navgizmo) { width: 96px; height: 96px; }
    .vname { position: absolute; left: calc(var(--u) * 1); bottom: calc(var(--u) * 1); font-size: calc(var(--t) * 1.1); color: var(--dim); pointer-events: none; letter-spacing: .06em; z-index: 2; }
    .over { position: absolute; inset: 0 0 auto 0; height: 60%; display: grid; place-items: center; pointer-events: none; }
    .msg { color: var(--dim); letter-spacing: .08em; }
    .msg.bad { color: #ff9aa0; }
    .info { position: absolute; left: calc(var(--u) * 1); top: calc(var(--u) * .6); font-size: calc(var(--t) * 1.3); color: #ffc890; pointer-events: none; }
    .topbtns { position: absolute; right: calc(var(--u) * .8); top: calc(var(--u) * .6); display: flex; gap: calc(var(--u) * .5); }
    .topbtns .btn { height: auto; padding: calc(var(--u) * .35) calc(var(--u) * .9); font-size: calc(var(--t) * 1.2); opacity: .85; }
    .viewer.solo { grid-template-columns: minmax(0, 1fr); }
    .viewer.solo .side, .viewer.solo .opts { display: none; }
    .play { display: flex; align-items: center; gap: calc(var(--u) * .9); margin-top: calc(var(--u) * .6); }
    .play input[type=range] { flex: 1; accent-color: var(--blue); }
    .play .speed { flex: 0 0 calc(var(--u) * 8); }
    .ok { color: var(--ok-text); word-break: break-all; }
    .rtabs { display: grid; grid-template-columns: repeat(2, 1fr); gap: calc(var(--u) * .3); margin-bottom: calc(var(--u) * .4); }
    .rtabs .tab { font-size: calc(var(--t) * 1.05); padding: calc(var(--u) * .45) calc(var(--u) * .2); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
    .layers { list-style: none; margin: 0; padding: calc(var(--u) * .3); background: var(--well); border-radius: var(--r-sm); flex: none; max-height: calc(var(--u) * 46); overflow-y: auto; }
    .layers li { display: flex; align-items: center; gap: calc(var(--u) * .5); padding: calc(var(--u) * .15) calc(var(--u) * .2); border-radius: 3px; }
    .layers li.on { background: rgba(255, 160, 64, .18); }
    .layers li button { flex: 1; min-width: 0; text-align: left; background: none; border: 0; cursor: pointer; color: var(--lav); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; font-size: calc(var(--t) * 1.15); padding: 0; }
    .layers li button:hover { color: #fff; }
    .props { display: grid; grid-template-columns: auto 1fr; gap: calc(var(--u) * .3) calc(var(--u) * .8); margin: 0; font-size: calc(var(--t) * 1.15); }
    .props dt { color: var(--dim); }
    .props dd { margin: 0; color: var(--lav); word-break: break-all; }
</style>
