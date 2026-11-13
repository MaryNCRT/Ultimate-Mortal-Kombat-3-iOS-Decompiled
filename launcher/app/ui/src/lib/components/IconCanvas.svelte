<script>
    /* A profile icon (profileIcon.js), redrawn whenever the profile, the
     * fighter or the pictures change. */
    import { app, artOf } from '#lib/app.svelte.js';
    import { drawIcon } from '#lib/profileIcon.js';

    /* alt: the fighter's colour less one, 0..5 (app.svelte.js artOf) */
    let { icon, color, shade, color2, angle = -1, alt = 0, size = 96, class: cls = '', onclick = undefined } = $props();
    let cv;

    $effect(() => {
        app.artTick;                    /* the pictures arrived */
        const art = artOf(icon, alt);
        if (cv) drawIcon(cv, art, color, shade, color2, angle);
    });
</script>

<canvas bind:this={cv} width={size} height={size} class={cls} {onclick}></canvas>
