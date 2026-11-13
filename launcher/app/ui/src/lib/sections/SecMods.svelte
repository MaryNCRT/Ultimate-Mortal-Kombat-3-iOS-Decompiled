<script>
    /* MODS: seven pages -- the MOD MANAGER, the MOD HUB, the CHARACTER VIEWER, the STAGE
     * VIEWER, the CHARACTER EDITOR, TEXTURES and EXPORT (sections/mods/). */
    import Page from '#lib/components/Page.svelte';
    import SubNav from '#lib/components/SubNav.svelte';
    import { ms, MODS_TABS } from '#lib/mods/store.svelte.js';
    import ModManager from './mods/ModManager.svelte';
    import ModHub from './mods/ModHub.svelte';
    import ModViewer from './mods/ModViewer.svelte';
    import StageViewer from './mods/StageViewer.svelte';
    import ModEditor from './mods/ModEditor.svelte';
    import ModExporter from './mods/ModExporter.svelte';
    import ModTextures from './mods/ModTextures.svelte';
    const INSIDE_TABS = [['models', 'vwRelModels'], ['tex', 'vwRelTex']];
</script>

<Page class="tight">
    {#if ms.tab === 'view' && ms.inside}
        <!-- inside a fighter: his MODELS and TEXTURES where the pages were -->
        <SubNav tabs={INSIDE_TABS} current={ms.leftTab} pick={v => { ms.leftTab = v; ms.chosen = false; }}
                step={() => { ms.leftTab = ms.leftTab === 'models' ? 'tex' : 'models'; ms.chosen = false; }} />
    {:else}
        <SubNav tabs={MODS_TABS} current={ms.tab} pick={v => ms.tab = v} />
    {/if}
    {#if ms.tab === 'mgr'}
        <ModManager />
    {:else if ms.tab === 'hub'}
        <ModHub />
    {:else if ms.tab === 'view'}
        <ModViewer />
    {:else if ms.tab === 'stage'}
        <StageViewer />
    {:else if ms.tab === 'edit'}
        <ModEditor />
    {:else if ms.tab === 'tex'}
        <ModTextures />
    {:else}
        <ModExporter />
    {/if}
</Page>
