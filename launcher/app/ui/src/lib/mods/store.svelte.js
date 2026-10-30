/* The MODS section's state, shared by its three pages: the mod list, the
 * game's catalogue, what the viewer has open (the exporter starts from it). */
import { api } from '../platform.js';
import { catalog, loadFighter } from './assets.js';

export const MODS_TABS = [['mgr', 'modsMgr'], ['hub', 'modsHub'], ['view', 'modsView'], ['stage', 'modsStage'], ['edit', 'modsEdit'], ['tex', 'modsTex'], ['exp', 'modsExp']];
/* the stages have a viewer of their own (STAGE VIEWER, StageViewer.svelte) */
export const VIEW_KINDS = [['characters', 'vwChars'], ['models', 'vwModels'], ['textures', 'vwTex']];

export const ms = $state({
    tab: 'mgr',
    /* MOD MANAGER */
    list: [], dir: '', busy: false, open: null, filter: 'all',
    /* the game's files */
    files: null, cat: null, noGame: false,
    /* the viewer: kind of thing, the chosen one, its settings */
    kind: 'characters', pick: null, search: '',
    pose: 'tpose', clip: -1, frame: 0, playing: false, speed: 1, loop: true, solo: false, outfit: '', bones: false, wire: false, solid: false, weights: false, floor: true, extras: true,
    bgStage: '',                        /* STAGE: the stage behind a fighter ('' none) */
    loading: false, error: '',
    /* SMOOTH MODEL (0 off, 1 or 2: the game's weld + Catmull-Clark level)
     * and SMOOTH ANIMATIONS (frames blended while playing), as in the game */
    smoothModel: 0, smoothAnims: false,
    /* what the viewer and the editor read: the game alone (''), the game
     * with every mod that is on ('*'), or with one mod (its folder) */
    layer: '',
    /* TEXTURES by fighter (a fighter's id, '' all); `rev` counts REFRESH */
    texFighter: '', rev: 0,
    /* REPLACE TEXTURE also writes the game's smaller copies (_LOW, _LITE) */
    autoSmall: true,
    autoIce: true,                      /* REPLACE TEXTURE: its _ICE copy too (image.js icePicture) */
    /* the CHARACTER EDITOR: the fighter, the mod its changes go to */
    edFighter: 0, edMod: '', edPage: 'attacks',
    /* TEXTURES: the group (mods/owners.js GROUPS), the fighter or stage */
    txGroup: 'characters', txItem: '',
});

/* ------------------------------------------------------------- mods -- */

export async function refreshMods() {
    const r = await api.modsList();
    if (!r) return;
    ms.list = r.mods;
    ms.dir = r.dir;
}

/* REFRESH: the mods' folders read again, for changes made in them outside
 * the launcher; the viewer opens what it shows again (ms.rev) */
export async function reloadMods() {
    await refreshMods();
    forgetMod();
    ms.rev++;
}

/* the order as it stands, to mods/mods.ini (the game reads it at start) */
export async function saveOrder() {
    await api.modsSave(ms.list.map(m => ({ folder: m.folder, on: m.on })));
    await refreshMods();
}

export async function moveMod(i, d) {
    const j = i + d;
    if (j < 0 || j >= ms.list.length) return;
    const l = ms.list.slice();
    [l[i], l[j]] = [l[j], l[i]];
    ms.list = l;
    await saveOrder();
}

export async function toggleMod(i) {
    ms.list[i].on = !ms.list[i].on;
    await saveOrder();
}

/* ------------------------------------------------------- game files -- */

const cache = new Map();
let cacheBytes = 0;

/* the files for the readers (assets.js): res/ through the main process,
 * the last ~200 MB kept */
export const src = {
    files: [],
    async read(rel, layer = ms.layer) {
        const key = layer + '|' + rel;
        if (cache.has(key)) return cache.get(key);
        const b = await api.resRead(rel, layer);
        if (b) {
            cache.set(key, b);
            cacheBytes += b.length;
            for (const [k, v] of cache) {
                if (cacheBytes < 200 << 20) break;
                cache.delete(k);
                cacheBytes -= v.length;
            }
        }
        return b;
    },
};

export async function loadCatalog(force) {
    if (ms.cat && !force) return;
    const files = (await api.resFiles()) || [];
    src.files = files;
    ms.noGame = !files.some(f => /\.skin$/i.test(f));
    ms.cat = catalog(files);
    ms.files = files.length;
}

/* fighters once loaded, by layer and id: the bind pose fit takes a moment */
const fighters = new Map();
export async function fighter(ch) {
    const key = ms.layer + '|' + ch.id;
    if (!fighters.has(key)) fighters.set(key, loadFighter(src, ch));
    try {
        return await fighters.get(key);
    } catch (e) {
        fighters.delete(key);
        throw e;
    }
}

/* what REPLACE MODEL fits a model to: the fighter's bones and skin as the
 * game ships them, so an earlier model's bones and weights are not under
 * it, with the animations the game will play -- the mod's own .skinanim
 * when it has one (REPLACE ANIMATION, corrective layers), as the game
 * reads it -- so the window plays what the fight will */
export async function gameFighter(ch) {
    const layer = ms.layer, key = 'imp:' + layer + '|' + ch.id;
    const from = { get files() { return src.files; }, read: rel => src.read(rel, /\.skinanim$/i.test(rel) ? layer : '') };
    if (!fighters.has(key)) fighters.set(key, loadFighter(from, ch));
    try {
        return await fighters.get(key);
    } catch (e) {
        fighters.delete(key);
        throw e;
    }
}

/* A mod's files changed (a model or an animation brought in): read them again. */
export function forgetMod() {
    for (const k of [...cache.keys()]) if (!k.startsWith('|')) { cacheBytes -= cache.get(k).length; cache.delete(k); }
    for (const k of [...fighters.keys()]) if (!k.startsWith('|')) fighters.delete(k);
}

/* The fight engine's tables (lib/mods.js gameTables), once. */
let tables = null;
export async function gameTables() {
    if (!tables) tables = (await api.gameTables()) || {};
    return tables;
}

/* the viewer's selection carried to the exporter */
export function exportThis() {
    ms.tab = 'exp';
}
