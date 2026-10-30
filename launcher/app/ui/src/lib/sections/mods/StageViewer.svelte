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
     * with the animation and the effects.
     *
     * Editing, as Blender (mods/stageedit.js): the element picked is moved,
     * turned and scaled in place (G / R / S, or the handles), I keys its
     * placement on the frame shown, Alt+I takes the frame's keys away, REC
     * keys every change; EDIT opens it in the model editor (StageEditor),
     * with its shape keys. The timeline shows the keys; the edits are kept a
     * stage (the launcher's "stage edits" folder) and exported. */
    import { onMount, onDestroy } from 'svelte';
    import * as THREE from 'three';
    import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
    import { tx } from '#lib/app.svelte.js';
    import { api } from '#lib/platform.js';
    import { hoverBlip, selectBlip, doneChord, failChord } from '#lib/sound.js';
    import { ms, src, loadCatalog, reloadMods } from '#lib/mods/store.svelte.js';
    import { loadStage, StageView, stageTextures, stageExport, mul, TICKS_PER_SECOND, PLAYER_SIZE } from '#lib/mods/stagerun.js';
    import { exportFiles } from '#lib/mods/assets.js';
    import { homeOf } from '#lib/mods/owners.js';
    import { ViewNav } from '#lib/mods/viewnav.js';
    import { Gizmo, EditCache, newEl, frameOf, trsAt, shapeAt, putKey, dropKey, copyTrs, saved } from '#lib/mods/stageedit.js';
    import KeyTimeline from './KeyTimeline.svelte';
    import StageEditor from './StageEditor.svelte';

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

    /* the edits: plain data (stageedit.js), erev says when they changed */
    let edits = { v: 1, els: {} }, cache = new EditCache(), gizmo = null, revN = 1;
    let erev = $state(0), rec = $state(false), interp = $state('bezier'), tool = $state(''), gizInfo = $state(''), editing = $state(false);
    let undoList = [], redoList = [], lastTag = '', saveWait = 0;

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
        renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true, logarithmicDepthBuffer: true });
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
        gizmo = new Gizmo({ scene, dom: renderer.domElement, camera: () => camera, controls,
            onStart: () => remember(), onChange: trs => setTrs(trs), onEnd: () => keep(), info: t => { gizInfo = t; } });
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
            if (n) { dropPending(); view.update(); tick = view.sim.ticks; follow(); }
        }
        scene.background = black ? new THREE.Color(0, 0, 0) : null;
        controls.update();
        renderer.render(scene, camera);
        nav.draw();
    }

    /* The fight's camera as a round starts (GameCode.c, SetToUseCamera): the
     * look-at 1.25 over the floor between the fighters, the eye 1.5 over it
     * and 8 back, 25 degrees; here in fighter units, y up, the front +z.
     * The game's projection has no far plane (Matrix.c
     * CreatePerspectiveMatrix writes 2fn/(n-f) where w is taken, so depth
     * never passes 0.5): skies far beyond its 400 -- Jade's Desert's, the
     * Bridge's -- are drawn, and are here. */
    function gameCamera() {
        const u = v => v / PLAYER_SIZE;
        nav.reframed();
        controls.target.set(0, u(1.25), 0);
        camera.near = u(0.33);
        camera.far = u(1e5);
        camera.position.set(0, u(1.5), u(8));
        camera.fov = 25;
        camera.updateProjectionMatrix();
        nav.axisView = '';
        nav.label();
    }

    function close() {
        editing = false;
        if (view) { scene.remove(view.group); view.dispose(); view = null; }
        cache.dispose();
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
            length = Math.max(1, stage.root.frames);
            await loadEdits(st, stage);
            view.edit = (d, mesh) => cache.apply(edits.els[d.key], frameOf(view.sim.ticks, length), mesh, d.key);
            await view.preload();
            if (pick !== st) { view.dispose(); view = null; return; }
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
        dropPending();
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
        const at = downAt;
        downAt = null;
        if (!at || !view || Math.hypot(e.clientX - at[0], e.clientY - at[1]) > 4 || gizmo.active || gizmo.tc.axis) return;
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
        if (gizmo) gizmo.detach();
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
            gizmo.detach();
            return;
        }
        placeGizmo(mesh);
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
        if (editing || !nav) return;
        const k = e.key.toLowerCase();
        if ((e.ctrlKey || e.metaKey) && (k === 'z' || k === 'y') && tag !== 'TEXTAREA' && !(tag === 'INPUT' && e.target.type === 'text')) {
            undo(k === 'z' && !e.shiftKey);
            e.preventDefault();
            return;
        }
        if (tag === 'INPUT' || tag === 'SELECT' || tag === 'TEXTAREA') return;
        if (gizmo.key(e)) { e.preventDefault(); e.stopPropagation(); return; }
        if (e.code === 'KeyI' && editable(selKey)) { if (e.altKey) deleteKeys(); else insertKey(); e.preventDefault(); return; }
        if (e.code === 'NumpadDivide' && selKey) { isolate(); e.preventDefault(); return; }
        if ((e.code === 'NumpadDecimal' || e.key === 'f' || e.key === 'F') && selKey) { frameSel(); e.preventDefault(); return; }
        if (e.key === 'Escape' && selKey) { clearSel(); e.preventDefault(); return; }
        if (e.key === ' ' && view) { playing = !playing; e.preventDefault(); return; }
        if (nav.key(e)) e.preventDefault();
    }

    /* -------------------------------------------------------------- edits -- */

    const editable = key => !!key && (key.startsWith('stage:') || key.startsWith('extra:'));
    const frameNow = () => frameOf(tick, length);
    async function loadEdits(st, stage) {
        const got = await api.stageEditsLoad(st.id);
        edits = got && got.els ? got : { v: 1, els: {} };
        /* an element whose object is not the one it was made on (another
         * mod layer's scene): left out */
        for (const [k, el] of Object.entries(edits.els)) {
            const [kind, i] = k.split(':');
            const name = kind === 'stage' ? (stage.root.objects[i] || {}).name : kind === 'extra' ? (stage.extras[i] || {}).name : null;
            if (name !== el.name) delete edits.els[k];
            else el.rev = revN++;
        }
        undoList = []; redoList = []; lastTag = '';
        erev++;
    }
    function ensureEl(key) {
        if (!edits.els[key]) {
            const mesh = view && view.live.get(key);
            if (!mesh) return null;
            const d = mesh.userData.draw;
            edits.els[key] = newEl(d.extra ? d.extra.name : d.scene.objects[d.obj].name, d.mesh.pos);
            edits.els[key].rev = revN++;
        }
        return edits.els[key];
    }
    /* something changed: drawn again, the panel, saved a moment later */
    function bump(reshaped) {
        const el = selKey && edits.els[selKey];
        if (el && reshaped) el.rev = revN++;
        erev++;
        if (view) { view.update(); follow(); }
    }
    function keep() {
        clearTimeout(saveWait);
        const id = pick && pick.id, json = JSON.stringify(saved(edits));
        saveWait = setTimeout(() => { if (id) api.stageEditsSave(id, json); }, 300);
    }
    /* before a change: Ctrl+Z goes back to here (one step a tag in a row) */
    function remember(tag = '') {
        if (tag && tag === lastTag) return;
        lastTag = tag;
        undoList.push(JSON.stringify(saved(edits)));
        if (undoList.length > 100) undoList.shift();
        redoList = [];
    }
    function undo(back) {
        const from = back ? undoList : redoList, to = back ? redoList : undoList;
        if (!from.length) return;
        to.push(JSON.stringify(saved(edits)));
        edits = JSON.parse(from.pop());
        for (const el of Object.values(edits.els)) el.rev = revN++;
        lastTag = '';
        bump();
        keep();
    }
    /* what was set on a frame and not keyed goes when the frame changes
     * (as the CORRECTIVE ANIMATION LAYERS' recorder) */
    function dropPending() {
        for (const el of Object.values(edits.els)) {
            if (el.pending) { el.pending = null; erev++; }
            for (const s of el.shapes) if (s.pending) { s.pending = null; el.rev = revN++; erev++; }
        }
    }
    function placeGizmo(mesh) {
        if (!editable(selKey) || editing) { gizmo.detach(); return; }
        const d = mesh.userData.draw, el = edits.els[selKey] || newEl('', d.mesh.pos);
        const p = el.pivot;
        const base = new THREE.Matrix4().fromArray(mul(d.world, view.stage.toOut)).multiply(new THREE.Matrix4().makeTranslation(p[0], p[1], p[2]));
        gizmo.attach(base, trsAt(el, frameNow()));
    }
    /* a placement set: keyed (REC), held on this frame (keys there), or the
     * element's own (no keys) */
    function setTrs(trs) {
        const el = ensureEl(selKey);
        if (!el) return;
        const f = frameNow();
        if (rec) { putKey(el.keys, { f, ...copyTrs(trs), i: interp }); el.pending = null; }
        else if (el.keys.length) el.pending = { f, trs: copyTrs(trs) };
        else el.base = copyTrs(trs);
        bump();
    }
    function insertKey() {
        const el = ensureEl(selKey);
        if (!el) return;
        remember();
        selectBlip();
        const f = frameNow();
        putKey(el.keys, { f, ...copyTrs(trsAt(el, f)), i: interp });
        el.pending = null;
        for (const s of el.shapes) if (s.pending) { putKey(s.keys, { f, v: s.pending.v, i: interp }); s.pending = null; }
        bump(true);
        keep();
    }
    function deleteKeys() {
        const el = edits.els[selKey];
        if (!el) return;
        const f = frameNow(), was = copyTrs(trsAt(el, f));
        remember();
        let any = dropKey(el.keys, f);
        if (any && !el.keys.length) el.base = was;
        for (const s of el.shapes) {
            const v = shapeAt(s, f);
            if (dropKey(s.keys, f)) { any = true; if (!s.keys.length) s.value = v; }
        }
        if (any) { selectBlip(); bump(true); keep(); }
    }
    /* LINEAR / BEZIER: the keys inserted from now on, and the frame's own */
    function setInterp(v) {
        interp = v;
        const el = selKey && edits.els[selKey], f = frameNow();
        if (!el) return;
        remember();
        for (const k of [...el.keys, ...el.shapes.flatMap(s => s.keys)]) if (k.f === f) k.i = v;
        bump(true);
        keep();
    }
    function resetEl() {
        if (!edits.els[selKey]) return;
        selectBlip();
        remember();
        delete edits.els[selKey];
        bump(true);
        keep();
    }
    function setTool(t) {
        selectBlip();
        tool = tool === t ? '' : t;
        gizmo.setTool(tool);
    }
    /* the model editor's: the placement, the keys, the shape keys */
    const act = {
        seek: f => { playing = false; seek(f); },
        play: () => { playing = !playing; },
        el: () => ensureEl(selKey),
        remember, keep, setTrs, insertKey, deleteKeys, setInterp,
        rec: () => { rec = !rec; },
        undo,
        reshaped: () => bump(true),
        close: () => closeEditor(),
        addShape() {
            const el = ensureEl(selKey);
            remember();
            let n = el.shapes.length + 1;
            while (el.shapes.some(s => s.name === 'Key ' + n)) n++;
            el.shapes.push({ name: 'Key ' + n, value: 0, mute: false, d: {}, keys: [] });
            bump(true); keep();
            return el.shapes.length - 1;
        },
        delShape(j) { const el = edits.els[selKey]; remember(); el.shapes.splice(j, 1); bump(true); keep(); },
        renameShape(j, name) {
            const el = edits.els[selKey], nm = name.trim() || el.shapes[j].name;
            if (el.shapes.some((s, i) => i !== j && s.name === nm)) { erev++; return; }
            remember(); el.shapes[j].name = nm; bump(); keep();
        },
        muteShape(j) { const el = edits.els[selKey]; remember(); el.shapes[j].mute = !el.shapes[j].mute; bump(true); keep(); },
        setShape(j, v) {
            const el = edits.els[selKey], s = el.shapes[j], f = frameNow();
            remember('v' + j + ':' + f);
            if (rec) { putKey(s.keys, { f, v, i: interp }); s.pending = null; }
            else if (s.keys.length) s.pending = { f, v };
            else s.value = v;
            bump(true); keep();
        },
        keyShape(j) {
            const el = edits.els[selKey], s = el.shapes[j], f = frameNow();
            remember();
            if (s.keys.some(k => k.f === f) && !s.pending) dropKey(s.keys, f);
            else { putKey(s.keys, { f, v: shapeAt(s, f), i: interp }); s.pending = null; }
            bump(true); keep();
        },
    };
    function openEditor() {
        if (!editable(selKey) || !ensureEl(selKey)) return;
        selectBlip();
        playing = false;
        editing = true;
        gizmo.detach();
        erev++;
    }
    function closeEditor() {
        editing = false;
        lastTag = '';
        bump();
    }
    /* the timeline's rows: the element picked's placement and shape keys */
    const rows = $derived.by(() => {
        erev;
        const el = sel && edits.els[sel.key];
        if (!el) return [];
        return [{ label: tx('stPlacement'), keys: el.keys.map(k => ({ f: k.f, i: k.i })) },
            ...el.shapes.map(s => ({ label: s.name, keys: s.keys.map(k => ({ f: k.f, i: k.i })) }))];
    });
    const editKey = $derived(sel && editable(sel.key) ? sel.key : null);
    const hasEdits = $derived.by(() => { erev; return !!(editKey && edits.els[editKey]); });
    const editorDraw = () => { const m = view && view.live.get(selKey); return m ? m.userData.draw : null; };
    const editorTex = d => { const st = ((d.extra ? d.extra.texture : d.mesh.texture) || '').toUpperCase(); return (view && view.ready.get(st)) || null; };

    /* ------------------------------------------------------------- export -- */

    async function exportStage() {
        if (!pick || busy) return;
        selectBlip();
        busy = true;
        done = '';
        try {
            const desc = await stageExport(src, pick, undefined, saved(edits));
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
        clearTimeout(saveWait);
        if (pick) api.stageEditsSave(pick.id, JSON.stringify(saved(edits)));
        close();
        gizmo && gizmo.dispose();
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
        {#if gizInfo}<div class="edit">{gizInfo}</div>{/if}
        <div class="topbtns" bind:this={topEl}>
            {#if pick}<button class="btn" onclick={() => { selectBlip(); gameCamera(); }}>{tx('stGameCam')}</button>{/if}
            <button class="btn" onclick={() => { selectBlip(); solo = !solo; }}>{solo ? tx('vwPanels') : tx('vwSolo')}</button>
            <button class="btn" onclick={toggleFull}>{full ? tx('vwFullExit') : tx('vwFull')}</button>
        </div>
        {#if view && pick}
            <div class="play">
                <button class="btn" onclick={() => { selectBlip(); playing = !playing; }}>{playing ? tx('vwPause') : tx('vwPlay')}</button>
                <button class="btn" onclick={() => { selectBlip(); seek(1); }}>{tx('stRestart')}</button>
                <span class="lbl grow">{tx('vwFrame')} {frameOf(tick, length)} / {length}</span>
                {#if editKey}
                    <button class="btn" class:hot={rec} onclick={() => { selectBlip(); rec = !rec; }} title={tx('stRecNote')}>{rec ? '● REC' : '○ REC'}</button>
                    <select class="sel interp" value={interp} onchange={e => setInterp(e.currentTarget.value)} title={tx('stInterp')}>
                        <option value="bezier">BEZIER</option><option value="linear">{tx('stLinear')}</option>
                    </select>
                {/if}
                <select class="sel speed" bind:value={speed} title={tx('vwSpeed')}>
                    {#each SPEEDS as v}<option value={v}>{v}x</option>{/each}
                </select>
            </div>
            <KeyTimeline {length} frame={frameOf(tick, length)} {rows} onSeek={f => { playing = false; seek(f); }} />
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
                {#if editKey && !sel.gone}
                    <span class="lbl head">{tx('stEditHead')}</span>
                    <button class="btn hot" onclick={openEditor}>{tx('stEdit')}</button>
                    <div class="trio">
                        <button class="btn" class:hot={tool === 'translate'} onclick={() => setTool('translate')}>{tx('stMove')}</button>
                        <button class="btn" class:hot={tool === 'rotate'} onclick={() => setTool('rotate')}>{tx('stTurn')}</button>
                        <button class="btn" class:hot={tool === 'scale'} onclick={() => setTool('scale')}>{tx('stScl')}</button>
                    </div>
                    <div class="pair">
                        <button class="btn" onclick={insertKey}>{tx('stKey')}</button>
                        <button class="btn" onclick={deleteKeys}>{tx('stDelKey')}</button>
                    </div>
                    {#if hasEdits}<button class="btn" onclick={resetEl}>{tx('stResetEdit')}</button>{/if}
                    <p class="note">{tx('stEditNote')}</p>
                {:else if !editKey}
                    <p class="note">{tx('stNoEdit')}</p>
                {/if}
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

{#if editing && editorDraw()}
    <StageEditor d={editorDraw()} tex={editorTex(editorDraw())} {erev} frame={frameOf(tick, length)} {length} {rows} {rec} {interp} {playing} {act} />
{/if}

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
    .play .interp { flex: 0 0 calc(var(--u) * 11); }
    .play .grow { flex: 1; }
    .main :global(.tl) { margin-top: calc(var(--u) * .4); max-height: calc(var(--u) * 14); overflow-y: auto; }
    .edit { position: absolute; left: 50%; top: calc(var(--u) * .6); transform: translateX(-50%); padding: 2px 10px; border-radius: var(--r-sm);
        background: rgba(10, 8, 32, .8); color: #fff; font-size: calc(var(--t) * 1.2); letter-spacing: .06em; pointer-events: none; white-space: nowrap; }
    .trio { display: grid; grid-template-columns: repeat(3, 1fr); gap: calc(var(--u) * .4); }
    .opts .trio .btn { width: auto; font-size: calc(var(--t) * 1.05); }
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
