/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * SMOOTH MODEL in the viewer: the game's own smoothing, Blender's Weld and
 * Subdivision Surface (src/thirdparty/blender/umk3_subsurf.c), as
 * WebAssembly (subsurf_wasm.js, made by scripts/build-subsurf.js). The same
 * C file the game links, so the viewer shows what the game draws. */
import { SUBSURF_WASM } from './subsurf_wasm.js';

let ready = null;

/* WebAssembly only compiles asynchronously in the window's main thread */
function load() {
    if (ready) return ready;
    ready = (async () => {
        const bytes = Uint8Array.from(atob(SUBSURF_WASM), c => c.charCodeAt(0));
        let mem = null;
        const { instance } = await WebAssembly.instantiate(bytes, {
            wasi_snapshot_preview1: {
                /* wasi's malloc asks for a seed; nothing here needs it random */
                random_get: (p, n) => { new Uint8Array(mem.buffer, p, n).fill(7); return 0; },
            },
        });
        mem = instance.exports.memory;
        instance.exports._initialize();
        return instance.exports;
    })();
    return ready;
}

/* Blender's Weld modifier default (0.001 m); the game's models are in cm */
export const WELD_DISTANCE = 0.1;

function put(w, arr) {
    const p = w.malloc(arr.byteLength);
    new Uint8Array(w.memory.buffer, p, arr.byteLength).set(new Uint8Array(arr.buffer, arr.byteOffset, arr.byteLength));
    return p;
}

/* One block of a fighter: its vertices' positions (where the weld is
 * measured), its triangles (block vertex indices) and a UV per corner.
 * Resolves to null when nothing is left to subdivide. */
export async function subsurfCreate(levels, pos, tri, triUv) {
    const w = await load();
    const nv = pos.length / 3, nt = tri.length / 3;
    const pp = put(w, Float32Array.from(pos)), pt = put(w, Uint16Array.from(tri)), pu = put(w, Float32Array.from(triUv));
    const h = w.umk_subsurf_new(levels, WELD_DISTANCE, nv, pp, nt, pt, pu);
    w.free(pp); w.free(pt); w.free(pu);
    if (!h) return null;
    const scratch = w.malloc(nv * 12);
    const nOut = w.umk_subsurf_num_verts(h), nTri = w.umk_subsurf_num_tris(h);
    return {
        welded: w.umk_subsurf_welded(h),
        verts: nOut,
        uv: new Float32Array(w.memory.buffer, w.umk_subsurf_uv(h), nOut * 2).slice(),
        index: new Uint32Array(w.memory.buffer, w.umk_subsurf_tris(h), nTri * 3).slice(),
        /* positions in, smoothed positions out (a copy) */
        eval(p) {
            new Float32Array(w.memory.buffer, scratch, nv * 3).set(p);
            if (!w.umk_subsurf_eval(h, scratch, 0)) return null;
            return new Float32Array(w.memory.buffer, w.umk_subsurf_pos(h), nOut * 3).slice();
        },
        /* SHARP EDGES (umk3_subsurf.h): degrees, 0..1; the next eval uses them */
        setSharp(angle, strength) { w.umk_subsurf_set_sharp(h, angle, strength); },
        sharpEdges() { return w.umk_subsurf_sharp_edges(h); },
        /* the correction pass per vertex, 0..1 (umk3_subsurf.h
         * umk_subsurf_set_fit), or null for all of it; copied by the C side */
        setFit(weights) {
            if (!weights) { w.umk_subsurf_set_fit(h, 0); return; }
            const p = put(w, Float32Array.from(weights));
            w.umk_subsurf_set_fit(h, p);
            w.free(p);
        },
        free() { w.free(scratch); w.umk_subsurf_free(h); },
    };
}
