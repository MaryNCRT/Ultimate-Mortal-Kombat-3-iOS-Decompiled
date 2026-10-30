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
    import { TransformControls } from 'three/addons/controls/TransformControls.js';
    import { app, tx } from '#lib/app.svelte.js';
    import { api } from '#lib/platform.js';
    import { selectBlip } from '#lib/sound.js';
    import { readModel, buildSkin, withSkin, solidPicture, poseName } from '#lib/mods/modelin.js';
    import { makeFloor, placeFloor, weightColors, heatColor, heatColors, applyLook, makeRigLines, poseRigLines } from '#lib/mods/viewkit.js';
    import { framePose, framePoseBlend, posedVerts, vmul, mul3, tr3 } from '#lib/mods/rig.js';
    import { decodePicture } from '#lib/mods/image.js';
    import { ViewNav } from '#lib/mods/viewnav.js';
    import { dataUrl } from '#lib/mods/thumbs.js';

    /* f the fighter; file what main.js file:pick gave; texture(stem) the
     * viewer's game textures; outfit the outfit shown; saved, picks: a
     * model opened again from its mod -- what the window was set to
     * (<skin>.import.json) and the pictures chosen for its materials;
     * onApply({ r, mdl, settings, file, picks }), onClose() */
    let { f, file, texture, outfit, saved = null, picks = null, fresh = false, onApply, onClose } = $props();

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
    /* the player as the viewer's: SMOOTH (blended between frames), SPEED */
    let smoothAnims = $state(false), speed = $state(1);
    const SPEEDS = [0.1, 0.25, 0.5, 0.75, 1, 1.5, 2, 3];
    let blendT = 0;
    let hidden = $state({});                /* material -> hidden */
    let big = $state(-1);                   /* the material seen large */
    /* ARM / LEG ANGLE and ROLL (buildSkin o.adjust), degrees */
    let adjust = $state({ armAngle: 0, armRoll: 0, legAngle: 0, legRoll: 0, auto: false });
    let tab = $state('rig');                 /* the side's tab: 'mats' or 'rig' */
    let joints = $state({});                /* bone -> [dx, dy, dz] (buildSkin o.joints) */
    let turns = $state({});                 /* bone -> [x, y, z] degrees (buildSkin o.turns) */
    let sel = $state(-1);                   /* the joint being moved */
    let mirror = $state(true), reweight = $state(false);
    /* SMOOTH WEIGHTS (passes): 3 on a file just picked, as measured on
     * Mary's Sonya; a model opened again keeps what it was saved with */
    let smooth = $state(fresh ? 3 : 0);
    let look = $state({ wire: false, solid: false, weights: false });
    let showRig = $state(true), showFloor = $state(true);
    let looksEl;                            /* the buttons the camera gizmo sits under */

    let renderer, scene, camera, controls, root, raf, meshes = [], floor, rigLines, lastPose = null, framed = false, nav;
    let viewName = $state('');              /* Blender's view name, top left */
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
        nav = new ViewNav({ host, renderer, controls, persp: camera, onLabel: t => { viewName = t; },
            onCamera: c => { camera = c; if (tcMove) { tcMove.camera = c; tcTurn.camera = c; } },
            names: { front: tx('nvFront'), back: tx('nvBack'), right: tx('nvRight'), left: tx('nvLeft'), top: tx('nvTop'), bottom: tx('nvBottom'),
                user: tx('nvUser'), persp: tx('nvPersp'), ortho: tx('nvOrtho') }, below: () => looksEl });
        nav.label();
        nav.gizmo.title = tx('nvKeys');
        floor = makeFloor();
        rigLines = makeRigLines();
        scene.add(floor, rigLines);
        new ResizeObserver(fit).observe(host);
        axisLine = new THREE.LineSegments(new THREE.BufferGeometry(), new THREE.LineBasicMaterial({ vertexColors: true, depthTest: false, transparent: true }));
        axisLine.renderOrder = 11;
        axisLine.visible = false;
        scene.add(axisLine);
        /* the gizmo on the bone picked (Blender's Move / Rotate / Transform
         * tools) */
        proxy = new THREE.Object3D();
        scene.add(proxy);
        tcMove = new TransformControls(camera, renderer.domElement);
        tcTurn = new TransformControls(camera, renderer.domElement);
        tcMove.setMode('translate');
        tcTurn.setMode('rotate');
        for (const tc of [tcMove, tcTurn]) {
            tc.size = 0.9;
            scene.add(tc.getHelper());
            tc.addEventListener('dragging-changed', e => { controls.enabled = !e.value; });
            tc.addEventListener('mouseDown', () => gizmoStart(tc));
            tc.addEventListener('objectChange', gizmoMove);
            tc.addEventListener('mouseUp', gizmoEnd);
        }
        /* both shown: an arrow, square or the middle wins over a ring */
        tcMove.addEventListener('axis-changed', e => {
            if (gizmo !== 'both') return;
            tcTurn.enabled = !e.value;
            if (e.value) tcTurn.axis = null;
        });
        renderer.domElement.addEventListener('pointerdown', onDown, { capture: true });
        renderer.domElement.addEventListener('contextmenu', e => e.preventDefault());
        window.addEventListener('pointermove', onMove);
        window.addEventListener('pointerup', onUp);
        window.addEventListener('keydown', onKey, true);
        window.addEventListener('keyup', onKeyUp, true);
        loop_();
    }
    function fit() {
        if (!renderer || !host) return;
        renderer.setSize(host.clientWidth, host.clientHeight, false);
        if (nav) nav.resize();
        else { camera.aspect = host.clientWidth / Math.max(1, host.clientHeight); camera.updateProjectionMatrix(); }
    }
    let last = 0, acc = 0;
    function loop_(t = 0) {
        raf = requestAnimationFrame(loop_);
        const dt = Math.min(0.1, (t - last) / 1000);
        last = t;
        if (playing && pf && clip >= 0) {
            acc += dt;
            if (acc >= 1 / (30 * speed)) {             /* the game's 30 a second, times SPEED */
                acc = 0;
                const c = pf.clips[clip];
                if (frame < c.to - c.from) frame++;
                else if (loop) frame = 0;
                else playing = false;
            }
            /* between two frames: drawn blended, every screen refresh */
            if (smoothAnims && !edit) {
                blendT = Math.min(1, acc * 30 * speed);
                pose();
            }
        } else blendT = 0;
        controls && controls.update();
        renderer && renderer.render(scene, camera);
        nav && nav.draw();
    }

    /* the model as the game will have it, built again after each change */
    async function rebuild() {
        if (!mdl) return;
        err = '';
        try {
            res = buildSkin(f, mdl, { turn, blockOf, keep, scale, adjust: { ...adjust }, joints: $state.snapshot(joints), turns: $state.snapshot(turns), reweight, smooth, paint });
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
        const colors = vertexColors();
        /* each vertex the skin has, the ones drawn of it (WEIGHT PAINT) */
        outs = new Map();
        pf.topo.src.forEach((sv, o) => { if (!outs.has(sv)) outs.set(sv, []); outs.get(sv).push(o); });
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
        /* the camera put once; after, where it was left */
        if (framed) return;
        framed = true;
        nav.reframed();
        controls.target.copy(c);
        camera.near = size / 500;
        camera.far = size * 50;
        camera.position.set(c.x + size * 0.15, c.y + size * 0.1, c.z + size * 1.1);
        camera.updateProjectionMatrix();
        nav.reframed();
        nav.label();
    }

    function pose() {
        if (!pf || !meshes.length) return;
        let p;
        if (clip >= 0) {
            const c = pf.clips[clip], at = c.from + Math.min(frame, c.to - c.from);
            const next = at < c.to ? at + 1 : loop ? c.from : at;
            p = smoothAnims && playing && blendT > 0 ? framePoseBlend(pf.rig, at, next, blendT) : framePose(pf.rig, at);
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
        syncGizmo();
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
        remember();
        mdl.materials[i].image = pic;
        mdl.materials[i].picked = got.bytes;
        mdl.materials[i].file = got.name;
        mdl.materials[i].game = false;
        await makeThumbs();
        await rebuild();
    }
    async function gameTex(i) {
        selectBlip();
        remember();
        mdl.materials[i].image = null;
        mdl.materials[i].picked = null;
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
        remember();
        blockOf[i] = Number(b);
        mdl.materials[i].rep = t;
        await makeThumbs();
        await rebuild();
    }

    let scaleWait;
    function setScale(v) {
        remember('scale');
        scale = Math.min(4, Math.max(0.25, Number(v) || 1));
        clearTimeout(scaleWait);
        scaleWait = setTimeout(rebuild, 150);
    }

    /* UNDO / REDO (Ctrl+Z, Ctrl+Y or Ctrl+Shift+Z): what the window is set
     * to, kept before each change -- a slider's run of moves as one */
    let undo = [], redo = [], lastKey = '', lastAt = 0;
    let undoCount = $state(0), redoCount = $state(0);
    function snap() {
        return { joints: $state.snapshot(joints), turns: $state.snapshot(turns), paint: { ...paint }, adjust: $state.snapshot(adjust),
            scale, keep, turn, reweight, smooth, blockOf: [...blockOf],
            mats: mdl ? mdl.materials.map(m => ({ image: m.image, file: m.file, game: m.game, rep: m.rep, picked: m.picked })) : [] };
    }
    function remember(key = '') {
        const now = Date.now();
        if (key && key === lastKey && now - lastAt < 800) { lastAt = now; return; }
        lastKey = key;
        lastAt = now;
        undo.push(snap());
        if (undo.length > 100) undo.shift();
        redo = [];
        undoCount = undo.length;
        redoCount = 0;
    }
    async function undoIt(back) {
        endEdit(false);
        const from = back ? undo : redo, to = back ? redo : undo;
        if (!from.length || !mdl) return;
        selectBlip();
        to.push(snap());
        const st = from.pop();
        undoCount = undo.length;
        redoCount = redo.length;
        lastKey = '';
        joints = st.joints; turns = st.turns; paint = st.paint; paintCount = Object.keys(paint).length;
        adjust = st.adjust; scale = st.scale; keep = st.keep; turn = st.turn; reweight = st.reweight; smooth = st.smooth; blockOf = st.blockOf;
        st.mats.forEach((m, i) => Object.assign(mdl.materials[i], m));
        await makeThumbs();
        await rebuild();
    }

    /* RESET TO A CLEAN T / A POSE: every change put back and the arms
     * turned level (T) or 45 degrees down (A) -- the model's own arms when
     * it keeps its proportions (or has the exporter's names), else the
     * fighter's joints it is fitted to -- so the rig starts with nothing
     * bent */
    async function cleanPose(rise) {
        selectBlip();
        remember();
        endEdit(false);
        joints = {}; turns = {}; paint = {}; paintCount = 0; scale = 1; reweight = false;
        adjust = { armAngle: 0, armRoll: 0, legAngle: 0, legRoll: 0, auto: false };
        await rebuild();
        if (res && res.pose && rise !== null) {
            /* the angle turns the file's arms where they are its own (KEEP
             * PROPORTIONS, the exporter's rig), else the fighter's -- a model
             * with no skeleton has no arms of its own to measure, and was
             * left in T by both buttons */
            const own = (keep || res.rig === 'names') && res.pose.file.arm !== null && res.pose.file.arm !== undefined;
            const from = own ? res.pose.file.arm : res.pose.fighter.arm;
            if (from !== null && from !== undefined) {
                adjust.armAngle = Math.max(-90, Math.min(90, Math.round(rise - from)));
                await rebuild();
            }
        }
    }

    /* By hand on the view, as Blender does. EDIT BONES: a click on a joint
     * picks it, a drag of it moves it; G moves the one picked and R turns
     * it (its children go round its joint), X / Y / Z keep to that axis
     * (again: free), a click or Enter keeps it, Esc or the right button
     * puts it back. WEIGHT PAINT: the brush paints the bone picked onto
     * the vertices under it -- ADD, SUBTRACT (or Ctrl), SMOOTH -- its
     * radius in pixels on the screen, the other bones' weights scaled so
     * each vertex's add up to one; seen as Blender's colours, blue 0 to
     * red 1. The painted weights are kept per vertex of the file (o.paint)
     * and saved with the import's settings. */
    let tool = $state('bones');             /* 'bones' or 'paint' */
    let brush = $state({ mode: 'add', radius: 40, strength: 0.5 });
    let editInfo = $state('');
    let brushAt = $state(null);             /* the brush's ring, px in the view */
    let paint = {};                         /* "mesh:vertex" -> [[bone, w]] */
    let paintCount = $state(0);
    let edit = null, stroke = false, downAt = null, mouse = null, outs = null, paintWait, axisLine;
    const ray = new THREE.Raycaster();
    const AX = [new THREE.Vector3(1, 0, 0), new THREE.Vector3(0, 1, 0), new THREE.Vector3(0, 0, 1)];
    const AXCOL = [0xff4466, 0x88ee44, 0x4488ff];
    const DEG = Math.PI / 180;
    /* The gizmo on the bone picked, as Blender's Move / Rotate / Transform
     * tools draw it: an arrow moves along one axis, a square on a plane
     * (2D), the middle free on the view (3D); a ring turns about one axis,
     * the white ring about the view, inside it freely (trackball). GLOBAL
     * the game's axes, LOCAL the bone's own. */
    let gizmo = $state('both');             /* 'move', 'turn', 'both' or 'none' */
    let orient = $state('global');          /* 'global' or 'local' */
    let tcMove, tcTurn, proxy;

    const rect = () => renderer.domElement.getBoundingClientRect();
    function ndc(e) {
        const r = rect();
        return new THREE.Vector2((e.clientX - r.left) / r.width * 2 - 1, -(e.clientY - r.top) / r.height * 2 + 1);
    }
    /* the joint under the pointer, or -1 */
    function jointAt(e) {
        if (!lastPose || !showRig) return -1;
        const r = rect(), m = ndc(e);
        let best = -1, bd = 0.03;
        lastPose.forEach((b, i) => {
            if (!b || i === f.rig.bones.root) return;
            const v = new THREE.Vector3(...b.T).project(camera);
            const d = Math.hypot(v.x - m.x, (v.y - m.y) * r.height / r.width);
            if (v.z < 1 && d < bd) { bd = d; best = i; }
        });
        return best;
    }
    const inView = e => { const r = rect(); return e.clientX >= r.left && e.clientX <= r.right && e.clientY >= r.top && e.clientY <= r.bottom; };
    const subtree = b => { const out = [b]; for (let k = 0; k < out.length; k++) out.push(...f.rig.bones.children[out[k]]); return out; };
    const viewSize = () => (root ? new THREE.Box3().setFromObject(root).getSize(new THREE.Vector3()).length() : 0) || 100;
    /* a row-form turn (v . R) as three.js's matrix */
    const rowMat = R => new THREE.Matrix4().set(R[0], R[3], R[6], 0, R[1], R[4], R[7], 0, R[2], R[5], R[8], 0, 0, 0, 0, 1);

    /* the gizmo where the bone picked is now (rest or the frame shown) */
    function syncGizmo() {
        if (!tcMove || (edit && edit.gizmo)) return;
        const on = tool === 'bones' && sel >= 0 && showRig && !edit && !!lastPose && !!lastPose[sel];
        const turnable = on && subtree(sel).length > 1;
        for (const [tc, want] of [[tcMove, on && (gizmo === 'move' || gizmo === 'both')], [tcTurn, turnable && (gizmo === 'turn' || gizmo === 'both')]]) {
            if (want && tc.object !== proxy) tc.attach(proxy);
            else if (!want && tc.object) tc.detach();
            tc.space = orient === 'local' ? 'local' : 'world';
        }
        tcTurn.enabled = true;
        if (!on) return;
        proxy.position.set(...lastPose[sel].T);
        const m = rowMat(lastPose[sel].R);
        if (m.determinant() > 0) proxy.quaternion.setFromRotationMatrix(m);
        else proxy.quaternion.identity();
        proxy.updateMatrixWorld();
    }
    $effect(() => { gizmo; orient; tool; sel; showRig; syncGizmo(); });
    /* Ctrl: steps (1 unit, 5 degrees; with Shift 0.1 and 1), on the gizmo too */
    function setSnaps(e) {
        if (!tcMove) return;
        tcMove.translationSnap = e.ctrlKey ? (e.shiftKey ? 0.1 : 1) * scale : null;
        tcTurn.rotationSnap = e.ctrlKey ? (e.shiftKey ? 1 : 5) * DEG : null;
    }
    /* the gizmo's handles as a constraint: 'X' an axis, 'XY' a plane (the
     * third axis left out), 'XYZ' free, 'E' the view, 'XYZE' trackball */
    function gizmoStart(tc) {
        const a = tc.axis || '', local = tc.space === 'local';
        if (!begin(tc === tcMove ? 'move' : 'turn', null, true)) { syncGizmo(); return; }
        edit.gizmo = true;
        if (a.length === 1) edit.con = { i: 'XYZ'.indexOf(a), plane: false, local };
        else if (a.length === 2) edit.con = { i: [0, 1, 2].find(i => !a.includes('XYZ'[i])), plane: true, local };
        edit.track = a === 'XYZE';
        edit.q0 = proxy.quaternion.clone();
    }
    function gizmoMove() {
        if (!edit || !edit.gizmo) return;
        if (edit.kind === 'move') return applyMove(proxy.position.clone().sub(edit.P));
        const q = proxy.quaternion.clone().multiply(edit.q0.clone().invert()).normalize();
        if (q.w < 0) q.set(-q.x, -q.y, -q.z, -q.w);
        const s = Math.sqrt(Math.max(0, 1 - q.w * q.w)), ang = 2 * Math.acos(Math.min(1, q.w));
        const u = s < 1e-6 ? AX[0].clone() : new THREE.Vector3(q.x / s, q.y / s, q.z / s);
        /* about a ring's axis: the angle's sign by that axis */
        if (edit.con && u.dot(axisOf(edit.con)) < 0) return applyTurn(u.negate(), -ang);
        applyTurn(u, ang);
    }
    function gizmoEnd() {
        if (edit && edit.gizmo) endEdit(!!edit.out);
        else syncGizmo();
    }

    /* At rest or on the frame of a clip shown: the edit is made on the
     * pose seen (a clip playing holds still while it is) and taken back to
     * the rest the skin is bound to -- a move through the parent's turn in
     * that frame, a turn about its axis through the bone's -- so what is
     * kept is what the game will play. */
    function begin(kind, at, drag) {
        if (sel < 0 || !res || tool !== 'bones' || !lastPose) return false;
        const b = sel, base = lastPose, P = new THREE.Vector3(...base[b].T);
        const moved = kind === 'turn' ? subtree(b).slice(1) : [b];
        if (!moved.length) { editInfo = tx('imNoChildren'); return false; }
        edit = { kind, b, drag, at: at ? [at.clientX, at.clientY] : null, con: null, track: false, num: null, P, out: null,
            pose: base, rest: clip < 0, wasPlaying: playing,
            base: new Map(moved.map(i => [i, new THREE.Vector3(...base[i].T)])) };
        playing = false;
        return true;
    }
    function startEdit(kind, at, drag = false) {
        if (!begin(kind, at, drag)) return;
        for (const tc of [tcMove, tcTurn]) if (tc && tc.object) tc.detach();
        updateEdit(at);
    }
    /* a constraint's axis: the game's, or the bone's own (LOCAL) */
    function axisOf(c) {
        if (!c.local) return AX[c.i].clone();
        const R = edit.pose[edit.b].R;
        return new THREE.Vector3(R[c.i * 3], R[c.i * 3 + 1], R[c.i * 3 + 2]).normalize();
    }
    const frameAxes = () => [0, 1, 2].map(i => axisOf({ i, local: !!(edit.con && edit.con.local) }));
    function rayAt(x, y) {
        const r = rect();
        ray.setFromCamera(new THREE.Vector2((x - r.left) / r.width * 2 - 1, -(y - r.top) / r.height * 2 + 1), camera);
        return ray.ray.clone();
    }
    /* where the line P + t a passes closest to a ray: t */
    function lineParam(r, P, a) {
        const w = P.clone().sub(r.origin), b = a.dot(r.direction), d = a.dot(w), e = r.direction.dot(w);
        const den = 1 - b * b;
        return den < 1e-6 ? 0 : (b * e - d) / den;
    }
    const toCam = P => (camera.isOrthographicCamera ? camera.getWorldDirection(new THREE.Vector3()).negate() : camera.position.clone().sub(P).normalize());

    /* the pointer's move as Blender takes it: free on the view's plane, along
     * an axis (the point of the axis under the pointer), or on a plane */
    function dragMove() {
        const { P, con } = edit, [x0, y0] = edit.at, [x1, y1] = edit.eff;
        const view = camera.getWorldDirection(new THREE.Vector3());
        const onPlane = (n, x, y) => rayAt(x, y).intersectPlane(new THREE.Plane().setFromNormalAndCoplanarPoint(n, P), new THREE.Vector3());
        const free = () => { const a = onPlane(view, x0, y0), b = onPlane(view, x1, y1); return a && b ? b.sub(a) : new THREE.Vector3(); };
        if (!con) return free();
        const a = axisOf(con);
        if (con.plane) {
            if (Math.abs(a.dot(view)) > 0.05) { const p = onPlane(a, x0, y0), q = onPlane(a, x1, y1); if (p && q) return q.sub(p); }
            const d = free();
            return d.sub(a.clone().multiplyScalar(d.dot(a)));
        }
        if (Math.abs(a.dot(view)) < 0.98) return a.clone().multiplyScalar(lineParam(rayAt(x1, y1), P, a) - lineParam(rayAt(x0, y0), P, a));
        const d = free();
        return a.clone().multiplyScalar(d.dot(a));
    }
    function snapMove(d) {
        if (!edit.snap) return d;
        const step = (edit.fine ? 0.1 : 1) * scale, out = new THREE.Vector3();
        for (const a of frameAxes()) out.addScaledVector(a, Math.round(d.dot(a) / step) * step);
        return out;
    }
    const numCount = () => (edit.kind === 'turn' ? 1 : !edit.con ? 3 : edit.con.plane ? 2 : 1);
    const numVals = () => edit.num.f.map(x => { const v = parseFloat(x); return Number.isFinite(v) ? v : 0; });
    function numMove() {
        const v = numVals(), c = edit.con, F = frameAxes(), out = new THREE.Vector3();
        if (c && !c.plane) return axisOf(c).multiplyScalar(v[0] * scale);
        const ks = c ? [0, 1, 2].filter(i => i !== c.i) : [0, 1, 2];
        ks.forEach((k, j) => out.addScaledVector(F[k], v[j] * scale));
        return out;
    }
    /* a turn: the pointer's angle about the joint on the screen, counted
     * past a full turn, about the view or the axis kept to; R twice the
     * trackball */
    function dragTurn() {
        const { P, con } = edit, [x0, y0] = edit.at, [x1, y1] = edit.eff;
        if (edit.track) {
            const up = new THREE.Vector3(0, 1, 0).applyQuaternion(camera.quaternion), right = new THREE.Vector3(1, 0, 0).applyQuaternion(camera.quaternion);
            const q = new THREE.Quaternion().setFromAxisAngle(up, (x1 - x0) * 0.01).multiply(new THREE.Quaternion().setFromAxisAngle(right, (y1 - y0) * 0.01));
            if (q.w < 0) q.set(-q.x, -q.y, -q.z, -q.w);
            const s = Math.sqrt(Math.max(0, 1 - q.w * q.w));
            return s < 1e-6 ? [up, 0] : [new THREE.Vector3(q.x / s, q.y / s, q.z / s), 2 * Math.acos(Math.min(1, q.w))];
        }
        const r = rect(), sp = P.clone().project(camera);
        const sx = (sp.x + 1) / 2 * r.width + r.left, sy = (1 - sp.y) / 2 * r.height + r.top;
        const raw = Math.atan2(sy - y1, x1 - sx);
        if (edit.prev === undefined) { edit.prev = Math.atan2(sy - y0, x0 - sx); edit.acc = 0; }
        let dd = raw - edit.prev;
        while (dd > Math.PI) dd -= 2 * Math.PI;
        while (dd < -Math.PI) dd += 2 * Math.PI;
        edit.acc += dd;
        edit.prev = raw;
        const c = toCam(P), u = con ? axisOf(con) : c;
        return u.dot(c) < 0 ? [u.negate(), edit.acc] : [u, edit.acc];
    }
    const snapAng = a => (edit.snap ? Math.round(a / ((edit.fine ? 1 : 5) * DEG)) * (edit.fine ? 1 : 5) * DEG : a);

    function updateEdit(e) {
        if (!edit || edit.gizmo) return;
        if (e) {
            if (!edit.at) edit.at = [e.clientX, e.clientY];
            if (!edit.eff) { edit.eff = [...edit.at]; edit.last = [...edit.at]; }
            /* Shift: ten times finer from where it was pressed */
            const k = e.shiftKey ? 0.1 : 1;
            edit.eff = [edit.eff[0] + (e.clientX - edit.last[0]) * k, edit.eff[1] + (e.clientY - edit.last[1]) * k];
            edit.last = [e.clientX, e.clientY];
            if (e.ctrlKey !== undefined) { edit.snap = e.ctrlKey; edit.fine = e.shiftKey; }
        }
        if (!edit.at || !edit.eff) return;
        if (edit.mmb) pickAxis();
        if (edit.kind === 'move') applyMove(edit.num ? numMove() : snapMove(dragMove()));
        else if (edit.num) applyTurn(edit.con ? axisOf(edit.con) : toCam(edit.P), numVals()[0] * DEG);
        else { const [u, a] = dragTurn(); applyTurn(u, edit.track ? a : snapAng(a)); }
    }
    /* the middle button held: the axis the pointer goes along */
    function pickAxis() {
        const [x0, y0] = edit.mmb, [x1, y1] = edit.last, dx = x1 - x0, dy = y1 - y0, l = Math.hypot(dx, dy);
        if (l < 12) return;
        const r = rect(), sp = edit.P.clone().project(camera), L = viewSize() * 0.05, local = orient === 'local';
        let best = 0, bd = -1;
        for (let i = 0; i < 3; i++) {
            const q = edit.P.clone().addScaledVector(axisOf({ i, local }), L).project(camera);
            const ax = (q.x - sp.x) * r.width, ay = -(q.y - sp.y) * r.height, al = Math.hypot(ax, ay) || 1;
            const sc = Math.abs((ax * dx + ay * dy) / (al * l));
            if (sc > bd) { bd = sc; best = i; }
        }
        edit.con = { i: best, plane: false, local };
    }
    /* X / Y / Z: the axis in the orientation set, again the other one (as
     * Blender: global, local), again none; with Shift the plane without it */
    function constrain(i, plane) {
        const c = edit.con, first = orient === 'local';
        if (c && c.i === i && c.plane === plane) edit.con = c.local === first ? { i, plane, local: !first } : null;
        else edit.con = { i, plane, local: first };
        edit.track = false;
        updateEdit(null);
    }
    /* typed numbers, as Blender's: digits, '.', '-' the sign, Backspace,
     * Tab the next field (X, Y, Z) */
    function typeNum(e) {
        const k = e.key;
        if (!edit.num) {
            if (k === 'Backspace') return;
            edit.num = { f: ['', '', ''], k: 0 };
        }
        const n = edit.num, cnt = numCount();
        if (k === 'Tab') n.k = (n.k + (e.shiftKey ? cnt - 1 : 1)) % cnt;
        else if (k === 'Backspace') { if (n.f[n.k]) n.f[n.k] = n.f[n.k].slice(0, -1); else if (!n.f.some(Boolean)) edit.num = null; }
        else if (k === '-') n.f[n.k] = n.f[n.k].startsWith('-') ? n.f[n.k].slice(1) : '-' + n.f[n.k];
        else if (k === '.' || k === ',') { if (!n.f[n.k].includes('.')) n.f[n.k] += '.'; }
        else n.f[n.k] += k;
        if (edit.num && edit.num.k >= cnt) edit.num.k = 0;
        updateEdit(null);
    }

    const fmt = x => (Math.abs(x) < 5e-5 ? 0 : x).toFixed(3);
    const numShow = i => (edit.num.k === i ? '[' + (edit.num.f[i] || '') + '|]' : edit.num.f[i] || '0');
    function conText() {
        const c = edit.con;
        if (edit.track) return ' · ' + tx('imTrackball');
        if (!c) return '';
        const sp = tx(c.local ? 'imLocal' : 'imGlobal');
        if (c.plane && edit.kind === 'move') return ' · ' + tx('imPlaneOf') + ' ' + sp + ' ' + 'XYZ'.replace('XYZ'[c.i], '');
        return ' · ' + tx('imAxisOf') + ' ' + sp + ' ' + 'XYZ'[c.i];
    }
    function applyMove(d) {
        edit.d = d;
        edit.out = new Map([[edit.b, edit.P.clone().add(d)]]);
        const v = edit.num ? [...Array(numCount()).keys()].map(numShow) : frameAxes().map(a => fmt(d.dot(a) / scale));
        editInfo = tx('imEditMove') + ' D: ' + v.join(' | ') + ' (' + fmt(d.length() / scale) + ')' + conText() + (edit.gizmo ? '' : ' · ' + tx('imEditKeys'));
        showAxis();
        previewEdit();
    }
    function applyTurn(u, ang) {
        const c = edit.con;
        edit.u = u;
        edit.ang = ang;
        edit.deg = ang / DEG * (c && u.dot(axisOf(c)) < 0 ? -1 : 1);
        const q = new THREE.Quaternion().setFromAxisAngle(u, ang), out = new Map();
        for (const [i, T] of edit.base) out.set(i, T.clone().sub(edit.P).applyQuaternion(q).add(edit.P));
        edit.out = out;
        editInfo = tx('imEditTurn') + ' ' + (edit.num ? numShow(0) : fmt(edit.deg)) + '°' + conText() + (edit.gizmo ? '' : ' · ' + tx('imEditKeys'));
        showAxis();
        previewEdit();
    }
    /* what the edit moves, as BONE POSITIONS' offsets (before the scale),
     * each joint's other side mirrored */
    function editDeltas() {
        const m = new Map(), { b } = edit, P0 = edit.pose, R0 = res.rest;
        /* the pose's frame to the rest's: v . R_pose^T . R_rest */
        const toRest = (v, i) => (edit.rest || i < 0 ? v : vmul(v, mul3(tr3(P0[i].R), R0[i].R)));
        if (edit.kind === 'move') {
            if (edit.d) m.set(b, toRest(edit.d.toArray(), f.rig.bones.parent[b]).map(x => x / scale));
        } else if (edit.u) {
            const q = new THREE.Quaternion().setFromAxisAngle(new THREE.Vector3(...toRest(edit.u.toArray(), b)).normalize(), edit.ang);
            const P = new THREE.Vector3(...R0[b].T);
            for (const i of edit.base.keys()) {
                const T = new THREE.Vector3(...R0[i].T);
                m.set(i, T.clone().sub(P).applyQuaternion(q).add(P).sub(T).toArray().map(x => x / scale));
            }
        }
        if (mirror) for (const [i, d] of [...m]) { const t = twin(i); if (t >= 0 && !m.has(t)) m.set(t, [-d[0], d[1], d[2]]); }
        return m;
    }
    function previewEdit() {
        let p;
        if (edit.rest) {
            const d = editDeltas();
            p = res.rest.map((r, i) => (d.has(i) ? { ...r, T: r.T.map((x, c) => x + d.get(i)[c] * scale) } : r));
        } else p = edit.pose.map((r, i) => (edit.out.has(i) ? { ...r, T: edit.out.get(i).toArray() } : r));
        poseRigLines(rigLines, p, pf.rig.bones.parent, true, sel);
        lastPose = p;
    }
    /* the axis (or a plane's two) kept to, through the joint */
    function showAxis() {
        if (!axisLine) return;
        const c = edit && !edit.gizmo && !edit.track ? edit.con : null;
        axisLine.visible = !!c;
        if (!c) return;
        const L = viewSize(), pts = [], cols = [];
        for (const i of c.plane && edit.kind === 'move' ? [0, 1, 2].filter(i => i !== c.i) : [c.i]) {
            const a = axisOf({ i, local: c.local }), col = new THREE.Color(AXCOL[i]);
            pts.push(edit.P.clone().addScaledVector(a, -L), edit.P.clone().addScaledVector(a, L));
            cols.push(col.r, col.g, col.b, col.r, col.g, col.b);
        }
        axisLine.geometry.dispose();
        axisLine.geometry = new THREE.BufferGeometry().setFromPoints(pts);
        axisLine.geometry.setAttribute('color', new THREE.Float32BufferAttribute(cols, 3));
    }
    function endEdit(keepIt) {
        if (!edit) return;
        if (edit.wasPlaying) playing = true;
        const c = edit.con;
        if (keepIt && edit.out && edit.kind === 'turn' && c && !c.local && !edit.track && edit.rest) {
            /* about a global axis: into the bone's ROTATION field */
            const { b, deg } = edit, axis = c.i;
            edit = null;
            editInfo = '';
            if (axisLine) axisLine.visible = false;
            if (!deg) { pose(); return; }
            remember();
            const next = { ...turns }, add = (i, dv) => { const t = [...(next[i] || [0, 0, 0])]; t[axis] = ((t[axis] + dv + 540) % 360) - 180; next[i] = t; };
            add(b, deg);
            const o = mirror ? twin(b) : -1;
            if (o >= 0) add(o, axis === 0 ? deg : -deg);
            turns = next;
            rebuild();
            return;
        }
        const d = keepIt && edit.out ? editDeltas() : null;
        edit = null;
        editInfo = '';
        if (axisLine) axisLine.visible = false;
        if (d && [...d.values()].some(v => v.some(x => Math.abs(x) > 1e-4))) {
            remember();
            const next = { ...joints };
            for (const [i, v] of d) next[i] = (next[i] || [0, 0, 0]).map((x, c) => x + v[c]);
            joints = next;
            rebuild();
        } else pose();
    }

    /* WEIGHT PAINT */
    function setBone(list, b, w) {
        const others = list.filter(([x]) => x !== b), so = others.reduce((a, x) => a + x[1], 0);
        let out;
        if (so > 1e-6) out = [[b, w], ...others.map(([x, y]) => [x, y * (1 - w) / so])];
        else {
            const par = f.rig.bones.parent[b];
            out = w >= 1 - 1e-6 || par < 0 ? [[b, 1]] : [[b, w], [par, 1 - w]];
        }
        out = out.filter(x => x[1] > 1e-3).sort((a, c) => c[1] - a[1]).slice(0, 4);
        const t = out.reduce((a, x) => a + x[1], 0) || 1;
        return out.length ? out.map(([x, y]) => [x, y / t]) : [[b, 1]];
    }
    function paintAt(e) {
        ray.setFromCamera(ndc(e), camera);
        const hit = ray.intersectObjects(meshes.filter(m => m.visible), false)[0];
        if (!hit) return;
        const r = renderer.domElement.getBoundingClientRect();
        const R = brush.radius * (camera.isOrthographicCamera ? (camera.top - camera.bottom) / camera.zoom
            : 2 * camera.position.distanceTo(hit.point) * Math.tan(camera.fov * Math.PI / 360)) / r.height;
        const pos = meshes[0].geometry.attributes.position.array, W = pf.weights, src = pf.topo.src;
        const h = hit.point, near = new Map();
        for (let o = 0; o < src.length; o++) {
            const d = Math.hypot(pos[o * 3] - h.x, pos[o * 3 + 1] - h.y, pos[o * 3 + 2] - h.z);
            if (d >= R) continue;
            const t = d / R, fall = 1 - t * t * (3 - 2 * t);
            if (fall > (near.get(src[o]) || 0)) near.set(src[o], fall);
        }
        if (!near.size) return;
        const b = sel, listOf = s => {
            const o = outs.get(s)[0], l = [];
            for (let k = 0; k < 4; k++) if (W.wt[o * 4 + k] > 0) l.push([W.idx[o * 4 + k], W.wt[o * 4 + k]]);
            return l;
        };
        const wOf = l => l.reduce((a, [x, y]) => a + (x === b ? y : 0), 0);
        let avg = 0, fs = 0;
        if (brush.mode === 'smooth') { for (const [s, fall] of near) { avg += wOf(listOf(s)) * fall; fs += fall; } avg /= fs || 1; }
        const sub = brush.mode === 'sub' || (brush.mode === 'add' && e.ctrlKey);
        const colors = meshes[0].geometry.attributes.color.array;
        for (const [s, fall] of near) {
            const l = listOf(s), w0 = wOf(l), a = brush.strength * fall * 0.25;
            const w = brush.mode === 'smooth' ? w0 + (avg - w0) * a * 2 : sub ? Math.max(0, w0 - a) : Math.min(1, w0 + a);
            if (Math.abs(w - w0) < 1e-5) continue;
            const nl = setBone(l, b, w);
            for (const o of outs.get(s)) {
                for (let k = 0; k < 4; k++) { W.idx[o * 4 + k] = k < nl.length ? nl[k][0] : 0; W.wt[o * 4 + k] = k < nl.length ? nl[k][1] : 0; }
                heatColor(wOf(nl), colors, o * 3);
            }
            for (const key of res.src[s] || []) paint[key] = nl;
        }
        paintCount = Object.keys(paint).length;
        for (const m of meshes) m.geometry.attributes.color.needsUpdate = true;
    }
    function clearPaint() {
        selectBlip();
        remember();
        paint = {};
        paintCount = 0;
        rebuild();
    }
    /* the colours: WEIGHT PAINT the bone picked, else every bone's */
    function vertexColors() {
        return tool === 'paint' && sel >= 0 ? heatColors(pf.weights, pf.topo.src.length, sel) : weightColors(pf.weights, pf.topo.src.length);
    }
    function recolor() {
        if (!pf || !meshes.length) return;
        const c = vertexColors();
        for (const m of meshes) { m.geometry.attributes.color.array.set(c); m.geometry.attributes.color.needsUpdate = true; }
    }
    function setTool(t) {
        selectBlip();
        endEdit(false);
        tool = t;
        if (t === 'paint') { look.weights = true; look.solid = false; }
    }
    $effect(() => { tool; sel; recolor(); });

    /* the pointer: in front of the camera's own handling of it */
    function onDown(e) {
        if (edit && edit.gizmo) {
            if (e.button === 2) { e.stopImmediatePropagation(); endEdit(false); }
            return;
        }
        if (edit && !edit.drag) {
            e.stopImmediatePropagation();
            if (e.button === 1) { edit.mmb = [e.clientX, e.clientY]; return; }
            endEdit(e.button === 0);
            return;
        }
        if (e.button !== 0) return;
        /* a handle of the gizmo under the pointer: the gizmo takes it */
        if (tool === 'bones' && tcMove) {
            const m = ndc(e), pt = { x: m.x, y: m.y, button: 0 };
            for (const tc of [tcMove, tcTurn])
                if (tc.object && tc.enabled) { tc.pointerHover(pt); if (tc.axis) return; }
        }
        downAt = [e.clientX, e.clientY];
        const j = jointAt(e);
        if (tool === 'bones' && j >= 0) {
            e.stopImmediatePropagation();
            if (j !== sel) selectBlip();
            sel = j;
            tab = 'rig';
            startEdit('move', e, true);
        } else if (tool === 'paint' && j >= 0) {
            e.stopImmediatePropagation();
            selectBlip();
            sel = j;
        } else if (tool === 'paint' && sel >= 0 && pf) {
            ray.setFromCamera(ndc(e), camera);
            if (ray.intersectObjects(meshes.filter(m => m.visible), false).length) {
                e.stopImmediatePropagation();
                remember();
                stroke = true;
                clearTimeout(paintWait);
                paintAt(e);
            }
        }
    }
    function onMove(e) {
        mouse = e;
        if (renderer && tool === 'paint' && inView(e)) {
            const r = host.getBoundingClientRect();
            brushAt = { x: e.clientX - r.left, y: e.clientY - r.top };
        } else brushAt = null;
        if (edit && !edit.gizmo) updateEdit(e);
        else if (stroke) paintAt(e);
    }
    function onUp(e) {
        if (stroke) {
            stroke = false;
            paintWait = setTimeout(rebuild, 200);
        }
        if (edit && edit.mmb && e.button === 1) { edit.mmb = null; return; }
        if (edit && edit.drag && !edit.gizmo) endEdit(!!downAt && Math.hypot(e.clientX - downAt[0], e.clientY - downAt[1]) > 3);
        downAt = null;
    }
    function onKey(e) {
        const tag = e.target && e.target.tagName;
        const k = e.key.toLowerCase();
        setSnaps(e);
        if ((e.ctrlKey || e.metaKey) && (k === 'z' || k === 'y') && !(tag === 'INPUT' && e.target.type === 'text') && tag !== 'TEXTAREA') {
            e.preventDefault();
            e.stopPropagation();
            undoIt(k === 'z' && !e.shiftKey);
            return;
        }
        if (tag === 'INPUT' || tag === 'SELECT' || tag === 'TEXTAREA') return;
        if (!edit && nav && nav.key(e)) { e.preventDefault(); e.stopPropagation(); return; }
        if (edit && edit.gizmo) {
            if (k === 'escape') endEdit(false);
            else return;
        } else if (edit) {
            if (k === 'control' || k === 'shift') { edit.snap = e.ctrlKey; edit.fine = e.shiftKey; updateEdit(null); return; }
            if (k === 'x' || k === 'y' || k === 'z') constrain('xyz'.indexOf(k), e.shiftKey);
            else if (k === 'enter' || k === ' ') endEdit(true);
            else if (k === 'escape') endEdit(false);
            else if (k === 'r' && edit.kind === 'turn') {
                /* R again: the trackball, from where the pointer is */
                edit.track = !edit.track;
                edit.con = null;
                if (mouse) edit.at = [mouse.clientX, mouse.clientY];
                edit.eff = null;
                edit.prev = undefined;
                updateEdit(mouse);
            }
            else if (k === 'g' || k === 'r') { const at = mouse; endEdit(false); startEdit(k === 'g' ? 'move' : 'turn', at); }
            else if (/^[0-9.,-]$/.test(e.key) || e.key === 'Backspace' || e.key === 'Tab') typeNum(e);
            else return;
        } else if (tool === 'bones' && sel >= 0 && mouse && inView(mouse) && (k === 'g' || k === 'r')) startEdit(k === 'g' ? 'move' : 'turn', mouse);
        else if (tool === 'paint' && (k === '[' || k === ']')) brush.radius = Math.min(200, Math.max(5, brush.radius + (k === ']' ? 5 : -5)));
        else return;
        e.preventDefault();
        e.stopPropagation();
    }
    function onKeyUp(e) {
        setSnaps(e);
        if (edit && !edit.gizmo && (e.key === 'Control' || e.key === 'Shift')) { edit.snap = e.ctrlKey; edit.fine = e.shiftKey; updateEdit(null); }
    }
    /* MOVE / ROTATE on the side: as G / R, from the middle of the view */
    function editButton(kind) {
        selectBlip();
        const r = renderer.domElement.getBoundingClientRect();
        startEdit(kind, mouse && inView(mouse) ? mouse : { clientX: r.left + r.width / 2, clientY: r.top + r.height / 2 });
    }

    /* BONE POSITIONS: the joint picked, moved; its other side mirrored */
    const twin = i => {
        const n = f.names[i], t = n.endsWith('.L') ? n.slice(0, -2) + '.R' : n.endsWith('.R') ? n.slice(0, -2) + '.L' : null;
        return t ? f.names.indexOf(t) : -1;
    };
    let jointWait;
    function setJoint(k, v) {
        if (sel < 0) return;
        remember('j' + sel + k);
        const d = [...(joints[sel] || [0, 0, 0])];
        d[k] = Math.min(100, Math.max(-100, Number(v) || 0));
        const next = { ...joints, [sel]: d };
        const o = mirror ? twin(sel) : -1;
        if (o >= 0) next[o] = [-d[0], d[1], d[2]];
        joints = next;
        clearTimeout(jointWait);
        jointWait = setTimeout(rebuild, 120);
    }
    async function resetJoints(all) {
        selectBlip();
        remember();
        if (all) { joints = {}; turns = {}; }
        else if (sel >= 0) {
            const next = { ...joints }, nt = { ...turns };
            for (const b of [sel, mirror ? twin(sel) : -1]) if (b >= 0) { delete next[b]; delete nt[b]; }
            joints = next;
            turns = nt;
        }
        await rebuild();
    }
    /* ROTATION X / Y / Z, as Blender's fields: the bone picked turned about
     * its joint, its children with it; the other side mirrored (about X the
     * same, about Y and Z the other way) */
    let turnWait;
    function setTurn(k, v) {
        if (sel < 0) return;
        remember('t' + sel + k);
        const d = [...(turns[sel] || [0, 0, 0])];
        d[k] = Math.min(180, Math.max(-180, Number(v) || 0));
        const next = { ...turns, [sel]: d };
        const o = mirror ? twin(sel) : -1;
        if (o >= 0) next[o] = [d[0], -d[1], -d[2]];
        turns = next;
        clearTimeout(turnWait);
        turnWait = setTimeout(rebuild, 120);
    }
    let smoothWait;
    function setSmooth(v) {
        remember('smooth');
        smooth = Number(v);
        clearTimeout(smoothWait);
        smoothWait = setTimeout(rebuild, 150);
    }
    async function setReweight(v) {
        selectBlip();
        remember();
        reweight = v;
        await rebuild();
    }

    let adjWait;
    function setAdjust(key, v) {
        remember('a' + key);
        adjust[key] = typeof v === 'boolean' ? v : Math.min(90, Math.max(-90, Number(v) || 0));
        clearTimeout(adjWait);
        adjWait = setTimeout(rebuild, 150);
    }
    const poseLine = (deg, leg) => (deg === null || deg === undefined ? '-'
        : (leg ? '' : poseName(deg) + ' · ') + Math.round(deg) + '°');

    async function setKeep(v) {
        selectBlip();
        remember();
        keep = v;
        await rebuild();
    }

    async function turnIt() {
        selectBlip();
        remember();
        turn = (turn + 1) % 4;
        await rebuild();
    }

    function apply() {
        if (!res) return;
        selectBlip();
        /* what this window was set to, kept beside the skin in the mod */
        const settings = { file: file.name, keep, scale, turn, adjust: $state.snapshot(adjust), joints: $state.snapshot(joints),
            turns: $state.snapshot(turns), reweight, smooth, paint, blockOf: $state.snapshot(blockOf), replaces: mdl.materials.map(m => m.rep || ''),
            game: mdl.materials.map(m => !!m.game), picked: mdl.materials.map(m => (m.picked ? m.file : '')) };
        const chosen = Object.fromEntries(mdl.materials.map((m, i) => [i, m.picked]).filter(([, b]) => b));
        onApply({ r: res, mdl, settings, file, picks: chosen });
    }

    /* a model opened again from its mod: as it was saved */
    async function restoreSaved() {
        const st = saved, n = mdl.materials.length;
        keep = !!st.keep; scale = st.scale || 1; turn = st.turn || 0; reweight = !!st.reweight; smooth = Number(st.smooth) || 0;
        adjust = { ...adjust, ...(st.adjust || {}) };
        joints = st.joints || {}; turns = st.turns || {};
        paint = st.paint || {}; paintCount = Object.keys(paint).length;
        if (st.blockOf) blockOf = mdl.materials.map((_, i) => Math.min(st.blockOf[i] ?? blockOf[i], f.rig.skin.length - 1));
        for (let i = 0; i < n; i++) {
            const m = mdl.materials[i];
            m.rep = (st.replaces || [])[i] || '';
            if ((st.game || [])[i]) { m.image = null; m.game = true; }
            const b = picks && picks[i];
            const pic = b && await decodePicture(b).catch(() => null);
            if (pic) { m.image = pic; m.picked = b; m.file = (st.picked || [])[i] || m.file; m.game = false; }
        }
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
            if (saved) await restoreSaved();
            await makeThumbs();
            await rebuild();
            /* a skeleton in the file: its own rig and bones (KEEP
             * PROPORTIONS), the model as it is */
            if (!saved && res && res.rig) { keep = true; await rebuild(); }
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
        window.removeEventListener('pointermove', onMove);
        window.removeEventListener('pointerup', onUp);
        window.removeEventListener('keydown', onKey, true);
        window.removeEventListener('keyup', onKeyUp, true);
        tcMove && tcMove.dispose();
        tcTurn && tcTurn.dispose();
        nav && nav.dispose();
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
                <div class="view" bind:this={host}><div class="vname">{viewName}</div></div>
                {#if editInfo}<div class="edit">{editInfo}</div>{/if}
                {#if brushAt && tool === 'paint'}<div class="ring" style="left: {brushAt.x}px; top: {brushAt.y}px; width: {brush.radius * 2}px; height: {brush.radius * 2}px"></div>{/if}
                <div class="looks" bind:this={looksEl}>
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
                            <button class="btn" onclick={() => { if (!playing && frame >= pf.clips[clip].to - pf.clips[clip].from) frame = 0; playing = !playing; }}>{playing ? tx('vwPause') : tx('vwPlay')}</button>
                            <label class="chk"><input type="checkbox" bind:checked={loop}><span>{tx('vwLoop')}</span></label>
                            <label class="chk"><input type="checkbox" bind:checked={smoothAnims}><span>{tx('vwSmoothAnims')}</span></label>
                            <input type="range" min="0" max={pf.clips[clip].to - pf.clips[clip].from} bind:value={frame}>
                            <span class="lbl">{tx('vwFrame')} {frame + 1}</span>
                            <select class="sel speed" bind:value={speed} title={tx('vwSpeed')}>
                                {#each SPEEDS as v}<option value={v}>{v}x</option>{/each}
                            </select>
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
                    <div class="pair">
                        <button class="btn" disabled={!undoCount} onclick={() => undoIt(true)} title="Ctrl+Z">{tx('imUndo')} ({undoCount})</button>
                        <button class="btn" disabled={!redoCount} onclick={() => undoIt(false)} title="Ctrl+Y">{tx('imRedo')} ({redoCount})</button>
                    </div>
                    <div class="pair">
                        <button class="btn" disabled={!res} onclick={() => cleanPose(0)}>{tx('imCleanT')}</button>
                        <button class="btn" disabled={!res} onclick={() => cleanPose(-45)}>{tx('imCleanA')}</button>
                        <button class="btn" disabled={!res} onclick={() => cleanPose(null)}>{tx('imCleanDef')}</button>
                    </div>
                    <p class="note">{tx(res && !res.rig ? 'imCleanBare' : 'imCleanNote')}</p>
                    {#if res && !res.matched}
                        <button class="btn" onclick={turnIt}>{tx('imTurn')}</button>
                    {/if}
                    {#if res && res.rig}
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
                        {#each f.names as n, i}{#if i !== f.rig.bones.root}<option value={i}>{n}{(joints[i] && joints[i].some(x => x)) || (turns[i] && turns[i].some(x => x)) ? ' *' : ''}</option>{/if}{/each}
                    </select>
                    <div class="pair">
                        <button class="btn" class:hot={tool === 'bones'} onclick={() => setTool('bones')}>{tx('imToolBones')}</button>
                        <button class="btn" class:hot={tool === 'paint'} onclick={() => setTool('paint')}>{tx('imToolPaint')}</button>
                    </div>
                    {#if tool === 'bones'}
                        <span class="lbl">{tx('imGizmo')}</span>
                        <div class="pair">
                            {#each [['move', 'imGzMove'], ['turn', 'imGzTurn'], ['both', 'imGzBoth'], ['none', 'imGzNone']] as [g, key]}
                                <button class="btn" class:hot={gizmo === g} onclick={() => { selectBlip(); gizmo = g; }}>{tx(key)}</button>
                            {/each}
                        </div>
                        <div class="pair">
                            <button class="btn" class:hot={orient === 'global'} onclick={() => { selectBlip(); orient = 'global'; }}>{tx('imGlobal')}</button>
                            <button class="btn" class:hot={orient === 'local'} onclick={() => { selectBlip(); orient = 'local'; }}>{tx('imLocal')}</button>
                        </div>
                        <div class="pair">
                            <button class="btn" disabled={sel < 0} onclick={() => editButton('move')}>{tx('imMove')}</button>
                            <button class="btn" disabled={sel < 0} onclick={() => editButton('turn')}>{tx('imRotate')}</button>
                        </div>
                        <p class="note">{tx('imBonesKeys')}</p>
                    {:else}
                        {#if sel < 0}<p class="note">{tx('imPaintPick')}</p>{/if}
                        <div class="pair">
                            <button class="btn" class:hot={brush.mode === 'add'} onclick={() => brush.mode = 'add'}>{tx('imBrushAdd')}</button>
                            <button class="btn" class:hot={brush.mode === 'sub'} onclick={() => brush.mode = 'sub'}>{tx('imBrushSub')}</button>
                            <button class="btn" class:hot={brush.mode === 'smooth'} onclick={() => brush.mode = 'smooth'}>{tx('imBrushSmooth')}</button>
                        </div>
                        <label class="scale">
                            <span class="lbl">{tx('imBrushRadius')} {brush.radius}px</span>
                            <input type="range" min="5" max="200" step="1" bind:value={brush.radius}>
                        </label>
                        <label class="scale">
                            <span class="lbl">{tx('imBrushStrength')} {brush.strength.toFixed(2)}</span>
                            <input type="range" min="0.05" max="1" step="0.05" bind:value={brush.strength}>
                        </label>
                        <button class="btn" disabled={!paintCount} onclick={clearPaint}>{tx('imPaintClear')} ({paintCount})</button>
                        <p class="note">{tx('imPaintNote')}</p>
                    {/if}
                    {#if sel >= 0 && tool === 'bones'}
                        <span class="lbl head">{tx('imLocation')}</span>
                        {#each ['X', 'Y', 'Z'] as ax, k}
                            <label class="scale">
                                <span class="lbl ax">{ax}</span>
                                <input type="range" min="-100" max="100" step="0.5" value={(joints[sel] || [0, 0, 0])[k]} oninput={e => setJoint(k, e.currentTarget.value)}>
                                <input class="num" type="number" step="0.5" value={((joints[sel] || [0, 0, 0])[k]).toFixed(1)} onchange={e => setJoint(k, e.currentTarget.value)}>
                                <button class="eye" onclick={() => setJoint(k, 0)} title="0">0</button>
                            </label>
                        {/each}
                        <span class="lbl head">{tx('imRotation')}</span>
                        {#each ['X', 'Y', 'Z'] as ax, k}
                            <label class="scale">
                                <span class="lbl ax">{ax}</span>
                                <input type="range" min="-180" max="180" step="1" value={(turns[sel] || [0, 0, 0])[k]} oninput={e => setTurn(k, e.currentTarget.value)}>
                                <input class="num" type="number" step="1" value={Math.round((turns[sel] || [0, 0, 0])[k])} onchange={e => setTurn(k, e.currentTarget.value)}>
                                <button class="eye" onclick={() => setTurn(k, 0)} title="0">0</button>
                            </label>
                        {/each}
                    {/if}
                    <label class="chk"><input type="checkbox" bind:checked={mirror}><span>{tx('imMirror')}</span></label>
                    <label class="scale" title={tx('imSmoothNote')}>
                        <span class="lbl">{tx('imSmooth')} {smooth}</span>
                        <input type="range" min="0" max="10" step="1" value={smooth} oninput={e => setSmooth(e.currentTarget.value)}>
                    </label>
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
    .scale .num { flex: 0 0 calc(var(--u) * 6.5); background: var(--well); color: #fff; border: 1px solid var(--line); border-radius: var(--r-sm); padding: 1px 4px; }
    .lbl.ax { flex: 0 0 auto; }
    .over { position: absolute; inset: 0 0 auto 0; height: 60%; display: grid; place-items: center; pointer-events: none; }
    .msg { color: var(--dim); letter-spacing: .08em; }
    .edit { position: absolute; left: 50%; top: calc(var(--u) * .6); transform: translateX(-50%); padding: 2px 10px; border-radius: var(--r-sm);
        background: rgba(10, 8, 32, .8); color: #fff; font-size: calc(var(--t) * 1.2); letter-spacing: .06em; pointer-events: none; white-space: nowrap; }
    .vname { position: absolute; left: calc(var(--u) * 1); bottom: calc(var(--u) * .6); font-size: calc(var(--t) * 1.1); color: var(--dim); pointer-events: none; letter-spacing: .06em; }
    .ring { position: absolute; transform: translate(-50%, -50%); border: 1px solid #fff; border-radius: 50%; pointer-events: none;
        box-shadow: 0 0 0 1px rgba(0, 0, 0, .5); }
    .msg.bad { color: #ff9aa0; max-width: 80%; text-align: center; }
    .info { position: absolute; left: calc(var(--u) * 1); top: calc(var(--u) * .6); font-size: calc(var(--t) * 1.3); color: var(--dim); pointer-events: none; }
    .play { display: flex; align-items: center; gap: calc(var(--u) * .9); margin-top: calc(var(--u) * .6); }
    .play .sel { flex: 0 0 calc(var(--u) * 22); }
    .play .sel.speed { flex: 0 0 calc(var(--u) * 8); }
    .play .lbl { white-space: nowrap; }
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
