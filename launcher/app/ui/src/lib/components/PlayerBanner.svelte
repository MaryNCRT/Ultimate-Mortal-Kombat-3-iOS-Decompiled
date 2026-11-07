<script>
    /* The Player Banner (0.4.2, Mary), the launcher's twin of the ONLINE
     * screen's: the profile's own gradient on brushed steel, a slow sheen
     * across it, the fighter -- the icon's, who is the favourite --
     * standing out of it with no box round him, the name, the
     * account's state, the id and the favourite. A bronze mount, a gold bar
     * on top, the red line under. Nothing here is saved: PROFILE's fields
     * below it change the profile, and the banner follows. */
    import { onMount } from 'svelte';
    import { app, tx, profArt, profArtAlt } from '#lib/app.svelte.js';
    import { api } from '#lib/platform.js';
    import { FIGHTERS } from '#lib/fighters.js';

    let { grad = '' } = $props();
    const p = $derived(app.prof);
    const who = $derived(p ? p.icon : 0);      /* the icon is the favourite (Mary) */

    /* the account's state: signed in, a guest, or no server */
    let acc = $state(null);
    onMount(async () => { acc = await api.account.status().catch(() => null); });
    const state = $derived(!acc || !acc.server ? ['off', tx('bnOffline')]
                         : acc.user ? ['on', tx('bnSigned') + ' ' + acc.user] : ['guest', tx('bnGuest')]);

    /* the fighter's art (an image, or the alternate outfit's canvas) */
    let cv = $state();
    $effect(() => {
        app.artTick;
        const art = (p && p.alt && profArtAlt[who]) || profArt[who];
        if (!cv) return;
        const g = cv.getContext('2d');
        g.clearRect(0, 0, cv.width, cv.height);
        if (!art) return;
        const w = art.naturalWidth || art.width, h = art.naturalHeight || art.height;
        if (!w || !h) return;
        const s = Math.min(cv.width / w, cv.height / h);
        g.drawImage(art, (cv.width - w * s) / 2, cv.height - h * s, w * s, h * s);
    });
    const names = $derived(FIGHTERS[app.ui === 'ES' ? 'ES' : 'EN']);
</script>

{#if p}
    <div class="banner">
        <div class="plate" style:background-image="linear-gradient(90deg, rgba(0,0,0,.3), rgba(0,0,0,0) 14%, rgba(0,0,0,0) 86%, rgba(0,0,0,.3)), var(--tex-grain), {grad || 'linear-gradient(#3a8ee6, #12304e)'}">
            <i class="sheen"></i>
        </div>
        <canvas class="fighter" bind:this={cv} width="320" height="320"></canvas>
        <div class="info">
            <div class="name">{p.name || tx('profDefault')}</div>
            <div class="st {state[0]}"><i></i>{state[1]}</div>
            <div class="facts">
                <span><b>{tx('profId')}</b><code>{p.id}</code></span>
                <span><b>{tx('profFav')}</b>{names[who]}</span>
            </div>
        </div>
    </div>
{/if}

<style>
    .banner {
        position: relative; flex: none;
        height: calc(var(--u) * 15); margin: calc(var(--u) * 3.6) 0 calc(var(--u) * 1.2);
    }
    /* the steel plate: the gradient, the grain over it, the ends in shade;
     * mounted in bronze, a gold bar on top, the red line under */
    .plate {
        position: absolute; inset: 0; overflow: hidden;
        background-size: 100% 100%, auto, 100% 100%;
        border: 2px solid #2e3846; border-top: 0; border-bottom: 0;
        box-shadow: inset 0 0 0 1px rgba(0, 0, 0, .7), inset 0 calc(var(--u) * .3) calc(var(--u) * 1) rgba(255, 255, 255, .18),
                    0 calc(var(--u) * .5) calc(var(--u) * 1.4) rgba(0, 0, 0, .7);
    }
    .banner::before, .banner::after {
        content: ""; position: absolute; left: calc(var(--u) * -.5); right: calc(var(--u) * -.5); z-index: 2; pointer-events: none;
    }
    .banner::before { top: calc(var(--u) * -.55); height: calc(var(--u) * .7); background: linear-gradient(#fff3c8, #e6b75c 30%, #9a6424 70%, #3a220c); box-shadow: 0 2px 4px rgba(0, 0, 0, .8); }
    .banner::after { bottom: calc(var(--u) * -.45); height: calc(var(--u) * .5); background: linear-gradient(#e0402e, #6a0b05); box-shadow: 0 2px 4px rgba(0, 0, 0, .8); }
    /* a slow light across the metal */
    .sheen {
        position: absolute; top: 0; bottom: 0; left: -40%; width: 35%;
        background: linear-gradient(100deg, transparent, rgba(255, 255, 255, .16) 45%, rgba(255, 255, 255, .28) 50%, rgba(255, 255, 255, .16) 55%, transparent);
        animation: sheen 7s ease-in-out infinite;
    }
    @keyframes sheen { 0%, 55% { transform: translateX(0); } 100% { transform: translateX(460%); } }
    /* the fighter stands out of the banner, cut out, no box */
    .fighter {
        position: absolute; right: calc(var(--u) * 3); bottom: 0; z-index: 3;
        height: calc(var(--u) * 21); width: calc(var(--u) * 21);
        filter: drop-shadow(0 0 calc(var(--u) * .2) #000) drop-shadow(calc(var(--u) * -.6) calc(var(--u) * .4) calc(var(--u) * .8) rgba(0, 0, 0, .7));
        animation: rise .6s ease-out;
    }
    @keyframes rise { from { opacity: 0; transform: translateY(calc(var(--u) * 1)); } }
    .info {
        position: absolute; left: calc(var(--u) * 2.6); top: 0; bottom: 0; right: calc(var(--u) * 25); z-index: 3;
        display: flex; flex-direction: column; justify-content: center; gap: calc(var(--u) * .5);
        text-shadow: 0 2px 0 #000, 0 0 calc(var(--u) * .6) rgba(0, 0, 0, .9);
    }
    .name {
        font-family: var(--font-head); font-weight: 900; text-transform: uppercase;
        font-size: calc(var(--u) * 3.6); letter-spacing: .06em; color: #fff; line-height: 1.05;
        white-space: nowrap; overflow: hidden; text-overflow: ellipsis;
    }
    .st { display: flex; align-items: center; gap: calc(var(--u) * .6); font-weight: 800; letter-spacing: .1em; font-size: calc(var(--u) * 1.5); text-transform: uppercase; color: #e6ebf3; }
    .st i { width: calc(var(--u) * 1); height: calc(var(--u) * 1); border-radius: 50%; background: #8a8a8a; box-shadow: 0 0 0 1px #000, 0 0 0 2px #46546a; }
    .st.on i { background: #4fd36b; box-shadow: 0 0 0 1px #000, 0 0 0 2px #46546a, 0 0 calc(var(--u) * .7) #4fd36b; }
    .st.guest i { background: #e6bb63; }
    .facts { display: flex; flex-wrap: wrap; gap: calc(var(--u) * .4) calc(var(--u) * 2.4); font-size: calc(var(--u) * 1.35); color: #eef3fb; text-transform: uppercase; }
    .facts b { color: #ffe2a0; letter-spacing: .12em; margin-right: calc(var(--u) * .7); font-weight: 800; }
    .facts code { font-family: var(--font-mono); letter-spacing: .14em; }
</style>
