<script>
    /* The fighter's window (0.4.2, Mary): over the page, the window behind
     * darkened, every fighter with his icon and name; a click chooses him
     * -- the icon and the favourite are one -- and closes it. Esc or a click
     * outside closes it too. Its outfit row chooses the first or second. */
    import { onMount, onDestroy } from 'svelte';
    import { app, tx } from '#lib/app.svelte.js';
    import { portal } from '#lib/portal.js';
    import { hoverBlip, selectBlip } from '#lib/sound.js';
    import { FIGHTERS } from '#lib/fighters.js';
    import IconCanvas from './IconCanvas.svelte';

    let { p, pick, close } = $props();
    const names = $derived(FIGHTERS[app.ui === 'ES' ? 'ES' : 'EN']);

    function onKey(e) {
        if (e.key !== 'Escape') return;
        e.stopPropagation();
        e.preventDefault();
        selectBlip();
        close();
    }
    onMount(() => { app.overlay++; window.addEventListener('keydown', onKey, true); });
    onDestroy(() => { app.overlay--; window.removeEventListener('keydown', onKey, true); });

    /* the fighter; the window stays open for his colour (Mary, 0.4.10:
     * the gallery chooses both) */
    function choose(i) {
        selectBlip();
        pick({ icon: i });
    }
</script>

<div class="veil" role="presentation" use:portal onclick={e => { if (e.target === e.currentTarget) { selectBlip(); close(); } }}>
    <div class="win" role="dialog" aria-modal="true" aria-label={tx('profFavLbl')}>
        <i class="orn tl"></i><i class="orn tr"></i><i class="orn bl"></i><i class="orn br"></i>
        <h2>{tx('profPickTitle')}</h2>
        <div class="grid">
            {#each names as n, i}
                <button class="cell" class:on={i === p.icon} onpointerenter={hoverBlip} onclick={() => choose(i)}>
                    <IconCanvas size={96} icon={i} color={p.color} shade={p.shade} color2={p.color2} angle={p.angle} alt={p.alt} />
                    <span>{n}</span>
                </button>
            {/each}
        </div>
        <div class="foot">
            <span class="tabs sub cols6">
                {#each [0, 1, 2, 3, 4, 5] as a}
                    <button class="tab" class:active={(p.alt | 0) === a} onpointerenter={hoverBlip}
                            onclick={() => { selectBlip(); pick({ alt: a }); }}>{tx('profColour')} {a + 1}</button>
                {/each}
            </span>
            <button class="btn" onpointerenter={hoverBlip} onclick={() => { selectBlip(); close(); }}>{tx('profPickClose')}</button>
        </div>
    </div>
</div>

<style>
    .veil {
        position: fixed; inset: 0; z-index: 50;
        display: flex; align-items: center; justify-content: center;
        background: radial-gradient(ellipse at 50% 45%, rgba(4, 6, 10, .72), rgba(0, 0, 0, .9));
        backdrop-filter: blur(3px) saturate(.6);
        animation: fade .18s ease-out;
    }
    @keyframes fade { from { opacity: 0; } }
    .win {
        position: relative; width: min(92vw, calc(var(--u) * 100)); max-height: 88vh;
        display: flex; flex-direction: column;
        padding: calc(var(--u) * 2) calc(var(--u) * 2.4);
        background: var(--veil); box-shadow: var(--glow-in), 0 calc(var(--u) * 1) calc(var(--u) * 4) #000;
        border-left: calc(var(--u) * .45) solid #2e3846; border-right: calc(var(--u) * .45) solid #2e3846;
        border-top: calc(var(--u) * .6) solid #e6b75c; border-bottom: calc(var(--u) * .45) solid #8e150c;
        animation: rise .22s ease-out;
    }
    @keyframes rise { from { transform: translateY(calc(var(--u) * 1.5)) scale(.98); opacity: 0; } }
    .orn { position: absolute; width: calc(var(--u) * 2.4); height: calc(var(--u) * 2.4); background-size: contain; background-repeat: no-repeat; pointer-events: none; }
    .orn.tl { left: calc(var(--u) * -.9); top: calc(var(--u) * -1); background-image: var(--orn-tl); }
    .orn.tr { right: calc(var(--u) * -.9); top: calc(var(--u) * -1); background-image: var(--orn-tr); }
    .orn.bl { left: calc(var(--u) * -.9); bottom: calc(var(--u) * -.9); background-image: var(--orn-bl); }
    .orn.br { right: calc(var(--u) * -.9); bottom: calc(var(--u) * -.9); background-image: var(--orn-br); }
    h2 { font-size: calc(var(--u) * 2.2); text-align: center; }
    .grid {
        display: grid; grid-template-columns: repeat(8, minmax(0, 1fr)); gap: calc(var(--u) * .8);
        overflow-y: auto; padding: calc(var(--u) * .4);
    }
    .cell {
        display: flex; flex-direction: column; align-items: center; gap: calc(var(--u) * .4);
        padding: calc(var(--u) * .5) calc(var(--u) * .3) calc(var(--u) * .6);
        background: var(--plate); border: 1px solid #2e3846; border-radius: 3px; cursor: pointer;
        box-shadow: var(--bevel); transition: border-color .12s, transform .12s, box-shadow .12s;
    }
    .cell :global(canvas) { width: 100%; height: auto; border-radius: 10%; box-shadow: 0 0 0 1px #000; }
    .cell span {
        font-size: calc(var(--u) * 1.05); font-weight: 800; letter-spacing: .04em; color: var(--text);
        text-transform: uppercase; text-align: center; line-height: 1.15; text-shadow: 0 1px 0 #000;
        white-space: nowrap; overflow: hidden; text-overflow: ellipsis; max-width: 100%;
    }
    .cell:hover { border-color: #e6bb63; transform: translateY(-2px); box-shadow: var(--bevel), var(--hover-glow); }
    .cell.on {
        border-color: #e6bb63;
        background: linear-gradient(rgba(215, 228, 255, .2), rgba(255, 255, 255, 0) 50%), var(--tex-grain), linear-gradient(#a3201a, #420604);
        box-shadow: var(--bevel), 0 0 calc(var(--u) * 1) rgba(196, 25, 15, .6);
    }
    .foot { display: flex; justify-content: space-between; align-items: center; margin-top: calc(var(--u) * 1.4); gap: calc(var(--u) * 1.4); }
    /* colours 1..6 share the row's width, never past the CLOSE button (Mary:
     * nothing overlaps anything) */
    .cols6 { flex: 1; min-width: 0; display: grid; grid-template-columns: repeat(6, minmax(0, 1fr)); gap: calc(var(--u) * .6); }
    .cols6 .tab { min-width: 0; padding-left: calc(var(--u) * .4); padding-right: calc(var(--u) * .4); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; text-align: center; }
    .foot > .btn { flex: none; }
</style>
