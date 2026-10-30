/* STAGE VIEWER's edits (ui/src/lib/mods/stageedit.js) and the export of
 * shape keys (export.js, usd.js): run with `node test/stageedit.test.mjs`. */
import assert from 'node:assert/strict';
import { curve, trsAt, shapeAt, putKey, dropKey, newEl, frameOf, shapedPositions, localMatrix, saved } from '../ui/src/lib/mods/stageedit.js';
import { toGlb, toFbx } from '../ui/src/lib/mods/export.js';
import { stageOf, writeUsda, writeUsdc } from '../ui/src/lib/mods/usd.js';

const near = (a, b, e = 1e-6) => assert.ok(Math.abs(a - b) < e, a + ' != ' + b);

/* curves: hold outside, straight when linear, eased (flat at the ends) when bezier */
const lin = [{ f: 1, v: 0, i: 'linear' }, { f: 11, v: 10, i: 'linear' }];
near(curve(lin, 0, k => k.v), 0);
near(curve(lin, 6, k => k.v), 5);
near(curve(lin, 20, k => k.v), 10);
const bez = [{ f: 1, v: 0, i: 'bezier' }, { f: 11, v: 10, i: 'bezier' }];
near(curve(bez, 6, k => k.v), 5);
assert.ok(curve(bez, 2, k => k.v) < 1, 'bezier eases in');
/* a turn is flat: no overshoot past the middle key */
const peak = [{ f: 1, v: 0 }, { f: 5, v: 10 }, { f: 9, v: 0 }];
for (let f = 1; f <= 9; f += 0.25) assert.ok(curve(peak, f, k => k.v) <= 10 + 1e-9);

/* keys on an element */
const pos = new Float32Array([0, 0, 0, 2, 0, 0, 0, 2, 0]);
const el = newEl('THING', pos);
assert.deepEqual(el.pivot, [1, 1, 0]);
putKey(el.keys, { f: 1, T: [0, 0, 0], R: [0, 0, 0], S: [1, 1, 1], i: 'linear' });
putKey(el.keys, { f: 11, T: [10, 0, 0], R: [0, 90, 0], S: [2, 2, 2], i: 'linear' });
near(trsAt(el, 6).T[0], 5);
near(trsAt(el, 6).R[1], 45);
near(trsAt(el, 6).S[2], 1.5);
el.pending = { f: 6, trs: { T: [7, 0, 0], R: [0, 0, 0], S: [1, 1, 1] } };
near(trsAt(el, 6).T[0], 7);
near(trsAt(el, 7).T[0], 6);
el.pending = null;
assert.equal(dropKey(el.keys, 11), true);
assert.equal(el.keys.length, 1);
/* the pivot stays put under a scale */
el.keys = [];
el.base = { T: [0, 0, 0], R: [0, 0, 0], S: [3, 3, 3] };
const M = localMatrix(el, 1).elements;
near(1 * M[0] + 1 * M[4] + M[12], 1);
near(1 * M[1] + 1 * M[5] + M[13], 1);

/* shape keys */
el.base = { T: [0, 0, 0], R: [0, 0, 0], S: [1, 1, 1] };
el.basis = { 2: [0, 1, 0] };
el.shapes.push({ name: 'Up', value: 0.5, mute: false, d: { 1: [0, 4, 0] }, keys: [] });
let p = shapedPositions(el, pos, 1);
near(p[5 - 1], 2); near(p[7], 3);           /* vertex 1 y = 2, vertex 2 y = 3 */
p = shapedPositions(el, pos, 1, -1);
near(p[4], 0);
putKey(el.shapes[0].keys, { f: 1, v: 0, i: 'linear' });
putKey(el.shapes[0].keys, { f: 5, v: 1, i: 'linear' });
near(shapeAt(el.shapes[0], 3), 0.5);
assert.equal(frameOf(1, 30), 1);
assert.equal(frameOf(31, 30), 1);
assert.equal(frameOf(30, 30), 30);
const kept = saved({ v: 1, els: { 'stage:0': el, 'stage:1': newEl('EMPTY', pos) } });
assert.deepEqual(Object.keys(kept.els), ['stage:0']);

/* the export: one mesh on one bone, a shape key keyed over 3 frames */
const R = [1, 0, 0, 0, 1, 0, 0, 0, 1];
const desc = {
    name: 'T_STAGE', fps: 30,
    meshes: [{ name: 'THING', pos, uv: new Float32Array(6), faces: new Uint32Array([0, 1, 2]), material: 0,
        skin: { idx: new Int32Array(12), wt: new Float32Array([1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0]) },
        morphs: [{ name: 'Up', delta: new Float32Array([0, 0, 0, 0, 4, 0, 0, 0, 0]), weight: 0 }] }],
    materials: [{ name: 'M', texture: '', blend: 'opaque', color: [1, 1, 1], alpha: 1, unlit: true }],
    skeleton: { names: ['THING'], parent: [-1], rest: [{ R, T: [0, 0, 0] }] },
    clips: [{ name: 'Stage', poses: [0, 1, 2].map(() => [{ R, T: [0, 0, 0] }]), weights: { 0: new Float32Array([0, 0.5, 1]) } }],
};
const glb = Object.values(toGlb(desc))[0];
const jl = new DataView(glb.buffer, glb.byteOffset).getUint32(12, true);
const gj = JSON.parse(new TextDecoder().decode(glb.subarray(20, 20 + jl)));
assert.equal(gj.meshes[0].primitives[0].targets.length, 1);
assert.deepEqual(gj.meshes[0].extras.targetNames, ['Up']);
const wch = gj.animations[0].channels.find(c => c.target.path === 'weights');
assert.ok(wch && gj.nodes[wch.target.node].mesh === 0, 'a weights channel on the mesh node');
assert.equal(gj.accessors[gj.animations[0].samplers[wch.sampler].output].count, 3);

const fbx = new TextDecoder('latin1').decode(Object.values(toFbx(desc))[0]);
for (const w of ['BlendShapeChannel', 'BlendShape', 'Shape', 'DeformPercent']) assert.ok(fbx.includes(w), 'FBX has ' + w);

const usda = new TextDecoder().decode(writeUsda(stageOf(desc)));
for (const w of ['def BlendShape "Up"', 'skel:blendShapes', 'skel:blendShapeTargets', 'blendShapeWeights.timeSamples', 'uniform token[] blendShapes'])
    assert.ok(usda.includes(w), 'USDA has ' + w);

assert.ok(writeUsdc(stageOf(desc)).length > 88);

console.log('stageedit: ok');
