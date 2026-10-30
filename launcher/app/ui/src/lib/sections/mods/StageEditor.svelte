<script>
    /* The model editor for one element of a stage (STAGE VIEWER's EDIT), in
     * the manner of IMPORT A MODEL: the element alone, its mesh with its
     * texture, vertices and wireframe, its UVs on the texture, its placement.
     *
     * OBJECT: G / R / S or the handles move, turn and scale it; the numbers
     * on the side set it exactly. SCULPT (Tab): a drag on the mesh grabs the
     * vertices around the pointer with a smooth falloff, Blender's
     * proportional editing -- the radius [ and ], or the wheel while
     * dragging -- into the shape key picked (Basis, or one of the keys,
     * seen alone at its full value as Blender's edit mode shows it).
     *
     * Keys as in the viewer: I keys the placement, Alt+I takes the frame's
     * keys away, REC keys every change, a shape key's ◆ keys its value. The
     * edits are the viewer's (act): one undo, one save.
     *   d: the element's draw (stagerun.js), tex its texture, frame/length
     *   the stage's, rows the timeline's, erev changes with the edits */
    import { onMount, onDestroy } from 'svelte';
    import * as THREE from 'three';
    import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
    import { tx } from '#lib/app.svelte.js';
    import { selectBlip } from '#lib/sound.js';
    import { ViewNav } from '#lib/mods/viewnav.js';
    import { Gizmo, trsAt, shapeAt, localMatrix, shapedPositions, copyTrs } from '#lib/mods/stageedit.js';
    import KeyTimeline from './KeyTimeline.svelte';
    import { portal } from '#lib/portal.js';

    let { d, tex, erev, frame, length, rows, rec, interp, playing, act } = $props();

    let host = $state(), topEl = $state(), uvEl = $state();
    let viewName = $state(''), info = $state('');
    let tool = $state(''), space = $state('local');
    let mode = $state('object');            /* 'object' | 'sculpt' */
    let active = $state(-1);                /* the shape key sculpted: -1 the Basis */
    let radius = $state(0.2);               /* of the mesh's size */
    let brushAt = $state(null);

    let renderer, scene, camera, controls, nav, gizmo, raf, obj, mesh, wire, points, geo, brushW = null;
    const orig = Float32Array.from(d.mesh.pos);
    const nv = orig.length / 3;
    const name = d.extra ? d.extra.name : d.scene.objects[d.obj].name;
    const stem = (d.extra ? d.extra.texture : d.mesh.texture) || '';
    let size = 1;

    function setup() {
        const el = act.el();
        renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
        renderer.setPixelRatio(window.devicePixelRatio);
        renderer.outputColorSpace = THREE.LinearSRGBColorSpace;
        host.appendChild(renderer.domElement);
        scene = new THREE.Scene();
        camera = new THREE.PerspectiveCamera(35, 1, 0.01, 1e7);
        controls = new OrbitControls(camera, renderer.domElement);
        controls.enableDamping = true;
        nav = new ViewNav({ host, renderer, controls, persp: camera, onCamera: c => { camera = c; }, onLabel: t => { viewName = t; },
            names: { front: tx('nvFront'), back: tx('nvBack'), right: tx('nvRight'), left: tx('nvLeft'), top: tx('nvTop'), bottom: tx('nvBottom'),
                user: tx('nvUser'), persp: tx('nvPersp'), ortho: tx('nvOrtho') }, below: () => topEl });
        nav.gizmo.title = tx('nvKeys');
        nav.label();

        /* the mesh: its own geometry, the vertices as the edits shape them */
        geo = new THREE.BufferGeometry();
        geo.setAttribute('position', new THREE.BufferAttribute(Float32Array.from(orig), 3));
        geo.setAttribute('uv', new THREE.BufferAttribute(Float32Array.from(d.mesh.uv), 2));
        geo.setAttribute('color', new THREE.BufferAttribute(new Float32Array(nv * 3), 3));
        geo.setIndex(new THREE.BufferAttribute(Uint32Array.from(d.mesh.faces), 1));
        const see = d.mode !== 'opaque' && d.mode !== 'atst';
        mesh = new THREE.Mesh(geo, new THREE.MeshBasicMaterial({ map: tex, side: THREE.DoubleSide, transparent: see, alphaTest: d.mode === 'atst' ? 0.9 : 0, depthWrite: !see }));
        wire = new THREE.Mesh(geo, new THREE.MeshBasicMaterial({ color: 0x8070ff, wireframe: true, transparent: true, opacity: 0.35, depthTest: false }));
        points = new THREE.Points(geo, new THREE.PointsMaterial({ size: 4, sizeAttenuation: false, vertexColors: true, depthTest: false }));
        wire.renderOrder = 1; points.renderOrder = 2;
        obj = new THREE.Group();
        obj.matrixAutoUpdate = false;
        obj.add(mesh, wire, points);
        scene.add(obj);
        geo.computeBoundingBox();
        const bb = geo.boundingBox;
        size = bb.getSize(new THREE.Vector3()).length() || 1;
        scene.add(new THREE.GridHelper(size * 2, 10, 0x504890, 0x2a2660));

        gizmo = new Gizmo({ scene, dom: renderer.domElement, camera: () => camera, controls,
            onStart: () => act.remember(), onChange: trs => act.setTrs(trs), onEnd: () => act.keep(), info: t => { info = t; } });
        renderer.domElement.addEventListener('pointerdown', onDown, { capture: true });
        renderer.domElement.addEventListener('wheel', onWheel, { capture: true, passive: false });
        window.addEventListener('pointermove', onMove);
        window.addEventListener('pointerup', onUp);
        window.addEventListener('keydown', onKey, true);
        new ResizeObserver(fit).observe(host);
        const c = new THREE.Vector3(...el.pivot);
        controls.target.copy(c);
        camera.position.copy(c).add(new THREE.Vector3(0.4, 0.5, 1).normalize().multiplyScalar(size * 1.6));
        camera.near = size / 1000;
        camera.far = size * 1000;
        camera.updateProjectionMatrix();
        refresh();
        loop();
    }
    function fit() {
        if (!renderer || !host) return;
        renderer.setSize(host.clientWidth, host.clientHeight, false);
        nav.resize();
    }
    function loop() {
        raf = requestAnimationFrame(loop);
        controls.update();
        renderer.render(scene, camera);
        nav.draw();
    }

    /* the element on the frame: shaped, placed, the gizmo on it */
    function refresh() {
        const el = act.el();
        if (!geo || !el) return;
        const pos = shapedPositions(el, orig, frame, mode === 'sculpt' ? active : undefined);
        geo.attributes.position.array.set(pos);
        geo.attributes.position.needsUpdate = true;
        geo.computeBoundingSphere();
        obj.matrix.copy(localMatrix(el, frame));
        obj.matrixWorldNeedsUpdate = true;
        obj.updateMatrixWorld(true);
        paintPoints(drag ? drag.w : brushW);
        if (mode === 'object') {
            const p = el.pivot;
            gizmo.attach(new THREE.Matrix4().makeTranslation(p[0], p[1], p[2]), trsAt(el, frame));
        } else gizmo.detach();
    }
    $effect(() => { erev; frame; mode; active; refresh(); });

    /* the vertices: lavender, orange as much as a grab takes them */
    function paintPoints(w) {
        const c = geo.attributes.color.array;
        for (let v = 0; v < nv; v++) { c[v * 3] = 0.75; c[v * 3 + 1] = 0.72; c[v * 3 + 2] = 1; }
        if (w) for (const [v, x] of w) { c[v * 3] = 0.75 + 0.25 * x; c[v * 3 + 1] = 0.72 - 0.35 * x; c[v * 3 + 2] = 1 - 0.85 * x; }
        geo.attributes.color.needsUpdate = true;
    }

    /* ---------------------------------------------------------- sculpt -- */

    const ray = new THREE.Raycaster();
    let drag = null;
    function ndc(e) {
        const r = renderer.domElement.getBoundingClientRect();
        return new THREE.Vector2(((e.clientX - r.left) / r.width) * 2 - 1, -((e.clientY - r.top) / r.height) * 2 + 1);
    }
    function onPlane(e, P) {
        ray.setFromCamera(ndc(e), camera);
        const n = camera.getWorldDirection(new THREE.Vector3());
        return ray.ray.intersectPlane(new THREE.Plane().setFromNormalAndCoplanarPoint(n, P), new THREE.Vector3()) || P.clone();
    }
    /* the vertices within the radius of a point (the mesh's own space) and
     * how much each is taken: Blender's smooth falloff */
    function around(local) {
        const R = radius * size, pos = geo.attributes.position.array, w = new Map();
        for (let v = 0; v < nv; v++) {
            const dd = Math.hypot(pos[v * 3] - local.x, pos[v * 3 + 1] - local.y, pos[v * 3 + 2] - local.z);
            if (dd < R) { const t = dd / R; w.set(v, 1 - t * t * (3 - 2 * t)); }
        }
        return w;
    }
    /* the radius on the screen, in pixels, at a point */
    function ringOf(P, h) {
        const R = radius * size * new THREE.Vector3().setFromMatrixScale(obj.matrixWorld).x;
        if (camera.isOrthographicCamera) return R * camera.zoom / (camera.top - camera.bottom) * h;
        return R / (camera.position.distanceTo(P) * Math.tan(camera.fov * Math.PI / 360)) * h / 2;
    }
    function hitOf(e) {
        ray.setFromCamera(ndc(e), camera);
        return ray.intersectObject(mesh, false)[0] || null;
    }
    const target = () => { const el = act.el(); return active >= 0 && el.shapes[active] ? el.shapes[active].d : el.basis; };
    function onDown(e) {
        if (mode !== 'sculpt' || e.button !== 0 || gizmo.active) return;
        const hit = hitOf(e);
        if (!hit) return;
        e.stopImmediatePropagation();
        act.remember();
        const local = obj.worldToLocal(hit.point.clone()), w = around(local), t = target(), d0 = new Map();
        for (const v of w.keys()) d0.set(v, t[v] ? [...t[v]] : [0, 0, 0]);
        drag = { P: hit.point.clone(), L: local, w, d0 };
        controls.enabled = false;
    }
    function onMove(e) {
        if (!renderer) return;
        if (drag) {
            const now = obj.worldToLocal(onPlane(e, drag.P)), dl = now.sub(drag.L), t = target();
            for (const [v, x] of drag.w) { const a = drag.d0.get(v); t[v] = [a[0] + dl.x * x, a[1] + dl.y * x, a[2] + dl.z * x]; }
            act.reshaped();
            return;
        }
        if (mode === 'sculpt' && e.target === renderer.domElement) {
            const hit = hitOf(e), r = host.getBoundingClientRect();
            brushW = hit ? around(obj.worldToLocal(hit.point.clone())) : null;
            brushAt = hit ? { x: e.clientX - r.left, y: e.clientY - r.top, r: ringOf(hit.point, r.height) } : null;
            paintPoints(brushW);
        } else if (brushAt) { brushAt = null; brushW = null; paintPoints(null); }
    }
    function onUp() {
        if (!drag) return;
        drag = null;
        controls.enabled = true;
        act.keep();
        paintPoints(null);
    }
    function onWheel(e) {
        if (!drag) return;
        e.preventDefault();
        e.stopImmediatePropagation();
        setRadius(radius * (e.deltaY > 0 ? 1 / 1.1 : 1.1));
        const t = target(), w = around(drag.L);
        for (const v of w.keys()) if (!drag.d0.has(v)) drag.d0.set(v, t[v] ? [...t[v]] : [0, 0, 0]);
        /* the ones the radius left: back where they were */
        for (const v of drag.w.keys()) if (!w.has(v)) t[v] = [...drag.d0.get(v)];
        drag.w = w;
        act.reshaped();
    }
    const setRadius = v => { radius = Math.min(2, Math.max(0.005, v)); };
    function setMode(m) {
        selectBlip();
        if (gizmo.active) return;
        mode = m;
        brushAt = null;
        brushW = null;
    }

    /* ------------------------------------------------------------ keys -- */

    function onKey(e) {
        const tag = e.target && e.target.tagName;
        const k = e.key.toLowerCase();
        e.stopPropagation();
        if ((e.ctrlKey || e.metaKey) && (k === 'z' || k === 'y') && tag !== 'TEXTAREA' && !(tag === 'INPUT' && e.target.type === 'text')) {
            if (!drag && !gizmo.active) act.undo(k === 'z' && !e.shiftKey);
            e.preventDefault();
            return;
        }
        if (tag === 'INPUT' || tag === 'SELECT' || tag === 'TEXTAREA') return;
        if (mode === 'object' && gizmo.key(e)) { e.preventDefault(); return; }
        if (k === 'tab') setMode(mode === 'object' ? 'sculpt' : 'object');
        else if (e.code === 'KeyI') { if (e.altKey) act.deleteKeys(); else act.insertKey(); }
        else if (k === '[' || k === ']') setRadius(radius * (k === ']' ? 1.15 : 1 / 1.15));
        else if (k === ' ') act.play();
        else if (nav.key(e)) { /* the views */ }
        else return;
        e.preventDefault();
    }

    /* ---------------------------------------------------------- the side -- */

    const trs = $derived.by(() => { erev; const el = act.el(); return el ? trsAt(el, frame) : null; });
    const shapes = $derived.by(() => {
        erev;
        const el = act.el();
        return el ? el.shapes.map(s => ({ name: s.name, v: shapeAt(s, frame), mute: !!s.mute, keyed: s.keys.some(k => k.f === frame), pending: !!(s.pending && s.pending.f === frame) })) : [];
    });
    $effect(() => { if (active >= shapes.length) active = shapes.length - 1; });
    function setField(c, k, v) {
        const n = Number(v);
        if (!Number.isFinite(n) || !trs) return;
        act.remember();
        const t = copyTrs(trs);
        t[c][k] = n;
        act.setTrs(t);
        act.keep();
    }
    function setTool(t) {
        selectBlip();
        tool = tool === t ? '' : t;
        gizmo.setTool(tool);
    }
    function addShape() {
        selectBlip();
        active = act.addShape();
    }

    /* the UVs on the texture */
    function drawUV() {
        if (!uvEl) return;
        const cv = uvEl, W = cv.width = cv.clientWidth * devicePixelRatio, H = cv.height = cv.clientHeight * devicePixelRatio;
        const g = cv.getContext('2d');
        g.clearRect(0, 0, W, H);
        const img = tex && tex.image;
        if (img && img.data) {
            const id = new ImageData(new Uint8ClampedArray(img.data.buffer, img.data.byteOffset, img.width * img.height * 4), img.width, img.height);
            const c2 = new OffscreenCanvas(img.width, img.height);
            c2.getContext('2d').putImageData(id, 0, 0);
            g.drawImage(c2, 0, 0, W, H);
        } else if (img) g.drawImage(img, 0, 0, W, H);
        const uv = d.mesh.uv, f = d.mesh.faces;
        g.strokeStyle = 'rgba(255, 170, 70, .85)';
        g.lineWidth = devicePixelRatio;
        g.beginPath();
        for (let t = 0; t < f.length; t += 3) {
            for (let j = 0; j < 4; j++) {
                const v = f[t + (j % 3)], x = uv[v * 2] * W, y = uv[v * 2 + 1] * H;
                if (j) g.lineTo(x, y); else g.moveTo(x, y);
            }
        }
        g.stroke();
    }

    onMount(() => { setup(); requestAnimationFrame(drawUV); });
    onDestroy(() => {
        cancelAnimationFrame(raf);
        window.removeEventListener('pointermove', onMove);
        window.removeEventListener('pointerup', onUp);
        window.removeEventListener('keydown', onKey, true);
        if (gizmo) gizmo.dispose();
        if (geo) geo.dispose();
        if (mesh) { mesh.material.dispose(); wire.material.dispose(); points.material.dispose(); }
        nav && nav.dispose();
        renderer && renderer.dispose();
    });
    const f3 = v => +v.toFixed(3);
</script>

<div class="veil" role="presentation" use:portal>
    <div class="win" role="dialog" aria-label={tx('seTitle')}>
        <header>
            <span class="ttl">{tx('seTitle')} · {name}</span>
            <button class="btn x" onclick={() => { selectBlip(); act.close(); }} aria-label={tx('seClose')}>✕</button>
        </header>
        <div class="body">
            <section class="main">
                <div class="view" bind:this={host}>
                    <div class="vname">{viewName}</div>
                    {#if mode === 'sculpt' && brushAt}<span class="ring" style:left="{brushAt.x}px" style:top="{brushAt.y}px" style:width="{brushAt.r * 2}px" style:height="{brushAt.r * 2}px"></span>{/if}
                </div>
                {#if info}<div class="edit">{info}</div>{/if}
                <div class="topbtns" bind:this={topEl}>
                    <button class="btn" class:hot={mode === 'object'} onclick={() => setMode('object')}>{tx('seObject')}</button>
                    <button class="btn" class:hot={mode === 'sculpt'} onclick={() => setMode('sculpt')}>{tx('seSculpt')}</button>
                </div>
                <div class="play">
                    <button class="btn" onclick={() => { selectBlip(); act.play(); }}>{playing ? tx('vwPause') : tx('vwPlay')}</button>
                    <span class="lbl grow">{tx('vwFrame')} {frame} / {length}</span>
                    <button class="btn" class:hot={rec} onclick={() => { selectBlip(); act.rec(); }} title={tx('stRecNote')}>{rec ? '● REC' : '○ REC'}</button>
                    <button class="btn" onclick={act.insertKey}>{tx('stKey')}</button>
                    <button class="btn" onclick={act.deleteKeys}>{tx('stDelKey')}</button>
                    <select class="sel interp" value={interp} onchange={e => act.setInterp(e.currentTarget.value)} title={tx('stInterp')}>
                        <option value="bezier">BEZIER</option><option value="linear">{tx('stLinear')}</option>
                    </select>
                </div>
                <KeyTimeline {length} {frame} {rows} onSeek={act.seek} />
            </section>

            <aside class="side">
                <dl class="props">
                    <dt>{tx('stMesh')}</dt><dd>{d.mesh.name || name}</dd>
                    <dt>{tx('stTexture')}</dt><dd>{stem || '—'}</dd>
                    <dt>{tx('stGeo')}</dt><dd>{nv} / {d.mesh.faces.length / 3}</dd>
                </dl>
                <span class="lbl head">UV</span>
                <canvas class="uv" bind:this={uvEl}></canvas>

                <span class="lbl head">{tx('stPlacement')}</span>
                {#if mode === 'object'}
                    <div class="trio">
                        <button class="btn" class:hot={tool === 'translate'} onclick={() => setTool('translate')}>{tx('stMove')}</button>
                        <button class="btn" class:hot={tool === 'rotate'} onclick={() => setTool('rotate')}>{tx('stTurn')}</button>
                        <button class="btn" class:hot={tool === 'scale'} onclick={() => setTool('scale')}>{tx('stScl')}</button>
                    </div>
                    <label class="chk"><input type="checkbox" checked={space === 'world'} onchange={e => { space = e.currentTarget.checked ? 'world' : 'local'; gizmo.setSpace(space); }}><span>{tx('seGlobal')}</span></label>
                {/if}
                {#if trs}
                    {#each [['T', 'stPos'], ['R', 'stRot'], ['S', 'stScale']] as [c, key]}
                        <div class="xyz">
                            <span class="lbl">{tx(key)}</span>
                            {#each [0, 1, 2] as k}
                                <input class="num" type="number" step={c === 'R' ? 1 : c === 'S' ? 0.05 : 0.01} value={f3(trs[c][k])} onchange={e => setField(c, k, e.currentTarget.value)}>
                            {/each}
                        </div>
                    {/each}
                {/if}

                <span class="lbl head">{tx('seShapes')}</span>
                <ul class="shapes">
                    <li class:on={active === -1}>
                        <button class="nm" onclick={() => { selectBlip(); active = -1; }}>Basis</button>
                    </li>
                    {#each shapes as s, j}
                        <li class:on={active === j}>
                            <input class="nmin" value={s.name} spellcheck="false" onfocus={() => { active = j; }} onchange={e => act.renameShape(j, e.currentTarget.value)}>
                            <input type="range" min="0" max="1" step="0.01" value={s.v} title={s.v.toFixed(2)} oninput={e => act.setShape(j, Number(e.currentTarget.value))}>
                            <button class="eye" class:hot={s.keyed && !s.pending} onclick={() => act.keyShape(j)} title={tx('seKeyShape')}>◆</button>
                            <button class="eye" class:off={s.mute} onclick={() => act.muteShape(j)} title={tx('seMute')}>{s.mute ? '○' : '●'}</button>
                            <button class="eye" onclick={() => { selectBlip(); act.delShape(j); }} title={tx('seDelShape')}>✕</button>
                        </li>
                    {/each}
                </ul>
                <button class="btn" onclick={addShape}>{tx('seAddShape')}</button>

                <span class="lbl head">{tx('seSculpt')}</span>
                <label class="scale"><span class="lbl">{tx('seRadius')} {Math.round(radius * 100)}%</span>
                    <input type="range" min="0.005" max="1" step="0.005" value={radius} oninput={e => setRadius(Number(e.currentTarget.value))}></label>
                <p class="note">{mode === 'sculpt' ? tx('seSculptOn') + ' ' + (active >= 0 && shapes[active] ? shapes[active].name : 'Basis') + '.' : ''} {tx('seNote')}</p>
            </aside>
        </div>
    </div>
</div>

<style>
    .veil { position: fixed; inset: 0; z-index: 1000; background: rgba(5, 4, 20, .72); display: grid; place-items: center; }
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
    .view { position: relative; flex: 1; min-height: 0; border-radius: var(--r-sm); overflow: hidden; border: 1px solid var(--line);
        background: radial-gradient(ellipse at 50% 35%, #2a2560, #0a0820 75%); }
    .view :global(canvas) { width: 100%; height: 100%; display: block; }
    .view :global(canvas.navgizmo) { width: 96px; height: 96px; }
    .vname { position: absolute; left: calc(var(--u) * 1); bottom: calc(var(--u) * .6); font-size: calc(var(--t) * 1.1); color: var(--dim); pointer-events: none; letter-spacing: .06em; }
    .ring { position: absolute; transform: translate(-50%, -50%); border: 1px solid #ffa040; border-radius: 50%; pointer-events: none; }
    .edit { position: absolute; left: 50%; top: calc(var(--u) * .6); transform: translateX(-50%); padding: 2px 10px; border-radius: var(--r-sm);
        background: rgba(10, 8, 32, .8); color: #fff; font-size: calc(var(--t) * 1.2); letter-spacing: .06em; pointer-events: none; white-space: nowrap; }
    .topbtns { position: absolute; right: calc(var(--u) * .8); top: calc(var(--u) * .6); display: flex; gap: calc(var(--u) * .5); }
    .topbtns .btn { height: auto; padding: calc(var(--u) * .35) calc(var(--u) * .9); font-size: calc(var(--t) * 1.2); }
    .play { display: flex; align-items: center; gap: calc(var(--u) * .7); margin-top: calc(var(--u) * .6); }
    .play .btn { height: auto; padding: calc(var(--u) * .35) calc(var(--u) * .8); }
    .play .grow { flex: 1; }
    .play .interp { flex: 0 0 calc(var(--u) * 11); }
    .main :global(.tl) { margin-top: calc(var(--u) * .4); max-height: calc(var(--u) * 14); overflow-y: auto; }
    .side { display: flex; flex-direction: column; gap: calc(var(--u) * .5); min-height: 0; overflow-y: auto; padding-right: calc(var(--u) * .3); }
    .side .btn { white-space: normal; height: auto; min-height: var(--ctl-h); flex: none; }
    .head { margin-top: calc(var(--u) * .6); border-top: 1px solid var(--line); padding-top: calc(var(--u) * .5); }
    .props { display: grid; grid-template-columns: auto 1fr; gap: calc(var(--u) * .3) calc(var(--u) * .8); margin: 0; font-size: calc(var(--t) * 1.15); }
    .props dt { color: var(--dim); }
    .props dd { margin: 0; color: var(--lav); word-break: break-all; }
    .uv { flex: none; width: 100%; aspect-ratio: 1; border-radius: var(--r-sm); border: 1px solid var(--line);
        background: repeating-conic-gradient(#1b1840 0 25%, #14112f 0 50%) 0 0 / 12px 12px; }
    .trio { display: grid; grid-template-columns: repeat(3, 1fr); gap: calc(var(--u) * .4); }
    .trio .btn { font-size: calc(var(--t) * 1.05); }
    .xyz { display: grid; grid-template-columns: calc(var(--u) * 8) repeat(3, minmax(0, 1fr)); gap: calc(var(--u) * .3); align-items: center; }
    .num { min-width: 0; background: var(--well); color: #fff; border: 1px solid var(--line); border-radius: var(--r-sm); padding: 1px 4px; }
    .shapes { list-style: none; margin: 0; padding: calc(var(--u) * .3); background: var(--well); border-radius: var(--r-sm); flex: none; display: flex; flex-direction: column; gap: 2px; }
    .shapes li { display: flex; align-items: center; gap: calc(var(--u) * .4); padding: 2px 4px; border-radius: 3px; }
    .shapes li.on { background: rgba(255, 160, 64, .18); }
    .shapes .nm { flex: 1; text-align: left; background: none; border: 0; color: var(--lav); cursor: pointer; padding: 0; }
    .shapes .nmin { flex: 0 0 calc(var(--u) * 8); min-width: 0; background: transparent; color: var(--lav); border: 1px solid transparent; border-radius: 3px; padding: 0 2px; }
    .shapes .nmin:focus { border-color: var(--line); color: #fff; }
    .shapes input[type=range] { flex: 1; min-width: 0; accent-color: var(--blue); }
    .eye { background: none; border: 0; color: var(--lav); cursor: pointer; padding: 0 2px; }
    .eye.hot { color: #ffa040; }
    .eye.off { color: var(--dim); }
    .scale { display: flex; flex-direction: column; gap: 4px; }
    .scale input { accent-color: var(--blue); }
</style>
