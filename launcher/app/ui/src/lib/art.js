/* The backdrop's pictures, made here once (no game art): the marble the
 * ring segments are filled with and the cloud of light behind everything. */

function mulberry32(seed) {
    return () => {
        seed = (seed + 0x6D2B79F5) | 0;
        let t = Math.imul(seed ^ (seed >>> 15), 1 | seed);
        t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
        return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
}
const sm = t => t * t * (3 - 2 * t);

/* Dark blue marble: a tiling turbulence, so it turns with the segments and
 * costs nothing per frame. */
export function marbleURL(S) {
    const rnd = mulberry32(11);
    const oct = [];
    for (let o = 0, P = 4; o < 5; o++, P *= 2) {
        const v = new Float32Array(P * P);
        for (let i = 0; i < v.length; i++) v[i] = rnd();
        oct.push({ P, v });
    }
    const noise = ({ P, v }, x, y) => {     /* tiles: the lattice wraps */
        const fx = x * P, fy = y * P, ix = Math.floor(fx), iy = Math.floor(fy);
        const tx = sm(fx - ix), ty = sm(fy - iy);
        const at = (a, b) => v[((b % P) + P) % P * P + ((a % P) + P) % P];
        const a = at(ix, iy) + (at(ix + 1, iy) - at(ix, iy)) * tx;
        const b = at(ix, iy + 1) + (at(ix + 1, iy + 1) - at(ix, iy + 1)) * tx;
        return a + (b - a) * ty;
    };
    const TABLE = [0, 0.03, 0.07, 0.16, 0.5, 1];
    const cv = document.createElement('canvas');
    cv.width = cv.height = S;
    const g = cv.getContext('2d'), im = g.createImageData(S, S);
    for (let y = 0; y < S; y++)
        for (let x = 0; x < S; x++) {
            let t = 0, w = 0.5;
            for (const o of oct) {
                t += Math.abs(noise(o, x / S, y / S) * 2 - 1) * w;
                w *= 0.5;
            }
            t = Math.min(1, t * 1.05);
            const k = t * (TABLE.length - 1), i = Math.min(TABLE.length - 2, Math.floor(k));
            const v = TABLE[i] + (TABLE[i + 1] - TABLE[i]) * (k - i);
            const p = (y * S + x) * 4;
            im.data[p] = (0.72 * v + 0.02) * 255;
            im.data[p + 1] = (0.78 * v + 0.04) * 255;
            im.data[p + 2] = Math.min(1, 0.88 * v + 0.13) * 255;
            im.data[p + 3] = 255;
        }
    g.putImageData(im, 0, 0);
    return cv.toDataURL('image/png');
}

/* The cloud, as the console release's menus: crimson in the top left and
 * the bottom right, a streak of blue light down the middle and blue across
 * it, a little teal under the menu, purple between, in soft
 * billows. Value noise in a few octaves, a small picture the page stretches
 * over the window. BRIGHT scales its light: kept low, so the rings and the
 * text stand out of a dark field. */
const BRIGHT = 0.7;

export function nebulaURL(W, H) {
    const field = seed => {
        const rnd = mulberry32(seed);
        const oct = [];
        for (let o = 0, P = 3; o < 6; o++, P *= 2) {
            const v = new Float32Array(P * P);
            for (let i = 0; i < v.length; i++) v[i] = rnd();
            oct.push({ P, v });
        }
        return (x, y) => {
            let t = 0, w = 0.5, n = 0;
            for (const { P, v } of oct) {
                const fx = x * P, fy = y * P, ix = Math.floor(fx), iy = Math.floor(fy);
                const tx = sm(fx - ix), ty = sm(fy - iy);
                const at = (a, b) => v[(b % P) * P + (a % P)];
                const a = at(ix, iy) + (at(ix + 1, iy) - at(ix, iy)) * tx;
                const b = at(ix, iy + 1) + (at(ix + 1, iy + 1) - at(ix, iy + 1)) * tx;
                t += (a + (b - a) * ty) * w;
                n += w;
                w *= 0.55;
            }
            return t / n;
        };
    };
    const dens = field(7), hue = field(23);
    const RED = [190, 30, 40], PUR = [86, 34, 120], BLU = [40, 80, 230], TEA = [30, 130, 140];
    const cv = document.createElement('canvas');
    cv.width = W; cv.height = H;
    const g = cv.getContext('2d'), im = g.createImageData(W, H);
    const cl = v => Math.max(0, Math.min(1, v));
    for (let y = 0; y < H; y++)
        for (let x = 0; x < W; x++) {
            const u = x / W, v = y / H;
            const h = hue(u * 1.3, v * 1.3), d = dens(u * 1.6 + 3, v * 1.6 + 3);
            const near = (x0, y0, r) => cl(1 - Math.hypot((u - x0) * 1.78, v - y0) / r);
            const wr = (near(0.05, 0.05, 0.85) * 2.2 + near(1, 0.95, 0.75) * 1.8 + near(1, 0.3, 0.45)) * (0.6 + h * 0.8);
            const wb = (cl(1 - Math.abs(u - 0.53) * 9) * 2.2 + cl(1 - Math.abs(v - 0.5) * 3) * 0.9) * (1.4 - h * 0.8);
            const wt = near(0.4, 0.62, 0.35) * 1.4;
            const wp = 0.5;
            const sum = wr + wb + wt + wp;
            const k = (0.14 + 1.35 * Math.pow(cl(d * 1.3 - 0.15), 1.6)) * BRIGHT;
            const p = (y * W + x) * 4;
            for (let c = 0; c < 3; c++)
                im.data[p + c] = Math.min(255, (RED[c] * wr + BLU[c] * wb + TEA[c] * wt + PUR[c] * wp) / sum * k);
            im.data[p + 3] = 255;
        }
    g.putImageData(im, 0, 0);
    return cv.toDataURL('image/png');
}
