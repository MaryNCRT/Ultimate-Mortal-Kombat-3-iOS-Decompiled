/* What belongs to one fighter beside the fighter: the other models the game
 * keeps of them (headless, floating, babality, cut in half, diced, in the
 * pit's spikes ...) and their textures sorted by use. Found by name: the
 * game's .meshset files name the fighter they show last (JAX_DICE_KANO is
 * Kano, cut by Jax) or first (KANO_DECAP). */

/* a model's name to what it is, first match wins */
const KINDS = [
    [/^BABALITY_/, 'Babality'],
    [/_DECAP$/, 'Headless'],
    [/_FLOAT$/, 'Floating'],
    [/^CUTUP1_/, 'Cut in half'],
    [/^CUTUP_BY_([A-Z]+)_/, 'Cut in half by'],
    [/^JAX_DICE_/, 'Diced (Jax)'],
    [/^JAX_SQUASH_/, 'Squashed (Jax)'],
    [/^LAIR_.*2$/, "Scorpion's Lair 2"],
    [/^LAIR_/, "Scorpion's Lair"],
    [/^SUBWAY_.*2$/, 'Subway 2'],
    [/^SUBWAY_/, 'Subway'],
    [/^MILEENA_SUK_/, 'Inhaled (Mileena)'],
    [/^NAILSCARED_/, 'Nailed (Mileena)'],
    [/^SHEEVA_SKINRIP_/, 'Skinned (Sheeva)'],
    [/^STRYKER_TAZER_|_TAZER_/, 'Tased'],
    [/^ANIMALITY_/, 'Animality'],
    [/^FRIENDSHIP_/, 'Friendship'],
    [/_KOD_|^KOD_/, 'Kiss of death'],
    [/_MOUTH_/, 'Mouth'],
    [/_EXPLODE\d*$/, 'Exploded'],
];

const pretty = s => s.replace(/_/g, ' ').toLowerCase().replace(/\b\w/g, c => c.toUpperCase());

function kindOf(name) {
    for (const [re, what] of KINDS) {
        const m = re.exec(name);
        if (m) return { key: what, arg: m[1] ? pretty(m[1]) : '' };
    }
    return null;
}

/* the fighter a model shows: its last word, else its first, else the
 * fighter its name starts with (SONYAANIMALITYDEATH) */
function ownerOf(name, ids) {
    const words = name.split('_');
    const last = words[words.length - 1].replace(/\d+$/, '');
    if (ids.has(last)) return last;
    if (ids.has(words[0])) return words[0];
    for (const id of [...ids].sort((a, b) => b.length - a.length))
        if (name.startsWith(id)) return id;
    return null;
}

/* fighter id -> [{ item (the catalogue's model), key, arg }]: key is what
 * it is (KINDS, translated by the page: i18n dv), arg the one who did it */
let g_cache = null;
export function derivedModels(cat) {
    if (g_cache && g_cache.cat === cat) return g_cache.map;
    const ids = new Set(cat.characters.map(c => c.id));
    const map = new Map();
    for (const m of cat.models) {
        const name = m.name.toUpperCase();
        if (/_STANDARD$/.test(name)) continue;
        const id = ownerOf(name, ids);
        if (!id) continue;
        if (!map.has(id)) map.set(id, []);
        map.get(id).push({ item: m, ...(kindOf(name) || { key: pretty(m.name), arg: '' }) });
    }
    for (const l of map.values()) l.sort((a, b) => (a.key + a.arg).localeCompare(b.key + b.arg));
    g_cache = { cat, map };
    return map;
}

/* A texture's place among a fighter's, a stage's or the menus':
 *   outfits     <F>_DIFFUSE, _DIFFUSE2, _ICE ... and their smaller copies
 *   portraits   the pictures of the menus (PORTRAIT, VERSUS, the select)
 *   babality    the baby
 *   other       the rest
 * For the menus and the effects, the name's first word, when several
 * share it. Returns [[group, [textures]]] in that order. */
export function textureGroups(list, owner) {
    const groups = new Map();
    const put = (g, t) => { if (!groups.has(g)) groups.set(g, []); groups.get(g).push(t); };
    if (owner === 'characters') {
        for (const t of list) {
            const n = t.name.toUpperCase();
            put(/BAB(Y|ALITY)/.test(n) ? 'Babality'
                : /PORTRAIT|VERSUS|VS_|SELECT|CARD|BIO|FACE|IPAD/.test(n) ? 'Portraits'
                : /_DIFFUSE|_ICE|_SKIN/.test(n) ? 'Outfits' : 'Other', t);
        }
        return ['Outfits', 'Portraits', 'Babality', 'Other'].filter(g => groups.has(g)).map(g => [g, groups.get(g)]);
    }
    if (owner === 'stages') return list.length ? [['Stage', list]] : [];
    const first = t => t.name.toUpperCase().replace(/^IPAD_?/, '').split(/[_\d]/)[0] || 'Other';
    const count = new Map();
    for (const t of list) count.set(first(t), (count.get(first(t)) || 0) + 1);
    for (const t of list) put(count.get(first(t)) >= 3 ? pretty(first(t)) : 'Other', t);
    return [...groups.keys()].sort((a, b) => (a === 'Other') - (b === 'Other') || a.localeCompare(b)).map(g => [g, groups.get(g)]);
}

/* SPECIAL MODELS (Mary, 2026-11-12): the models that belong to a move or an
 * ending rather than to a fighter's own roster slot.
 *
 * Most of them are hidden inside a fighter's own <F>_STANDARD.meshset, next
 * to the fighter's body: Nightwolf's carries Raiden (eleven meshes
 * Raiden001..011, textured RAYDEN_DIFFUSE -- Mary: it is his Friendship's),
 * the animals the animalities turn into, arcade cabinets, a snowman ...
 * They are found by what the meshset says, nothing more: meshes of 300
 * triangles or more whose texture is not the fighter's own, grouped by
 * texture and named by their meshes' common name. What is in every
 * fighter's file (the white shell, the skeletons, the ice man) and a
 * roster fighter's own skin seen in another's file are left out.
 *
 * Then the separate .meshset files named for one (ANIMALITY_..., FRIENDSHIP_...).
 *
 * Returns [{ id, name, file, only (mesh indices, or null for all), owner
 * (fighter id or ''), key (i18n dv: 'Friendship', 'Inside', ...), frames }],
 * Raiden first. `read(file)` -> bytes; parseMeshset from formats.js. */
const SPECIAL_SKIP = /^(WHITE|HIRESSKELETON|ROBOT_SKELETON|FROZEN_MANB|PARTICLE.*)$/i;
/* what is known of one beyond its meshes: who said so */
const SPECIAL_KNOWN = { RAYDEN_DIFFUSE: { name: 'Raiden', key: 'Friendship' } };   /* Mary, 2026-11-12 */

let g_special = null;
export async function specialModels(cat, read, parseMeshset) {
    if (g_special && g_special.cat === cat) return g_special.list;
    const ids = cat.characters.map(c => c.id);
    const skins = new RegExp('^(' + ids.join('|') + ')_DIFFUSE', 'i');
    const out = [];
    for (const ch of cat.characters) {
        const file = ch.file + '.meshset';
        let set;
        try { set = parseMeshset((await read(file)) || new Uint8Array(0)); } catch { continue; }
        const groups = new Map();
        set.forEach((m, i) => {
            const t = (m.texture || '').toUpperCase(), n = (m.name || '').toUpperCase().replace(/[^A-Z0-9]/g, '');
            if (!t || SPECIAL_SKIP.test(t) || t.startsWith(ch.id) || /^EVENT/.test(n)) return;
            /* a skin: a roster fighter's (<F>_DIFFUSE), or Sonya's own
             * frozen copy ('SONIA DIFF') */
            if ((skins.test(t) || /DIFF/.test(t)) && !SPECIAL_KNOWN[t]) return;
            if (!groups.has(t)) groups.set(t, []);
            groups.get(t).push({ i, name: m.name || '', tris: m.faces.length / 3 });
        });
        /* one name per group: the stem most of its meshes share */
        const byName = new Map();
        for (const [t, ms] of groups) {
            if (Math.max(...ms.map(m => m.tris)) < 300) continue;
            const count = new Map();
            for (const m of ms) {
                const s = m.name.replace(/[\d\s_]+$/, '').replace(/STAND$/i, '').trim() || t;
                count.set(s, (count.get(s) || 0) + 1);
            }
            const stem = [...count].sort((a, b) => b[1] - a[1])[0][0];
            const known = SPECIAL_KNOWN[t];
            /* a modeller's default name (Object006, Cylinder013) or one
             * that holds the texture's (Skin_ZSphereTopology_Bunny002):
             * the texture's */
            const words = stem.replace(/([a-z])([A-Z])/g, '$1_$2').toUpperCase().split(/[_\s]+/);
            const generic = /^(OBJECT|CYLINDER|BOX|RECTANGLE|PLANE|SPHERE|MESH)$/.test(words[0]);
            const name = known ? known.name : pretty(generic || words.includes(t) ? t : words.join('_'));
            const g = byName.get(name) || { name, only: [], textures: [], key: known ? known.key : 'Inside' };
            g.only.push(...ms.map(m => m.i));
            g.textures.push(t);
            byName.set(name, g);
        }
        for (const g of byName.values())
            out.push({ id: 'special:' + ch.id + ':' + g.name, name: g.name, file, only: g.only.sort((a, b) => a - b),
                owner: ch.id, key: g.key, frames: g.only.length, textures: g.textures });
    }
    for (const m of cat.models) {
        const k = /^(ANIMALITY|FRIENDSHIP)_(.+)$/i.exec(m.name);
        if (!k) continue;
        const who = ids.find(id => k[2].toUpperCase().startsWith(id)) || '';
        out.push({ id: m.id, name: pretty(m.name), file: m.id, only: null, owner: who,
            key: k[1].toUpperCase() === 'ANIMALITY' ? 'Animality' : 'Friendship', frames: 0, textures: [] });
    }
    const rank = s => (s.key === 'Friendship' && s.only ? 0 : 1);
    out.sort((a, b) => rank(a) - rank(b) || a.owner.localeCompare(b.owner) || a.name.localeCompare(b.name));
    g_special = { cat, list: out };
    return out;
}
