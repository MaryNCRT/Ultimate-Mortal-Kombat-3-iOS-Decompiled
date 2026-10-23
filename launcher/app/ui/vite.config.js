/* The launcher's window as a SvelteKit app, built to static files that
 * Electron opens from disk (main.js, loadFile renderer/index.html):
 *
 *   - adapter-static writes the one page to ../renderer, the folder
 *     electron-builder packs; nothing is rendered ahead of time (the page
 *     needs window.umk3, which only the preload gives it)
 *   - a file:// path is no route, so the router reads location.hash
 *   - relative paths, so the page finds its scripts wherever it is unpacked
 *   - the same Content-Security-Policy as before; SvelteKit adds the hash of
 *     its own start-up script to it
 *   - no inline <style> or data: scripts: the CSP allows 'self' only */
import adapter from '@sveltejs/adapter-static';
import { sveltekit } from '@sveltejs/kit/vite';
import { defineConfig } from 'vite';

export default defineConfig({
    plugins: [sveltekit({
        adapter: adapter({ pages: '../renderer', assets: '../renderer', fallback: 'index.html', strict: false }),
        router: { type: 'hash' },
        paths: { relative: true },
        csp: {
            mode: 'hash',
            directives: {
                'default-src': ['self'],
                /* the viewer's SMOOTH MODEL is WebAssembly (mods/subsurf.js) */
                'script-src': ['self', 'wasm-unsafe-eval'],
                'style-src': ['self'],
                'img-src': ['self', 'data:'],
            },
        },
    })],
    build: { assetsInlineLimit: 0 },
});
