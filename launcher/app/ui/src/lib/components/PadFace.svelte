<script>
    /* A pad button's picture in a make's set, or its name; combo: "A+B"
     * with a plus between, "—" for none. */
    import { app } from '#lib/app.svelte.js';
    import { padFile, padName } from '#lib/glyphs.js';

    let { kind, b = '', combo = false } = $props();
    const parts = $derived((combo ? String(b || '').split('+').filter(Boolean) : [b])
        .map(p => ({ p, src: p ? app.glyphs[padFile(kind, p)] : null })));
</script>

{#each parts as { p, src }, i}
    {#if i}<span class="gplus">+</span>{/if}
    {#if src}
        <img class="glyph" {src} alt="" draggable="false">
    {:else}
        <span class="gname">{p ? padName(kind, p) : '—'}</span>
    {/if}
{:else}
    <span class="gname">—</span>
{/each}
