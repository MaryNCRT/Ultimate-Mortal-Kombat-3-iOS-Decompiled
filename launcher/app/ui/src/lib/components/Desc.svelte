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
    /* in the backdrop's box: x 820..1325, y 615..765 -- a stone tablet in
     * a bronze frame, the gold brackets in its corners */
    .desc {
        position: fixed; z-index: 5; box-sizing: border-box; overflow: hidden;
        left: calc(50vw + (820 - 800) * var(--k)); top: calc(50vh + (615 - 450) * var(--k));
        width: calc(var(--k) * 505); height: calc(var(--k) * 150);
        padding: calc(var(--k) * 14) calc(var(--k) * 24) calc(var(--k) * 22);
        font-size: calc(var(--k) * 26 * var(--fit, 1)); color: var(--text);
        text-shadow: 0 1px 0 #000;
        background: var(--orn-tl) left 2px top 2px / calc(var(--k) * 22) no-repeat,
                    var(--orn-tr) right 2px top 2px / calc(var(--k) * 22) no-repeat,
                    var(--orn-bl) left 2px bottom 2px / calc(var(--k) * 22) no-repeat,
                    var(--orn-br) right 2px bottom 2px / calc(var(--k) * 22) no-repeat,
                    var(--panel);
        border: 2px solid #46546a; border-top-color: #e6bb63; border-radius: 2px;
        box-shadow: var(--glow-in), 0 calc(var(--k) * 6) calc(var(--k) * 18) rgba(0, 0, 0, .75);
    }
</style>
