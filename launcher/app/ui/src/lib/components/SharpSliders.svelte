<script>
    /* SHARP EDGES for SMOOTH MODELS (src/thirdparty/blender/umk3_subsurf.h):
     * which edges the smoothing keeps -- those whose faces turn by more than
     * the angle -- and how much of them, as two separate sliders. Dragging
     * changes app.cfg at once, for the model viewer's live preview; letting
     * go writes umk3.ini, which the game reads (sharp_edge_angle=,
     * sharp_edge_strength=). DEFAULT puts both back. */
    import { app, tx, save } from '#lib/app.svelte.js';
    import { selectBlip, hoverBlip } from '#lib/sound.js';

    let { disabled = false } = $props();

    /* lib/ini.js SHARP_ANGLE / SHARP_STRENGTH */
    const DEF_ANGLE = 80, DEF_STRENGTH = 100;
    const c = $derived(app.cfg);
    const angle = $derived(c.sharpAngle ?? DEF_ANGLE);
    const strength = $derived(c.sharpStrength ?? DEF_STRENGTH);

    function live(key, v) {
        app.cfg[key] = Number(v);
    }
    function keep(key, v) {
        save({ [key]: Number(v) });
    }
    function reset() {
        selectBlip();
        save({ sharpAngle: DEF_ANGLE, sharpStrength: DEF_STRENGTH });
    }
</script>

<div class="sharp" class:off={disabled}>
    <label class="slider" title={tx('sharpAngleTip')}>
        <span class="lbl">{tx('sharpAngle')}</span>
        <input type="range" min="0" max="180" step="1" value={angle} {disabled}
               oninput={e => live('sharpAngle', e.currentTarget.value)}
               onchange={e => keep('sharpAngle', e.currentTarget.value)}>
        <span class="val">{angle}°</span>
    </label>
    <label class="slider" title={tx('sharpStrengthTip')}>
        <span class="lbl">{tx('sharpStrength')}</span>
        <input type="range" min="0" max="100" step="1" value={strength} {disabled}
               oninput={e => live('sharpStrength', e.currentTarget.value)}
               onchange={e => keep('sharpStrength', e.currentTarget.value)}>
        <span class="val">{strength}%</span>
    </label>
    <p class="hint">{tx('sharpHint')}</p>
    <button class="btn" {disabled} onpointerenter={hoverBlip} onclick={reset}>{tx('sharpDefault')}</button>
</div>

<style>
    .sharp { display: grid; gap: calc(var(--u) * .5); }
    .sharp.off { opacity: .45; }
    .slider { display: grid; grid-template-columns: minmax(0, 1fr) minmax(0, 1.4fr) calc(var(--u) * 4.5); align-items: center; gap: calc(var(--u) * .8); }
    .slider input { width: 100%; accent-color: var(--blue, #4d8dff); }
    .val { font-family: var(--font-mono, monospace); text-align: right; }
    .hint { margin: 0; font-size: calc(var(--t) * 1.2); opacity: .75; }
    .btn { justify-self: start; }
</style>
