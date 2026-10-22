<script>
    /* A progress bar: blue, red when it failed. */
    let { pct = 0, fail = false } = $props();
</script>

<div class="bar" class:fail><i style:width="{Math.max(0, Math.min(100, pct || 0))}%"></i></div>

<style>
    .bar {
        flex: 1; height: calc(var(--u) * 1.6); border: 1px solid var(--line);
        border-radius: calc(var(--u) * .8); overflow: hidden; background: rgba(0, 0, 0, .5);
    }
    i { display: block; height: 100%; width: 0; background: linear-gradient(90deg, var(--blue), var(--code)); transition: width .3s; }
    .fail i { background: linear-gradient(90deg, var(--red), #ff9c9c); }
</style>
