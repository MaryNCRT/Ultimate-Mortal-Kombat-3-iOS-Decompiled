<script>
    /* The title, as on the console's main menu: top right, over the small
     * ring, with a dark border hugging its letters and a white-blue halo
     * (the light and rays behind it are the backdrop's). It ships inside the
     * launcher; logo.png in the game folder, if the player put one there,
     * replaces it. */
    import { app } from '#lib/app.svelte.js';
    import logo from '#lib/assets/logo.png';
</script>

<div class="logo">
    <img src={app.logo || logo} alt="Ultimate Mortal Kombat 3" draggable="false">
</div>

<style>
    /* in the backdrop's box: x 770..1320, from y 62 */
    .logo {
        position: fixed; z-index: 4;
        left: calc(50vw + (770 - 800) * var(--k)); top: calc(50vh + (62 - 450) * var(--k));
        width: calc(var(--k) * 550);
    }
    /* the border: four hard shadows, each round the last; then the halo */
    img {
        display: block; width: 100%;
        filter: drop-shadow(calc(var(--k) * 2.5) 0 0 var(--ink)) drop-shadow(calc(var(--k) * -2.5) 0 0 var(--ink))
                drop-shadow(0 calc(var(--k) * 2.5) 0 var(--ink)) drop-shadow(0 calc(var(--k) * -2.5) 0 var(--ink))
                drop-shadow(0 0 calc(var(--k) * 5) rgba(235, 240, 255, .8))
                drop-shadow(0 0 calc(var(--k) * 20) rgba(130, 165, 255, .55));
    }
</style>
