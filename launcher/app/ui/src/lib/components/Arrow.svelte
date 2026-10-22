<script>
    /* A blue chevron, as on the console's online pages. */
    let { dir = 'l', onclick, hidden = false } = $props();
</script>

<button class="arr {dir}" class:gone={hidden} aria-label={dir === 'l' ? '<' : '>'} {onclick}></button>

<style>
    .arr {
        flex: none; width: calc(var(--u) * 2.4); height: calc(var(--u) * 3.2);
        border: 0; padding: 0; cursor: pointer;
        background: var(--grad-arrow);
        filter: drop-shadow(0 0 calc(var(--u) * .5) rgba(80, 170, 255, .8));
    }
    .arr.l { clip-path: polygon(100% 0, 58% 0, 0 50%, 58% 100%, 100% 100%, 42% 50%); }
    .arr.r { clip-path: polygon(0 0, 42% 0, 100% 50%, 42% 100%, 0 100%, 58% 50%); }
    .arr:hover { background: var(--grad-arrow-hot); }
    .gone { visibility: hidden; }
</style>
