<script>
    /* The player's badge, top right -- as the game's ONLINE screen draws it
     * at its top (Mary, 0.4.2), docked in the command bar's left end: a small Player Banner, the profile's own
     * gradient on brushed steel, a gold bar on top and the red line under,
     * the fighter standing out of its left end, the name and EDIT PROFILE.
     * A click chooses PROFILE. */
    import { app, tx, setSection, profArt, profArtAlt } from '#lib/app.svelte.js';
    import { selectBlip } from '#lib/sound.js';

    /* docked: at the left end of the command bar (Mary: so it covers nothing) */
    let { docked = false } = $props();

    function open() {
        selectBlip();
        setSection('profile', true);
    }

    const p = $derived(app.prof);
    const hex = c => '#' + String(c || '000000').toLowerCase();
    /* the gradient as SecProfile.svelte's gradCss, always as an image */
    const grad = $derived.by(() => {
        if (!p) return 'none';
        let a = hex(p.color), b = hex(p.color2), deg = 180;
        if (p.shade === 'C' && p.angle >= 0) deg = p.angle;
        else if (p.shade === 'L') [a, b] = [b, a];
        return `linear-gradient(${deg}deg, ${a}, ${b})`;
    });

    let cv = $state();
    $effect(() => {
        app.artTick;
        if (!p || !cv) return;
        const art = (p.alt && profArtAlt[p.icon]) || profArt[p.icon];
        const g = cv.getContext('2d');
        g.clearRect(0, 0, cv.width, cv.height);
        if (!art) return;
        const w = art.naturalWidth || art.width, h = art.naturalHeight || art.height;
        if (!w || !h) return;
        const s = Math.min(cv.width / w, cv.height / h);
        g.drawImage(art, (cv.width - w * s) / 2, cv.height - h * s, w * s, h * s);
    });
</script>

{#if p}
    <button class="badge" class:docked onclick={open}>
        <span class="plate" style:--g={grad} style:background-image="linear-gradient(90deg, rgba(0,0,0,.3), rgba(0,0,0,0) 30%), var(--tex-grain), {grad}"><i class="sheen"></i></span>
        <canvas bind:this={cv} width="160" height="160"></canvas>
        <span class="txt">
            <b>{p.name || tx('profDefault')}</b>
            <small>{tx('bdEdit')}</small>
        </span>
    </button>
{/if}

<style>
    .badge {
        position: fixed; top: calc(var(--u) * 4.6); right: calc(var(--u) * 1.6); z-index: 9;
        width: calc(var(--u) * 18.5); height: calc(var(--u) * 5);
        padding: 0; border: 0; background: none; cursor: pointer; text-align: left;
    }
    /* docked: filling the bar's left end, cut on its slant, the gradient
     * right to the tip (Mary) */
    .badge.docked { position: absolute; top: 2px; bottom: 2px; left: 2px; right: auto; height: auto; width: calc(var(--u) * 22); }
    .badge.docked .plate {
        clip-path: polygon(calc(var(--u) * 3.4) 0, 100% 0, 100% 100%, 0 100%);
        border-left: 0; background-image: var(--tex-grain), var(--g) !important;
        background-size: auto, 100% 100%; background-blend-mode: overlay, normal;
    }
    .badge.docked::before { left: calc(var(--u) * 3.4); right: 0; top: 0; height: 3px; }
    .badge.docked::after { left: 0; right: 0; bottom: 0; height: 3px; }
    .badge.docked canvas { left: calc(var(--u) * 2.6); }
    .badge.docked .txt { left: calc(var(--u) * 9); }
    .plate {
        position: absolute; inset: 0; overflow: hidden; display: block;
        background-size: 100% 100%, auto, 100% 100%;
        background-blend-mode: normal, overlay, normal;
        border-left: 2px solid #2e3846; border-right: 2px solid #2e3846;
        box-shadow: inset 0 0 0 1px rgba(0, 0, 0, .7), 0 calc(var(--u) * .3) calc(var(--u) * .8) rgba(0, 0, 0, .6);
    }
    .badge::before, .badge::after { content: ""; position: absolute; left: -3px; right: -3px; z-index: 2; pointer-events: none; }
    .badge::before { top: -3px; height: 4px; background: linear-gradient(#fff3c8, #e6b75c 35%, #9a6424 75%, #3a220c); box-shadow: 0 1px 2px #000; }
    .badge::after { bottom: -3px; height: 4px; background: linear-gradient(#e0402e, #6a0b05); box-shadow: 0 1px 2px #000; }
    .sheen {
        position: absolute; top: 0; bottom: 0; left: -40%; width: 35%;
        background: linear-gradient(100deg, transparent, rgba(255, 255, 255, .14) 45%, rgba(255, 255, 255, .24) 50%, rgba(255, 255, 255, .14) 55%, transparent);
        animation: sheen 7s ease-in-out infinite;
    }
    @keyframes sheen { 0%, 55% { transform: translateX(0); } 100% { transform: translateX(460%); } }
    /* the fighter out of the left end, no box */
    canvas {
        position: absolute; left: calc(var(--u) * -.6); bottom: 0; z-index: 3;
        width: calc(var(--u) * 6.6); height: calc(var(--u) * 6.6);
        filter: drop-shadow(0 0 1px #000) drop-shadow(calc(var(--u) * .3) calc(var(--u) * .2) calc(var(--u) * .4) rgba(0, 0, 0, .7));
    }
    .txt {
        position: absolute; left: calc(var(--u) * 6.2); right: calc(var(--u) * .8); top: 0; bottom: 0; z-index: 3;
        display: flex; flex-direction: column; justify-content: center; line-height: 1.05;
        text-shadow: 0 2px 0 #000, 0 0 calc(var(--u) * .5) rgba(0, 0, 0, .9);
    }
    .txt b {
        font-family: var(--font-head); font-weight: 900; font-style: italic; text-transform: uppercase;
        font-size: calc(var(--u) * 1.9); letter-spacing: .04em; color: #fff;
        white-space: nowrap; overflow: hidden; text-overflow: ellipsis;
    }
    .txt small { font-style: italic; font-weight: 800; text-transform: uppercase; font-size: calc(var(--u) * 1.05); letter-spacing: .08em; color: #e6ebf3; }
    .badge:hover .plate { filter: brightness(1.15); }
    .badge:hover::before { background: linear-gradient(#fff, #ffd98a 35%, #c27a2a 75%, #5a3410); }
</style>
