/* STAGE SELECT's pictures for the stages the game has none for. The game
 * ships a CS_BG sheet for five of its stages (debug_menu.c k_stages); for
 * the rest the launcher renders the stage itself -- its scene and meshset,
 * textured, from where the fight's camera stands when a round starts -- into
 * "aditional textures/stages/<ID>.png", where the game's st_thumb finds it.
 * Once per stage: a picture already there is kept (the player may have put
 * a better one). */
import * as THREE from 'three';
import { api } from '../platform.js';
import { src, ms, loadCatalog } from './store.svelte.js';
import { stageDesc, textureFile, isPng } from './assets.js';
import { decodePvr } from './formats.js';

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
    const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, preserveDrawingBuffer: true });
    renderer.setSize(W, H, false);
    renderer.outputColorSpace = THREE.SRGBColorSpace;
    const textures = new Map();
    try {
        for (const st of todo) {
            const scene = new THREE.Scene();
            scene.background = new THREE.Color(0x05040f);
            scene.add(new THREE.AmbientLight(0xffffff, 2.2));
            const sun = new THREE.DirectionalLight(0xffffff, 1.2);
            sun.position.set(0.3, 1, 0.8);
            scene.add(sun);
            const root = new THREE.Group();
            const d = await stageDesc(src, st);
            for (const m of d.meshes) root.add(await mesh(m, d.materials[m.material].stem, textures));
            scene.add(root);
            const camera = new THREE.PerspectiveCamera(25, W / H, 1, 10);
            fightCamera(camera);
            renderer.render(scene, camera);
            const png = new Uint8Array(await (await canvas.convertToBlob({ type: 'image/png' })).arrayBuffer());
            await api.stagePicPut(st.id, png);
            root.traverse(o => { if (o.geometry) o.geometry.dispose(); if (o.material) o.material.dispose(); });
        }
    } finally {
        for (const t of textures.values()) { const x = await t; if (x) x.dispose(); }
        renderer.dispose();
    }
}

/* The camera as a round starts (GameCode.c, the new camera and
 * SetToUseCamera): the look-at between the fighters at x 0, 1.25 over the
 * floor; the eye 1.5 over it and pulled back 8 -- the farthest of its 6..8,
 * the fighters starting apart -- with the game's 25 degree view
 * (LIMEDS_Misc.c). The game's world has z up and the camera behind on -y;
 * here y is up and the front is +z, in fighter units (the world over
 * PlayerSize, 0.0102: assets.js STAGE_SCALE). */
const PLAYER_SIZE = 0.0101562496;
function fightCamera(camera) {
    const u = v => v / PLAYER_SIZE;
    camera.near = u(0.5);
    camera.far = u(400);
    camera.position.set(0, u(1.5), u(8));
    camera.lookAt(0, u(1.25), 0);
    camera.updateProjectionMatrix();
}

async function mesh(m, stem, textures) {
    const g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.BufferAttribute(Float32Array.from(m.pos), 3));
    g.setAttribute('uv', new THREE.BufferAttribute(Float32Array.from(m.uv), 2));
    g.setIndex(new THREE.BufferAttribute(Uint32Array.from(m.faces), 1));
    g.computeVertexNormals();
    const map = stem ? await texture(stem, textures) : null;
    return new THREE.Mesh(g, new THREE.MeshLambertMaterial({ map, color: map ? 0xffffff : 0x9a96c8, side: THREE.DoubleSide, alphaTest: 0.35 }));
}

function texture(stem, textures) {
    if (!textures.has(stem))
        textures.set(stem, (async () => {
            const file = textureFile(src.files, stem);
            const b = file && await src.read(file);
            if (!b) return null;
            let t;
            if (isPng(b)) {
                t = new THREE.Texture(await createImageBitmap(new Blob([b]), { imageOrientation: 'none' }));
            } else {
                const img = decodePvr(b);
                if (!img) return null;
                t = new THREE.DataTexture(img.rgba, img.width, img.height, THREE.RGBAFormat);
            }
            t.flipY = false;
            t.colorSpace = THREE.SRGBColorSpace;
            t.wrapS = t.wrapT = THREE.RepeatWrapping;
            t.magFilter = THREE.LinearFilter;
            t.needsUpdate = true;
            return t;
        })());
    return textures.get(stem);
}
