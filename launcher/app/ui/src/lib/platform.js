/* The bridge to the main process (preload.js) and the system it runs on. */
export const api = window.umk3;
export const isWin = (api && api.platform) ? api.platform === 'win32' : true;
