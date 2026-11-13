<script>
    /* A section's pages, the second row of the console's online pages:
     * between arrows, light lines over and under the chosen one. Left /
     * right (the d-pad) turn them too (app.svelte.js, stepPage). */
    import { tx, stepPage } from '#lib/app.svelte.js';
    import { hoverBlip, selectBlip } from '#lib/sound.js';
    import Arrow from './Arrow.svelte';

    /* tabs: [[value, text key]]; current; pick(value); dot: the values
     * with something new (the red dot) */
    /* step: what the arrows do (default: the section's pages) */
    let { tabs, current, pick, dot = [], step = null } = $props();

    function choose(v, el) {
        selectBlip();
        pick(v);
        el.blur();
    }
</script>

<div class="subnav" class:many={tabs.length > 5}>
    <Arrow dir="l" onclick={() => (step ? step(-1) : stepPage(-1))} />
    {#each tabs as [v, key]}
        <button class="pg" class:active={current === v} class:news={dot.includes(v)} onpointerenter={hoverBlip}
                onclick={e => choose(v, e.currentTarget)}>{tx(key)}</button>
    {/each}
    <Arrow dir="r" onclick={() => (step ? step(1) : stepPage(1))} />
</div>

<style>
    /* one row however many pages (MODS has seven: EXPORT fell to a second
     * line); the chosen one is the lit red plate, gold lines over and under,
     * the separator's blades under the row */
    .subnav {
        display: flex; align-items: center; justify-content: space-between; flex-wrap: nowrap;
        gap: 0 calc(var(--u) * .6); margin: 0 0 calc(var(--u) * 1.8);
        padding-bottom: calc(var(--u) * 1.1);
        background: var(--orn-rule) center bottom / auto calc(var(--u) * 1) no-repeat;
    }
    .pg {
        position: relative; flex: 0 1 auto; min-width: 0; background: none; border: 0; border-radius: 0; cursor: pointer;
        font-family: var(--font-head); text-transform: uppercase; font-weight: 900;
        font-size: calc(var(--t) * 1.7); letter-spacing: .08em; color: var(--dim); white-space: nowrap;
        padding: calc(var(--u) * .6) calc(var(--u) * 1.6);
        text-shadow: 0 calc(var(--u) * .15) calc(var(--u) * .3) #000;
        transition: color .15s;
    }
    .subnav.many .pg { font-size: calc(var(--t) * 1.35); padding: calc(var(--u) * .6) calc(var(--u) * .9); letter-spacing: .04em; }
    .pg:hover { color: #ffffff; }
    .pg.active {
        color: #fff;
        background: linear-gradient(rgba(255, 214, 170, .25), rgba(255, 255, 255, 0) 50%), var(--tex-grain), linear-gradient(#a3201a, #420604);
        box-shadow: var(--bevel), 0 0 calc(var(--u) * 1) rgba(196, 25, 15, .45);
    }
    .pg.active::before, .pg.active::after {
        content: ""; position: absolute; left: 0; right: 0; height: 2px;
        background: linear-gradient(90deg, transparent, #f1cc78 15%, #fff0c0 50%, #f1cc78 85%, transparent);
    }
    /* the red dot, right of the text (the ::before / ::after are the lines) */
    .pg.news::after {
        content: ""; position: absolute; right: calc(var(--u) * .9); top: 50%; left: auto;
        width: calc(var(--u) * 1); height: calc(var(--u) * 1); margin-top: calc(var(--u) * -.5);
        border-radius: 50%; background: var(--red); box-shadow: 0 0 calc(var(--u) * .6) var(--red);
    }
    .pg.news { padding-right: calc(var(--u) * 3); }
    .pg.active::before { top: 0; }
    .pg.active:not(.news)::after { bottom: 0; }
</style>
