/* CORRECTIVE ANIMATION LAYERS: turns put on top of the game's animations,
 * a bone at a time, keyed on frames -- an elbow through the body moved out,
 * a custom model's arm kept off its hips.
 *
 *   layers: [{ name, clip (an index into the fighter's clips, or -1 for all
 *              of them), on, weight (0..1),
 *              keys: { bone: [{ f, r: [x, y, z] }] } }]
 *
 * A key is a turn in degrees about the bone's own X, Y and Z (in that order)
 * at frame f of the clip; between keys the turn goes straight from one to
 * the next, before the first and after the last it holds. A layer on every
 * clip has no frames: its bone's first key holds throughout. The layers on
 * a frame add up, each times its weight.
 *
 * The turn is put on the bone's own rotation as the .skinanim stores it --
 * each frame a quaternion per bone, relative to its parent -- so the
 * children follow, and the same arithmetic bakes the file (bakeLayers) that
 * the viewer shows (withLayers). Saved in the mod as the .skinanim, beside
 * it the layers (<fighter>.layers.json) and the animation they were put on
 * (<fighter>.skinanim.orig), so they can be changed and baked again. */
import { axisAngle, mul3, quatToMat, matToQuat } from './rig.js';

const DEG = Math.PI / 180;

/* degrees about X, then Y, then Z, as a row-form rotation */
export function eulerMat(r) {
    return mul3(mul3(axisAngle([1, 0, 0], r[0] * DEG), axisAngle([0, 1, 0], r[1] * DEG)), axisAngle([0, 0, 1], r[2] * DEG));
}

/* a bone's keys at frame k: [x, y, z], or null without keys */
export function keyAt(keys, k, all) {
    if (!keys || !keys.length) return null;
    if (all || k <= keys[0].f) return keys[0].r;
    const last = keys[keys.length - 1];
    if (k >= last.f) return last.r;
    let i = 0;
    while (i + 1 < keys.length && keys[i + 1].f <= k) i++;
    const a = keys[i], b = keys[i + 1], t = (k - a.f) / ((b.f - a.f) || 1);
    return a.r.map((v, c) => v + (b.r[c] - v) * t);
}

/* the turns on clip ci's frame k: bone -> [x, y, z], summed over the layers
 * on it by weight; `extra` (bone, r) a turn being set, not keyed yet */
export function turnsAt(layers, ci, k, extra) {
    const out = new Map();
    const add = (b, r, w) => {
        const cur = out.get(b) || [0, 0, 0];
        out.set(b, cur.map((v, c) => v + r[c] * w));
    };
    for (const L of layers || []) {
        if (!L.on || (L.clip >= 0 && L.clip !== ci)) continue;
        for (const [b, keys] of Object.entries(L.keys)) {
            const r = keyAt(keys, k, L.clip < 0);
            if (r) add(Number(b), r, L.weight ?? 1);
        }
    }
    if (extra) out.set(extra.bone, extra.r);
    return out;
}

/* one frame's quaternions (x y z w per bone, the animation's bone order)
 * with the turns: get(n) and set(n, q) read and write the n-th */
function turnFrame(order, nb, turns, get, set) {
    order.forEach((i, n) => {
        if (n >= nb) return;
        const t = turns.get(i);
        if (!t || !t.some(x => x)) return;
        const q = get(n);
        set(n, matToQuat(mul3(eulerMat(t), quatToMat(q[0], q[1], q[2], q[3]))));
    });
}

/* parseAnim's animation with the layers on: a copy of its quaternions,
 * for the viewer. clips: the fighter's ({ from, to }) */
export function withLayers(anim, bones, layers, clips, extra) {
    if ((!layers || !layers.length) && !extra) return anim;
    const quat = Float32Array.from(anim.quat), nb = anim.bones;
    clips.forEach((c, ci) => {
        for (let k = 0; k <= c.to - c.from; k++) {
            const turns = turnsAt(layers, ci, k, extra && extra.clip === ci && extra.frame === k ? extra : null);
            if (!turns.size) continue;
            const f = c.from + k;
            turnFrame(bones.order, nb, turns,
                n => Array.from(quat.subarray((f * nb + n) * 4, (f * nb + n) * 4 + 4)),
                (n, q) => quat.set(q, (f * nb + n) * 4));
        }
    });
    return { ...anim, quat };
}

/* the .skinanim's bytes with the layers baked in (importAnim's layout: a
 * 12- or 16-byte header, then each frame the root's position at +4 and
 * 20 bytes a bone from +16, its quaternion first) */
export function bakeLayers(f, layers, animBytes) {
    const out = animBytes.slice();
    const d = new DataView(out.buffer, out.byteOffset, out.byteLength);
    const frames0 = d.getInt32(4, true), fs0 = d.getInt32(8, true);
    const hdr = 12 + frames0 * fs0 === out.length ? 12 : 16;
    const fsize = hdr === 12 ? fs0 : d.getInt32(12, true);
    const { bones, anim } = f.rig;
    f.clips.forEach((c, ci) => {
        for (let k = 0; k <= c.to - c.from; k++) {
            const turns = turnsAt(layers, ci, k);
            if (!turns.size) continue;
            const base = hdr + (c.from + k) * fsize;
            turnFrame(bones.order, anim.bones, turns,
                n => [0, 1, 2, 3].map(j => d.getFloat32(base + 16 + n * 20 + j * 4, true)),
                (n, q) => q.forEach((v, j) => d.setFloat32(base + 16 + n * 20 + j * 4, v, true)));
        }
    });
    return out;
}
