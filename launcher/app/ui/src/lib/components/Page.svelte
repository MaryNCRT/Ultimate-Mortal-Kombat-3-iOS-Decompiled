<script>
    /* A section's page on the backdrop (0.4.2, Mary): a stone panel in a
     * metal frame, as the ONLINE screen's panels -- a gold bar along the top,
     * bronze down the sides and along the foot, the cast gold brackets in the
     * corners (materials.css). The frame stays put; what is inside scrolls
     * when it is long. */
    let { children, class: cls = '', bare = false } = $props();
</script>

<!-- bare: inside another page (SETTINGS' CONTROLS), no frame or scroll of its own -->
{#if bare}{@render children()}{:else}
    <section class="page {cls}">
        <i class="orn tl"></i><i class="orn tr"></i><i class="orn bl"></i><i class="orn br"></i>
        <div class="scroll">{@render children()}</div>
    </section>
{/if}

<style>
    .page {
        position: relative; display: flex; flex-direction: column; flex: 1 1 auto; min-height: 0;
        background: var(--veil); box-shadow: var(--glow-in), 0 calc(var(--u) * .6) calc(var(--u) * 1.6) rgba(0, 0, 0, .7);
        border: 1px solid #050403;
        border-left: calc(var(--u) * .45) solid transparent; border-right: calc(var(--u) * .45) solid transparent;
        border-image: linear-gradient(90deg, #151b24, #5d6e88 30%, #283240 60%, #10151c) 1;
    }
    /* the gold bar along the top, the bronze one along the foot: lit lip,
     * the metal, a dark line under */
    .page::before, .page::after {
        content: ""; position: absolute; left: calc(var(--u) * -.6); right: calc(var(--u) * -.6); z-index: 1;
        pointer-events: none;
    }
    .page::before {
        top: calc(var(--u) * -.5); height: calc(var(--u) * .75);
        background: linear-gradient(#fff3c8, #e6b75c 30%, #9a6424 70%, #3a220c);
        box-shadow: 0 calc(var(--u) * .15) calc(var(--u) * .4) rgba(0, 0, 0, .8);
    }
    .page::after {
        bottom: calc(var(--u) * -.45); height: calc(var(--u) * .55);
        background: linear-gradient(#93a3bb, #46546a 40%, #151b24);
        box-shadow: 0 calc(var(--u) * .2) calc(var(--u) * .5) rgba(0, 0, 0, .8);
    }
    .orn {
        position: absolute; z-index: 2; width: calc(var(--u) * 2.4); height: calc(var(--u) * 2.4);
        pointer-events: none; background-size: contain; background-repeat: no-repeat;
    }
    .orn.tl { left: calc(var(--u) * -.7); top: calc(var(--u) * -.6); background-image: var(--orn-tl); }
    .orn.tr { right: calc(var(--u) * -.7); top: calc(var(--u) * -.6); background-image: var(--orn-tr); }
    .orn.bl { left: calc(var(--u) * -.7); bottom: calc(var(--u) * -.6); background-image: var(--orn-bl); filter: sepia(.5) brightness(.8); }
    .orn.br { right: calc(var(--u) * -.7); bottom: calc(var(--u) * -.6); background-image: var(--orn-br); filter: sepia(.5) brightness(.8); }
    /* tight: MODS, whose pages want every pixel */
    .page.tight .scroll { padding: calc(var(--u) * 1) calc(var(--u) * 1.2); }
    .scroll {
        display: flex; flex-direction: column; flex: 1 1 auto; min-height: 0;
        overflow-x: hidden; overflow-y: auto;
        padding: calc(var(--u) * 2.2) calc(var(--u) * 2.6);
    }
</style>
