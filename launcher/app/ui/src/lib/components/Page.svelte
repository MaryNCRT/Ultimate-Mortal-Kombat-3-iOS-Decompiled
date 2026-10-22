<script>
    /* A section's page on the backdrop: a light veil with faded rules over
     * and under it instead of a framed box, scrolling when it is long. */
    let { children, class: cls = '' } = $props();
</script>

<section class="page {cls}">{@render children()}</section>

<style>
    .page {
        display: flex; flex-direction: column; flex: 1 1 auto; min-height: 0;
        overflow-x: hidden; overflow-y: auto;
        padding: calc(var(--u) * 2.2) calc(var(--u) * 2.6);
        background: var(--veil); box-shadow: var(--glow-in);
        border-top: 1px solid transparent; border-bottom: 1px solid transparent;
        border-image: var(--rim-fade) 1;
    }
    .page::-webkit-scrollbar { width: calc(var(--u) * .9); }
    .page::-webkit-scrollbar-thumb { background: rgba(200, 200, 255, .35); border-radius: calc(var(--u) * .45); }
    .page::-webkit-scrollbar-track { background: transparent; }
</style>
