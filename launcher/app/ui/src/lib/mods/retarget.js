/* REPLACE MODEL onto the fighter's bones from any humanoid rig: the file's
 * bones are known by their names, whatever tool made them, and each is
 * given the fighter's bone that does the same job, so the file's own
 * weights move the model (modelin.js buildSkin).
 *
 * Rigs whose names this reads (a word list, not one table per tool, so
 * their variants -- prefixes, numbers, twist bones -- come too):
 *
 *   Mixamo             mixamorig:Hips, LeftArm, LeftForeArm, LeftUpLeg,
 *                      LeftLeg, LeftToeBase, Spine1, LeftHandIndex1
 *   AccuRig / Character Creator / ActorCore (Reallusion)
 *                      CC_Base_Hip, CC_Base_Pelvis, CC_Base_Waist,
 *                      CC_Base_Spine01, CC_Base_L_Upperarm, CC_Base_L_Calf,
 *                      CC_Base_L_UpperarmTwist01, RL_BoneRoot
 *   ValveBiped (Source) ValveBiped.Bip01_Pelvis, Bip01_Spine4,
 *                      Bip01_L_UpperArm, Bip01_L_Forearm, Bip01_L_Toe0,
 *                      the helpers (Ulna, Wrist, Bicep, Elbow, Trapezius)
 *   3ds Max Biped      Bip001 L UpperArm, Bip001 L Calf, Bip001 HeadNub
 *   Unreal mannequin   pelvis, spine_01, upperarm_l, lowerarm_l, calf_l,
 *                      ball_l, upperarm_twist_01_l
 *   Unity humanoid / VRM  Hips, LeftUpperArm, LeftLowerLeg, UpperChest,
 *                      J_Bip_C_Hips, J_Bip_L_LowerArm
 *   Blender (Rigify, metarig)  DEF-upper_arm.L, DEF-shin.L, DEF-spine.003
 *   Daz Genesis        hip, abdomenLower, lShldrBend, lForearmBend, lShin
 *
 * Roles are the fighter's own names (rig.js boneNames): hips, spine (the
 * chain, mapped in order onto spine1.. and chest), neck, head, and per side
 * clavicle, upperarm, forearm, hand, thigh, shin, foot, toe. Fingers go to
 * the hand; bones with no role (end bones, the face, hair, clothes) to the
 * nearest bone above them that has one. */

const SIDE_L = new Set(['left', 'l', 'lf', 'lft']);
const SIDE_R = new Set(['right', 'r', 'rt', 'rgt']);
const NOISE = /^(mixamorig\d*|valve|biped|bip0*\d*|cc|base|rl|j|c|def|org|mch|sk|b|bn|jnt|joint|bone|bones|char|character|armature|skeleton|rig|deform)$/;
const WEAK = /^(twist\d*|roll\d*|share|adj|adjust|correct|corrective|fix|helper|xtra|extra|aux)$/;
const END = /^(end|nub|tip|top|null|ik|pole|target|ctrl|control|socket|attach|weapon|prop|footsteps)$/;
const FACE = /^(eye|face|jaw|teeth|tongue|brow|lid|lip|cheek|nose|ear|hair|breast|skirt|cape|cloth)/;

function words(name) {
    const s = String(name).split(/[:|]/).pop()
        .replace(/([a-z0-9])([A-Z])/g, '$1 $2').replace(/([A-Z])([A-Z][a-z])/g, '$1 $2');
    return s.toLowerCase().split(/[^a-z0-9]+/).filter(Boolean);
}

/* A file bone's role: { role, weak } (weak: a twist or helper bone, which
 * moves vertices but does not say where the joint is), or null. */
export function humanoidRole(name) {
    let side = '', weak = false;
    const keep = [];
    for (const t of words(name)) {
        if (SIDE_L.has(t)) { side = side || '.L'; continue; }
        if (SIDE_R.has(t)) { side = side || '.R'; continue; }
        if (NOISE.test(t)) continue;
        if (END.test(t)) return null;
        if (WEAK.test(t)) { weak = true; continue; }
        keep.push(t);
    }
    const j = keep.join('');
    if (!j || FACE.test(j)) return null;
    const sided = r => (side ? { role: r + side, weak } : null);
    if (/thumb|index|middle|ring|pinky|pinkie|little|finger|digit|carpal/.test(j)) return sided('finger');
    if (/forearm|lowerarm|lowarm|loarm|elbow|ulna/.test(j)) return sided('forearm');
    if (/upperarm|uparm|humerus|bicep|shldr|^arm\d*$/.test(j)) return sided('upperarm');
    if (/clavicle|clav|collar|shoulder|trapezius/.test(j)) return sided('clavicle');
    if (/hand|wrist|palm/.test(j)) return sided('hand');
    if (/thigh|upleg|upperleg|femur/.test(j) || (side && /^hip\d*$/.test(j))) return sided('thigh');
    if (/calf|shin|knee|lowerleg|lowleg|loleg|^leg\d*$/.test(j)) return sided('shin');
    if (/toe|ball/.test(j)) return sided('toe');
    if (/foot|ankle|heel/.test(j)) return sided('foot');
    if (/^head\d*$/.test(j)) return { role: 'head', weak: weak || !!side };
    if (/neck/.test(j)) return { role: 'neck', weak: weak || !!side };
    if (/^(hips?|pelvis)\d*$/.test(j)) return { role: 'hips', weak: weak || !!side };
    if (/spine|chest|torso|waist|abdomen|belly|ribcage|stomach|^back\d*$/.test(j)) return side ? null : { role: 'spine', weak };
    return null;
}

/* a fighter bone's role, from its name */
function fighterRole(n) {
    let m;
    if (n === 'hips' || n === 'neck' || n === 'head') return n;
    if (/^spine\d+$/.test(n) || n === 'chest') return 'spine';
    if ((m = /^(clavicle|upperarm|forearm|hand|thigh|shin|foot|toe)(\.[LR])$/.exec(n))) return m[1] + m[2];
    return null;
}

const NEXT = { clavicle: 'upperarm', upperarm: 'forearm', forearm: 'hand', thigh: 'shin', shin: 'foot', foot: 'toe' };
const FROM = { hand: 'forearm', toe: 'foot', head: 'neck' };
/* the joints the placement is fitted on: the body, not the arms, which
 * one rig holds out (T) and another down (A) */
const CORE = /^(hips|spine|neck|head|thigh|shin|foot)/;

/* The file's bones on the fighter's:
 *   kind     'names': the fighter's own names (a file from the exporter);
 *            'humanoid': by role
 *   byFile   per file bone, the fighter bone it moves with (-1 none)
 *   prim     fighter bone -> the file bone that says where its joint is
 *   link     fighter bone -> { child, from }: the fighter bone its own
 *            points to (-1 none) and, for an end (hand, head, toe), the
 *            one whose turn it takes
 *   core     fighter bone -> whether the placement fits on it
 * or null when fewer than three bones are known. */
export function mapBones(f, mdl) {
    const nb = mdl.bones.length;
    if (!nb) return null;
    const byName = new Map(f.names.map((n, i) => [n.toLowerCase(), i]));
    const exact = mdl.bones.map(b => byName.get(String(b.name).toLowerCase()) ?? -1);
    if (exact.filter(g => g >= 0).length >= 3) {
        const prim = new Map();
        exact.forEach((g, i) => { if (g >= 0 && !prim.has(g)) prim.set(g, i); });
        return { kind: 'names', byFile: exact, prim, link: new Map(), core: () => true };
    }

    const parent = i => (mdl.bones[i].parent ?? -1);
    const depth = i => { let d = 0; for (let p = parent(i); p >= 0 && d < 1000; p = parent(p)) d++; return d; };
    const order = [...mdl.bones.keys()].sort((a, b) => depth(a) - depth(b));
    const roles = mdl.bones.map(b => humanoidRole(b.name));

    const gOf = new Map(), fspine = [];
    f.names.forEach((n, g) => {
        const r = fighterRole(n);
        if (r === 'spine') fspine.push(g);
        else if (r && !gOf.has(r)) gOf.set(r, g);
    });
    const sk = n => (n === 'chest' ? 1e9 : Number(n.slice(5)));
    fspine.sort((a, b) => sk(f.names[a]) - sk(f.names[b]));

    const byFile = new Array(nb).fill(-1);
    /* the file's spine onto the fighter's, in order, however many each has */
    const fileSpine = order.filter(i => roles[i] && roles[i].role === 'spine' && !roles[i].weak);
    fileSpine.forEach((i, k) => {
        if (fspine.length) byFile[i] = fspine[Math.min(fspine.length - 1, Math.floor((k + 0.5) * fspine.length / fileSpine.length))];
    });
    for (const i of order) {
        const r = roles[i];
        if (!r || r.role === 'spine') continue;
        const role = r.role.startsWith('finger') ? 'hand' + r.role.slice(6) : r.role;
        const g = gOf.get(role);
        if (g !== undefined) byFile[i] = g;
    }
    const prim = new Map();
    for (const i of order) {
        const g = byFile[i], r = roles[i];
        if (g < 0 || !r || r.weak || r.role.startsWith('finger') || prim.has(g)) continue;
        prim.set(g, i);
    }
    /* the rest move with the nearest bone above them that has a role */
    for (const i of order) if (byFile[i] < 0 && parent(i) >= 0) byFile[i] = byFile[parent(i)];
    if (prim.size < 3) return null;

    const roleOfG = new Map();
    for (const [r, g] of gOf) roleOfG.set(g, r);
    fspine.forEach(g => roleOfG.set(g, 'spine'));
    const link = new Map();
    for (const [g, r] of roleOfG) {
        const [base, sd = ''] = r.split(/(?=\.)/);
        let child = -1, from = -1;
        if (NEXT[base] && gOf.has(NEXT[base] + sd)) child = gOf.get(NEXT[base] + sd);
        if (base === 'spine') {
            const k = fspine.indexOf(g);
            child = k + 1 < fspine.length ? fspine[k + 1] : (gOf.get('neck') ?? -1);
        }
        if (base === 'neck') child = gOf.get('head') ?? -1;
        if (FROM[base] && gOf.has(FROM[base] + sd)) from = gOf.get(FROM[base] + sd);
        link.set(g, { child, from });
    }
    return { kind: 'humanoid', byFile, prim, link, core: g => CORE.test(roleOfG.get(g) || '') };
}
