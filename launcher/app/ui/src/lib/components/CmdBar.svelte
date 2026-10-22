<script>
    /* The command bar across the foot, as on the console: what Enter / A
     * and Esc / B do, F11 on the keyboard, and the game's state at the
     * right. The pictures are of whichever was used last, pad or keys. */
    import { app, tx, padKindOf } from '#lib/app.svelte.js';
    import KeyFace from './KeyFace.svelte';
    import PadFace from './PadFace.svelte';

    const kind = $derived(padKindOf(1));
</script>

<footer class="cmdbar">
    <span class="cmds">
        <span class="cg">{#if app.usingPad}<PadFace {kind} b="SOUTH" />{:else}<KeyFace vk={13} />{/if}</span>
        <span>{tx('cSelect')}</span>
        <span class="cg">{#if app.usingPad}<PadFace {kind} b="EAST" />{:else}<KeyFace vk={27} />{/if}</span>
        <span>{tx('cBack')}</span>
        {#if !app.usingPad}
            <span class="cg"><KeyFace vk={122} /></span>
            <span>{tx('cFull')}</span>
        {/if}
    </span>
    <span class="status {app.status.kind}">{app.status.text}</span>
</footer>

<style>
    .cmdbar {
        grid-area: cmd; position: relative;
        margin: 0 calc(var(--u) * 5.5) calc(var(--u) * 4.5);
        height: calc(var(--u) * 5.6);
        display: flex; align-items: center; justify-content: center;
        padding: 0 calc(var(--u) * 4);
        font-size: calc(var(--t) * 1.8); letter-spacing: .06em; z-index: 6; color: #fff;
        text-shadow: 0 1px 2px #000;
    }
    /* the bar, as the console's: its ends cut on a slant, a light outline,
     * dark glass with fine lines along it */
    .cmdbar::before, .cmdbar::after {
        content: ""; position: absolute; inset: 0; z-index: -1;
        clip-path: polygon(calc(var(--u) * 3.5) 0, calc(100% - calc(var(--u) * 3.5)) 0, 100% 100%, 0 100%);
    }
    .cmdbar::before { background: linear-gradient(90deg, rgba(215, 220, 255, .25), rgba(215, 220, 255, .75) 50%, rgba(215, 220, 255, .25)); }
    .cmdbar::after {
        inset: 2px;
        background: repeating-linear-gradient(rgba(200, 205, 255, .1) 0 1px, transparent 1px calc(var(--u) * .7)),
                    linear-gradient(rgba(255, 255, 255, .1), rgba(255, 255, 255, 0) 45%),
                    linear-gradient(rgba(33, 29, 76, .85), rgba(8, 6, 26, .92));
    }
    .cmds { display: flex; align-items: center; gap: calc(var(--u) * .8); }
    .cg { display: inline-flex; align-items: center; margin-left: calc(var(--u) * 1.6); }
    .cg:first-child { margin-left: 0; }
    .cg :global(.glyph) { height: calc(var(--u) * 3.4); }
    .status { position: absolute; right: calc(var(--u) * 4); color: var(--dim); }
    .status.ready { color: var(--ok-text); }
    .status.need { color: var(--bad-text); }
    .status.building, .status.playing { color: var(--gold1); }
</style>
