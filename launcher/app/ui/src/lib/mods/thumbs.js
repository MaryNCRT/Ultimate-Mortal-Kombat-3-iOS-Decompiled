/* Small pictures of the game's textures, for the lists: a texture file
 * (PVR or PNG) drawn at most `max` on its longer side, as a data: URL (the
 * page allows no blob: pictures), with its own size. Kept by layer and
 * file; `lazy` makes one only when its place in a list comes into sight. */
import { isPng } from './assets.js';
import { decodePvr } from './formats.js';

const cache = new Map();

export function thumb(src, file, layer = '', max = 128) {
    const key = layer + '|' + file + '|' + max;
    if (!cache.has(key)) cache.set(key, make(src, file, max).catch(() => null));
    return cache.get(key);
}

/* a mod changed: its pictures made again */
export function forgetThumbs() {
    for (const k of [...cache.keys()])
        if (!k.startsWith('|')) cache.delete(k);
}

async function make(src, file, max) {
    const b = await src.read(file);
    if (!b) return null;
    let pic, w, h;
    if (isPng(b)) {
        pic = await createImageBitmap(new Blob([b]));
        w = pic.width; h = pic.height;
    } else {
        const img = decodePvr(b);
        if (!img) return null;
        w = img.width; h = img.height;
        pic = await createImageBitmap(new ImageData(img.rgba, w, h));
    }
    const k = Math.min(1, max / Math.max(w, h));
    const c = new OffscreenCanvas(Math.max(1, Math.round(w * k)), Math.max(1, Math.round(h * k)));
    const g = c.getContext('2d');
    g.imageSmoothingQuality = 'high';
    g.drawImage(pic, 0, 0, c.width, c.height);
    return { url: await dataUrl(await c.convertToBlob({ type: 'image/png' })), w, h };
}

export function dataUrl(blob) {
    return new Promise((res, rej) => {
        const r = new FileReader();
        r.onload = () => res(r.result);
        r.onerror = rej;
        r.readAsDataURL(blob);
    });
}

/* use:lazy={fn}: fn() once the element is in sight */
export function lazy(node, fn) {
    let done = false;
    const io = new IntersectionObserver(es => {
        if (!done && es.some(e => e.isIntersecting)) {
            done = true;
            io.disconnect();
            fn();
        }
    }, { rootMargin: '200px' });
    io.observe(node);
    return { destroy: () => io.disconnect() };
}
