<script>
    /* MODS: five pages -- the MOD MANAGER, the MODEL VIEWER, the CHARACTER
     * EDITOR, TEXTURES and EXPORT (sections/mods/). */
    import Page from '#lib/components/Page.svelte';
    import SubNav from '#lib/components/SubNav.svelte';
    import { ms, MODS_TABS } from '#lib/mods/store.svelte.js';
    import ModManager from './mods/ModManager.svelte';
    import ModViewer from './mods/ModViewer.svelte';
    import ModEditor from './mods/ModEditor.svelte';
    import ModExporter from './mods/ModExporter.svelte';
    import ModTextures from './mods/ModTextures.svelte';
</script>

<Page>
    <SubNav tabs={MODS_TABS} current={ms.tab} pick={v => ms.tab = v} />
    {#if ms.tab === 'mgr'}
        <ModManager />
    {:else if ms.tab === 'view'}
        <ModViewer />
    {:else if ms.tab === 'edit'}
        <ModEditor />
    {:else if ms.tab === 'tex'}
        <ModTextures />
    {:else}
        <ModExporter />
    {/if}
</Page>
