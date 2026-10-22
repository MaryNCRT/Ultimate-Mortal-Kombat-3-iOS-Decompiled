<script>
    /* A profile icon (profileIcon.js), redrawn whenever the profile, the
     * fighter or the pictures change. */
    import { app, profArt } from '#lib/app.svelte.js';
    import { drawIcon } from '#lib/profileIcon.js';

    let { icon, color, shade, color2, size = 96, class: cls = '', onclick = undefined } = $props();
    let cv;

    $effect(() => {
        app.artTick;                    /* the pictures arrived */
        if (cv) drawIcon(cv, profArt[icon] || null, color, shade, color2);
    });
</script>

<canvas bind:this={cv} width={size} height={size} class={cls} {onclick}></canvas>
