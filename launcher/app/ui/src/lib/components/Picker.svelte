<script>
    /* The pad's buttons to choose from, under the slot that opened it (or
     * over it, near the window's foot). A click elsewhere closes it. */
    import { app, closePicker } from '#lib/app.svelte.js';
    import { PB } from '#lib/glyphs.js';
    import { selectBlip } from '#lib/sound.js';
    import PadFace from './PadFace.svelte';

    let box = $state();
    let pos = $state({ x: -9999, y: -9999 });

    $effect(() => {
        const p = app.picker;
        if (!p || !box) return;
        const r = p.rect, w = box.offsetWidth, h = box.offsetHeight;
        pos = {
            x: Math.max(4, Math.min(window.innerWidth - w - 4, r.right - w)),
            y: r.bottom + h + 4 < window.innerHeight ? r.bottom + 4 : r.top - h - 4,
        };
    });

    function pick(e, b) {
        e.stopPropagation();
        selectBlip();
        const f = app.picker.pick;
        closePicker();
        f(b);
    }
</script>

{#if app.picker}
    <div class="picker" bind:this={box} style:left="{pos.x}px" style:top="{pos.y}px">
        {#each [''].concat(PB) as b}
            <button class="pick" title={b || '—'} onclick={e => pick(e, b)}>
                <PadFace kind={app.picker.kind} {b} />
            </button>
        {/each}
    </div>
{/if}

<style>
    .picker {
        position: fixed; z-index: 50;
        display: grid; grid-template-columns: repeat(7, auto); gap: calc(var(--u) * .3);
        padding: calc(var(--u) * .6);
        background: var(--popup); border: 1px solid var(--rim); border-radius: var(--r-md);
        box-shadow: 0 calc(var(--u) * .5) calc(var(--u) * 2) rgba(0, 0, 0, .7);
    }
    .pick {
        display: flex; align-items: center; justify-content: center;
        min-width: calc(var(--u) * 4); height: calc(var(--u) * 4); padding: calc(var(--u) * .3);
        background: none; border: 1px solid transparent; border-radius: var(--r-sm); cursor: pointer;
    }
    .pick :global(.glyph) { height: calc(var(--u) * 3); }
    .pick:hover { border-color: #fff; background: rgba(255, 255, 255, .08); }
</style>
