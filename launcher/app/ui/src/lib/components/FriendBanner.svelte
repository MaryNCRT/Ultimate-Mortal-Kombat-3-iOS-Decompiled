<script>
    /* A friend as the game's ONLINE FRIENDS draws one (Mary, 0.4.4): the
     * friend's own Player Banner -- their gradient on brushed steel, the gold
     * bar on top and the red line under, their fighter out of its left end,
     * the name in the heading face -- and the actions on dark steel plates
     * at its right end (RETAR ...). `look` is the server's, as friends.txt
     * carries it to the game: icon|colour|L/D/C|colour2|angle|outfit
     * (gamecode_stubs.c port_online_friend_look). */
    import { app, tx, profArt, profArtAlt } from '#lib/app.svelte.js';

    let { p, tag = '', children } = $props();

    const SHADE2 = { L: 'ffffff', D: '000000', C: '6e0b12' };
    const lk = $derived.by(() => {
        const f = String(p.look || '').split('|');
        let icon = parseInt(f[0], 10);
        if (!(icon >= 0 && icon < 24)) icon = 0;
        const color = /^[0-9a-f]{6}$/i.test(f[1] || '') ? f[1].toLowerCase() : '3a8ee6';
        const shade = /^[dD]/.test(f[2] || '') ? 'D' : /^[cC]/.test(f[2] || '') ? 'C' : 'L';
        const color2 = /^[0-9a-f]{6}$/i.test(f[3] || '') ? f[3].toLowerCase() : SHADE2[shade];
        const angle = /^\d/.test(f[4] || '') ? parseInt(f[4], 10) : -1;
        return { icon, color, shade, color2, angle, alt: f[5] === '1' };
    });
    /* the gradient as ProfileBadge.svelte draws the player's own */
    const grad = $derived.by(() => {
        let a = '#' + lk.color, b = '#' + lk.color2, deg = 180;
        if (lk.shade === 'C' && lk.angle >= 0) deg = lk.angle;
        else if (lk.shade === 'L') [a, b] = [b, a];
        return `linear-gradient(${deg}deg, ${a}, ${b})`;
    });

    let cv = $state();
    $effect(() => {
        app.artTick;
        if (!cv) return;
        const art = (lk.alt && profArtAlt[lk.icon]) || profArt[lk.icon];
        const g = cv.getContext('2d');
        g.clearRect(0, 0, cv.width, cv.height);
        if (!art) return;
        const w = art.naturalWidth || art.width, h = art.naturalHeight || art.height;
        if (!w || !h) return;
        const s = Math.min(cv.width / w, cv.height / h);
        g.drawImage(art, (cv.width - w * s) / 2, cv.height - h * s, w * s, h * s);
    });
</script>

<div class="fb">
    <span class="plate" style:background-image="linear-gradient(90deg, rgba(0,0,0,.25), rgba(0,0,0,0) 22%, rgba(0,0,0,0) 70%, rgba(0,0,0,.35)), var(--tex-grain), {grad}"><i class="sheen"></i></span>
    <canvas bind:this={cv} width="200" height="200"></canvas>
    <span class="txt">
        <b>{p.name || '—'}</b>
        <small>{tag ? tag + ' · ' : ''}<code>{p.code}</code></small>
    </span>
    <span class="act">{@render children?.()}</span>
</div>

<style>
    .fb {
        position: relative; height: calc(var(--u) * 7.6); margin: calc(var(--u) * .5) calc(var(--u) * .2) calc(var(--u) * .9);
    }
    .plate {
        position: absolute; inset: 0; overflow: hidden; display: block;
        background-size: 100% 100%, auto, 100% 100%;
        background-blend-mode: normal, overlay, normal;
        border-left: 3px solid #b8321c; border-right: 3px solid #b8321c;
        box-shadow: inset 0 0 0 1px rgba(0, 0, 0, .6), 0 calc(var(--u) * .3) calc(var(--u) * .9) rgba(0, 0, 0, .7);
    }
    .fb::before, .fb::after { content: ""; position: absolute; left: -3px; right: -3px; z-index: 2; pointer-events: none; }
    .fb::before { top: -4px; height: 5px; background: linear-gradient(#fff3c8, #e6b75c 35%, #9a6424 75%, #3a220c); box-shadow: 0 1px 2px #000; }
    .fb::after { bottom: -4px; height: 5px; background: linear-gradient(#e0402e, #6a0b05); box-shadow: 0 1px 2px #000; }
    .sheen {
        position: absolute; top: 0; bottom: 0; left: -40%; width: 30%;
        background: linear-gradient(100deg, transparent, rgba(255, 255, 255, .12) 45%, rgba(255, 255, 255, .22) 50%, rgba(255, 255, 255, .12) 55%, transparent);
        animation: sheen 8s ease-in-out infinite;
    }
    @keyframes sheen { 0%, 60% { transform: translateX(0); } 100% { transform: translateX(560%); } }
    /* the fighter out of the left end, taller than the plate, no box */
    canvas {
        position: absolute; left: calc(var(--u) * -.4); bottom: -2px; z-index: 3;
        width: calc(var(--u) * 9.6); height: calc(var(--u) * 9.6);
        filter: drop-shadow(0 0 1px #000) drop-shadow(calc(var(--u) * .3) calc(var(--u) * .2) calc(var(--u) * .4) rgba(0, 0, 0, .7));
        pointer-events: none;
    }
    .txt {
        position: absolute; left: calc(var(--u) * 10.2); right: calc(var(--u) * 24); top: 0; bottom: 0; z-index: 3;
        display: flex; flex-direction: column; justify-content: center; line-height: 1.05; gap: calc(var(--u) * .25);
        text-shadow: 0 2px 0 #000, 0 0 calc(var(--u) * .5) rgba(0, 0, 0, .9);
    }
    .txt b {
        font-family: var(--font-head); font-weight: 900; font-style: italic; text-transform: uppercase;
        font-size: calc(var(--u) * 2.6); letter-spacing: .03em; color: #fff;
        white-space: nowrap; overflow: hidden; text-overflow: ellipsis;
    }
    .txt small { font-style: italic; font-weight: 800; text-transform: uppercase; font-size: calc(var(--u) * 1.05); letter-spacing: .08em; color: #f1f3f7; }
    .txt code { font-family: var(--font-mono); font-style: normal; letter-spacing: .12em; }
    /* the actions: the game's dark steel plates, gold-edged */
    .act {
        position: absolute; right: calc(var(--u) * 1.2); top: 0; bottom: 0; z-index: 3;
        display: flex; align-items: center; gap: calc(var(--u) * .8);
    }
    .act :global(.fbtn) {
        min-width: calc(var(--u) * 11); height: calc(var(--u) * 4.4); padding: 0 calc(var(--u) * 1.4);
        font-family: var(--font-head); font-weight: 900; font-style: italic; text-transform: uppercase;
        font-size: calc(var(--u) * 1.6); letter-spacing: .04em; color: #fff; text-shadow: 0 2px 0 #000;
        background: linear-gradient(rgba(255, 255, 255, .1), rgba(255, 255, 255, 0) 45%), var(--tex-grain), linear-gradient(#2a2f37, #0b0d10);
        border: 2px solid #8a6a32; box-shadow: inset 0 0 0 1px #000, 0 2px 4px rgba(0, 0, 0, .7);
        cursor: pointer; transition: border-color .12s, filter .12s;
    }
    .act :global(.fbtn:hover:not(:disabled)) { border-color: #e6bb63; filter: brightness(1.2); }
    .act :global(.fbtn.small) { min-width: 0; font-size: calc(var(--u) * 1.15); height: calc(var(--u) * 3.2); padding: 0 calc(var(--u) * .9); }
    .act :global(.fbtn.red) { background: linear-gradient(rgba(255, 255, 255, .12), rgba(255, 255, 255, 0) 45%), var(--tex-grain), linear-gradient(#8e1a12, #3a0603); }
    .act :global(.fbtn:disabled) { opacity: .5; cursor: default; }
</style>
