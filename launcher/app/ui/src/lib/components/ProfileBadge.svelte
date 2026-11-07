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
    .badge {
        position: fixed; top: calc(var(--u) * 4.2); right: calc(var(--u) * 1.6); z-index: 9;
        display: flex; align-items: center; gap: calc(var(--u) * .9);
        padding: calc(var(--u) * .5) calc(var(--u) * 1.4) calc(var(--u) * .5) calc(var(--u) * .5);
        border: 2px solid rgba(220, 225, 237, .85); border-radius: calc(var(--u) * 1.1);
        background:
            radial-gradient(ellipse 75% 60% at 50% -10%, rgba(255, 255, 255, .2) 0 99%, transparent 100%),
            linear-gradient(#2a2a2e, #000);
        box-shadow: 0 calc(var(--u) * .3) calc(var(--u) * .8) rgba(0, 0, 0, .5);
        color: #fff; font-weight: 700; font-size: calc(var(--t) * 1.6);
        letter-spacing: .04em; cursor: pointer; max-width: calc(var(--u) * 30);
    }
    .badge :global(canvas) { width: calc(var(--u) * 4.2); height: calc(var(--u) * 4.2); flex: none; }
    .badge span { white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
    .badge:hover { border-color: #fff; }
</style>
