/* The plain skin under a piece on its bones (Kung Lao's hat), as the game
 * does with SMOOTH MODELS off (smooth_models.c tuck): a vertex in the piece's
 * box, 2 cm wider, past its surface seen from its centre by less than 3 cm,
 * goes back 0.3 cm inside it. The original model's head comes through the
 * hat in 4 points. pos is changed in place; pieces are { v, faces }. */
export function tuckUnder(pos, pieces) {
    let moved = 0;
    for (const { v, faces } of pieces) {
        if (!v || !faces) continue;
        const lo = [1e30, 1e30, 1e30], hi = [-1e30, -1e30, -1e30], c = [0, 0, 0];
        for (let i = 0; i < v.length; i += 3)
            for (let k = 0; k < 3; k++) {
                lo[k] = Math.min(lo[k], v[i + k] - 2);
                hi[k] = Math.max(hi[k], v[i + k] + 2);
                c[k] += v[i + k] * 3 / v.length;
            }
        for (let i = 0; i < pos.length; i += 3) {
            if (pos[i] < lo[0] || pos[i] > hi[0] || pos[i + 1] < lo[1] || pos[i + 1] > hi[1]
                || pos[i + 2] < lo[2] || pos[i + 2] > hi[2]) continue;
            const d = [pos[i] - c[0], pos[i + 1] - c[1], pos[i + 2] - c[2]];
            const len = Math.hypot(d[0], d[1], d[2]);
            if (len <= 0) continue;
            let best = -1;
            for (let f = 0; f < faces.length; f += 3) {
                const a = faces[f] * 3, b = faces[f + 1] * 3, e = faces[f + 2] * 3;
                const e1 = [v[b] - v[a], v[b + 1] - v[a + 1], v[b + 2] - v[a + 2]];
                const e2 = [v[e] - v[a], v[e + 1] - v[a + 1], v[e + 2] - v[a + 2]];
                const h = [d[1] * e2[2] - d[2] * e2[1], d[2] * e2[0] - d[0] * e2[2], d[0] * e2[1] - d[1] * e2[0]];
                const det = e1[0] * h[0] + e1[1] * h[1] + e1[2] * h[2];
                if (Math.abs(det) < 1e-9) continue;
                const s = [c[0] - v[a], c[1] - v[a + 1], c[2] - v[a + 2]];
                const u = (s[0] * h[0] + s[1] * h[1] + s[2] * h[2]) / det;
                if (u < 0 || u > 1) continue;
                const q = [s[1] * e1[2] - s[2] * e1[1], s[2] * e1[0] - s[0] * e1[2], s[0] * e1[1] - s[1] * e1[0]];
                const w = (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) / det;
                if (w < 0 || u + w > 1) continue;
                const t = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) / det;
                if (t > 0 && t < 1 && t > best) best = t;
            }
            if (best < 0 || (1 - best) * len > 3) continue;
            const back = Math.max(0, best - 0.3 / len);
            for (let k = 0; k < 3; k++) pos[i + k] = c[k] + d[k] * back;
            moved++;
        }
    }
    return moved;
}
