<script>
    /* A section's pages, the second row of the console's online pages:
     * between arrows, light lines over and under the chosen one. Left /
     * right (the d-pad) turn them too (app.svelte.js, stepPage). */
    import { tx, stepPage } from '#lib/app.svelte.js';
    import { hoverBlip, selectBlip } from '#lib/sound.js';
    import Arrow from './Arrow.svelte';

    /* tabs: [[value, text key]]; current; pick(value) */
    let { tabs, current, pick } = $props();

    function choose(v, el) {
        selectBlip();
        pick(v);
        el.blur();
    }
</script>

<div class="subnav">
    <Arrow dir="l" onclick={() => stepPage(-1)} />
    {#each tabs as [v, key]}
        <button class="pg" class:active={current === v} onpointerenter={hoverBlip}
                onclick={e => choose(v, e.currentTarget)}>{tx(key)}</button>
    {/each}
    <Arrow dir="r" onclick={() => stepPage(1)} />
</div>

<style>
    .subnav {
        display: flex; align-items: center; justify-content: space-between;
        gap: calc(var(--u) * 2); margin: 0 0 calc(var(--u) * 1.8);
    }
    .pg {
        position: relative; background: none; border: 0; border-radius: 0; cursor: pointer;
        font-size: calc(var(--u) * 2); letter-spacing: .03em; color: #8e9fd4; white-space: nowrap;
        padding: calc(var(--u) * .7) calc(var(--u) * 2.4);
        text-shadow: 0 calc(var(--u) * .15) calc(var(--u) * .3) #000;
    }
    .pg:hover, .pg.active { color: #dfe5ff; }
    .pg.active::before, .pg.active::after {
        content: ""; position: absolute; left: 0; right: 0; height: 2px;
        background: linear-gradient(90deg, transparent, rgba(205, 215, 255, .9) 18%, rgba(205, 215, 255, .9) 82%, transparent);
        box-shadow: 0 0 calc(var(--u) * .6) rgba(140, 160, 255, .7);
    }
    .pg.active::before { top: 0; }
    .pg.active::after { bottom: 0; }
</style>
