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
    /* the name on a black-steel plate: a bronze frame, a gold lip on top,
     * a red edge at each end (the ONLINE screen's heading) */
    h1 {
        position: relative; margin: 0 calc(var(--u) * 1.4);
        padding: calc(var(--u) * .35) calc(var(--u) * 3.2) calc(var(--u) * .45);
        font-family: var(--font-head); text-transform: uppercase; font-weight: 900;
        font-size: calc(var(--t) * 3); letter-spacing: .14em; color: #eef3fb; white-space: nowrap;
        text-shadow: 0 2px 0 #000, 0 0 calc(var(--u) * 1.2) rgba(140, 170, 230, .35);
        background: linear-gradient(rgba(215, 228, 255, .12), rgba(255, 255, 255, 0) 50%), var(--tex-grain), linear-gradient(#202834, #07090d);
        border-top: 2px solid #e6bb63; border-bottom: 2px solid #2e3846;
        box-shadow: var(--bevel);
        clip-path: polygon(calc(var(--u) * 1.2) 0, calc(100% - calc(var(--u) * 1.2)) 0, 100% 50%, calc(100% - calc(var(--u) * 1.2)) 100%, calc(var(--u) * 1.2) 100%, 0 50%);
    }
    h1::before, h1::after {
        content: ""; position: absolute; top: 22%; bottom: 22%; width: calc(var(--u) * .5);
        background: linear-gradient(#e0402e, #7a0d06);
    }
    h1::before { left: calc(var(--u) * 1.5); }
    h1::after { right: calc(var(--u) * 1.5); }
    .g { display: inline-flex; }
    .g :global(.glyph) { height: calc(var(--u) * 3.2); }
    .g :global(.kkey) { gap: 0; }
    .gone { visibility: hidden; }
</style>
