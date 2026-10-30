/* use:portal -- a window put on the page's top (document.body), over the
 * launcher's bar and the player's card, which sit above anything a section
 * draws. The sizes the section set (--u, --t ...) go with it. */
const KEEP = ['--u', '--t', '--k', '--ctl-h', '--r-sm', 'font-family', 'color'];

export function portal(node) {
    const cs = getComputedStyle(node.parentElement || document.body);
    for (const v of KEEP) {
        const x = v.startsWith('--') ? cs.getPropertyValue(v) : cs[v === 'font-family' ? 'fontFamily' : v];
        if (x) node.style.setProperty(v, x);
    }
    document.body.appendChild(node);
    return { destroy() { node.remove(); } };
}
