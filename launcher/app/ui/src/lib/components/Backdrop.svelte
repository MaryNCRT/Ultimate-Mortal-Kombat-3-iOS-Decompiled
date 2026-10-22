<script>
    /* The backdrop, after the console release's menus, shared by the menu
     * and every section: a cloud of red, purple and blue light, dark circuit
     * traces across it, and rings of marbled segments with glowing white
     * rims. On the menu also the player's picture inside the small ring and
     * the light behind the title. All in a 1600 x 900 box that crops to
     * cover the window (--k in tokens.css is one unit of it). Made here; no
     * game art. */
    import { app } from '#lib/app.svelte.js';
    import { marbleURL, nebulaURL } from '#lib/art.js';

    const nebula = nebulaURL(400, 225);
    const marble = marbleURL(512);

    /* the circuit traces: long runs with 45-degree bends */
    const TRACES = [
        'M180 120 H520 L580 180 H900 L960 120 H1240', 'M300 205 H560 L610 255 H760',
        'M1000 230 H1180 L1240 290 H1460',
        'M120 330 H420 L480 390 H700', 'M820 350 H1050 L1110 290 H1400',
        'M200 455 H380 L440 395', 'M600 470 H860 L920 530 H1180 L1240 470 H1500',
        'M150 560 H330 L390 620 H640', 'M760 600 H940 L1000 660 H1300',
        'M260 700 H520 L580 640 H700', 'M1080 740 H1260 L1320 680 H1480',
        'M420 790 H690 L750 730 H980'];

    /* n short thick blocks round a ring, gap degrees apart, the first at from */
    const blocks = (n, gap, from) => {
        const out = [], step = 360 / n;
        for (let i = 0; i < n; i++) out.push([from + i * step, from + (i + 1) * step - gap]);
        return out;
    };
    /* measured off the console's menu: eight blocks a ring, 36 degrees long
     * with 9 between. The small ring sits round the picture, under the
     * title; the big one round the window, a block either side of the
     * middle, its inside clear of the small one's outside by 39 units
     * (227 apart, 227 + 342 + 39 = 608) */
    const SMALL = { cx: 1077, cy: 454, r0: 284, r1: 342, spans: blocks(8, 9, 158) };
    const BIG = { cx: 850, cy: 450, r0: 608, r1: 717, spans: blocks(8, 9, 3) };

    const rad = a => a * Math.PI / 180;
    function seg({ cx, cy, r0, r1 }, [a0, a1]) {
        const p = (r, a) => (cx + r * Math.cos(rad(a))).toFixed(1) + ' ' + (cy + r * Math.sin(rad(a))).toFixed(1);
        const big = a1 - a0 > 180 ? 1 : 0;
        return 'M' + p(r1, a0) + ' A' + r1 + ' ' + r1 + ' 0 ' + big + ' 1 ' + p(r1, a1) +
               ' L' + p(r0, a1) + ' A' + r0 + ' ' + r0 + ' 0 ' + big + ' 0 ' + p(r0, a0) + ' Z';
    }

    /* the light behind the title, on the dragon's medallion: [angle,
     * length, half width] of each ray, the long ones along the title */
    const RAYS = [[-90, 190, 13], [-118, 160, 10], [-62, 160, 10], [180, 290, 11],
                  [0, 340, 11], [-160, 190, 8], [-20, 210, 8], [135, 170, 10],
                  [45, 170, 10], [100, 140, 8], [80, 140, 8], [160, 180, 6], [20, 200, 6]];

    /* the picture's box inside the small ring */
    const HERO = { x: 777, y: 285, w: 600, h: 338 };
</script>

<div class="space" style:background-image="url({nebula})" aria-hidden="true"></div>
<svg class="field" viewBox="0 0 1600 900" preserveAspectRatio="xMidYMid slice" aria-hidden="true">
    <defs>
        <pattern id="marble" patternUnits="userSpaceOnUse" width="512" height="512">
            <image href={marble} width="512" height="512" />
        </pattern>
        <!-- the picture fades out towards every edge: an ellipse its own
             shape, the centre a little low so the top (the fight's bars)
             goes first -->
        <radialGradient id="heroFade" cy="0.6" r="0.55">
            <stop offset="0.35" stop-color="#fff" />
            <stop offset="0.7" stop-color="#fff" stop-opacity="0.55" />
            <stop offset="1" stop-color="#fff" stop-opacity="0" />
        </radialGradient>
        <mask id="heroMask">
            <rect x={HERO.x} y={HERO.y} width={HERO.w} height={HERO.h} fill="url(#heroFade)" />
        </mask>
        <!-- the rims: brushed silver, lighter along the edge that faces up -->
        <linearGradient id="rimGrad" x1="0" y1="0" x2="0.3" y2="1">
            <stop offset="0" stop-color="#ffffff" />
            <stop offset="0.45" stop-color="#b4b8cc" />
            <stop offset="0.7" stop-color="#e9ebf6" />
            <stop offset="1" stop-color="#8f93ab" />
        </linearGradient>
        <radialGradient id="burstGlow">
            <stop offset="0" stop-color="#fff" stop-opacity="0.95" />
            <stop offset="0.25" stop-color="#cfe0ff" stop-opacity="0.6" />
            <stop offset="1" stop-color="#7fa0ff" stop-opacity="0" />
        </radialGradient>
        <linearGradient id="burstRay">
            <stop offset="0" stop-color="#fff" stop-opacity="0.95" />
            <stop offset="0.6" stop-color="#dfe6f5" stop-opacity="0.75" />
            <stop offset="1" stop-color="#bcd0ff" stop-opacity="0" />
        </linearGradient>
    </defs>

    <g>
        {#each TRACES as d}
            <path {d} class="trace-edge" />
            <path {d} class="trace" />
        {/each}
    </g>

    {#snippet ring(r, cls)}
        <g class={cls} style:transform-origin="{r.cx}px {r.cy}px">
            {#each r.spans as span}
                {@const d = seg(r, span)}
                <path {d} class="seg-body" />
                <!-- the rim's glow: wide faint strokes under the sharp one -->
                <path {d} class="seg-glow2" />
                <path {d} class="seg-glow" />
                <path {d} class="seg-rim" />
                <path {d} class="seg-hi" />
            {/each}
        </g>
    {/snippet}

    {@render ring(BIG, 'ring-big')}

    {#if !app.full}
        {#if app.hero}
            <g mask="url(#heroMask)">
                <image href={app.hero} x={HERO.x} y={HERO.y} width={HERO.w} height={HERO.h}
                       preserveAspectRatio="xMidYMid slice" />
                <rect class="hero-tint" x={HERO.x} y={HERO.y} width={HERO.w} height={HERO.h} />
            </g>
        {/if}
        <!-- the thin ring round the picture: a dark band, light either side -->
        <circle class="inner-band" cx={SMALL.cx} cy={SMALL.cy} r="254" />
        <circle class="inner-edge" cx={SMALL.cx} cy={SMALL.cy} r="248" />
        <circle class="inner-edge" cx={SMALL.cx} cy={SMALL.cy} r="260" />
        {@render ring(SMALL, 'ring-small')}
        <g class="burst" transform="translate(1040 190)">
            <circle r="210" fill="url(#burstGlow)" />
            {#each RAYS as [a, l, w]}
                <path d="M0 {-w} L{l} 0 L0 {w} Z" fill="url(#burstRay)" transform="rotate({a})" />
            {/each}
        </g>
    {/if}
</svg>

<style>
    .space {
        position: fixed; inset: 0;
        background-color: var(--bg);
        background-size: cover; background-position: center;
    }
    /* darker towards the window's edges */
    .space::after {
        content: ""; position: absolute; inset: 0;
        background: radial-gradient(ellipse 75% 70% at 50% 48%, rgba(3, 2, 10, .1) 45%, rgba(3, 2, 10, .6) 100%);
    }
    .field { position: fixed; inset: 0; width: 100%; height: 100%; }

    /* the traces: dark grooves with a grey edge either side */
    .trace-edge { fill: none; stroke: rgba(150, 150, 185, .22); stroke-width: 15; stroke-linejoin: miter; }
    .trace { fill: none; stroke: rgba(6, 4, 18, .85); stroke-width: 11; stroke-linejoin: miter; }

    /* the segments: marble with a sharp white rim that glows */
    .seg-body { fill: url(#marble); }
    .seg-glow2 { fill: none; stroke: #a493ff; stroke-width: 26; stroke-opacity: .14; stroke-linejoin: round; }
    .seg-glow { fill: none; stroke: #d2c9ff; stroke-width: 13; stroke-opacity: .3; stroke-linejoin: round; }
    .seg-rim { fill: none; stroke: url(#rimGrad); stroke-width: 7; stroke-linejoin: round; }
    .seg-hi { fill: none; stroke: #fff; stroke-width: 1.4; stroke-opacity: .9; stroke-linejoin: round; }
    .ring-small .seg-rim { stroke-width: 5.5; }

    /* the rings turn slowly, the small one the other way */
    .ring-big, .ring-small { transform-box: view-box; }
    .ring-big { animation: spin 240s linear infinite; }
    .ring-small { animation: spin 160s linear infinite reverse; }
    @keyframes spin { to { transform: rotate(360deg); } }

    /* the picture: tinted blue, a thin circle round it */
    .hero-tint { fill: rgba(30, 50, 150, .28); }
    .inner-band { fill: none; stroke: rgba(8, 6, 26, .55); stroke-width: 12; }
    .inner-edge { fill: none; stroke: rgba(205, 215, 255, .3); stroke-width: 1.6; }

    .burst { animation: burst 4s ease-in-out infinite alternate; }
    @keyframes burst { from { opacity: .75; } to { opacity: 1; } }
</style>
