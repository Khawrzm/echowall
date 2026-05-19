// electron/main.js
const { app, BrowserWindow, BrowserView, ipcMain, session } = require('electron');
const path = require('path');
const fs = require('fs');
const { getLlama, LlamaChatSession } = require('node-llama-cpp');

// ── State ──────────────────────────────────────────────────────────────────
const state = {
  mainWindow: null,
  tabs: [],
  activeTabId: null,
  llama: null,
  llamaModel: null,
  llamaContext: null,
  llamaSession: null,
  modelLoaded: false,
  SHELL_HEIGHT: 80,
  SIDECAR_WIDTH: 320
};

// ── App Ready ──────────────────────────────────────────────────────────────
app.whenReady().then(async () => {
  await createMainWindow();
  await initLlama();
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit();
});

app.on('activate', () => {
  if (BrowserWindow.getAllWindows().length === 0) createMainWindow();
});

// ── Main Window ────────────────────────────────────────────────────────────
async function createMainWindow() {
  state.mainWindow = new BrowserWindow({
    width: 1440,
    height: 900,
    minWidth: 800,
    minHeight: 600,
    frame: false,
    titleBarStyle: 'hidden',
    backgroundColor: '#0d0d0d',
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
      webviewTag: false
    }
  });

  state.mainWindow.loadFile(path.join(__dirname, '../ui/shell.html'));

  state.mainWindow.on('resize', () => repositionActiveTab());

  state.mainWindow.webContents.on('did-finish-load', () => {
    createTab('https://www.google.com');
  });
}

// ── Tab Management ─────────────────────────────────────────────────────────
function getContentBounds() {
  const [w, h] = state.mainWindow.getContentSize();
  const sidecarVisible = state.mainWindow.sidecarOpen || false;
  return {
    x: 0,
    y: state.SHELL_HEIGHT,
    width: sidecarVisible ? w - state.SIDECAR_WIDTH : w,
    height: h - state.SHELL_HEIGHT
  };
}

function createTab(url = 'https://www.google.com') {
  const view = new BrowserView({
    webPreferences: {
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
      preload: path.join(__dirname, 'tab-preload.js')
    }
  });

  const id = `tab_${Date.now()}`;
  const tab = { id, view, url, title: 'New Tab', favicon: null };
  state.tabs.push(tab);

  state.mainWindow.addBrowserView(view);
  view.setBounds(getContentBounds());
  view.setAutoResize({ width: true, height: true });
  view.webContents.loadURL(url);

  view.webContents.on('did-navigate', (_, navUrl) => {
    tab.url = navUrl;
    notifyShell('tab-updated', { id, url: navUrl, title: tab.title });
  });

  view.webContents.on('page-title-updated', (_, title) => {
    tab.title = title;
    notifyShell('tab-updated', { id, url: tab.url, title });
  });

  view.webContents.on('page-favicon-updated', (_, favicons) => {
    tab.favicon = favicons[0] || null;
    notifyShell('tab-updated', { id, favicon: tab.favicon });
  });

  view.webContents.on('did-start-loading', () => {
    notifyShell('tab-loading', { id, loading: true });
  });

  view.webContents.on('did-stop-loading', () => {
    notifyShell('tab-loading', { id, loading: false });
  });

  setActiveTab(id);
  notifyShell('tab-created', { id, url, title: 'New Tab' });
  return id;
}

function setActiveTab(id) {
  const tab = state.tabs.find(t => t.id === id);
  if (!tab) return;

  state.tabs.forEach(t => {
    if (t.id !== id) state.mainWindow.removeBrowserView(t.view);
  });

  state.mainWindow.addBrowserView(tab.view);
  tab.view.setBounds(getContentBounds());
  state.activeTabId = id;
  notifyShell('tab-activated', { id, url: tab.url, title: tab.title });
}

function closeTab(id) {
  const idx = state.tabs.findIndex(t => t.id === id);
  if (idx === -1) return;

  const tab = state.tabs[idx];
  state.mainWindow.removeBrowserView(tab.view);
  tab.view.webContents.destroy();
  state.tabs.splice(idx, 1);

  notifyShell('tab-closed', { id });

  if (state.tabs.length === 0) {
    createTab();
  } else if (state.activeTabId === id) {
    const next = state.tabs[Math.min(idx, state.tabs.length - 1)];
    setActiveTab(next.id);
  }
}

function repositionActiveTab() {
  if (!state.activeTabId) return;
  const tab = state.tabs.find(t => t.id === state.activeTabId);
  if (tab) tab.view.setBounds(getContentBounds());
}

// ── Hybrid Router (LLM) ────────────────────────────────────────────────────
async function initLlama() {
  const modelDir = path.join(
    process.resourcesPath ? path.join(process.resourcesPath, 'models') : path.join(__dirname, '../models')
  );

  if (!fs.existsSync(modelDir)) {
    fs.mkdirSync(modelDir, { recursive: true });
    console.log('[LLM] models/ directory created. Drop a .gguf model file there.');
    return;
  }

  const ggufFiles = fs.readdirSync(modelDir).filter(f => f.endsWith('.gguf'));
  if (ggufFiles.length === 0) {
    console.log('[LLM] No .gguf model found. Inference disabled until model is added.');
    return;
  }

  try {
    const modelPath = path.join(modelDir, ggufFiles[0]);
    console.log(`[LLM] Loading model: ${modelPath}`);

    state.llama = await getLlama();
    state.llamaModel = await state.llama.loadModel({ modelPath });
    state.llamaContext = await state.llamaModel.createContext({ contextSize: 4096 });
    state.llamaSession = new LlamaChatSession({ contextSequence: state.llamaContext.getSequence() });
    state.modelLoaded = true;

    console.log('[LLM] Model loaded. Hybrid router active.');
    notifyShell('llm-status', { loaded: true, model: ggufFiles[0] });
  } catch (err) {
    console.error('[LLM] Failed to load model:', err.message);
    notifyShell('llm-status', { loaded: false, error: err.message });
  }
}

async function hybridRoute(prompt) {
  if (!state.modelLoaded || !state.llamaSession) {
    return { source: 'offline', text: '[Model not loaded] ' + prompt };
  }

  try {
    let response = '';
    await state.llamaSession.prompt(prompt, {
      onToken(chunk) {
        const text = state.llamaModel.detokenize(chunk);
        response += text;
        notifyShell('llm-stream', { chunk: text });
      }
    });
    return { source: 'local', text: response };
  } catch (err) {
    return { source: 'error', text: err.message };
  }
}

// ── IPC Handlers ───────────────────────────────────────────────────────────
ipcMain.handle('navigate', (_, { url }) => {
  const tab = state.tabs.find(t => t.id === state.activeTabId);
  if (!tab) return;
  const normalized = url.startsWith('http') ? url : `https://${url}`;
  tab.view.webContents.loadURL(normalized);
});

ipcMain.handle('tab-new', (_, { url }) => {
  return createTab(url || 'https://www.google.com');
});

ipcMain.handle('tab-close', (_, { id }) => {
  closeTab(id || state.activeTabId);
});

ipcMain.handle('tab-switch', (_, { id }) => {
  setActiveTab(id);
});

ipcMain.handle('tab-back', () => {
  const tab = state.tabs.find(t => t.id === state.activeTabId);
  if (tab && tab.view.webContents.canGoBack()) tab.view.webContents.goBack();
});

ipcMain.handle('tab-forward', () => {
  const tab = state.tabs.find(t => t.id === state.activeTabId);
  if (tab && tab.view.webContents.canGoForward()) tab.view.webContents.goForward();
});

ipcMain.handle('tab-reload', () => {
  const tab = state.tabs.find(t => t.id === state.activeTabId);
  if (tab) tab.view.webContents.reload();
});

ipcMain.handle('llm-prompt', async (_, { prompt }) => {
  notifyShell('agent-state', { state: 'thinking' });
  const result = await hybridRoute(prompt);
  notifyShell('agent-state', { state: 'idle' });
  return result;
});

ipcMain.handle('agent-action', async (_, command) => {
  const tab = state.tabs.find(t => t.id === state.activeTabId);
  if (!tab) return { ok: false, error: 'No active tab' };

  notifyShell('agent-state', { state: 'acting' });
  try {
    let result;
    switch (command.action) {
      case 'click':
        result = await tab.view.webContents.executeJavaScript(
          `(function(){ const el = document.querySelector(${JSON.stringify(command.selector)}); if(el){ el.click(); return true; } return false; })()`
        );
        break;
      case 'fill':
        result = await tab.view.webContents.executeJavaScript(
          `(function(){ const el = document.querySelector(${JSON.stringify(command.selector)}); if(el){ el.focus(); el.value=${JSON.stringify(command.value)}; el.dispatchEvent(new Event('input',{bubbles:true})); el.dispatchEvent(new Event('change',{bubbles:true})); return true; } return false; })()`
        );
        break;
      case 'nav':
        tab.view.webContents.loadURL(command.value);
        result = true;
        break;
      case 'scroll':
        result = await tab.view.webContents.executeJavaScript(
          `window.scrollBy(${command.x || 0}, ${command.y || 500}); true;`
        );
        break;
      case 'snapshot':
        result = await tab.view.webContents.executeJavaScript(
          `document.title + '|||' + document.body.innerText.slice(0, 2000)`
        );
        break;
      default:
        result = false;
    }
    notifyShell('agent-state', { state: 'success' });
    setTimeout(() => notifyShell('agent-state', { state: 'idle' }), 1500);
    return { ok: true, result };
  } catch (err) {
    notifyShell('agent-state', { state: 'idle' });
    return { ok: false, error: err.message };
  }
});

ipcMain.handle('sidecar-toggle', (_, { open }) => {
  state.mainWindow.sidecarOpen = open;
  repositionActiveTab();
});

ipcMain.handle('window-control', (_, { action }) => {
  switch (action) {
    case 'minimize': state.mainWindow.minimize(); break;
    case 'maximize':
      state.mainWindow.isMaximized()
        ? state.mainWindow.unmaximize()
        : state.mainWindow.maximize();
      break;
    case 'close': state.mainWindow.close(); break;
  }
});

// ── Helpers ────────────────────────────────────────────────────────────────
function notifyShell(channel, data) {
  if (state.mainWindow && !state.mainWindow.isDestroyed()) {
    state.mainWindow.webContents.send(channel, data);
  }
}
