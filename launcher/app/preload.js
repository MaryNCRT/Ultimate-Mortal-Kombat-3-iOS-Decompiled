'use strict';

const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('umk3', {
    root: () => ipcRenderer.invoke('root:get'),
    logo: () => ipcRenderer.invoke('logo:get'),
    loadConfig: () => ipcRenderer.invoke('config:load'),
    saveConfig: delta => ipcRenderer.invoke('config:save', delta),
    browseIpa: () => ipcRenderer.invoke('ipa:browse'),
    browseFrame: () => ipcRenderer.invoke('frame:browse'),
    startBuild: ipa => ipcRenderer.invoke('build:start', ipa),
    cancelBuild: () => ipcRenderer.invoke('build:cancel'),
    play: () => ipcRenderer.invoke('play'),
    listPads: () => ipcRenderer.invoke('pads:list'),
    close: () => ipcRenderer.invoke('window:close'),
    minimize: () => ipcRenderer.invoke('window:minimize'),
    maximize: () => ipcRenderer.invoke('window:maximize'),
    fullscreen: () => ipcRenderer.invoke('window:fullscreen'),
    onBuildData: cb => ipcRenderer.on('build:data', (_e, d) => cb(d)),
    shot: () => ipcRenderer.invoke('shot:capture'),
    debugLayout: payload => ipcRenderer.invoke('debug:layout', payload),
});