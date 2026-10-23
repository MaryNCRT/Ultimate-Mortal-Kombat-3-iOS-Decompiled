<script>
    /* The gradient picker (Mary's design): the gradient as CSS -- two colours
     * and an angle, linear-gradient(<deg>deg, #from, #to), or one colour --
     * from a dial, two colour fields and presets. `value` is that CSS ('' for
     * none); onchange(value) is called for every change the player makes.
     * PROFILE's background uses it (SecProfile.svelte, profiles.ini). */
    import { tx } from '#lib/app.svelte.js';

    let { value = $bindable(''), label = '', onchange = undefined } = $props();

    const PRESETS = [
        'linear-gradient(to bottom, #600020, #200010)',
        'linear-gradient(to bottom, #7a1f45, #1d0c17)',
        'linear-gradient(to bottom, #5c0931, #0a0509)',
        'linear-gradient(to bottom, #ff2f8f, #5c0931)',
        'linear-gradient(to bottom, #ff5fa8, #3d1224)',
        'linear-gradient(135deg, #b3001b, #2a0f1e)',
        'linear-gradient(135deg, #ff0040, #08040a)',
        'linear-gradient(to bottom, #0b1030, #08040a)',
        'linear-gradient(135deg, #4d6fd1, #1a2340)',
        'linear-gradient(to bottom, #000000, #5c0931)',
        '#000000', '#1a0c13', '#3d1224', '#5c0931', '#7a1f45',
        '#ff2f8f', '#ff5fa8', '#ffffff',
    ];

    const DIRECTION_TO_ANGLE = { 'to top': 0, 'to right': 90, 'to bottom': 180, 'to left': 270 };

    let colorFrom = $state('#7a1f45');
    let colorTo = $state('#1d0c17');
    let angle = $state(180);
    let dial;
    let draggingDial = false;

    const customGradient = () =>
        colorFrom === colorTo ? colorFrom : `linear-gradient(${Math.round(angle)}deg, ${colorFrom}, ${colorTo})`;

    const syncFromValue = raw => {
        const hexes = raw.match(/#[0-9a-fA-F]{3,8}/g);
        if (hexes?.length) {
            colorFrom = hexes[0];
            colorTo = hexes[1] ?? hexes[0];
        }
        const deg = /(\d+(?:\.\d+)?)deg/.exec(raw);
        if (deg) {
            angle = ((parseFloat(deg[1]) % 360) + 360) % 360;
            return;
        }
        const named = /(to (?:top|right|bottom|left))/.exec(raw);
        if (named) angle = DIRECTION_TO_ANGLE[named[1]] ?? 180;
    };

    const set = v => {
        value = v;
        if (onchange) onchange(v);
    };

    const applyCustom = () => set(customGradient());

    const applyPreset = preset => {
        syncFromValue(preset);
        set(preset);
    };

    const dialAngle = event => {
        const rect = dial.getBoundingClientRect();
        const dx = event.clientX - (rect.left + rect.width / 2);
        const dy = event.clientY - (rect.top + rect.height / 2);
        angle = Math.round((Math.atan2(dx, -dy) * 180) / Math.PI + 360) % 360;
        applyCustom();
    };

    const onDialDown = event => {
        event.preventDefault();
        draggingDial = true;
        dial.setPointerCapture(event.pointerId);
        dialAngle(event);
    };

    const onDialMove = event => {
        if (draggingDial) dialAngle(event);
    };

    const onDialUp = () => {
        draggingDial = false;
    };

    const copy = async () => {
        if (!value) return;
        try {
            await navigator.clipboard.writeText(value);
        } catch {
            /* clipboard unavailable */
        }
    };

    $effect(() => {
        if (value) syncFromValue(value);
    });
</script>

<div class="bg-picker">
    <span class="bg-label">{label}</span>
    <div class="bg-preview-big" style:background={value || '#050505'}>
        {#if !value}<span class="bg-preview-empty">{tx('gpNone')}</span>{/if}
    </div>

    <div class="bg-controls">
        <div
            class="dial"
            bind:this={dial}
            role="slider"
            tabindex="0"
            aria-label={tx('gpAngle')}
            aria-valuenow={Math.round(angle)}
            aria-valuemin={0}
            aria-valuemax={360}
            onpointerdown={onDialDown}
            onpointermove={onDialMove}
            onpointerup={onDialUp}
            onpointercancel={onDialUp}
            onkeydown={e => {
                if (e.key === 'ArrowLeft' || e.key === 'ArrowDown') { angle = (angle + 355) % 360; applyCustom(); }
                if (e.key === 'ArrowRight' || e.key === 'ArrowUp') { angle = (angle + 5) % 360; applyCustom(); }
            }}
        >
            <span class="dial-ring"></span>
            <span class="dial-arm" style:transform={`translate(-50%, -100%) rotate(${angle}deg)`}></span>
            <span class="dial-handle" style:transform={`translate(-50%, -50%) rotate(${angle}deg) translateY(-20px)`}></span>
            <span class="dial-center"></span>
        </div>
        <div class="bg-custom">
            <label class="color-field"><input type="color" value={colorFrom}
                oninput={e => { colorFrom = e.currentTarget.value; applyCustom(); }} /><span>{tx('gpFrom')}</span></label>
            <label class="color-field"><input type="color" value={colorTo}
                oninput={e => { colorTo = e.currentTarget.value; applyCustom(); }} /><span>{tx('gpTo')}</span></label>
            <span class="angle-value">{Math.round(angle)}°</span>
        </div>
    </div>

    <div class="bg-swatches" role="listbox" aria-label={tx('gpPresets')}>
        {#each PRESETS as preset}
            <button
                type="button"
                class="bg-swatch"
                class:active={value === preset}
                style:background={preset}
                title={preset}
                aria-label={preset}
                onclick={() => applyPreset(preset)}
            ></button>
        {/each}
    </div>

    <div class="bg-output">
        <input readonly {value} aria-label="CSS" />
        <button type="button" onclick={copy}>{tx('gpCopy')}</button>
        <button type="button" onclick={() => set('')}>{tx('gpClear')}</button>
    </div>
</div>

<style>
    .bg-picker {
        display: grid;
        gap: 10px;
        width: min(440px, 100%);
        padding: 16px;
        border: 1px solid #542637;
        background: #0f080c;
        color: #f5e6ef;
        font-family: 'Consolas', 'Courier New', monospace;
        box-sizing: border-box;
    }

    .bg-label {
        color: #a98796;
        font-size: 0.66rem;
        letter-spacing: 0.08em;
        text-transform: uppercase;
    }

    .bg-preview-big {
        display: grid;
        height: 52px;
        place-items: center;
        border: 1px solid #542637;
        transition: background 0.15s ease;
    }

    .bg-preview-empty {
        color: #a98796;
        font-size: 0.66rem;
    }

    .bg-controls {
        display: flex;
        align-items: center;
        gap: 14px;
    }

    .dial {
        position: relative;
        width: 56px;
        height: 56px;
        flex: 0 0 auto;
        cursor: pointer;
        touch-action: none;
    }

    .dial:focus-visible {
        outline: 1px solid #ff5fa8;
        outline-offset: 2px;
    }

    .dial-ring {
        position: absolute;
        inset: 0;
        border: 1px solid #542637;
        border-radius: 50%;
        background: radial-gradient(circle, #1d0c17 0%, #0a0509 100%);
        box-shadow: inset 0 0 8px #000;
    }

    .dial-arm {
        position: absolute;
        top: 50%;
        left: 50%;
        width: 2px;
        height: 20px;
        transform-origin: 50% 100%;
        background: linear-gradient(to top, transparent, #ff5fa8);
    }

    .dial-handle {
        position: absolute;
        top: 50%;
        left: 50%;
        width: 10px;
        height: 10px;
        border: 1px solid #ff5fa8;
        border-radius: 50%;
        background: #ff2f8f;
        box-shadow: 0 0 6px rgba(255, 47, 143, 0.7);
    }

    .dial-center {
        position: absolute;
        top: 50%;
        left: 50%;
        width: 4px;
        height: 4px;
        border-radius: 50%;
        background: #ff5fa8;
        transform: translate(-50%, -50%);
    }

    .bg-custom {
        display: flex;
        align-items: center;
        gap: 12px;
        flex: 1;
        min-width: 0;
    }

    .color-field {
        display: grid;
        justify-items: center;
        gap: 3px;
        color: #a98796;
        font-size: 0.6rem;
        cursor: pointer;
    }

    .color-field input[type='color'] {
        width: 36px;
        height: 30px;
        padding: 1px;
        border: 1px solid #542637;
        background: #0a0a0a;
        cursor: pointer;
    }

    .angle-value {
        margin-left: auto;
        color: #ff5fa8;
        font-size: 0.75rem;
    }

    .bg-swatches {
        display: grid;
        grid-template-columns: repeat(auto-fill, minmax(42px, 1fr));
        gap: 5px;
    }

    .bg-swatch {
        height: 26px;
        padding: 0;
        border: 1px solid #3a1a28;
        cursor: pointer;
    }

    .bg-swatch:hover,
    .bg-swatch:focus-visible {
        border-color: #ff5fa8;
        box-shadow: 0 0 6px rgba(255, 47, 143, 0.45);
    }

    .bg-swatch.active {
        border-color: #ff2f8f;
        outline: 1px solid #ff2f8f;
        box-shadow: 0 0 8px rgba(255, 47, 143, 0.6);
    }

    .bg-output {
        display: flex;
        gap: 8px;
        align-items: center;
    }

    .bg-output input {
        flex: 1;
        min-width: 0;
        padding: 6px 8px;
        border: 1px solid #542637;
        background: #050505;
        color: #f5e6ef;
        font-family: inherit;
        font-size: 0.7rem;
    }

    .bg-output button {
        padding: 6px 12px;
        border: 1px solid #ff5fa8;
        background: transparent;
        color: #ff5fa8;
        font-family: inherit;
        font-size: 0.7rem;
        cursor: pointer;
    }

    .bg-output button:hover {
        background: rgba(255, 95, 168, 0.12);
        box-shadow: 0 0 6px rgba(255, 47, 143, 0.4);
    }
</style>
