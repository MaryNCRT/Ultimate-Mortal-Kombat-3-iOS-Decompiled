/* Where things go: the folders of an export and of a mod.
 *
 *     Characters/<fighter>/    a fighter's models and textures
 *     Stages/<stage>/          a stage's model and textures
 *     Models/<model>/          the other models (finishers, props)
 *     Interface/               the menus' pictures: textures no model uses
 *     Effects/                 textures only the other models use
 *
 * The game finds a mod's files by name in any of its folders
 * (runtime/mods.c), so the folders are for the player, not the game. */
import { parseMeshset } from './formats.js';

export const GROUPS = [['characters', 'Characters'], ['stages', 'Stages'], ['interface', 'Interface'], ['effects', 'Effects']];

/* the folder of a catalogue item: kind as in the catalogue */
export function homeOf(kind, item) {
    if (kind === 'characters') return 'Characters/' + item.name;
    if (kind === 'stages') return 'Stages/' + item.name;
    if (kind === 'models') return 'Models/' + item.name;
    return '';
}

let g_owners = null;

/* Every texture's owner, by stem (upper case): { group, name, home }.
 *   a texture a stage's meshset uses                 that stage (JADE_SKY
 *                                                    is Jade's Desert's)
 *   a stem that names a fighter, first at its start  that fighter (portraits)
 *   a texture another meshset uses                  Effects
 *   anything else                                   Interface */
export async function textureOwners(src, cat) {
    if (g_owners && g_owners.cat === cat) return g_owners.map;
    const map = new Map();
    const stems = cat.textures.map(t => t.name.toUpperCase());
    /* longest first: OLDSUBZERO before SUBZERO, OLDSMOKE before SMOKE */
    const fighters = cat.characters.slice().sort((a, b) => b.id.length - a.id.length);
    const read = async file => {
        try { return parseMeshset((await src.read(file)) || new Uint8Array(0)); } catch { return []; }
    };
    const stageOf = new Map(), modelUse = new Set();
    for (const st of cat.stages)
        for (const m of await read(st.meshset))
            if (m.texture && !stageOf.has(m.texture.toUpperCase())) stageOf.set(m.texture.toUpperCase(), st);
    for (const md of cat.models)
        for (const m of await read(md.id))
            if (m.texture) modelUse.add(m.texture.toUpperCase());
    const fighter = (h, start) => fighters.find(f => (start ? h.startsWith(f.id) : h.includes(f.id)));

    for (const s of stems) {
        /* the smaller copies go with their texture */
        const full = s.replace(/_(LOW|LITE)(\d*)$/, '$2');
        const st = stageOf.get(full) || stageOf.get(s);
        let f;
        if (st) map.set(s, { group: 'stages', name: st.name, home: homeOf('stages', st) });
        else if (/COMPLETEMAP/.test(s))         /* a stage's map on the menus */
            map.set(s, { group: 'interface', name: 'Interface', home: 'Interface' });
        else if ((f = fighter(full, true)) || (f = fighter(full, false)))
            map.set(s, { group: 'characters', name: f.name, home: homeOf('characters', f) });
        else if (modelUse.has(full) || modelUse.has(s))
            map.set(s, { group: 'effects', name: 'Effects', home: 'Effects' });
        else
            map.set(s, { group: 'interface', name: 'Interface', home: 'Interface' });
    }
    g_owners = { cat, map };
    return map;
}

/* the folder a texture goes to in a mod */
export async function textureDir(src, cat, stem) {
    return cat ? textureHome(await textureOwners(src, cat), stem) : '';
}

/* ... and in the exports: a fighter's or a stage's in its Textures */
export const exportTexDir = home => (/^(Characters|Stages|Models)\//.test(home) ? home + '/Textures' : home || 'Interface');

/* a texture's folder, from the owners (Interface when not known) */
export const textureHome = (owners, stem) => (owners.get(stem.toUpperCase()) || { home: 'Interface' }).home;
