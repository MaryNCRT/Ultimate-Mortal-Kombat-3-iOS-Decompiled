<script>
    /* A chevron, as on the console's online pages: cast gold (0.4.2). */
    let { dir = 'l', onclick, hidden = false } = $props();
</script>

<button class="arr {dir}" class:gone={hidden} aria-label={dir === 'l' ? '<' : '>'} {onclick}></button>

<style>
    /* a cast gold chevron with a dark edge (0.4.2) */
    .arr {
        flex: none; width: calc(var(--u) * 2.4); height: calc(var(--u) * 3.2);
        border: 0; padding: 0; cursor: pointer;
        background: var(--grad-arrow);
        filter: drop-shadow(0 calc(var(--u) * .15) 0 #000) drop-shadow(0 0 calc(var(--u) * .4) rgba(140, 170, 230, .45));
        transition: filter .15s;
    }
    .arr.l { clip-path: polygon(100% 0, 58% 0, 0 50%, 58% 100%, 100% 100%, 42% 50%); }
    .arr.r { clip-path: polygon(0 0, 42% 0, 100% 50%, 42% 100%, 0 100%, 58% 50%); }
    .arr:hover { background: var(--grad-arrow-hot); filter: drop-shadow(0 calc(var(--u) * .15) 0 #000) drop-shadow(0 0 calc(var(--u) * .7) rgba(255, 170, 60, .8)); }
    .arr:active { transform: translateY(1px); }
    .gone { visibility: hidden; }
</style>
