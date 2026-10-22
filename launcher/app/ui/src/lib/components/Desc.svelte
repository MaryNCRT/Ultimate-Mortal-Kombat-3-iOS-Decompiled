<script>
    /* What the chosen line does, in the console's box under the picture:
     * a rimmed glass panel with two small rings in its lower corners. */
    import { app, tx, DESC } from '#lib/app.svelte.js';
</script>

<div class="desc">{tx(DESC[app.sec])}</div>

<style>
    /* in the backdrop's box: x 820..1325, y 615..765 */
    .desc {
        position: fixed; z-index: 5;
        left: calc(50vw + (820 - 800) * var(--k)); top: calc(50vh + (615 - 450) * var(--k));
        width: calc(var(--k) * 505); height: calc(var(--k) * 150);
        padding: calc(var(--k) * 14) calc(var(--k) * 18);
        font-size: calc(var(--k) * 26); color: var(--text);
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
