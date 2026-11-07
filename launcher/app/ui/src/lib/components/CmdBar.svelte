<script>
    /* The command bar across the foot, as on the console: what Enter / A
     * and Esc / B do, F11 on the keyboard, and the game's state at the
     * right. The pictures are of whichever was used last, pad or keys. */
    import { app, tx, padKindOf } from '#lib/app.svelte.js';
    import KeyFace from './KeyFace.svelte';
    import PadFace from './PadFace.svelte';
    import ProfileBadge from './ProfileBadge.svelte';

    const kind = $derived(padKindOf(1));
</script>

<footer class="cmdbar" class:gone={app.overlay > 0}>
    <ProfileBadge docked />
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
        font-size: calc(var(--t) * 1.8); letter-spacing: .08em; z-index: 6; color: #e6ebf3;
        text-transform: uppercase; font-weight: 700;
        text-shadow: 0 1px 2px #000;
    }
    /* the bar, as the console's: its ends cut on a slant -- a gilded rim
     * round brushed black steel, a lit lip along the top */
    .cmdbar::before, .cmdbar::after {
        content: ""; position: absolute; inset: 0; z-index: -1;
        clip-path: polygon(calc(var(--u) * 3.5) 0, calc(100% - calc(var(--u) * 3.5)) 0, 100% 100%, 0 100%);
    }
    .cmdbar::before { background: linear-gradient(#fff0c0, #d9a650 25%, #46546a 60%, #151b24); }
    .cmdbar::after {
        inset: 2px;
        background: linear-gradient(rgba(215, 228, 255, .14), rgba(255, 255, 255, 0) 45%),
                    var(--tex-grain),
                    linear-gradient(#1e2530, #06080c);
    }
    .cmdbar.gone { visibility: hidden; }
    .cmds { display: flex; align-items: center; gap: calc(var(--u) * .8); }
    .cg { display: inline-flex; align-items: center; margin-left: calc(var(--u) * 1.6); }
    .cg:first-child { margin-left: 0; }
    .cg :global(.glyph) { height: calc(var(--u) * 3.4); }
    .status { position: absolute; right: calc(var(--u) * 4); color: var(--dim); }
    .status.ready { color: var(--ok-text); }
    .status.need { color: var(--bad-text); }
    .status.building, .status.playing { color: var(--gold1); }
</style>
