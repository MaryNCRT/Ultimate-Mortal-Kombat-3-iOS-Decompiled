<script>
    /* AUDIO: MUSIC and EFFECTS AND VOICES, 0..100 over the game's own
     * volumes (music_volume=, sfx_volume=; the game's settings menu has the
     * same two bars). Dragging moves the bar, letting go writes umk3.ini;
     * the arrow keys and the pad's left / right move it a step when it has
     * the focus. */
    import { app, tx, save } from '#lib/app.svelte.js';

    const c = $derived(app.cfg);
    const music = $derived(c.musicVolume ?? 100);
    const sfx = $derived(c.sfxVolume ?? 100);

    function live(key, v) {
        app.cfg[key] = Number(v);
    }
    function keep(key, v) {
        save({ [key]: Number(v) });
    }
</script>

<div class="bars">
    {#each [['musicVolume', 'volMusic', music], ['sfxVolume', 'volSfx', sfx]] as [key, label, v]}
        <label class="bar">
            <span class="lbl">{tx(label)}</span>
            <input type="range" min="0" max="100" step="5" value={v} style="--p: {v}%"
                   oninput={e => live(key, e.currentTarget.value)}
                   onchange={e => keep(key, e.currentTarget.value)}>
            <span class="val">{v}%</span>
        </label>
    {/each}
</div>

<style>
    .bars { display: grid; gap: calc(var(--u) * .9); }
    .bar { display: grid; grid-template-columns: minmax(0, 1fr) minmax(0, 1.6fr) calc(var(--u) * 4.5); align-items: center; gap: calc(var(--u) * .8); }
    .val { font-family: var(--font-mono, monospace); text-align: right; }
    /* the game's bar: a dark well, red into gold up to the value, a white knob */
    input[type=range] { -webkit-appearance: none; appearance: none; width: 100%; height: calc(var(--u) * 1.6); background: transparent; cursor: pointer; }
    input[type=range]::-webkit-slider-runnable-track {
        height: calc(var(--u) * .55); border-radius: 999px;
        background: linear-gradient(90deg, #cc1a14, #ffc733) 0 / var(--p) 100% no-repeat, #101219;
        box-shadow: inset 0 1px 2px rgba(0, 0, 0, .7), 0 1px 0 rgba(255, 255, 255, .08);
    }
    input[type=range]::-webkit-slider-thumb {
        -webkit-appearance: none; appearance: none; margin-top: calc(var(--u) * -.45);
        width: calc(var(--u) * 1.45); height: calc(var(--u) * 1.45); border-radius: 50%;
        background: radial-gradient(circle at 50% 35%, #fff, #cfd3da);
        border: 2px solid #8b8f99; box-shadow: 0 2px 4px rgba(0, 0, 0, .6);
    }
    input[type=range]:focus-visible { outline: none; }
    input[type=range]:focus-visible::-webkit-slider-thumb,
    input[type=range]:hover::-webkit-slider-thumb { border-color: var(--blue, #4d8dff); }
</style>
