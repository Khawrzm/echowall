// electron/preload.js
const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('electronAPI', {
  navigate:      (url)    => ipcRenderer.invoke('navigate',       { url }),
  back:          ()       => ipcRenderer.invoke('tab-back'),
  forward:       ()       => ipcRenderer.invoke('tab-forward'),
  reload:        ()       => ipcRenderer.invoke('tab-reload'),
  newTab:        (url)    => ipcRenderer.invoke('tab-new',        { url }),
  closeTab:      (id)     => ipcRenderer.invoke('tab-close',      { id }),
  switchTab:     (id)     => ipcRenderer.invoke('tab-switch',     { id }),
  llmPrompt:     (prompt) => ipcRenderer.invoke('llm-prompt',     { prompt }),
  agentAction:   (cmd)    => ipcRenderer.invoke('agent-action',   cmd),
  toggleSidecar: (open)   => ipcRenderer.invoke('sidecar-toggle', { open }),
  windowControl: (action) => ipcRenderer.invoke('window-control', { action }),
  sessionSave:   ()       => ipcRenderer.invoke('session-save'),
  on: (channel, cb) => {
    const allowed = [
      'tab-created','tab-updated','tab-closed','tab-activated',
      'tab-loading','llm-status','llm-stream','agent-state'
    ];
    if (!allowed.includes(channel)) return;
    const handler = (_, data) => cb(data);
    ipcRenderer.on(channel, handler);
    return () => ipcRenderer.removeListener(channel, handler);
  }
});
