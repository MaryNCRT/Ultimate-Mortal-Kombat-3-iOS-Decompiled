<script>
    /* Two slots for an action's pad buttons: either of the two presses it.
     * Each opens the picker (Picker.svelte) under it. */
    import { openPicker } from '#lib/app.svelte.js';
    import { selectBlip } from '#lib/sound.js';
    import PadFace from './PadFace.svelte';

    let { value, kind, change } = $props();
    const cur = $derived(String(value || '').split('+').filter(Boolean));

    function open(e, i) {
        e.stopPropagation();
        selectBlip();
        openPicker(e.currentTarget.getBoundingClientRect(), kind, b => {
            const next = cur.slice();
            next[i] = b;
            change(next.filter(Boolean).filter((x, j, a) => a.indexOf(x) === j).join('+'));
        });
    }
</script>

<span class="padsels">
    {#each [0, 1] as i}
        <button class="slot" onclick={e => open(e, i)}><PadFace {kind} b={cur[i] || ''} combo /></button>
    {/each}
</span>

<style>
    .slot {
        display: inline-flex; align-items: center; justify-content: center; gap: calc(var(--u) * .2);
        min-width: calc(var(--u) * 5.5); height: calc(var(--u) * 3); padding: 0 calc(var(--u) * .5);
        background: rgba(4, 3, 18, .55); border: 1px solid var(--line); border-radius: var(--r-sm);
        cursor: pointer;
    }
    .slot :global(.glyph) { height: calc(var(--u) * 2.4); }
    .slot:hover { border-color: #fff; }
</style>
