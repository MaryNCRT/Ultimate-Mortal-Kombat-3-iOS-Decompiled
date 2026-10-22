/* The profile's icon: a fighter's bio and endings art (the game's own
 * <NAME>_VERSUS.PNG, read from res\Textures, nothing imported) over a
 * glossy Frutiger Aero gradient of the profile's colour. runtime/profiles.c
 * draws the same icon with the same numbers. */

/* The palette, the same as the game's settings menu (settings_menu.c,
 * pal_make): 15 hues across and a grey column, seven shades from light to
 * dark; the colour picker beside it gives any other. */
function hslHex(h, s, l) {
    const c = (1 - Math.abs(2 * l - 1)) * s, x = c * (1 - Math.abs((h / 60) % 2 - 1)), m = l - c / 2;
    const [r, g, b] = h < 60 ? [c, x, 0] : h < 120 ? [x, c, 0] : h < 180 ? [0, c, x]
                    : h < 240 ? [0, x, c] : h < 300 ? [x, 0, c] : [c, 0, x];
    return [r, g, b].map(v => Math.floor((v + m) * 255 + 0.5).toString(16).padStart(2, '0')).join('').toUpperCase();
}
export const PROF_COLORS = [];
{
    const L = [0.88, 0.76, 0.64, 0.52, 0.40, 0.28, 0.16];
    const GREY = ['FFFFFF', 'D4D4D4', 'AAAAAA', '808080', '555555', '2B2B2B', '000000'];
    L.forEach((l, r) => {
        for (let c = 0; c < 15; c++) PROF_COLORS.push(hslHex(c * 24, 0.85, l));
        PROF_COLORS.push(GREY[r]);
    });
}

function hexRgb(h) {
    const n = parseInt(h, 16);
    return [(n >> 16) & 255, (n >> 8) & 255, n & 255];
}
const mix = (a, b, t) => a.map((v, i) => Math.round(v + (b[i] - v) * t));
const css = (c, a) => 'rgba(' + c.join(',') + ',' + (a === undefined ? 1 : a) + ')';

function roundRect(g, x, y, w, h, r) {
    g.beginPath();
    g.moveTo(x + r, y);
    g.arcTo(x + w, y, x + w, y + h, r);
    g.arcTo(x + w, y + h, x, y + h, r);
    g.arcTo(x, y + h, x, y, r);
    g.arcTo(x, y, x + w, y, r);
    g.closePath();
}

/* Draws the icon into a canvas: art is the fighter's picture (or null). */
export function drawIcon(cv, art, color, shade, color2) {
    const g = cv.getContext('2d'), S = cv.width, c = hexRgb(color);
    const white = [255, 255, 255], black = [0, 0, 0];
    const dark = shade === 'D', o = hexRgb(color2 || (dark ? '000000' : 'FFFFFF'));
    const c2 = shade === 'C' ? o : null;

    g.clearRect(0, 0, S, S);
    g.save();
    roundRect(g, 0, 0, S, S, S * 0.18);
    g.clip();
    /* the sky. light: the second colour (white unless chosen) into the
     * colour, deeper at the bottom; dark: the colour at the top going down
     * to the second (black unless chosen); custom: the colour to the second */
    let gr = g.createLinearGradient(0, 0, 0, S);
    gr.addColorStop(0, css(c2 ? mix(c, white, 0.2) : dark ? mix(c, white, 0.15) : mix(c, o, 0.55)));
    gr.addColorStop(0.5, css(c2 ? mix(c, c2, 0.5) : dark ? mix(c, o, 0.45) : c));
    gr.addColorStop(1, css(c2 ? c2 : dark ? mix(c, o, 0.92) : mix(c, black, 0.3)));
    g.fillStyle = gr;
    g.fillRect(0, 0, S, S);
    /* the glow rising from the bottom */
    gr = g.createRadialGradient(S / 2, S * 1.05, 0, S / 2, S * 1.05, S * 0.6);
    const gc = c2 ? mix(c2, white, 0.3) : dark ? c : mix(c, o, 0.6);
    gr.addColorStop(0, css(gc, c2 ? 0.45 : dark ? 0.35 : 0.75));
    gr.addColorStop(1, css(gc, 0));
    g.fillStyle = gr;
    g.fillRect(0, 0, S, S);
    /* the fighter */
    if (art)
        g.drawImage(art, 0, 0, S, S);
    /* the gloss: a white bubble over the top half */
    gr = g.createLinearGradient(0, 0, 0, S * 0.5);
    gr.addColorStop(0, 'rgba(255,255,255,' + (c2 ? 0.45 : dark ? 0.35 : 0.55) + ')');
    gr.addColorStop(1, 'rgba(255,255,255,' + (c2 ? 0.04 : dark ? 0.03 : 0.04) + ')');
    g.fillStyle = gr;
    g.beginPath();
    g.ellipse(S / 2, 0, S * 0.72, S * 0.5, 0, 0, Math.PI * 2);
    g.fill();
    g.restore();
    /* the rim */
    roundRect(g, S * 0.0125, S * 0.0125, S * 0.975, S * 0.975, S * 0.17);
    g.lineWidth = S * 0.025;
    g.strokeStyle = 'rgba(255,255,255,0.7)';
    g.stroke();
}
