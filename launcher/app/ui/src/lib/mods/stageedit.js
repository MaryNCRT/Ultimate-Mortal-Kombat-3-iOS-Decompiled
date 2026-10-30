/* STAGE VIEWER's edits: an element of a stage moved, turned, scaled and
 * reshaped on top of what the engine does with it, keyed on frames as in
 * Blender. The same numbers drive the viewer (StageView's `edit` hook), the
 * model editor and the export (stagerun.js stageExport).
 *
 *   edits: { v: 1, els: { [draw key]: El } }       one file a stage
 *   El: { name, pivot: [x y z] (the mesh's middle, its own space),
 *         base: { T, R (degrees, X then Y then Z), S }   with no keys
 *         keys: [{ f, T, R, S, i: 'linear' | 'bezier' }]
 *         basis: { vertex: [dx dy dz] }                  the Basis reshaped
 *         shapes: [{ name, value, mute, d: { vertex: [dx dy dz] },
 *                    keys: [{ f, v, i }] }] }
 *
 * As the CORRECTIVE ANIMATION LAYERS (corrective.js): the edit is put on top
 * of the element's own animation, before the first key and after the last
 * it holds. A key's `i` is how it goes on to the next one: straight
 * (linear) or Blender's automatic clamped bezier (an ease, flat at a turn).
 * Frames are the stage's own (1..its length), as its timeline shows them.
 *
 * The placement: the mesh about its pivot, moved by T, turned by R, scaled
 * by S, in its own space, then where the engine puts it:
 *   matrix = engine * T(pivot) * compose(T, R, S) * T(-pivot)
 * The shape: the vertex + the Basis' delta + each key's delta times its value. */
import * as THREE from 'three';
import { TransformControls } from 'three/addons/controls/TransformControls.js';

const DEG = Math.PI / 180;
export const blankTrs = () => ({ T: [0, 0, 0], R: [0, 0, 0], S: [1, 1, 1] });

export function newEl(name, pos) {
    const n = pos.length / 3, lo = [Infinity, Infinity, Infinity], hi = [-Infinity, -Infinity, -Infinity];
    for (let i = 0; i < n; i++) for (let k = 0; k < 3; k++) { lo[k] = Math.min(lo[k], pos[i * 3 + k]); hi[k] = Math.max(hi[k], pos[i * 3 + k]); }
    return { name, pivot: n ? lo.map((v, k) => (v + hi[k]) / 2) : [0, 0, 0], base: blankTrs(), keys: [], basis: {}, shapes: [] };
}

/* the stage's frame on tick t (the timeline loops with the stage) */
export const frameOf = (t, length) => ((Math.max(1, t) - 1) % Math.max(1, length)) + 1;

/* a curve through keys sorted by f; get(key) its number */
export function curve(keys, f, get) {
    const n = keys.length;
    if (f <= keys[0].f) return get(keys[0]);
    if (f >= keys[n - 1].f) return get(keys[n - 1]);
    let i = 0;
    while (keys[i + 1].f <= f) i++;
    const a = keys[i], b = keys[i + 1], h = b.f - a.f, t = (f - a.f) / h, va = get(a), vb = get(b);
    if (a.i === 'linear') return va + (vb - va) * t;
    /* auto clamped: the slope through the neighbours, flat at an end or a turn */
    const slope = j => {
        if (j <= 0 || j >= n - 1) return 0;
        const p = get(keys[j - 1]), c = get(keys[j]), q = get(keys[j + 1]);
        return (c - p) * (q - c) <= 0 ? 0 : (q - p) / (keys[j + 1].f - keys[j - 1].f);
    };
    const ma = slope(i) * h, mb = slope(i + 1) * h, t2 = t * t, t3 = t2 * t;
    return (2 * t3 - 3 * t2 + 1) * va + (t3 - 2 * t2 + t) * ma + (-2 * t3 + 3 * t2) * vb + (t3 - t2) * mb;
}

/* the placement on frame f: { T, R, S } */
export function trsAt(el, f) {
    if (el.pending && el.pending.f === f) return el.pending.trs;
    if (!el.keys.length) return el.base;
    const out = blankTrs();
    for (const c of ['T', 'R', 'S']) for (let k = 0; k < 3; k++) out[c][k] = curve(el.keys, f, x => x[c][k]);
    return out;
}
export function shapeAt(sh, f) {
    if (sh.pending && sh.pending.f === f) return sh.pending.v;
    return sh.keys.length ? curve(sh.keys, f, x => x.v) : sh.value;
}

export function trsMatrix({ T, R, S }) {
    return new THREE.Matrix4().compose(new THREE.Vector3(...T), new THREE.Quaternion().setFromEuler(new THREE.Euler(R[0] * DEG, R[1] * DEG, R[2] * DEG, 'XYZ')), new THREE.Vector3(...S));
}
/* T(pivot) * TRS * T(-pivot), three's column form (its array is the
 * engine's row form) */
export function localMatrix(el, f) {
    const p = el.pivot;
    return new THREE.Matrix4().makeTranslation(p[0], p[1], p[2]).multiply(trsMatrix(trsAt(el, f))).multiply(new THREE.Matrix4().makeTranslation(-p[0], -p[1], -p[2]));
}
export const isIdentity = t => t.T.every(v => Math.abs(v) < 1e-7) && t.R.every(v => Math.abs(v) < 1e-7) && t.S.every(v => Math.abs(v - 1) < 1e-7);

export const hasShape = el => Object.keys(el.basis).length > 0 || el.shapes.some(s => Object.keys(s.d).length);

/* the vertices on frame f; `solo` a shape key's index: it alone, at 1 (the
 * one being sculpted, as Blender's edit mode shows it), -1 the Basis alone */
export function shapedPositions(el, orig, f, solo) {
    const out = Float32Array.from(orig);
    const add = (d, w) => {
        if (!w) return;
        for (const [v, x] of Object.entries(d)) { const o = v * 3; out[o] += x[0] * w; out[o + 1] += x[1] * w; out[o + 2] += x[2] * w; }
    };
    add(el.basis, 1);
    if (solo === undefined) el.shapes.forEach(s => { if (!s.mute) add(s.d, shapeAt(s, f)); });
    else if (solo >= 0 && el.shapes[solo]) add(el.shapes[solo].d, 1);
    return out;
}

/* a key put in a sorted list, or replacing the one on its frame */
export function putKey(keys, k) {
    const i = keys.findIndex(x => x.f === k.f);
    if (i >= 0) keys[i] = k;
    else { keys.push(k); keys.sort((a, b) => a.f - b.f); }
}
export function dropKey(keys, f) {
    const i = keys.findIndex(x => x.f === f);
    if (i >= 0) keys.splice(i, 1);
    return i >= 0;
}
export const copyTrs = t => ({ T: [...t.T], R: [...t.R], S: [...t.S] });

/* the edits to keep: no pending values, numbers rounded, empty elements out */
export function saved(edits) {
    const r = v => +v.toFixed(5), rd = d => Object.fromEntries(Object.entries(d).map(([k, x]) => [k, x.map(r)]).filter(([, x]) => x.some(Boolean)));
    const els = {};
    for (const [k, el] of Object.entries(edits.els)) {
        const out = { name: el.name, pivot: el.pivot.map(r), base: copyTrs(el.base), keys: el.keys.map(x => ({ f: x.f, T: x.T.map(r), R: x.R.map(r), S: x.S.map(r), i: x.i })),
            basis: rd(el.basis), shapes: el.shapes.map(s => ({ name: s.name, value: r(s.value), mute: !!s.mute, d: rd(s.d), keys: s.keys.map(x => ({ f: x.f, v: r(x.v), i: x.i })) })) };
        if (out.keys.length || !isIdentity(out.base) || out.shapes.length || Object.keys(out.basis).length) els[k] = out;
    }
    return { v: 1, els };
}

/* The edits on a StageView: view.edit = edits.hook(() => frame). The
 * reshaped elements get geometries of their own, made again only when a
 * value changes. */
export class EditCache {
    constructor() { this.geo = new Map(); }       /* key -> { base, g, sig } */
    apply(el, f, mesh, key) {
        const own = this.geo.get(key);
        if (!el) {
            if (own && mesh.geometry === own.g) mesh.geometry = own.base;
            return;
        }
        mesh.matrix.multiply(localMatrix(el, f));
        if (!hasShape(el)) { if (own && mesh.geometry === own.g) mesh.geometry = own.base; return; }
        let e = own;
        if (!e || (mesh.geometry !== e.g && mesh.geometry !== e.base)) {
            if (e) e.g.dispose();
            const base = mesh.geometry;
            e = { base, g: base.clone(), sig: '' };
            this.geo.set(key, e);
        }
        const sig = el.rev + '|' + el.shapes.map(s => (s.mute ? 0 : shapeAt(s, f))).join(',');
        if (sig !== e.sig) {
            e.sig = sig;
            e.g.attributes.position.array.set(shapedPositions(el, e.base.attributes.position.array, f));
            e.g.attributes.position.needsUpdate = true;
            e.g.computeBoundingSphere();
        }
        mesh.geometry = e.g;
    }
    dispose() { for (const e of this.geo.values()) e.g.dispose(); this.geo.clear(); }
}

/* ------------------------------------------------------------ the gizmo --
 * The placement of an element, moved by hand: Blender's G / R / S (from the
 * pointer; X / Y / Z an axis, again the element's own, again none; Shift
 * with it the plane without it; typed numbers, Tab the next field; a click,
 * Enter or Space keeps it, Esc or the right button drops it) and the
 * TransformControls handles (MOVE / ROTATE / SCALE).
 *
 * `base` stands where the pivot is (engine * T(pivot)), the proxy in it is
 * the T, R, S being set. onChange(trs) while it moves, onStart() before,
 * onEnd(kept) after; info(text) the line that says what is being done. */

const AX = [new THREE.Vector3(1, 0, 0), new THREE.Vector3(0, 1, 0), new THREE.Vector3(0, 0, 1)];
const AXCOL = [0xff5060, 0x80e040, 0x5090ff];

export class Gizmo {
    constructor({ scene, dom, camera, controls, onChange, onStart, onEnd, info }) {
        Object.assign(this, { scene, dom, cam: camera, controls, onChange, onStart, onEnd, info: info || (() => {}) });
        this.base = new THREE.Object3D();
        this.base.matrixAutoUpdate = false;
        this.proxy = new THREE.Object3D();
        this.base.add(this.proxy);
        scene.add(this.base);
        this.tc = new TransformControls(camera(), dom);
        this.tc.size = 0.9;
        this.tc.setSpace('local');
        this.helper = this.tc.getHelper();
        scene.add(this.helper);
        this.tc.addEventListener('dragging-changed', e => { controls.enabled = !e.value; });
        this.tc.addEventListener('mouseDown', () => this.onStart());
        this.tc.addEventListener('objectChange', () => this.onChange(this.trs()));
        this.tc.addEventListener('mouseUp', () => this.onEnd(true));
        this.tc.enabled = false;
        this.helper.visible = false;
        this.line = new THREE.LineSegments(new THREE.BufferGeometry(), new THREE.LineBasicMaterial({ vertexColors: true, depthTest: false, transparent: true }));
        this.line.renderOrder = 1e7;
        this.line.visible = false;
        scene.add(this.line);
        this.m = null;
        this.mouse = null;
        this.tool = '';
        this._down = e => this.pointerDown(e);
        this._move = e => { this.mouse = e; if (this.m) this.update(); };
        dom.addEventListener('pointerdown', this._down, { capture: true });
        dom.addEventListener('contextmenu', this._ctx = e => { if (this.m) e.preventDefault(); });
        window.addEventListener('pointermove', this._move);
    }
    get active() { return !!this.m || this.tc.dragging; }
    /* where the element stands: the base's matrix, its T R S */
    attach(baseMatrix, trs) {
        this.base.matrix.copy(baseMatrix);
        this.base.matrixWorldNeedsUpdate = true;
        if (!this.m && !this.tc.dragging) this.set(trs);
        this.base.updateMatrixWorld(true);
        this.on = true;
        this.showTool();
    }
    detach() { this.on = false; if (this.m) this.end(false); this.showTool(); }
    set(trs) {
        this.proxy.position.set(...trs.T);
        this.proxy.rotation.set(trs.R[0] * DEG, trs.R[1] * DEG, trs.R[2] * DEG, 'XYZ');
        this.proxy.scale.set(...trs.S);
    }
    trs() {
        const p = this.proxy;
        return { T: p.position.toArray(), R: [p.rotation.x, p.rotation.y, p.rotation.z].map(v => v / DEG), S: p.scale.toArray() };
    }
    /* the handles: '' none, 'translate', 'rotate', 'scale' */
    setTool(t) { this.tool = t; this.showTool(); }
    showTool() {
        const on = this.on && !!this.tool;
        this.tc.camera = this.cam();
        if (on) { this.tc.setMode(this.tool); if (this.tc.object !== this.proxy) this.tc.attach(this.proxy); }
        else if (this.tc.object) this.tc.detach();
        this.tc.enabled = on;
        this.helper.visible = on;
    }
    setSpace(s) { this.tc.setSpace(s); }

    ndc(e) {
        const r = this.dom.getBoundingClientRect();
        return new THREE.Vector2(((e.clientX - r.left) / r.width) * 2 - 1, -((e.clientY - r.top) / r.height) * 2 + 1);
    }
    inView(e) {
        const r = this.dom.getBoundingClientRect();
        return e && e.clientX >= r.left && e.clientX <= r.right && e.clientY >= r.top && e.clientY <= r.bottom;
    }
    screenOf(v) {
        const r = this.dom.getBoundingClientRect(), p = v.clone().project(this.cam());
        return [r.left + (p.x + 1) / 2 * r.width, r.top + (1 - p.y) / 2 * r.height];
    }
    /* the pointer on the plane through the pivot facing the camera */
    onPlane(e, P) {
        const cam = this.cam(), ray = new THREE.Raycaster();
        ray.setFromCamera(this.ndc(e), cam);
        const n = cam.getWorldDirection(new THREE.Vector3());
        const hit = ray.ray.intersectPlane(new THREE.Plane().setFromNormalAndCoplanarPoint(n, P), new THREE.Vector3());
        return hit || P.clone();
    }

    /* G / R / S: 'move' | 'turn' | 'scale', from the pointer */
    start(kind) {
        if (!this.on) return false;
        const e = this.inView(this.mouse) ? this.mouse : (() => { const r = this.dom.getBoundingClientRect(); return { clientX: r.left + r.width / 2, clientY: r.top + r.height / 2 }; })();
        this.proxy.updateMatrixWorld(true);
        const P = new THREE.Vector3().setFromMatrixPosition(this.proxy.matrixWorld);
        this.onStart();
        this.m = { kind, at: e, P, W0: this.proxy.matrixWorld.clone(), pos: this.proxy.position.clone(), quat: this.proxy.quaternion.clone(), scl: this.proxy.scale.clone(), con: null, num: null };
        this.m.hit0 = this.onPlane(e, P);
        const c = this.screenOf(P);
        this.m.c = c;
        this.m.a0 = Math.atan2(e.clientY - c[1], e.clientX - c[0]);
        this.m.d0 = Math.hypot(e.clientX - c[0], e.clientY - c[1]) || 1;
        this.update();
        return true;
    }
    axis(con) {
        if (!con.local) return AX[con.i].clone();
        const q = new THREE.Quaternion();
        this.m.W0.decompose(new THREE.Vector3(), q, new THREE.Vector3());
        return AX[con.i].clone().applyQuaternion(q).normalize();
    }
    nums() {
        const n = this.m.num;
        return n ? n.f.map(s => { const v = parseFloat(s); return Number.isFinite(v) ? v : null; }) : null;
    }
    update() {
        const m = this.m, e = this.inView(this.mouse) ? this.mouse : m.at, con = m.con, nums = this.nums();
        const fmt = v => (Math.abs(v) < 5e-5 ? 0 : v).toFixed(3);
        const conText = con ? ' · ' + (con.plane && m.kind !== 'turn' ? 'XYZ'.replace('XYZ'[con.i], '') : 'XYZ'[con.i]) + (con.local ? ' (LOCAL)' : ' (GLOBAL)') : '';
        const show = i => (m.num ? (m.num.k === i ? '[' + m.num.f[i] + '|]' : m.num.f[i] || '0') : null);
        let text;
        if (m.kind === 'move') {
            let d;
            if (nums) {
                d = new THREE.Vector3();
                if (con && !con.plane) d.addScaledVector(this.axis(con), nums[0] ?? 0);
                else [0, 1, 2].forEach(i => d.addScaledVector(con ? this.axis({ i, local: con.local }) : AX[i], (con && i === con.i) ? 0 : nums[i] ?? 0));
            } else {
                d = this.onPlane(e, m.P).sub(m.hit0);
                if (con) {
                    const a = this.axis(con);
                    if (con.plane) d.addScaledVector(a, -d.dot(a));
                    else {
                        /* along the axis: the pointer's screen motion projected on the axis' screen direction */
                        const s0 = this.screenOf(m.P), s1 = this.screenOf(m.P.clone().add(a));
                        const ax = s1[0] - s0[0], ay = s1[1] - s0[1], l2 = ax * ax + ay * ay || 1;
                        d = a.multiplyScalar(((e.clientX - m.at.clientX) * ax + (e.clientY - m.at.clientY) * ay) / l2);
                    }
                }
            }
            this.world(new THREE.Matrix4().makeTranslation(d.x, d.y, d.z).multiply(m.W0));
            text = 'MOVE ' + (m.num ? [0, 1, 2].map(show).join(' | ') : [d.x, d.y, d.z].map(fmt).join(' | '));
        } else if (m.kind === 'turn') {
            const cam = this.cam();
            let ang = Math.atan2(e.clientY - m.c[1], e.clientX - m.c[0]) - m.a0;
            let u = con ? this.axis(con) : cam.getWorldDirection(new THREE.Vector3()).negate();
            /* the screen turns clockwise as y grows down: about the axis toward the eye */
            const toEye = cam.position.clone().sub(m.P);
            if (u.dot(toEye) < 0) ang = -ang;
            ang = -ang;
            if (nums) { ang = (nums[0] ?? 0) * DEG; if (con) u = this.axis(con); }
            const P = m.P;
            this.world(new THREE.Matrix4().makeTranslation(P.x, P.y, P.z).multiply(new THREE.Matrix4().makeRotationAxis(u, ang))
                .multiply(new THREE.Matrix4().makeTranslation(-P.x, -P.y, -P.z)).multiply(m.W0));
            text = 'ROTATE ' + (m.num ? show(0) : fmt(ang / DEG)) + '°';
        } else {
            let f = Math.hypot(e.clientX - m.c[0], e.clientY - m.c[1]) / m.d0;
            if (nums) f = nums[0] ?? 1;
            const s = m.scl.clone();
            if (con && con.plane) [0, 1, 2].forEach(i => { if (i !== con.i) s.setComponent(i, s.getComponent(i) * f); });
            else if (con) s.setComponent(con.i, s.getComponent(con.i) * f);
            else s.multiplyScalar(f);
            this.proxy.position.copy(m.pos);
            this.proxy.quaternion.copy(m.quat);
            this.proxy.scale.copy(s);
            text = 'SCALE ' + (m.num ? show(0) : fmt(f));
        }
        this.proxy.updateMatrixWorld(true);
        this.showLine();
        this.info(text + conText);
        this.onChange(this.trs());
    }
    /* a world matrix for the proxy: into the base's space, its scale kept */
    world(W) {
        const L = this.base.matrixWorld.clone().invert().multiply(W);
        const p = new THREE.Vector3(), q = new THREE.Quaternion(), s = new THREE.Vector3();
        L.decompose(p, q, s);
        this.proxy.position.copy(p);
        this.proxy.quaternion.copy(q);
        this.proxy.scale.copy(this.m.scl);
    }
    showLine() {
        const c = this.m && this.m.con;
        this.line.visible = !!c;
        if (!c) return;
        const cam = this.cam(), L = cam.isOrthographicCamera ? (cam.right - cam.left) / cam.zoom * 4 : cam.position.distanceTo(this.m.P) * 4;
        const pts = [], cols = [];
        for (const i of c.plane && this.m.kind !== 'turn' ? [0, 1, 2].filter(i => i !== c.i) : [c.i]) {
            const a = this.axis({ i, local: c.local }), col = new THREE.Color(AXCOL[i]);
            pts.push(this.m.P.clone().addScaledVector(a, -L), this.m.P.clone().addScaledVector(a, L));
            cols.push(col.r, col.g, col.b, col.r, col.g, col.b);
        }
        this.line.geometry.dispose();
        this.line.geometry = new THREE.BufferGeometry().setFromPoints(pts);
        this.line.geometry.setAttribute('color', new THREE.Float32BufferAttribute(cols, 3));
    }
    end(keep) {
        const m = this.m;
        if (!m) return;
        if (!keep) {
            this.proxy.position.copy(m.pos);
            this.proxy.quaternion.copy(m.quat);
            this.proxy.scale.copy(m.scl);
            this.onChange(this.trs());
        }
        this.m = null;
        this.line.visible = false;
        this.info('');
        this.onEnd(keep);
    }
    pointerDown(e) {
        if (!this.m) return false;
        e.stopImmediatePropagation();
        e.preventDefault();
        this.end(e.button === 0);
        return true;
    }
    /* a key for the gizmo: true when it took it */
    key(e) {
        const k = e.key.toLowerCase();
        if (!this.m) {
            if (e.ctrlKey || e.altKey || e.metaKey || !this.on) return false;
            if (k === 'g') return this.start('move');
            if (k === 'r') return this.start('turn');
            if (k === 's') return this.start('scale');
            return false;
        }
        const m = this.m;
        if (k === 'x' || k === 'y' || k === 'z') {
            const i = 'xyz'.indexOf(k), plane = e.shiftKey, c = m.con;
            if (c && c.i === i && c.plane === plane) m.con = c.local ? null : { i, plane, local: true };
            else m.con = { i, plane, local: false };
        } else if (k === 'enter' || k === ' ') { this.end(true); return true; }
        else if (k === 'escape') { this.end(false); return true; }
        else if (k === 'g' || k === 'r' || k === 's') {
            const kind = k === 'g' ? 'move' : k === 'r' ? 'turn' : 'scale';
            if (kind !== m.kind) { this.end(false); this.start(kind); }
            return true;
        } else if (/^[0-9.,-]$/.test(e.key) || k === 'backspace' || k === 'tab') {
            if (!m.num) { if (k === 'backspace') return true; m.num = { f: ['', '', ''], k: 0 }; }
            const n = m.num, cnt = m.kind === 'move' && !(m.con && !m.con.plane) ? 3 : 1;
            if (k === 'tab') n.k = (n.k + (e.shiftKey ? cnt - 1 : 1)) % cnt;
            else if (k === 'backspace') { if (n.f[n.k]) n.f[n.k] = n.f[n.k].slice(0, -1); else if (!n.f.some(Boolean)) m.num = null; }
            else if (e.key === '-') n.f[n.k] = n.f[n.k].startsWith('-') ? n.f[n.k].slice(1) : '-' + n.f[n.k];
            else if (e.key === '.' || e.key === ',') { if (!n.f[n.k].includes('.')) n.f[n.k] += '.'; }
            else n.f[n.k] += e.key;
            if (m.num && m.num.k >= cnt) m.num.k = 0;
        } else return k !== 'control' && k !== 'shift' && k !== 'alt' ? true : false;
        this.update();
        return true;
    }
    dispose() {
        this.dom.removeEventListener('pointerdown', this._down, { capture: true });
        this.dom.removeEventListener('contextmenu', this._ctx);
        window.removeEventListener('pointermove', this._move);
        this.tc.detach();
        this.tc.dispose();
        this.scene.remove(this.base, this.helper, this.line);
        this.line.geometry.dispose();
    }
}
