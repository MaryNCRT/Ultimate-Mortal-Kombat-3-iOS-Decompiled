/* STAGE SELECT's pictures for the stages the game has none for. The game
 * ships a CS_BG sheet for five of its stages (debug_menu.c k_stages); for
 * the rest the launcher renders the stage itself -- its scene and meshset,
 * textured, from the front as the viewer opens it -- into
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
            const camera = new THREE.PerspectiveCamera(45, W / H, 1, 10);
            aim(camera, root);
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

/* the camera as the viewer frames a stage: on its core (the larger,
 * far-off pieces -- sky, distant walls -- left out), from the front */
function aim(camera, obj) {
    let box = new THREE.Box3().setFromObject(obj);
    const parts = obj.children.map(o => new THREE.Box3().setFromObject(o)).filter(b => !b.isEmpty());
    const sizes = parts.map(b => b.getSize(new THREE.Vector3()).length()).sort((a, b) => a - b);
    const cut = sizes[Math.floor(sizes.length * 0.7)] || Infinity;
    const near = new THREE.Box3();
    for (const b of parts) if (b.getSize(new THREE.Vector3()).length() <= cut && b.getCenter(new THREE.Vector3()).length() < cut * 3) near.union(b);
    if (!near.isEmpty()) box = near;
    const c = box.getCenter(new THREE.Vector3()), size = box.getSize(new THREE.Vector3()).length() || 1;
    camera.near = size / 500;
    camera.far = size * 50;
    camera.position.set(c.x + size * 0.15, c.y + size * 0.1, c.z + size * 1.1);
    camera.lookAt(c);
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
