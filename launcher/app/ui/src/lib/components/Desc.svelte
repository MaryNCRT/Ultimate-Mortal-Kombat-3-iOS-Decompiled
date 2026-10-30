<script>
    /* What the chosen line does, in the console's box under the picture:
     * a rimmed glass panel with two small rings in its lower corners. A text
     * too long for the box is drawn smaller until it fits (it ran out under
     * the bar: MODS' in English). */
    import { app, tx, DESC } from '#lib/app.svelte.js';

    let box = $state(), scale = $state(1);
    const text = $derived(tx(DESC[app.sec]));
    function fitText() {
        if (!box) return;
        let s = 1;
        box.style.setProperty('--fit', '1');
        while (s > 0.5 && box.scrollHeight > box.clientHeight + 1) {
            s -= 0.05;
            box.style.setProperty('--fit', String(s));
        }
        scale = s;
    }
    $effect(() => { text; fitText(); });
    $effect(() => {
        const r = new ResizeObserver(fitText);
        r.observe(document.documentElement);
        return () => r.disconnect();
    });
</script>

<div class="desc" bind:this={box} style:--fit={scale}>{text}</div>

<style>
    /* in the backdrop's box: x 820..1325, y 615..765 */
    .desc {
        position: fixed; z-index: 5; box-sizing: border-box; overflow: hidden;
        left: calc(50vw + (820 - 800) * var(--k)); top: calc(50vh + (615 - 450) * var(--k));
        width: calc(var(--k) * 505); height: calc(var(--k) * 150);
        padding: calc(var(--k) * 14) calc(var(--k) * 18) calc(var(--k) * 22);
        font-size: calc(var(--k) * 26 * var(--fit, 1)); color: var(--text);
        background: var(--panel);
        border: 1px solid var(--rim); border-radius: var(--r-sm);
        box-shadow: inset 0 0 calc(var(--u) * 2) rgba(120, 125, 255, .15);
    }
    .desc::before, .desc::after {
        content: ""; position: absolute; bottom: calc(var(--u) * .4);
        width: calc(var(--u) * 1.8); height: calc(var(--u) * .8);
        background: radial-gradient(circle at 25% 50%, transparent calc(var(--u) * .2), var(--rim) calc(var(--u) * .22) calc(var(--u) * .32), transparent calc(var(--u) * .34)),
                    radial-gradient(circle at 75% 50%, transparent calc(var(--u) * .2), var(--rim) calc(var(--u) * .22) calc(var(--u) * .32), transparent calc(var(--u) * .34));
    }
    .desc::before { left: calc(var(--u) * .6); }
    .desc::after { right: calc(var(--u) * .6); }
</style>
