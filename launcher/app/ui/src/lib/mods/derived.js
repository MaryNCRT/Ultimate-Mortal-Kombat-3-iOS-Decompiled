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
