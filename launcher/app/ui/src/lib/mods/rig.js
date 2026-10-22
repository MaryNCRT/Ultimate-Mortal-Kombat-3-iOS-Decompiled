/* A fighter, put together from its .bones, .skin, .skinanim and frame list:
 * the skeleton in any frame, the skinned mesh, and the two rest poses the
 * exporters offer.
 *
 * Conventions are the game's (docs/formats/SKIN-FORMAT.md): row vectors, a
 * bone's global transform is { R: 3x3 row-major, T: [x y z] } and a point
 * goes to the world as p * R + T; a bone's rotation in a frame is its
 * quaternion (x y z w) times its parent's, its position its .bones offset
 * through the parent (the root's comes from the frame).
 *
 * ## Why the rest pose has to be rebuilt
 *
 * The .bones file has no rest pose: every bone's axes point along its local
 * X, so a skeleton exported with no rotations unrolls into a line -- the
 * broken "T-pose" an exporter gets by reading the files as they are. The
 * pose the mesh was bound in is not stored anywhere either. What the .skin
 * does keep is each vertex once per bone that moves it, in that bone's own
 * space: in the bind pose all of those copies land on the same point. So
 * the bind pose is the one that brings them together, and it is found here
 * by alternating least squares -- the copies' mean, then each bone's best
 * rigid fit to it (Horn's quaternion method), again -- starting from the
 * frame of the animation where they already agree best. For 19 of the 29
 * fighters that frame IS the bind pose (the copies agree to 0.002-0.07
 * units on a 180-unit figure); for the ninjas, Sonya, Mileena, Jade and
 * Sindel it is not in the animation at all, and the fit brings them to
 * about 0.3. */

/* ------------------------------------------------------------- maths -- */

export function quatToMat(x, y, z, w) {
    /* GetMFromQuat2: row-vector form */
    const xx = 2 * x * x, yy = 2 * y * y, zz = 2 * z * z;
    const xy = 2 * x * y, xz = 2 * x * z, yz = 2 * y * z;
    const wx = 2 * w * x, wy = 2 * w * y, wz = 2 * w * z;
    return [1 - (yy + zz), xy + wz, xz - wy,
        xy - wz, 1 - (xx + zz), yz + wx,
        xz + wy, yz - wx, 1 - (xx + yy)];
}

/* the inverse of quatToMat (a unit quaternion x y z w) */
export function matToQuat(m) {
    const t = m[0] + m[4] + m[8];
    let x, y, z, w;
    if (t > 0) {
        const s = Math.sqrt(t + 1) * 2;
        w = s / 4; x = (m[5] - m[7]) / s; y = (m[6] - m[2]) / s; z = (m[1] - m[3]) / s;
    } else if (m[0] > m[4] && m[0] > m[8]) {
        const s = Math.sqrt(1 + m[0] - m[4] - m[8]) * 2;
        x = s / 4; w = (m[5] - m[7]) / s; y = (m[1] + m[3]) / s; z = (m[2] + m[6]) / s;
    } else if (m[4] > m[8]) {
        const s = Math.sqrt(1 + m[4] - m[0] - m[8]) * 2;
        y = s / 4; w = (m[6] - m[2]) / s; x = (m[1] + m[3]) / s; z = (m[5] + m[7]) / s;
    } else {
        const s = Math.sqrt(1 + m[8] - m[0] - m[4]) * 2;
        z = s / 4; w = (m[1] - m[3]) / s; x = (m[2] + m[6]) / s; y = (m[5] + m[7]) / s;
    }
    const n = Math.hypot(x, y, z, w) || 1;
    return [x / n, y / n, z / n, w / n];
}

export function mul3(a, b) {
    const o = new Array(9);
    for (let r = 0; r < 3; r++)
        for (let c = 0; c < 3; c++)
            o[r * 3 + c] = a[r * 3] * b[c] + a[r * 3 + 1] * b[3 + c] + a[r * 3 + 2] * b[6 + c];
    return o;
}

export const tr3 = a => [a[0], a[3], a[6], a[1], a[4], a[7], a[2], a[5], a[8]];
export const vmul = (v, m) => [v[0] * m[0] + v[1] * m[3] + v[2] * m[6], v[0] * m[1] + v[1] * m[4] + v[2] * m[7], v[0] * m[2] + v[1] * m[5] + v[2] * m[8]];
const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
const add = (a, b) => [a[0] + b[0], a[1] + b[1], a[2] + b[2]];
const norm = a => { const l = Math.hypot(a[0], a[1], a[2]) || 1; return [a[0] / l, a[1] / l, a[2] / l]; };
const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];

/* the row-vector rotation taking direction u onto v */
function rotBetween(u, v) {
    u = norm(u); v = norm(v);
    const c = dot(u, v);
    let ax = cross(u, v);
    let s = Math.hypot(ax[0], ax[1], ax[2]);
    if (s < 1e-9) {
        if (c > 0) return [1, 0, 0, 0, 1, 0, 0, 0, 1];
        ax = Math.abs(u[0]) < 0.9 ? cross(u, [1, 0, 0]) : cross(u, [0, 1, 0]);
        s = 0;
    }
    return axisAngle(norm(ax), Math.atan2(s, c));
}

/* row-vector rotation by `ang` about unit axis k */
export function axisAngle(k, ang) {
    const c = Math.cos(ang), s = Math.sin(ang), t = 1 - c, [x, y, z] = k;
    /* the column-vector Rodrigues matrix, transposed */
    return tr3([t * x * x + c, t * x * y - s * z, t * x * z + s * y,
        t * x * y + s * z, t * y * y + c, t * y * z - s * x,
        t * x * z - s * y, t * y * z + s * x, t * z * z + c]);
}

/* Horn: the rotation R (row form) minimising sum w |p R + t - q|^2 */
export function fitRigid(P, Q, W) {
    let sw = 0;
    const cp = [0, 0, 0], cq = [0, 0, 0];
    for (let i = 0; i < W.length; i++) {
        sw += W[i];
        for (let k = 0; k < 3; k++) { cp[k] += P[i * 3 + k] * W[i]; cq[k] += Q[i * 3 + k] * W[i]; }
    }
    for (let k = 0; k < 3; k++) { cp[k] /= sw; cq[k] /= sw; }
    const S = new Array(9).fill(0);
    for (let i = 0; i < W.length; i++)
        for (let a = 0; a < 3; a++)
            for (let b = 0; b < 3; b++)
                S[a * 3 + b] += W[i] * (P[i * 3 + a] - cp[a]) * (Q[i * 3 + b] - cq[b]);
    const [Sxx, Sxy, Sxz, Syx, Syy, Syz, Szx, Szy, Szz] = S;
    const N = [
        [Sxx + Syy + Szz, Syz - Szy, Szx - Sxz, Sxy - Syx],
        [Syz - Szy, Sxx - Syy - Szz, Sxy + Syx, Szx + Sxz],
        [Szx - Sxz, Sxy + Syx, -Sxx + Syy - Szz, Syz + Szy],
        [Sxy - Syx, Szx + Sxz, Syz + Szy, -Sxx - Syy + Szz]];
    const q = topEigen(N);              /* w x y z, the column-form rotation */
    /* Horn's quaternion turns column vectors p -> q; quatToMat gives a
     * quaternion's row form, which is that same turn for row vectors */
    const Rrow = quatToMat(q[1], q[2], q[3], q[0]);
    const t = sub(cq, vmul(cp, Rrow));
    return { R: Rrow, T: t };
}

/* the eigenvector of the largest eigenvalue of a symmetric 4x4 (Jacobi) */
function topEigen(A) {
    const a = A.map(r => r.slice());
    const v = [[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]];
    for (let sweep = 0; sweep < 50; sweep++) {
        let off = 0;
        for (let p = 0; p < 4; p++) for (let q = p + 1; q < 4; q++) off += a[p][q] * a[p][q];
        if (off < 1e-20) break;
        for (let p = 0; p < 4; p++)
            for (let q = p + 1; q < 4; q++) {
                if (Math.abs(a[p][q]) < 1e-30) continue;
                const th = (a[q][q] - a[p][p]) / (2 * a[p][q]);
                const t = Math.sign(th || 1) / (Math.abs(th) + Math.sqrt(th * th + 1));
                const c = 1 / Math.sqrt(t * t + 1), s = t * c;
                for (let k = 0; k < 4; k++) {
                    const akp = a[k][p], akq = a[k][q];
                    a[k][p] = c * akp - s * akq; a[k][q] = s * akp + c * akq;
                }
                for (let k = 0; k < 4; k++) {
                    const apk = a[p][k], aqk = a[q][k];
                    a[p][k] = c * apk - s * aqk; a[q][k] = s * apk + c * aqk;
                }
                for (let k = 0; k < 4; k++) {
                    const vkp = v[k][p], vkq = v[k][q];
                    v[k][p] = c * vkp - s * vkq; v[k][q] = s * vkp + c * vkq;
                }
            }
    }
    let best = 0;
    for (let i = 1; i < 4; i++) if (a[i][i] > a[best][best]) best = i;
    return [v[0][best], v[1][best], v[2][best], v[3][best]];
}

/* ------------------------------------------------------------- the rig -- */

/* bones: parseBones; skin: parseSkin; anim: parseAnim */
export function makeRig(bones, skin, anim) {
    const rig = { bones, skin, anim, num: bones.num };
    /* every influence, flat: vertex (over all blocks), bone, weight, the
     * vertex in the bone's space (A / w) */
    const iv = [], ib = [], iw = [], ip = [];
    let base = 0;
    rig.blockBase = [];
    for (const blk of skin) {
        rig.blockBase.push(base);
        for (let v = 0; v < blk.numVerts; v++)
            for (let k = 0; k < 4; k++) {
                const b = blk.bones[v * 4 + k], w = blk.weights[v * 4 + k];
                if (b === 0xff || b >= bones.num || w <= 1e-6) continue;
                iv.push(base + v); ib.push(b); iw.push(w);
                const a = blk.a.subarray(v * 12 + k * 3, v * 12 + k * 3 + 3);
                ip.push(a[0] / w, a[1] / w, a[2] / w);
            }
        base += blk.numVerts;
    }
    rig.numVerts = base;
    rig.inf = { v: Int32Array.from(iv), b: Int32Array.from(ib), w: Float32Array.from(iw), p: Float32Array.from(ip) };
    rig.multi = new Uint8Array(iv.length);
    const cnt = new Int32Array(base);
    for (const v of iv) cnt[v]++;
    for (let i = 0; i < iv.length; i++) rig.multi[i] = cnt[iv[i]] > 1 ? 1 : 0;
    return rig;
}

/* the skeleton in animation frame f: [{ R, T }] by bone index */
export function framePose(rig, f) {
    const { bones, anim } = rig;
    const pal = new Array(bones.num);
    const nb = anim.bones;
    let n = 0;
    const walk = (i, PR, PT) => {
        const k = n++;
        let R;
        if (k < nb) {
            const o = (f * nb + k) * 4;
            R = quatToMat(anim.quat[o], anim.quat[o + 1], anim.quat[o + 2], anim.quat[o + 3]);
        } else R = [1, 0, 0, 0, 1, 0, 0, 0, 1];
        const t = k === 0 ? [anim.root[f * 3], anim.root[f * 3 + 1], anim.root[f * 3 + 2]]
            : [bones.offset[i * 3], bones.offset[i * 3 + 1], bones.offset[i * 3 + 2]];
        pal[i] = { R: mul3(R, PR), T: add(vmul(t, PR), PT) };
        for (const c of bones.children[i]) walk(c, pal[i].R, pal[i].T);
    };
    walk(bones.root, [1, 0, 0, 0, 1, 0, 0, 0, 1], [0, 0, 0]);
    return pal;
}

/* every vertex through a pose, as DrawSkinnedMesh2: sum of A R + w T */
export function skinPositions(rig, pose) {
    const { v, b, w, p } = rig.inf;
    const out = new Float32Array(rig.numVerts * 3);
    for (let i = 0; i < v.length; i++) {
        const { R, T } = pose[b[i]], wi = w[i];
        const x = p[i * 3] * wi, y = p[i * 3 + 1] * wi, z = p[i * 3 + 2] * wi;
        out[v[i] * 3] += x * R[0] + y * R[3] + z * R[6] + wi * T[0];
        out[v[i] * 3 + 1] += x * R[1] + y * R[4] + z * R[7] + wi * T[1];
        out[v[i] * 3 + 2] += x * R[2] + y * R[5] + z * R[8] + wi * T[2];
    }
    return out;
}

/* how far a vertex's copies sit apart in a pose (0 in the bind pose) */
function spread(rig, pose, mean) {
    const { v, b, p, w } = rig.inf;
    let s = 0, n = 0;
    for (let i = 0; i < v.length; i++) {
        if (!rig.multi[i]) continue;
        const { R, T } = pose[b[i]];
        const q = add(vmul([p[i * 3], p[i * 3 + 1], p[i * 3 + 2]], R), T);
        s += Math.hypot(q[0] - mean[v[i] * 3], q[1] - mean[v[i] * 3 + 1], q[2] - mean[v[i] * 3 + 2]);
        n++;
    }
    return n ? s / n : 0;
}

/* The bind pose (see the top). Returns { pose, frame, error }. */
export function bindPose(rig, iterations = 40) {
    const frames = rig.anim.frames;
    const step = Math.max(1, Math.floor(frames / 60));
    let best = { e: Infinity, f: 0 };
    for (let f = 0; f < frames; f += step) {
        const pose = framePose(rig, f);
        const e = spread(rig, pose, skinPositions(rig, pose));
        if (e < best.e) best = { e, f };
    }
    const pose = framePose(rig, best.f);
    if (best.e < 0.1) return { pose, frame: best.f, error: best.e };

    const { v, b, w, p } = rig.inf;
    const perBone = Array.from({ length: rig.num }, () => []);
    for (let i = 0; i < v.length; i++) if (rig.multi[i]) perBone[b[i]].push(i);
    const count = new Int32Array(rig.num);
    for (let i = 0; i < b.length; i++) count[b[i]]++;
    let anchor = 0;
    for (let i = 1; i < rig.num; i++) if (count[i] > count[anchor]) anchor = i;
    for (let it = 0; it < iterations; it++) {
        const mean = skinPositions(rig, pose);
        for (let bone = 0; bone < rig.num; bone++) {
            const list = perBone[bone];
            if (bone === anchor || list.length < 3) continue;
            const P = new Float64Array(list.length * 3), Q = new Float64Array(list.length * 3), W = new Float64Array(list.length);
            list.forEach((i, j) => {
                P.set([p[i * 3], p[i * 3 + 1], p[i * 3 + 2]], j * 3);
                Q.set([mean[v[i] * 3], mean[v[i] * 3 + 1], mean[v[i] * 3 + 2]], j * 3);
                W[j] = w[i];
            });
            pose[bone] = fitRigid(P, Q, W);
        }
    }
    return { pose, frame: best.f, error: spread(rig, pose, skinPositions(rig, pose)) };
}

/* ----------------------------------------------------------- T-pose -- */

function subtree(bones, i) {
    const out = [i];
    for (const c of bones.children[i]) out.push(...subtree(bones, c));
    return out;
}

/* rotate bone i (and all under it) about its own joint by row rotation Q */
function turn(pose, bones, i, Q) {
    const J = pose[i].T;
    for (const d of subtree(bones, i)) {
        pose[d] = { R: mul3(pose[d].R, Q), T: add(vmul(sub(pose[d].T, J), Q), J) };
    }
}

function aim(pose, bones, i, next, dir) {
    const cur = sub(pose[next].T, pose[i].T);
    if (Math.hypot(...cur) < 1e-6) return;
    turn(pose, bones, i, rotBetween(cur, dir));
}

function pathUp(bones, i) {
    const out = [];
    for (; i >= 0; i = bones.parent[i]) out.push(i);
    return out;
}

/* the bones from `from` (not included) down to `to` (included) */
function pathDown(bones, from, to) {
    const up = pathUp(bones, to);
    const k = up.indexOf(from);
    return k < 0 ? null : up.slice(0, k).reverse();
}

function lca(bones, a, b) {
    const up = new Set(pathUp(bones, a));
    for (let i = b; i >= 0; i = bones.parent[i]) if (up.has(i)) return i;
    return -1;
}

/* The fighter's body, found from the skeleton's shape rather than from
 * bone numbers, which differ between fighters: the hands are the bones with
 * three or more short branches (the fingers) and the chest is where two of
 * them meet (Sheeva's upper pair); the feet are the two lowest ends of the
 * skeleton whose branches meet below the chest, at the hips. (The first branch above the root is not the hips: on Kano it
 * holds the belt's tassels.) Returns null for a skeleton without that
 * shape (Motaro, the centaur). */
export function anatomy(rig, pose) {
    const bones = rig.bones;
    const size = i => 1 + bones.children[i].reduce((a, c) => a + size(c), 0);
    const cand = [];
    for (let i = 0; i < bones.num; i++) {
        const ch = bones.children[i];
        if (ch.length >= 2 && ch.every(c => size(c) <= 3)) cand.push(i);
    }
    /* two of them with an arm each (three bones at least) between them and
     * where they meet; Sheeva has four: the pair that meets highest, the
     * upper */
    let hands = null;
    for (let a = 0; a < cand.length; a++)
        for (let b = a + 1; b < cand.length; b++) {
            const m = lca(bones, cand[a], cand[b]);
            if (pathDown(bones, m, cand[a]).length < 3 || pathDown(bones, m, cand[b]).length < 3) continue;
            if (!hands || pose[m].T[1] > pose[hands[2]].T[1]) hands = [cand[a], cand[b], m];
        }
    if (!hands) return null;
    const chest = hands.pop();
    const belowChest = new Set(pathUp(bones, chest));
    const leaves = [];
    for (let i = 0; i < bones.num; i++)
        if (!bones.children[i].length && !belowChest.has(i) && !pathUp(bones, i).includes(chest)) leaves.push(i);
    leaves.sort((a, b) => pose[a].T[1] - pose[b].T[1]);
    if (leaves.length < 2) return null;
    const foot0 = leaves[0];
    const foot1 = leaves.find(l => l !== foot0 && belowChest.has(lca(bones, foot0, l)));
    if (foot1 === undefined) return null;
    const hips = lca(bones, foot0, foot1);
    const legs = [pathDown(bones, hips, foot0), pathDown(bones, hips, foot1)];
    const spine = pathDown(bones, hips, chest);
    const arms = hands.map(h => pathDown(bones, chest, h));
    if (!spine || legs.some(l => !l || l.length < 3) || arms.some(a => !a || a.length < 3)) return null;
    /* each leg its own branch off the hips, apart from the spine and from
     * the other: Motaro's front legs hang from his spine, and are not legs */
    if (legs[0][0] === legs[1][0] || legs.some(l => l[0] === spine[0])) return null;
    return { hips, chest, spine, legs, arms };
}

/* Which way the fighter faces: where the knees point (a stance
 * bends them forward), or, with the legs straight, the feet. */
function forward(pose, legs) {
    let knees = [0, 0, 0], toes = [0, 0, 0];
    for (const l of legs) {
        const hip = pose[l[0]].T, knee = pose[l[1]].T, ankle = pose[l[2]].T;
        knees = add(knees, sub(knee, [(hip[0] + ankle[0]) / 2, (hip[1] + ankle[1]) / 2, (hip[2] + ankle[2]) / 2]));
        toes = add(toes, sub(pose[l[l.length - 1]].T, pose[l[l.length - 2]].T));
    }
    knees[1] = toes[1] = 0;
    return Math.hypot(...knees) > 2 ? knees : toes;
}
const forwardZ = (pose, legs) => forward(pose, legs)[2];

/* The figure stood up straight, facing +Z, legs down and arms out along
 * X: the T-pose rigs and retargeting tools expect. null where anatomy()
 * finds no biped. */
export function tPose(rig, rest) {
    const bones = rig.bones;
    const pose = rest.map(x => ({ R: x.R.slice(), T: x.T.slice() }));
    const an = anatomy(rig, pose);
    if (!an) return null;
    const { hips, chest, spine, legs, arms } = an;

    /* turn the whole figure so the hips' line runs along X and the toes
     * point along +Z */
    const across = sub(pose[legs[0][0]].T, pose[legs[1][0]].T);
    turn(pose, bones, bones.root, axisAngle([0, 1, 0], -Math.atan2(-across[2], across[0])));
    if (forwardZ(pose, legs) < 0) turn(pose, bones, bones.root, axisAngle([0, 1, 0], Math.PI));

    /* the spine straight up from the hips */
    let prev = hips;
    for (const s of spine) { aim(pose, bones, prev, s, [0, 1, 0]); prev = s; }
    /* the legs straight down: the thigh and the shin */
    for (const l of legs)
        for (let k = 0; k < 2; k++) aim(pose, bones, l[k], l[k + 1], [0, -1, 0]);
    /* the arms out to their own side: the upper arm and the forearm (the
     * collarbone stays; the hand follows the forearm) */
    for (const a of arms) {
        const side = Math.sign(pose[a[a.length - 1]].T[0] - pose[chest].T[0]) || 1;
        for (let k = a.length - 3; k < a.length - 1; k++) aim(pose, bones, a[k], a[k + 1], [side, 0, 0]);
    }
    /* the head facing ahead: fighting stances turn it to the opponent */
    faceAhead(rig, pose, chest, arms);
    /* the feet on the ground at 0 */
    let low = Infinity;
    const posed = skinPositions(rig, pose);
    for (let i = 1; i < posed.length; i += 3) low = Math.min(low, posed[i]);
    if (isFinite(low)) for (const x of pose) x.T[1] -= low;
    return pose;
}

function neckOf(bones, pose, chest, arms) {
    const rest = bones.children[chest].filter(c => !arms.some(a => a[0] === c));
    if (!rest.length) return -1;
    const top = i => Math.max(pose[i].T[1], ...bones.children[i].map(top));
    return rest.reduce((a, c) => (top(c) > top(a) ? c : a));
}

/* Turns the neck about the vertical so the face looks along +Z. A face is
 * left-right symmetric, so its sideways axis is the one whose mirror maps
 * the head's vertices onto themselves best; the turn brings that axis onto
 * X, the shorter way round (the body already faces +Z). */
function faceAhead(rig, pose, chest, arms) {
    const bones = rig.bones;
    const neck = neckOf(bones, pose, chest, arms);
    if (neck < 0) return;
    /* the neck and its short branches (the head); not a long one, which is
     * hair (Sindel's) and swings to one side */
    const head = new Set([neck, ...bones.children[neck].filter(c => subtree(bones, c).length <= 2).flatMap(c => subtree(bones, c))]);
    /* the vertices the neck and head carry, each by its strongest bone */
    const best = new Map();
    const { v, b, w } = rig.inf;
    for (let i = 0; i < v.length; i++) {
        const cur = best.get(v[i]);
        if (!cur || w[i] > cur[1]) best.set(v[i], [b[i], w[i]]);
    }
    const all = skinPositions(rig, pose), pts = [];
    for (const [vi, [bb]] of best) if (head.has(bb)) pts.push([all[vi * 3], all[vi * 3 + 1], all[vi * 3 + 2]]);
    if (pts.length < 20) return;
    const c = pts.reduce((a, p) => add(a, p), [0, 0, 0]).map(x => x / pts.length);
    const misfit = th => {
        const n = [Math.cos(th), 0, Math.sin(th)];
        let s = 0;
        for (const p of pts) {
            const d = dot(sub(p, c), n);
            const m = [p[0] - 2 * d * n[0], p[1], p[2] - 2 * d * n[2]];
            let near = Infinity;
            for (const q of pts) {
                const e = (q[0] - m[0]) ** 2 + (q[1] - m[1]) ** 2 + (q[2] - m[2]) ** 2;
                if (e < near) near = e;
            }
            s += Math.sqrt(near);
        }
        return s;
    };
    let th = 0, low = Infinity;
    for (let deg = 0; deg < 180; deg += 2) {
        const e = misfit(deg * Math.PI / 180);
        if (e < low) { low = e; th = deg * Math.PI / 180; }
    }
    for (const step of [1, 0.5, 0.25]) {
        for (const t of [th - step * Math.PI / 180, th + step * Math.PI / 180]) {
            const e = misfit(t);
            if (e < low) { low = e; th = t; }
        }
    }
    /* the sideways axis at angle th from X; bring it onto X (or -X) */
    let turnBy = th > Math.PI / 2 ? th - Math.PI : th;
    if (Math.abs(turnBy) < 0.02) return;
    turn(pose, bones, neck, axisAngle([0, 1, 0], turnBy));
}

/* Names for the bones, from their place in the body where anatomy() finds
 * one (hips, spine1.., chest, neck, head, clavicle/upperarm/forearm/hand
 * and thigh/shin/foot/toe .L .R, finger1_1..), bone_NN for the rest. Left
 * and right are the fighter's own, in a pose that faces +Z. */
export function boneNames(rig, pose) {
    const bones = rig.bones;
    const names = Array.from({ length: bones.num }, (_, i) => 'bone_' + String(i).padStart(2, '0'));
    const an = anatomy(rig, pose);
    if (!an) return names;
    const { hips, chest, spine, legs, arms } = an;
    const fwd = forward(pose, legs);
    /* facing +Z the fighter's left is +X: in general, left is up x forward */
    const left = cross([0, 1, 0], fwd);
    const sideOf = i => (dot(sub(pose[i].T, pose[hips].T), left) >= 0 ? '.L' : '.R');
    names[hips] = 'hips';
    spine.forEach((b, k) => { names[b] = k === spine.length - 1 ? 'chest' : 'spine' + (k + 1); });
    for (const l of legs) {
        const sd = sideOf(l[0]);
        ['thigh', 'shin', 'foot', 'toe'].forEach((n, k) => { if (l[k] !== undefined) names[l[k]] = n + sd; });
    }
    for (const a of arms) {
        const sd = sideOf(a[a.length - 1]);
        const roles = ['hand', 'forearm', 'upperarm', 'clavicle'];
        for (let k = 0; k < a.length && k < 4; k++) names[a[a.length - 1 - k]] = roles[k] + sd;
        bones.children[a[a.length - 1]].forEach((f, fi) => {
            let j = f, seg = 1;
            while (j !== undefined) { names[j] = 'finger' + (fi + 1) + '_' + seg++ + sd; j = bones.children[j][0]; }
        });
    }
    const neck = neckOf(bones, pose, chest, arms);
    if (neck >= 0) {
        names[neck] = 'neck';
        if (bones.children[neck].length) names[bones.children[neck][0]] = 'head';
    }
    if (bones.parent[hips] >= 0 && names[bones.parent[hips]].startsWith('bone_')) names[bones.parent[hips]] = 'pelvis';
    if (names[bones.root].startsWith('bone_')) names[bones.root] = 'root';
    return names;
}

/* ------------------------------------------------------ the mesh itself --
 * Triangles and UVs from the .skin, which keeps a UV per triangle corner; a
 * vertex is split where two triangles give it different UVs. Returns
 * { src (the skinned vertex each output vertex copies), uv, faces, blocks:
 *   [{ first, count }] in triangles }. */
export function meshTopology(rig) {
    const src = [], uv = [], faces = [], blocks = [];
    const key = new Map();
    rig.skin.forEach((blk, bi) => {
        const base = rig.blockBase[bi];
        blocks.push({ first: faces.length / 3, count: blk.numTris });
        for (let t = 0; t < blk.numTris; t++)
            for (let c = 0; c < 3; c++) {
                const sv = base + blk.tri[t * 3 + c];
                const u = blk.triUv[t * 6 + c * 2], v = blk.triUv[t * 6 + c * 2 + 1];
                const k = sv + ':' + u.toFixed(5) + ':' + v.toFixed(5);
                let o = key.get(k);
                if (o === undefined) {
                    o = src.length;
                    key.set(k, o);
                    src.push(sv);
                    uv.push(u, v);
                }
                faces.push(o);
            }
    });
    return { src: Int32Array.from(src), uv: Float32Array.from(uv), faces: Uint32Array.from(faces), blocks };
}

/* Per output vertex, up to four (bone, weight) pairs, the weights summing
 * to one -- what FBX clusters and USD joint primvars want. */
export function vertexWeights(rig, topo) {
    const per = Array.from({ length: rig.numVerts }, () => []);
    const { v, b, w } = rig.inf;
    for (let i = 0; i < v.length; i++) per[v[i]].push([b[i], w[i]]);
    const idx = new Int32Array(topo.src.length * 4), wt = new Float32Array(topo.src.length * 4);
    topo.src.forEach((s, o) => {
        const list = per[s].slice(0, 4);
        const sum = list.reduce((a, x) => a + x[1], 0) || 1;
        list.forEach(([bb, ww], k) => { idx[o * 4 + k] = bb; wt[o * 4 + k] = ww / sum; });
    });
    return { idx, wt };
}

/* a bone's transform relative to its parent: { R, T } */
export function localOf(pose, bones, i) {
    const p = bones.parent[i];
    if (p < 0) return pose[i];
    const PRt = tr3(pose[p].R);
    return { R: mul3(pose[i].R, PRt), T: vmul(sub(pose[i].T, pose[p].T), PRt) };
}

/* rest mesh positions for the output vertices, in a given pose */
export function posedVerts(rig, topo, pose) {
    const all = skinPositions(rig, pose);
    const out = new Float32Array(topo.src.length * 3);
    topo.src.forEach((s, o) => out.set(all.subarray(s * 3, s * 3 + 3), o * 3));
    return out;
}

/* The rest mesh for a rest pose other than the bind pose, fitted so that
 * ordinary skinning from it (what Blender does: each vertex through
 * bone(t) * rest(bone)^-1, blended by weight) lands where the game's own
 * skinning puts the vertex, over the animation's frames, in the least-squares
 * sense. Moving a mesh into a T-pose and back is not lossless -- the
 * blended joints shift -- and this is what keeps that shift small: on Kano
 * the largest miss falls from 3.7 units to well under one. */
export function fitRest(rig, topo, weights, rest, sample = 48) {
    const n = topo.src.length;
    const inv = rest.map(r => { const Rt = tr3(r.R); return { R: Rt, T: vmul(r.T, Rt).map(x => -x) }; });
    const A = new Float64Array(n * 9), B = new Float64Array(n * 3);
    const step = Math.max(1, Math.floor(rig.anim.frames / sample));
    for (let f = 0; f < rig.anim.frames; f += step) {
        const pose = framePose(rig, f);
        const game = skinPositions(rig, pose);
        const delta = pose.map((p, b) => ({ R: mul3(inv[b].R, p.R), T: add(vmul(inv[b].T, p.R), p.T) }));
        for (let v = 0; v < n; v++) {
            const M = [0, 0, 0, 0, 0, 0, 0, 0, 0], c = [0, 0, 0];
            for (let k = 0; k < 4; k++) {
                const w = weights.wt[v * 4 + k];
                if (!w) continue;
                const d = delta[weights.idx[v * 4 + k]];
                for (let i = 0; i < 9; i++) M[i] += w * d.R[i];
                for (let i = 0; i < 3; i++) c[i] += w * d.T[i];
            }
            const s = topo.src[v];
            const g = [game[s * 3] - c[0], game[s * 3 + 1] - c[1], game[s * 3 + 2] - c[2]];
            /* x M = g: x (M M^T) = g M^T */
            for (let r = 0; r < 3; r++) {
                for (let q = 0; q < 3; q++)
                    A[v * 9 + r * 3 + q] += M[r * 3] * M[q * 3] + M[r * 3 + 1] * M[q * 3 + 1] + M[r * 3 + 2] * M[q * 3 + 2];
                B[v * 3 + r] += g[0] * M[r * 3] + g[1] * M[r * 3 + 1] + g[2] * M[r * 3 + 2];
            }
        }
    }
    const out = posedVerts(rig, topo, rest);
    for (let v = 0; v < n; v++) {
        const m = A.subarray(v * 9, v * 9 + 9), b = B.subarray(v * 3, v * 3 + 3);
        const det = m[0] * (m[4] * m[8] - m[5] * m[7]) - m[1] * (m[3] * m[8] - m[5] * m[6]) + m[2] * (m[3] * m[7] - m[4] * m[6]);
        if (Math.abs(det) < 1e-9) continue;
        /* the symmetric system by Cramer's rule */
        const solve = (i) => {
            const c = m.slice();
            c[i] = b[0]; c[3 + i] = b[1]; c[6 + i] = b[2];
            return (c[0] * (c[4] * c[8] - c[5] * c[7]) - c[1] * (c[3] * c[8] - c[5] * c[6]) + c[2] * (c[3] * c[7] - c[4] * c[6])) / det;
        };
        out[v * 3] = solve(0); out[v * 3 + 1] = solve(1); out[v * 3 + 2] = solve(2);
    }
    return out;
}
