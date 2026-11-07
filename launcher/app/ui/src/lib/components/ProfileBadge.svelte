<script>
    /* The player's badge, top right: the game's main-menu badge, the
     * alerts' glossy box in black. A click chooses PROFILE. */
    import { app, tx, setSection } from '#lib/app.svelte.js';
    import { selectBlip } from '#lib/sound.js';
    import IconCanvas from './IconCanvas.svelte';

    function open() {
        selectBlip();
        setSection('profile', false);
    }
</script>

{#if app.prof}
    <button class="badge" onclick={open}>
        <IconCanvas icon={app.prof.icon} color={app.prof.color} shade={app.prof.shade} color2={app.prof.color2}
                    angle={app.prof.angle} alt={app.prof.alt} />
        <span>{app.prof.name || tx('profDefault')}</span>
    </button>
{/if}

<style>
    /* black steel in a bronze frame with a gold lip, the red line under */
    .badge {
        position: fixed; top: calc(var(--u) * 4.2); right: calc(var(--u) * 1.6); z-index: 9;
        display: flex; align-items: center; gap: calc(var(--u) * .9);
        padding: calc(var(--u) * .5) calc(var(--u) * 1.4) calc(var(--u) * .5) calc(var(--u) * .5);
        border: 2px solid #46546a; border-top-color: #e6bb63; border-bottom-color: var(--blood);
        border-radius: calc(var(--u) * .4);
        background: linear-gradient(rgba(215, 228, 255, .16), rgba(255, 255, 255, 0) 50%), var(--tex-grain), linear-gradient(#1e2530, #050609);
        box-shadow: var(--bevel), 0 calc(var(--u) * .3) calc(var(--u) * .8) rgba(0, 0, 0, .6);
        color: #eef3fb; font-weight: 800; font-size: calc(var(--t) * 1.6); text-transform: uppercase;
        letter-spacing: .06em; cursor: pointer; max-width: calc(var(--u) * 30); text-shadow: 0 1px 0 #000;
    }
    .badge :global(canvas) { width: calc(var(--u) * 4.2); height: calc(var(--u) * 4.2); flex: none; border-radius: 2px; box-shadow: 0 0 0 1px #000, 0 0 0 2px #2e3846; }
    .badge span { white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
    .badge:hover { border-color: #e6bb63; box-shadow: var(--bevel), var(--hover-glow); }
</style>
