/* Pictures the player brings in: any format the window decodes (PNG, JPEG,
 * WebP, GIF, BMP, ICO, AVIF, SVG) and TGA, which it does not, made PNG; and a
 * replaced texture's smaller copies, the ones the game takes by quality
 * (X_LOW, X_LITE; X2_LOW, X2_LITE, X_LITE2, X_LOW2 for the second outfit). */
import { textureFile, isPng } from './assets.js';
import { decodePvr } from './formats.js';

export const PICTURE_EXT = ['png', 'jpg', 'jpeg', 'jfif', 'webp', 'gif', 'bmp', 'ico', 'avif', 'svg', 'tga'];

/* TGA: true colour or grey, plain or RLE, 8/16/24/32 bits */
function decodeTga(b) {
    const idLen = b[0], cmap = b[1], type = b[2];
    const w = b[12] | (b[13] << 8), h = b[14] | (b[15] << 8), bpp = b[16], desc = b[17];
    if (cmap || ![2, 3, 10, 11].includes(type) || !w || !h) return null;
    const px = bpp >> 3, rle = type >= 9, grey = type === 3 || type === 11;
    const out = new Uint8ClampedArray(w * h * 4);
    let p = 18 + idLen + (b[5] | (b[6] << 8)) * ((b[7] + 7) >> 3), i = 0;
    const put = q => {
        let r, g, bl, a = 255;
        if (grey) r = g = bl = b[q];
        else if (px === 2) {
            const v = b[q] | (b[q + 1] << 8);
            r = ((v >> 10) & 31) * 255 / 31; g = ((v >> 5) & 31) * 255 / 31; bl = (v & 31) * 255 / 31;
        } else { bl = b[q]; g = b[q + 1]; r = b[q + 2]; if (px === 4) a = b[q + 3]; }
        const x = i % w, y = Math.floor(i / w);
        const ox = desc & 0x10 ? w - 1 - x : x, oy = desc & 0x20 ? y : h - 1 - y;
        const o = (oy * w + ox) * 4;
        out[o] = r; out[o + 1] = g; out[o + 2] = bl; out[o + 3] = a;
        i++;
    };
    while (i < w * h && p < b.length) {
        if (!rle) { put(p); p += px; continue; }
        const c = b[p++], n = (c & 127) + 1;
        if (c & 128) { for (let k = 0; k < n; k++) put(p); p += px; }
        else for (let k = 0; k < n; k++) { put(p); p += px; }
    }
    return new ImageData(out, w, h);
}

const MIME = [
    [b => isPng(b), 'image/png'], [b => b[0] === 0xff && b[1] === 0xd8, 'image/jpeg'],
    [b => b[0] === 0x47 && b[1] === 0x49, 'image/gif'], [b => b[0] === 0x42 && b[1] === 0x4d, 'image/bmp'],
    [b => b[8] === 0x57 && b[9] === 0x45 && b[10] === 0x42 && b[11] === 0x50, 'image/webp'],
    [b => b[4] === 0x66 && b[5] === 0x74 && b[6] === 0x79 && b[7] === 0x70, 'image/avif'],
    [b => b[0] === 0 && b[1] === 0 && b[2] === 1 && b[3] === 0, 'image/x-icon'],
    [b => /<svg|<\?xml/i.test(new TextDecoder().decode(b.subarray(0, 512))), 'image/svg+xml'],
];

function b64(b) {
    let s = '';
    for (let i = 0; i < b.length; i += 0x8000) s += String.fromCharCode.apply(null, b.subarray(i, i + 0x8000));
    return btoa(s);
}

/* the picture in `bytes` as something to draw, or null */
export async function decodePicture(bytes) {
    const b = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
    const known = MIME.find(([t]) => t(b));
    if (!known) {                       /* TGA has no signature to tell it by */
        const d = decodeTga(b);
        return d ? await createImageBitmap(d) : null;
    }
    /* through an <img>, which every format above decodes (SVG too); the
     * page allows data: pictures */
    const img = new Image();
    img.src = 'data:' + known[1] + ';base64,' + b64(b);
    await img.decode();
    return img;
}

/* `pic` drawn at w x h (its own size when 0), as PNG bytes */
export async function pngOf(pic, w = 0, h = 0) {
    w = w || pic.width || pic.naturalWidth;
    h = h || pic.height || pic.naturalHeight;
    const c = new OffscreenCanvas(w, h), g = c.getContext('2d');
    g.imageSmoothingEnabled = true;
    g.imageSmoothingQuality = 'high';
    g.drawImage(pic, 0, 0, w, h);
    return new Uint8Array(await (await c.convertToBlob({ type: 'image/png' })).arrayBuffer());
}

/* any picture as PNG, at most `max` on its longer side (0: as it is) */
export async function toPng(bytes, max = 0) {
    const pic = await decodePicture(bytes);
    if (!pic) return null;
    let w = pic.width || pic.naturalWidth, h = pic.height || pic.naturalHeight;
    if (max && Math.max(w, h) > max) {
        const k = max / Math.max(w, h);
        w = Math.max(1, Math.round(w * k));
        h = Math.max(1, Math.round(h * k));
    }
    return pngOf(pic, w, h);
}

/* the size of a game texture file, PNG or PVR */
async function sizeOf(src, file) {
    const b = await src.read(file);
    if (!b) return null;
    if (isPng(b)) return { w: (b[16] << 24 | b[17] << 16 | b[18] << 8 | b[19]) >>> 0, h: (b[20] << 24 | b[21] << 16 | b[22] << 8 | b[23]) >>> 0 };
    const img = decodePvr(b);
    return img && { w: img.width, h: img.height };
}

/* the game's smaller copies of texture `stem`, with their sizes */
export async function smallerCopies(src, stem) {
    const s = stem.toUpperCase();
    const names = [s + '_LOW', s + '_LITE'];
    if (/2$/.test(s)) names.push(s.slice(0, -1) + '_LITE2', s.slice(0, -1) + '_LOW2');
    const out = [];
    for (const n of names) {
        const file = textureFile(src.files, n);
        const size = file && await sizeOf(src, file);
        if (size) out.push({ stem: n, ...size });
    }
    return out;
}

/* The frozen look of a picture, as the game's own _ICE textures are made:
 * its brightness on a pale blue -- measured on SONYA, SCORPION, KITANA and
 * SINDEL_DIFFUSE against their _DIFFUSE_ICE: blue full, red 113 + 0.55 and
 * green 205 + 0.19 of the brightness. Transparency kept. */
export function icePicture(pic) {
    const w = pic.width || pic.naturalWidth, h = pic.height || pic.naturalHeight;
    const c = new OffscreenCanvas(w, h), g = c.getContext('2d');
    g.drawImage(pic, 0, 0);
    const d = g.getImageData(0, 0, w, h), px = d.data;
    for (let i = 0; i < px.length; i += 4) {
        const L = 0.299 * px[i] + 0.587 * px[i + 1] + 0.114 * px[i + 2];
        px[i] = Math.min(255, 113 + 0.55 * L);
        px[i + 1] = Math.min(255, 205 + 0.19 * L);
        px[i + 2] = 255;
    }
    g.putImageData(d, 0, 0);
    return c;
}

/* the frozen copy a texture has: <stem>_ICE when the game has one (a
 * fighter's <NAME>_DIFFUSE_ICE) or the texture is a replaced model's
 * material (<skin>_M<n>, runtime/skin_mats.c); its size, the game's */
async function iceOf(src, stem) {
    if (/_ICE\d*$/i.test(stem)) return null;
    const n = stem.toUpperCase() + '_ICE';
    const file = textureFile(src.files, n);
    if (file) return { stem: n, ...(await sizeOf(src, file) || {}) };
    return /_M\d+$/i.test(stem) ? { stem: n } : null;
}

/* REPLACE TEXTURE: the picture as `stem`.png in the mod, in its folder
 * `dir` (mods/owners.js), and, with `auto`, each smaller copy the game has,
 * at that copy's own size; with `ice`, its frozen copy too (icePicture),
 * and that one's smaller copies. Returns the files written. */
export async function putTexture(api, src, mod, stem, bytes, auto, dir = '', ice = true) {
    const pic = await decodePicture(bytes);
    if (!pic) return null;
    const at = name => (dir ? dir + '/' : '') + name;
    const files = [at(stem + '.png')];
    await api.modsPut(mod, files[0], await pngOf(pic));
    if (auto)
        for (const c of await smallerCopies(src, stem)) {
            await api.modsPut(mod, at(c.stem + '.png'), await pngOf(pic, c.w, c.h));
            files.push(at(c.stem + '.png'));
        }
    const ic = ice && await iceOf(src, stem);
    if (ic) {
        const frozen = icePicture(pic);
        await api.modsPut(mod, at(ic.stem + '.png'), await pngOf(frozen, ic.w || 0, ic.h || 0));
        files.push(at(ic.stem + '.png'));
        if (auto)
            for (const c of await smallerCopies(src, ic.stem)) {
                await api.modsPut(mod, at(c.stem + '.png'), await pngOf(frozen, c.w, c.h));
                files.push(at(c.stem + '.png'));
            }
    }
    return files;
}
