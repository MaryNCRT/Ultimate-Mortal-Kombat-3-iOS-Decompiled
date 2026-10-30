/* STAGE SELECT's pictures for the stages the game has none for. The game
 * ships a CS_BG sheet for five of its stages (debug_menu.c k_stages); for
 * the rest the launcher renders the stage itself, as the engine draws it
 * (mods/stagerun.js: its objects blended as the game blends them, the
 * effects its events fire -- Scislac's sky is one -- the far skies the
 * game never clips, on black), from where
 * the fight's camera stands when a round starts, a second in, into
 * "aditional textures/stages/<ID>.png", where the game's st_thumb finds it.
 * Once per stage: a picture already there is kept (the player may have put
 * a better one); main.js makes the launcher's older renders again. */
import * as THREE from 'three';
import { api } from '../platform.js';
import { src, ms, loadCatalog } from './store.svelte.js';
import { loadStage, StageView, stageTextures, PLAYER_SIZE, TICKS_PER_SECOND } from './stagerun.js';

const HAS_CS_BG = new Set(['SUBWAY', 'BELLTOWER', 'SOULCHAMBER', 'GRAVEYARD', 'TEMPLE']);
const W = 480, H = 320;

let running = null;
export function stagePictures() {
    if (!running) running = make().catch(e => console.warn('stage pictures:', e)).finally(() => { running = null; });
    return running;
}

async function make() {
    if (!api || !api.stagePics) return;
    await loadCatalog();
    if (!ms.cat || ms.noGame) return;
    const have = new Set(await api.stagePics());
    const todo = ms.cat.stages.filter(st => !HAS_CS_BG.has(st.id) && !have.has(st.id));
    if (!todo.length) return;

    const canvas = new OffscreenCanvas(W, H);
    const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, preserveDrawingBuffer: true, logarithmicDepthBuffer: true });
    renderer.setSize(W, H, false);
    renderer.outputColorSpace = THREE.LinearSRGBColorSpace;
    const tex = stageTextures(src);
    const used = new Set();
    try {
        for (const st of todo) {
            const scene = new THREE.Scene();
            scene.background = new THREE.Color(0, 0, 0);
            const view = new StageView(await loadStage(src, st), stem => { const p = tex(stem); p.then(t => t && used.add(t)); return p; });
            await view.preload();               /* every texture in before the picture */
            view.seek(TICKS_PER_SECOND);
            scene.add(view.group);
            const camera = new THREE.PerspectiveCamera(25, W / H, 1, 10);
            fightCamera(camera);
            renderer.render(scene, camera);
            const png = new Uint8Array(await (await canvas.convertToBlob({ type: 'image/png' })).arrayBuffer());
            await api.stagePicPut(st.id, png);
            view.dispose();
        }
    } finally {
        for (const t of used) t.dispose();
        renderer.dispose();
    }
}

/* The camera as a round starts (GameCode.c, the new camera and
 * SetToUseCamera): the look-at between the fighters at x 0, 1.25 over the
 * floor; the eye 1.5 over it and pulled back 8 -- the farthest of its 6..8,
 * the fighters starting apart -- with the game's 25 degree view
 * (LIMEDS_Misc.c). The game's world has z up and the camera behind on -y;
 * here y is up and the front is +z, in fighter units (the world over
 * PlayerSize, 0.0102). */
function fightCamera(camera) {
    const u = v => v / PLAYER_SIZE;
    camera.near = u(0.33);
    camera.far = u(1e5);         /* the game's projection has no far plane (StageViewer.svelte) */
    camera.position.set(0, u(1.5), u(8));
    camera.lookAt(0, u(1.25), 0);
    camera.updateProjectionMatrix();
}
