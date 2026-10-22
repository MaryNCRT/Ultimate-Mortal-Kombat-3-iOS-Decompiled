/* The menu's blips: square waves, made here, no samples. Decoration only. */
let g_ctx = null;

function beep(freq, dur, wave, vol) {
    try {
        g_ctx = g_ctx || new (window.AudioContext || window.webkitAudioContext)();
        const o = g_ctx.createOscillator(), g = g_ctx.createGain();
        o.type = wave || 'square';
        o.frequency.value = freq;
        g.gain.value = vol || 0.03;
        o.connect(g).connect(g_ctx.destination);
        o.start(g_ctx.currentTime);
        o.stop(g_ctx.currentTime + dur);
    } catch (err) { /* decoration only */ }
}

export const hoverBlip = () => beep(660, 0.035, 'square', 0.02);
export const selectBlip = () => { beep(440, 0.05, 'square', 0.035); beep(880, 0.045, 'square', 0.02); };
export const doneChord = () => { beep(523, 0.09); setTimeout(() => beep(784, 0.14), 90); };
export const failChord = () => { beep(220, 0.18, 'sawtooth'); setTimeout(() => beep(160, 0.18, 'sawtooth'), 120); };
