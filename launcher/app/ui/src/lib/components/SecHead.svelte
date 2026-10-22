<script>
    /* A section's header, as on the console's online pages: its name
     * between two blue arrows and the triggers' pictures (Q / E on the
     * keyboard); they turn to the next section. Gone while only COMPILE is
     * offered. */
    import { app, tx, TITLE, stepSection, padKindOf } from '#lib/app.svelte.js';
    import Arrow from './Arrow.svelte';
    import KeyFace from './KeyFace.svelte';
    import PadFace from './PadFace.svelte';

    const kind = $derived(padKindOf(1));
</script>

<div class="head">
    <Arrow dir="l" hidden={app.locked} onclick={() => stepSection(-1)} />
    <span class="g" class:gone={app.locked}>{#if app.usingPad}<PadFace {kind} b="L2" />{:else}<KeyFace vk={81} />{/if}</span>
    <h1>{tx(TITLE[app.sec])}</h1>
    <span class="g" class:gone={app.locked}>{#if app.usingPad}<PadFace {kind} b="R2" />{:else}<KeyFace vk={69} />{/if}</span>
    <Arrow dir="r" hidden={app.locked} onclick={() => stepSection(1)} />
</div>

<style>
    .head {
        display: flex; align-items: center; justify-content: center;
        gap: calc(var(--u) * 1.2); margin-bottom: calc(var(--u) * 1.6); flex: none;
    }
    h1 {
        margin: 0 calc(var(--u) * 1.4); font-size: calc(var(--t) * 3.6); font-weight: 600;
        letter-spacing: .02em; color: #fff; white-space: nowrap;
        text-shadow: var(--shadow-text), 0 0 calc(var(--u) * 1.2) rgba(150, 170, 255, .45);
    }
    .g { display: inline-flex; }
    .g :global(.glyph) { height: calc(var(--u) * 3.2); }
    .g :global(.kkey) { gap: 0; }
    .gone { visibility: hidden; }
</style>
