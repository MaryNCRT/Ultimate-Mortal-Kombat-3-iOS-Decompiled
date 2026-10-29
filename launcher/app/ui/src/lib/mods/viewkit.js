/* What the model viewer and REPLACE MODEL's window share in their 3D view:
 * the floor under the model, how it is drawn (its textures, plain, as
 * wire, or coloured by its bones' weights) and its rig (the bones as lines,
 * the joints as dots). */
import * as THREE from 'three';

/* The floor: a grid at y 0, where the fighters stand (game units, cm) */
export function makeFloor() {
    const g = new THREE.GridHelper(1, 24, 0x7a6cff, 0x2c2866);
    for (const m of [].concat(g.material)) { m.transparent = true; m.opacity = 0.55; m.depthWrite = false; }
    g.renderOrder = -1;
    return g;
}

/* the floor under a box: as wide as three times its height, at its feet
 * (y 0 for a fighter, which stands on it) */
export function placeFloor(floor, box, onGround) {
    if (box.isEmpty()) return;
    const s = box.getSize(new THREE.Vector3());
    floor.scale.setScalar(Math.max(s.x, s.y, s.z) * 3 || 1);
    floor.position.set((box.min.x + box.max.x) / 2, onGround ? 0 : box.min.y, (box.min.z + box.max.z) / 2);
}

/* a colour a bone: hues spread so neighbours differ */
export function boneColor(b) {
    return new THREE.Color().setHSL((b * 0.618034) % 1, 0.85, 0.58);
}

/* Per vertex, its bones' colours mixed by weight -- what WEIGHTS shows.
 * weights: { idx, wt }, four a vertex (rig.js vertexWeights). */
export function weightColors(weights, n) {
    const out = new Float32Array(n * 3), cache = new Map();
    const col = b => { if (!cache.has(b)) cache.set(b, boneColor(b)); return cache.get(b); };
    for (let v = 0; v < n; v++)
        for (let k = 0; k < 4; k++) {
            const w = weights.wt[v * 4 + k];
            if (!w) continue;
            const c = col(weights.idx[v * 4 + k]);
            out[v * 3] += w * c.r; out[v * 3 + 1] += w * c.g; out[v * 3 + 2] += w * c.b;
        }
    return out;
}

/* How a material draws: look { wire, solid (no texture), weights (the
 * vertices' colours) }. Changed only when the look does. */
export function applyLook(mt, look) {
    const key = (look.wire ? 'w' : '') + (look.solid ? 's' : '') + (look.weights ? 'v' : '');
    if (mt.userData.look === key) return;
    if (mt.userData.look === undefined) { mt.userData.map0 = mt.map; mt.userData.color0 = mt.color.getHex(); }
    mt.userData.look = key;
    const plain = look.solid || look.weights;
    mt.map = plain ? null : mt.userData.map0;
    mt.color.setHex(look.weights ? 0xffffff : plain ? 0xb8b4d8 : mt.userData.color0);
    mt.vertexColors = !!look.weights;
    mt.wireframe = !!look.wire;
    mt.needsUpdate = true;
}

/* The rig drawn over the model: its bones as lines, its joints as dots,
 * over everything */
export function makeRigLines() {
    const g = new THREE.Group();
    const lines = new THREE.LineSegments(new THREE.BufferGeometry(), new THREE.LineBasicMaterial({ color: 0xff5a5a, depthTest: false, transparent: true }));
    const dots = new THREE.Points(new THREE.BufferGeometry(), new THREE.PointsMaterial({ size: 7, sizeAttenuation: false, vertexColors: true, depthTest: false, transparent: true }));
    lines.renderOrder = dots.renderOrder = 10;
    g.add(lines, dots);
    g.userData = { lines, dots };
    return g;
}

/* the rig in a pose: pose[b].T a bone's joint, parent[b] its parent */
export function poseRigLines(rig, pose, parent, on) {
    const { lines, dots } = rig.userData;
    const seg = [], pts = [], col = [];
    if (on)
        parent.forEach((par, b) => {
            if (!pose[b]) return;
            if (par >= 0 && pose[par]) seg.push(...pose[par].T, ...pose[b].T);
            pts.push(...pose[b].T);
            const c = boneColor(b);
            col.push(c.r, c.g, c.b);
        });
    lines.geometry.setAttribute('position', new THREE.Float32BufferAttribute(seg, 3));
    dots.geometry.setAttribute('position', new THREE.Float32BufferAttribute(pts, 3));
    dots.geometry.setAttribute('color', new THREE.Float32BufferAttribute(col, 3));
    lines.geometry.computeBoundingSphere();
    dots.geometry.computeBoundingSphere();
}
