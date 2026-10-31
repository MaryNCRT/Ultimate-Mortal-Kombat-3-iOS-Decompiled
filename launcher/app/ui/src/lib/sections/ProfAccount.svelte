<script>
    /* ACCOUNT: log in, make an account or recover one (lib/account.js). A
     * GUEST plays with the profile code alone and has no leaderboards. The
     * recovery code is shown once, when it is given. A conflict -- this
     * computer and the cloud both changed -- is settled here. */
    import { onMount } from 'svelte';
    import { app, tx } from '#lib/app.svelte.js';
    import { api } from '#lib/platform.js';
    import { selectBlip, hoverBlip } from '#lib/sound.js';

    let st = $state(null);              /* {server, user, guest} */
    let mode = $state('login');         /* login | register | recover */
    let f = $state({ username: '', password: '', password2: '', email: '', recovery: '' });
    let msg = $state('');
    let busy = $state(false);
    let code = $state('');              /* a new recovery code, shown once */
    let email = $state('');
    let delArmed = $state(false);

    async function refresh() {
        st = await api.account.status();
        if (st.user) {
            const m = await api.account.me();
            if (m.ok) email = m.email || '';
        }
    }
    onMount(refresh);

    const err = e => tx('acErr_' + e) !== 'acErr_' + e ? tx('acErr_' + e) : tx('acErr_server_error');

    async function run(job) {
        if (busy) return;
        selectBlip();
        busy = true;
        msg = '';
        try {
            await job();
        } finally {
            busy = false;
        }
    }

    function synced(s) {
        if (!s) return '';
        if (s.error === 'conflict') { app.accConflict = true; return ''; }
        if (s.error) return err(s.error);
        return s.took === 'cloud' ? tx('acTookCloud') : s.took === 'local' ? tx('acTookLocal') : '';
    }

    function submit() {
        run(async () => {
            if (mode !== 'login' && f.password !== f.password2) { msg = tx('acErr_mismatch'); return; }
            const r = mode === 'register' ? await api.account.register($state.snapshot(f))
                    : mode === 'recover' ? await api.account.recover($state.snapshot(f))
                    : await api.account.login($state.snapshot(f));
            if (!r.ok) { msg = err(r.error); return; }
            code = r.recovery || '';
            f = { username: '', password: '', password2: '', email: '', recovery: '' };
            msg = synced(r.sync);
            await refresh();
        });
    }

    const logout = () => run(async () => { await api.account.logout(); code = ''; await refresh(); });
    const syncNow = () => run(async () => { msg = synced(await api.account.sync()) || tx('acSynced'); });
    const resolve = which => run(async () => {
        const r = await api.account.resolve(which);
        app.accConflict = false;
        msg = synced(r);
    });
    const saveEmail = () => run(async () => {
        const r = await api.account.email(email.trim());
        msg = r.ok ? tx('acSaved') : err(r.error);
    });
    function remove() {
        if (!delArmed) { delArmed = true; return; }
        run(async () => {
            const r = await api.account.remove(f.password);
            delArmed = false;
            f.password = '';
            if (!r.ok) { msg = err(r.error); return; }
            await refresh();
        });
    }
</script>

<div class="pane">
    {#if st && !st.server}
        <p class="note">{tx('acNoServer')}</p>
    {:else if st}
        {#if code}
            <div class="code">
                <h3>{tx('acCodeTitle')}</h3>
                <div class="idcode">{code}</div>
                <p class="note">{tx('acCodeNote')}</p>
                <button class="btn" onpointerenter={hoverBlip} onclick={() => { selectBlip(); code = ''; }}>{tx('acCodeDone')}</button>
            </div>
        {/if}

        {#if app.accConflict}
            <div class="code">
                <h3>{tx('acConflict')}</h3>
                <p class="note">{tx('acConflictNote')}</p>
                <div class="row">
                    <button class="btn" disabled={busy} onpointerenter={hoverBlip} onclick={() => resolve('local')}>{tx('acKeepLocal')}</button>
                    <button class="btn" disabled={busy} onpointerenter={hoverBlip} onclick={() => resolve('cloud')}>{tx('acKeepCloud')}</button>
                </div>
            </div>
        {/if}

        {#if st.user}
            <h3>{tx('acSignedAs')} <span class="who">{st.user}</span></h3>
            <p class="note">{tx('acSyncNote')}</p>
            <div class="row">
                <button class="btn hot" disabled={busy} onpointerenter={hoverBlip} onclick={syncNow}>{tx('acSync')}</button>
                <button class="btn" disabled={busy} onpointerenter={hoverBlip} onclick={logout}>{tx('acLogout')}</button>
            </div>
            <div class="fields">
                <div class="field wide">
                    <label class="lbl" for="acEmail">{tx('acEmail')}</label>
                    <input id="acEmail" class="edt" type="email" maxlength="254" spellcheck="false" bind:value={email}>
                </div>
            </div>
            <p class="warn">{tx('acDisclaimer')}</p>
            <div class="row"><button class="btn" disabled={busy} onpointerenter={hoverBlip} onclick={saveEmail}>{tx('acSaveEmail')}</button></div>

            <h3>{tx('acDelete')}</h3>
            <div class="fields">
                <div class="field wide">
                    <label class="lbl" for="acDelPass">{tx('acPassword')}</label>
                    <input id="acDelPass" class="edt" type="password" maxlength="128" bind:value={f.password}>
                </div>
            </div>
            <div class="row"><button class="btn" disabled={busy || !f.password} onpointerenter={hoverBlip} onclick={remove}>{tx(delArmed ? 'acDeleteAsk' : 'acDelete')}</button></div>
        {:else}
            <p class="note">{tx('acGuest')}</p>
            <span class="tabs sub">
                {#each [['login', 'acLogin'], ['register', 'acRegister'], ['recover', 'acRecover']] as [m, key]}
                    <button class="tab" class:active={mode === m} onpointerenter={hoverBlip}
                            onclick={() => { selectBlip(); mode = m; msg = ''; }}>{tx(key)}</button>
                {/each}
            </span>
            <p class="warn">{tx('acDisclaimer')}</p>
            <form class="fields" onsubmit={e => { e.preventDefault(); submit(); }}>
                <div class="field wide">
                    <label class="lbl" for="acUser">{tx('acUser')}</label>
                    <input id="acUser" class="edt" maxlength="20" spellcheck="false" autocomplete="username" bind:value={f.username}>
                </div>
                {#if mode === 'recover'}
                    <div class="field wide">
                        <label class="lbl" for="acRec">{tx('acRecCode')}</label>
                        <input id="acRec" class="edt" maxlength="40" spellcheck="false" bind:value={f.recovery}>
                    </div>
                {/if}
                <div class="field wide">
                    <label class="lbl" for="acPass">{tx(mode === 'recover' ? 'acNewPassword' : 'acPassword')}</label>
                    <input id="acPass" class="edt" type="password" maxlength="128"
                           autocomplete={mode === 'login' ? 'current-password' : 'new-password'} bind:value={f.password}>
                </div>
                {#if mode !== 'login'}
                    <div class="field wide">
                        <label class="lbl" for="acPass2">{tx('acPassword2')}</label>
                        <input id="acPass2" class="edt" type="password" maxlength="128" autocomplete="new-password" bind:value={f.password2}>
                    </div>
                {/if}
                {#if mode === 'register'}
                    <div class="field wide">
                        <label class="lbl" for="acMail">{tx('acEmailOpt')}</label>
                        <input id="acMail" class="edt" type="email" maxlength="254" spellcheck="false" bind:value={f.email}>
                    </div>
                {/if}
                <div class="row">
                    <button class="btn hot" type="submit" disabled={busy || !f.username || !f.password}
                            onpointerenter={hoverBlip}>{tx(mode === 'register' ? 'acRegister' : mode === 'recover' ? 'acRecover' : 'acLogin')}</button>
                </div>
            </form>
            <p class="note">{tx(mode === 'register' ? 'acRules' : 'acGuestNote')}</p>
        {/if}
        {#if msg}<p class="msg">{msg}</p>{/if}
    {/if}
</div>

<style>
    .fields { grid-template-columns: minmax(0, 1fr); max-width: calc(var(--u) * 40); }
    .row { display: flex; gap: calc(var(--u) * 1); flex-wrap: wrap; margin: calc(var(--u) * .8) 0; }
    .who { color: var(--code); }
    .warn { color: #ffb347; font-weight: bold; }
    .msg { color: #fff; }
    p { white-space: normal; overflow-wrap: anywhere; max-width: 100%; }
    .code { border: 2px solid var(--code); padding: calc(var(--u) * 1) calc(var(--u) * 1.6); margin-bottom: calc(var(--u) * 1.4); }
    .idcode {
        font-family: var(--font-mono); font-size: calc(var(--t) * 1.8);
        letter-spacing: .12em; color: var(--code); user-select: all;
    }
</style>
