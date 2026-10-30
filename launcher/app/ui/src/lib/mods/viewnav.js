/* The camera as Blender moves it, for the viewer and REPLACE MODEL's
 * window: the navigation gizmo in the corner (the axes as they turn with
 * the view -- a click on one looks down it, a drag of it orbits), the
 * numpad's views (1 front, 3 right, 7 top, with Ctrl the other side; 9 the
 * view opposite; 4 / 6 / 8 / 2 orbit by 15 degrees; 5 perspective or
 * orthographic), the top row's digits standing in for the numpad as
 * Blender's Emulate Numpad does, and Auto Perspective: an axis view is
 * orthographic, and orbiting from it goes back to perspective. The game's
 * axes: Y up, the fighter's front towards +Z. */
import * as THREE from 'three';

const AXES = [
    { v: new THREE.Vector3(1, 0, 0), name: 'X', color: '#ff3352' },
    { v: new THREE.Vector3(0, 1, 0), name: 'Y', color: '#8bdc00' },
    { v: new THREE.Vector3(0, 0, 1), name: 'Z', color: '#2890ff' },
];
/* a view looking from `dir`; straight down or up a hair off, so the
 * orbit's up stays the game's Y */
const VIEWS = {
    front: [0, 0, 1], back: [0, 0, -1], right: [1, 0, 0], left: [-1, 0, 0],
    top: [0, 1, 1e-4], bottom: [0, -1, 1e-4],
};
const KEY_VIEW = { 1: ['front', 'back'], 3: ['right', 'left'], 7: ['top', 'bottom'] };

export class ViewNav {
    /* persp the window's camera; onCamera(c) when the one drawn changes;
     * onLabel(text) the view's name ('' a free one, as Blender shows it) */
    constructor({ host, renderer, controls, persp, onCamera, onLabel, names }) {
        Object.assign(this, { host, renderer, controls, persp, onCamera, onLabel, names: names || {} });
        this.ortho = new THREE.OrthographicCamera(-1, 1, 1, -1, persp.near, persp.far);
        this.camera = persp;
        this.axisView = '';
        const c = this.gizmo = document.createElement('canvas');
        c.className = 'navgizmo';
        Object.assign(c.style, { position: 'absolute', right: '8px', top: '8px', width: '96px', height: '96px', cursor: 'pointer', zIndex: 2 });
        if (getComputedStyle(host).position === 'static') host.style.position = 'relative';
        host.appendChild(c);
        c.addEventListener('pointerdown', e => this.gizmoDown(e));
        /* orbited by hand off an axis view (panning and zooming keep it):
         * back to perspective */
        controls.addEventListener('change', this.changeFn = () => {
            if (!this.axisView || this.snapping) return;
            const d = this.camera.position.clone().sub(this.target).normalize();
            if (d.angleTo(new THREE.Vector3(...VIEWS[this.axisView]).normalize()) > 0.01) this.free();
        });
    }
    dispose() {
        this.controls.removeEventListener('change', this.changeFn);
        this.gizmo.remove();
    }
    get target() { return this.controls.target; }

    /* the camera drawn: perspective or orthographic, at the same view */
    use(cam) {
        if (cam === this.camera) return;
        const t = this.target, old = this.camera;
        const dir = old.position.clone().sub(t).normalize();
        const tanH = Math.tan(this.persp.fov * Math.PI / 360);
        if (cam.isOrthographicCamera) {
            const half = old.position.distanceTo(t) * tanH;
            this.ortho.userData.half = half;
            this.ortho.zoom = 1;
            this.ortho.near = -this.persp.far;
            this.ortho.far = this.persp.far;
            this.ortho.position.copy(old.position);
            this.ortho.quaternion.copy(old.quaternion);
            this.ortho.up.copy(old.up);
        } else {
            const half = (this.ortho.userData.half || 1) / (this.ortho.zoom || 1);
            this.persp.position.copy(t).addScaledVector(dir, half / tanH);
            this.persp.quaternion.copy(old.quaternion);
        }
        this.camera = cam;
        this.resize();
        this.controls.object = cam;
        this.snapping = true;
        this.controls.update();
        this.snapping = false;
        this.onCamera && this.onCamera(cam);
    }
    resize() {
        const w = this.host.clientWidth, h = Math.max(1, this.host.clientHeight);
        if (this.camera.isOrthographicCamera) {
            const half = this.ortho.userData.half || 1;
            Object.assign(this.ortho, { left: -half * w / h, right: half * w / h, top: half, bottom: -half });
        } else this.persp.aspect = w / h;
        this.camera.updateProjectionMatrix();
    }
    /* the window framed again: the orthographic camera's size follows */
    reframed() {
        if (this.camera.isOrthographicCamera) { this.use(this.persp); }
        this.ortho.near = -this.persp.far;
        this.ortho.far = this.persp.far;
    }

    /* look from one side, orthographic (Auto Perspective) */
    view(name) {
        const t = this.target, cam = this.camera;
        const dist = cam.isOrthographicCamera ? (this.ortho.userData.half / this.ortho.zoom) / Math.tan(this.persp.fov * Math.PI / 360) : cam.position.distanceTo(t);
        if (cam.isOrthographicCamera) this.use(this.persp);
        this.persp.position.copy(t).addScaledVector(new THREE.Vector3(...VIEWS[name]).normalize(), dist);
        this.persp.lookAt(t);
        this.snapping = true;
        this.controls.update();
        this.snapping = false;
        this.use(this.ortho);
        this.axisView = name;
        this.label();
    }
    free() {
        this.axisView = '';
        if (this.camera.isOrthographicCamera && this.autoPersp) this.use(this.persp);
        this.autoPersp = false;
        this.label();
    }
    label() {
        const n = this.names;
        const v = this.axisView ? (n[this.axisView] || this.axisView) : (n.user || 'USER');
        this.onLabel && this.onLabel(v + ' · ' + (this.camera.isOrthographicCamera ? (n.ortho || 'ORTHOGRAPHIC') : (n.persp || 'PERSPECTIVE')));
    }
    /* numpad 5 */
    toggle() {
        this.use(this.camera.isOrthographicCamera ? this.persp : this.ortho);
        this.autoPersp = false;
        this.label();
    }
    /* about the target: yaw about the game's Y, pitch about the view's side */
    orbit(yaw, pitch) {
        const t = this.target, cam = this.camera;
        const off = cam.position.clone().sub(t);
        off.applyAxisAngle(new THREE.Vector3(0, 1, 0), yaw);
        const side = new THREE.Vector3().crossVectors(new THREE.Vector3(0, 1, 0), off).normalize();
        const turned = off.clone().applyAxisAngle(side, pitch);
        if (Math.abs(turned.clone().normalize().y) < 0.9999) off.copy(turned);
        cam.position.copy(t).add(off);
        cam.lookAt(t);
        this.snapping = true;
        this.controls.update();
        this.snapping = false;
        this.axisView = '';
        if (this.camera.isOrthographicCamera && this.autoPersp) this.use(this.persp);
        this.label();
    }

    /* Blender's numpad keys; true when the key was one */
    key(e) {
        const m = /^(?:Numpad|Digit)(\d)$/.exec(e.code);
        if (!m) return false;
        const d = Number(m[1]), ctrl = e.ctrlKey || e.metaKey;
        const step = Math.PI / 12;
        if (KEY_VIEW[d]) { this.view(KEY_VIEW[d][ctrl ? 1 : 0]); this.autoPersp = true; }
        else if (d === 9) {
            const opp = { front: 'back', back: 'front', right: 'left', left: 'right', top: 'bottom', bottom: 'top' };
            if (this.axisView) { this.view(opp[this.axisView]); this.autoPersp = true; }
            else this.orbit(Math.PI, 0);
        } else if (d === 5) this.toggle();
        else if (d === 4) this.orbit(-step, 0);
        else if (d === 6) this.orbit(step, 0);
        else if (d === 8) this.orbit(0, -step);
        else if (d === 2) this.orbit(0, step);
        else return false;
        return true;
    }

    /* the gizmo: each axis as the view has it, the far ones first */
    ends() {
        const q = this.camera.quaternion.clone().invert(), out = [];
        for (const a of AXES)
            for (const s of [1, -1]) {
                const v = a.v.clone().multiplyScalar(s).applyQuaternion(q);
                out.push({ a, s, x: v.x, y: v.y, z: v.z });
            }
        return out.sort((p, r) => p.z - r.z);
    }
    draw() {
        const c = this.gizmo, dpr = window.devicePixelRatio || 1, W = 96;
        if (c.width !== W * dpr) { c.width = c.height = W * dpr; }
        const g = c.getContext('2d');
        g.setTransform(dpr, 0, 0, dpr, 0, 0);
        g.clearRect(0, 0, W, W);
        const R = 34, cx = W / 2, cy = W / 2;
        if (this.hover) { g.fillStyle = 'rgba(255,255,255,.12)'; g.beginPath(); g.arc(cx, cy, W / 2 - 1, 0, Math.PI * 2); g.fill(); }
        for (const p of this.ends()) {
            const x = cx + p.x * R, y = cy - p.y * R;
            if (p.s > 0) {
                g.strokeStyle = p.a.color; g.lineWidth = 2;
                g.beginPath(); g.moveTo(cx, cy); g.lineTo(x, y); g.stroke();
                g.fillStyle = p.a.color;
                g.beginPath(); g.arc(x, y, 9, 0, Math.PI * 2); g.fill();
                g.fillStyle = '#000'; g.font = 'bold 11px sans-serif'; g.textAlign = 'center'; g.textBaseline = 'middle';
                g.fillText(p.a.name, x, y + 0.5);
            } else {
                g.fillStyle = p.a.color + '55'; g.strokeStyle = p.a.color; g.lineWidth = 1.5;
                g.beginPath(); g.arc(x, y, 7, 0, Math.PI * 2); g.fill(); g.stroke();
            }
            p.px = x; p.py = y;
        }
    }
    gizmoDown(e) {
        e.preventDefault();
        e.stopPropagation();
        const r = this.gizmo.getBoundingClientRect(), x0 = e.clientX, y0 = e.clientY;
        let last = [x0, y0], dragged = false;
        const move = ev => {
            if (Math.hypot(ev.clientX - x0, ev.clientY - y0) > 3) dragged = true;
            if (!dragged) return;
            this.orbit(-(ev.clientX - last[0]) * 0.01, -(ev.clientY - last[1]) * 0.01);
            this.autoPersp = true;
            last = [ev.clientX, ev.clientY];
        };
        const up = ev => {
            window.removeEventListener('pointermove', move);
            window.removeEventListener('pointerup', up);
            if (dragged) return;
            const px = ev.clientX - r.left, py = ev.clientY - r.top;
            let best = null, bd = 12;
            for (const p of this.ends().reverse()) {
                const d = Math.hypot(p.px - px, p.py - py);
                if (p.px !== undefined && d < bd) { bd = d; best = p; }
            }
            if (!best) return;
            const n = { X: ['right', 'left'], Y: ['top', 'bottom'], Z: ['front', 'back'] }[best.a.name][best.s > 0 ? 0 : 1];
            /* the axis already looked down: the other side, as Blender */
            this.view(this.axisView === n ? { right: 'left', left: 'right', top: 'bottom', bottom: 'top', front: 'back', back: 'front' }[n] : n);
            this.autoPersp = true;
        };
        window.addEventListener('pointermove', move);
        window.addEventListener('pointerup', up);
    }
}
