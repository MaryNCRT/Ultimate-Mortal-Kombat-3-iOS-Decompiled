/* The MODS section's state, shared by its three pages: the mod list, the
 * game's catalogue, what the viewer has open (the exporter starts from it). */
import { api } from '../platform.js';
import { catalog, loadFighter } from './assets.js';

export const MODS_TABS = [['mgr', 'modsMgr'], ['view', 'modsView'], ['exp', 'modsExp']];
export const VIEW_KINDS = [['characters', 'vwChars'], ['stages', 'vwStages'], ['models', 'vwModels'], ['textures', 'vwTex']];

export const ms = $state({
    tab: 'mgr',
    /* MOD MANAGER */
    list: [], dir: '', busy: false, open: null, filter: 'all',
    /* the game's files */
    files: null, cat: null, noGame: false,
    /* the viewer: kind of thing, the chosen one, its settings */
    kind: 'characters', pick: null, search: '',
    pose: 'tpose', clip: -1, frame: 0, playing: false, outfit: '', bones: false, wire: false,
    loading: false, error: '',
});

/* ------------------------------------------------------------- mods -- */

export async function refreshMods() {
    const r = await api.modsList();
    if (!r) return;
    ms.list = r.mods;
    ms.dir = r.dir;
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
    async read(rel) {
        if (cache.has(rel)) return cache.get(rel);
        const b = await api.resRead(rel);
        if (b) {
            cache.set(rel, b);
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

/* fighters once loaded, by id: the bind pose fit takes a moment */
const fighters = new Map();
export async function fighter(ch) {
    if (!fighters.has(ch.id)) fighters.set(ch.id, loadFighter(src, ch));
    try {
        return await fighters.get(ch.id);
    } catch (e) {
        fighters.delete(ch.id);
        throw e;
    }
}

/* the viewer's selection carried to the exporter */
export function exportThis() {
    ms.tab = 'exp';
}
