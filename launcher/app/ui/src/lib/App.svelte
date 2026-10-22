<script>
    /* The launcher's window. On the menu, the console release's main menu:
     * the title, the menu, the description and the player's picture on the
     * rings. A section opened from the menu fills the window, as the
     * console's online pages: its header, its page, the command bar. The
     * backdrop and the bars are shared. */
    import './theme/tokens.css';
    import './theme/base.css';
    import { onMount } from 'svelte';
    import { app, init } from './app.svelte.js';
    import Backdrop from './components/Backdrop.svelte';
    import TitleBar from './components/TitleBar.svelte';
    import ProfileBadge from './components/ProfileBadge.svelte';
    import Logo from './components/Logo.svelte';
    import MainMenu from './components/MainMenu.svelte';
    import Desc from './components/Desc.svelte';
    import SecHead from './components/SecHead.svelte';
    import CmdBar from './components/CmdBar.svelte';
    import Picker from './components/Picker.svelte';
    import SecBuild from './sections/SecBuild.svelte';
    import SecPlay from './sections/SecPlay.svelte';
    import SecVideo from './sections/SecVideo.svelte';
    import SecControls from './sections/SecControls.svelte';
    import SecProfile from './sections/SecProfile.svelte';
    import SecMods from './sections/SecMods.svelte';

    const PAGES = { build: SecBuild, play: SecPlay, video: SecVideo, controls: SecControls, profile: SecProfile, mods: SecMods };
    const Sec = $derived(PAGES[app.sec]);

    onMount(init);
</script>

<Backdrop />
{#if app.cfg}
    <div class="app">
        <TitleBar />
        <ProfileBadge />
        {#if app.full && Sec}
            <main class="stage" class:wide={app.sec === 'mods'}>
                <SecHead />
                <Sec />
            </main>
        {:else}
            <Logo />
            <MainMenu />
            <Desc />
        {/if}
        <CmdBar />
        <Picker />
    </div>
{/if}

<style>
    /* the title bar, the middle, the command bar */
    .app {
        position: fixed; inset: 0;
        display: grid;
        grid-template-rows: calc(var(--u) * 4.2) minmax(0, 1fr) auto;
        grid-template-areas: "tb" "mid" "cmd";
    }
    .stage {
        grid-area: mid; position: relative; min-height: 0; z-index: 4;
        justify-self: center; width: min(100%, calc(var(--u) * 118));
        padding: calc(var(--u) * 1.2) calc(var(--u) * 4) calc(var(--u) * 1.6);
        display: flex; flex-direction: column;
    }
    /* MODS: the whole window, for the viewer and the editor */
    .stage.wide { width: 100%; padding: calc(var(--u) * .6) calc(var(--u) * 1.2) calc(var(--u) * .8); }
</style>
