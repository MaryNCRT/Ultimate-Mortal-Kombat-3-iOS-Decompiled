<script>
    /* A keyboard key's picture (kb/T_<file>_Key_Vintage), or its name. */
    import { app } from '#lib/app.svelte.js';
    import { kbFile, vkLabel } from '#lib/glyphs.js';

    let { vk } = $props();
    const src = $derived.by(() => {
        const f = kbFile(vk);
        return f ? app.glyphs['kb/T_' + f + '_Key_Vintage'] : null;
    });
</script>

<span class="kkey">
    {#if src}
        {#if vk >= 96 && vk <= 111}<span class="knum">NUM</span>{/if}
        <img class="glyph" {src} alt="" draggable="false">
    {:else}
        {vkLabel(vk)}
    {/if}
</span>
