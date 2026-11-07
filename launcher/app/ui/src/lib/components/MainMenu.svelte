<script>
    /* The menu, down the left on the big ring's curve as on the console's
     * main menu. The chosen line has a dark band with thin light edges and
     * the glass orb: one piece that slides to the line. No
     * game compiled yet: COMPILE alone. */
    import { app, tx, SECTIONS, TITLE, setSection } from '#lib/app.svelte.js';
    import { hoverBlip, selectBlip } from '#lib/sound.js';

    let lines = $state([]);
    let sel = $state({ x: 0, y: 0, w: 0, h: 0, on: false });
    let resized = $state(0);

    const shown = $derived(SECTIONS.filter(s => !(app.locked && s !== 'build')));

    /* where the band goes: from the line's orb to its end */
    $effect(() => {
        resized;
        const i = shown.indexOf(app.sec), a = lines[i];
        if (!a) return;
        const orb = a.querySelector('.orb');
        const x = orb ? orb.offsetLeft : a.offsetLeft;
        sel = { x, y: a.offsetTop, w: a.offsetLeft + a.offsetWidth - x, h: a.offsetHeight, on: true };
    });

    function pick(s, el) {
        selectBlip();
        setSection(s, true);
        el.blur();
    }
</script>

<svelte:window onresize={() => resized++} />

<nav class="menu">
    <div class="sel" class:on={sel.on} aria-hidden="true"
         style:transform="translate({sel.x}px, {sel.y}px)" style:width="{sel.w}px" style:height="{sel.h}px">
        <i class="orb"></i>
    </div>
    {#each shown as s, i (s)}
        <button class="mi" class:active={app.sec === s} class:news={(s === 'build' && app.upd.news) || (s === 'profile' && app.frIn > 0)}
                bind:this={lines[i]} onpointerenter={hoverBlip}
                onclick={e => pick(s, e.currentTarget)}>
            <i class="orb"></i><span>{tx(TITLE[s])}</span>
        </button>
    {/each}
</nav>

<style>
    /* measured off the console's menu, in the backdrop's box: the text from
     * x 368 at its leftmost, the band 58 high; --u here is the menu's own
     * unit. Nine lines (ACCOUNT made the ninth), as tall together as the
     * eight were -- each 3.1 high, 0.38 apart -- so they stay centred on the
     * ring: the first line's middle near y 226, the last near 694 */
    .menu {
        --u: calc(var(--k) * 17);
        position: fixed; z-index: 3;
        left: calc(50vw + (251 - 800) * var(--k)); top: calc(50vh + (197 - 450) * var(--k));
        display: flex; flex-direction: column; gap: calc(var(--u) * .38);
    }
    .mi {
        display: flex; align-items: center; gap: calc(var(--u) * 2.3);
        height: calc(var(--u) * 3.1);
        background: none; border: 0; border-top: 1px solid transparent; border-bottom: 1px solid transparent;
        cursor: pointer; text-align: left;
        font-size: calc(var(--u) * 1.65); font-weight: 700; letter-spacing: .02em;
        color: var(--lav); text-shadow: 0 2px 0 #000, 0 0 calc(var(--u) * .8) rgba(0, 0, 0, .9);
        padding: 0 calc(var(--u) * 5.5) 0 0; white-space: nowrap;
        transition: color .25s;
    }
    .mi:hover { color: #fff; }
    .mi.active { color: #fff; text-shadow: 0 2px 0 #000, 0 0 calc(var(--u) * 1) rgba(150, 180, 240, .55); }
    .mi:focus { outline: none; }
    /* the menu follows the ring's curve, as the console's: deepest at the
     * middle line, the first and last the furthest out, the same either side */
    .mi:nth-of-type(1) { margin-left: calc(var(--u) * 3.6); }
    .mi:nth-of-type(2) { margin-left: calc(var(--u) * 2); }
    .mi:nth-of-type(3) { margin-left: calc(var(--u) * .9); }
    .mi:nth-of-type(4) { margin-left: calc(var(--u) * .2); }
    .mi:nth-of-type(5) { margin-left: 0; }
    .mi:nth-of-type(6) { margin-left: calc(var(--u) * .2); }
    .mi:nth-of-type(7) { margin-left: calc(var(--u) * .9); }
    .mi:nth-of-type(8) { margin-left: calc(var(--u) * 2); }
    .mi:nth-of-type(9) { margin-left: calc(var(--u) * 3.6); }
    /* an update waiting: a red dot after COMPILE */
    .mi.news::after {
        content: ''; width: calc(var(--u) * .9); height: calc(var(--u) * .9); border-radius: 50%;
        background: var(--red); margin-left: calc(var(--u) * .8); box-shadow: 0 0 calc(var(--u) * .8) var(--red);
    }

    /* the glass orb with an M turning inside it; each line's own only keeps its text in place: the one
     * shown is the band's */
    .orb {
        position: relative; flex: none;
        width: calc(var(--u) * 4.6); height: calc(var(--u) * 2.2); border-radius: 50%;
        visibility: hidden; font-style: normal;
    }
    .sel .orb {
        position: absolute; left: 0; top: 50%;
        width: calc(var(--u) * 4.9); height: calc(var(--u) * 4.9); margin-top: calc(var(--u) * -2.45);
        visibility: visible;
        background: radial-gradient(circle at 36% 30%, #ffffff 0, #d6e0ee 6%, #7f91ac 20%, #34425a 42%, #121824 68%, #05070b);
        box-shadow: 0 0 0 calc(var(--u) * .14) #e6bb63, 0 0 0 calc(var(--u) * .3) #151b24,
                    inset calc(var(--u) * -.45) calc(var(--u) * -.15) calc(var(--u) * .9) rgba(170, 195, 235, .45),
                    0 0 calc(var(--u) * 1.4) rgba(120, 150, 210, .5);
        perspective: calc(var(--u) * 12);
    }
    .sel .orb::after {
        content: "M"; position: absolute; inset: 0;
        display: flex; align-items: center; justify-content: center;
        font-family: var(--font-impact); font-size: calc(var(--u) * 3);
        color: #d8141c; text-shadow: 0 calc(var(--u) * .12) 0 #2a0302, 0 0 calc(var(--u) * .5) rgba(255, 60, 30, .6);
        animation: m-turn 2.4s linear infinite;
    }
    @keyframes m-turn { to { transform: rotateY(360deg); } }
    .sel {
        position: absolute; left: 0; top: 0; height: 0; width: 0; z-index: -1;
        pointer-events: none; opacity: 0;
        background: linear-gradient(90deg, rgba(160, 24, 14, .55), rgba(60, 8, 4, .25) 30%, transparent 70%),
                    linear-gradient(90deg, transparent, rgba(8, 6, 5, .9) calc(var(--u) * 2), rgba(8, 6, 5, .78) 75%, transparent);
        border-top: 2px solid transparent; border-bottom: 2px solid transparent;
        border-image: linear-gradient(90deg, transparent, #e6bb63 12%, #5d6e88 60%, transparent) 1;
        transition: transform .3s cubic-bezier(.3, .7, .25, 1), width .3s, height .3s, opacity .2s;
    }
    .sel.on { opacity: 1; }
</style>
