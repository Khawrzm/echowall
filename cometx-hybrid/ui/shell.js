// ui/shell.js
const api = window.electronAPI;

const state = {
  tabs: {},
  activeId: null,
  sidecarOpen: false,
  streamingMsgEl: null,
  agentBusy: false
};

const $ = id => document.getElementById(id);
const tabsContainer = $('tabs-container');
const addressBar    = $('address-bar');
const agentBeam     = $('agent-beam');
const chatLog       = $('chat-log');
const actionLog     = $('action-log');
const sidecar       = $('sidecar');
const promptInput   = $('prompt-input');
const llmModelName  = $('llm-model-name');
const btnSend       = $('btn-send');

window.cometx = {
  navigate(raw) {
    const url = normalizeURL(raw);
    addressBar.value = url;
    api.navigate(url);
  },
  back()          { api.back(); },
  forward()       { api.forward(); },
  reload()        { api.reload(); },
  newTab(url)     { api.newTab(url); },
  closeTab(id, e) {
    e && e.stopPropagation();
    api.closeTab(id);
  },
  switchTab(id)   { api.switchTab(id); },
  toggleSidecar() {
    state.sidecarOpen = !state.sidecarOpen;
    sidecar.className = state.sidecarOpen ? 'sidecar-open' : 'sidecar-closed';
    api.toggleSidecar(state.sidecarOpen);
  },
  windowControl(action) { api.windowControl(action); },

  async sendPrompt() {
    if (state.agentBusy) return;
    const text = promptInput.value.trim();
    if (!text) return;
    promptInput.value = '';
    state.agentBusy = true;
    btnSend.disabled = true;

    appendMsg('user', text);
    const agentEl = appendMsg('agent', '', true);

    try {
      state.streamingMsgEl = agentEl;
      const result = await api.llmPrompt(text);

      if (!agentEl.textContent.trim()) {
        agentEl.textContent = result.text || '(no response)';
      }
      agentEl.classList.remove('msg-streaming');
      state.streamingMsgEl = null;

      tryExecuteAction(result.text, agentEl);
    } catch (err) {
      agentEl.textContent = '\u26A0 ' + err.message;
      agentEl.classList.remove('msg-streaming');
    } finally {
      state.agentBusy = false;
      btnSend.disabled = false;
    }
  }
};

api.on('tab-created', ({ id, url, title }) => {
  state.tabs[id] = { id, url, title, favicon: null, loading: false };
  renderTabBar();
});

api.on('tab-updated', ({ id, url, title, favicon }) => {
  if (!state.tabs[id]) return;
  if (url    !== undefined) state.tabs[id].url    = url;
  if (title  !== undefined) state.tabs[id].title  = title;
  if (favicon !== undefined) state.tabs[id].favicon = favicon;
  if (id === state.activeId && url) addressBar.value = url;
  renderTabBar();
});

api.on('tab-closed', ({ id }) => {
  delete state.tabs[id];
  renderTabBar();
});

api.on('tab-activated', ({ id, url, title }) => {
  state.activeId = id;
  if (state.tabs[id]) {
    state.tabs[id].url   = url;
    state.tabs[id].title = title;
  }
  addressBar.value = url || '';
  renderTabBar();
});

api.on('tab-loading', ({ id, loading }) => {
  if (!state.tabs[id]) return;
  state.tabs[id].loading = loading;
  renderTabBar();
});

api.on('llm-status', ({ loaded, model, error }) => {
  llmModelName.textContent = loaded ? model : (error ? '\u26A0 ' + error : 'No model');
  llmModelName.title = error || model || '';
});

api.on('llm-stream', ({ chunk }) => {
  if (!state.streamingMsgEl) return;
  state.streamingMsgEl.textContent += chunk;
  chatLog.scrollTop = chatLog.scrollHeight;
});

api.on('agent-state', ({ state: s }) => {
  const map = {
    idle:     'beam-idle',
    thinking: 'beam-thinking',
    acting:   'beam-acting',
    success:  'beam-success'
  };
  agentBeam.className = map[s] || 'beam-idle';
});

function renderTabBar() {
  tabsContainer.innerHTML = '';
  Object.values(state.tabs).forEach(tab => {
    const el = document.createElement('div');
    el.className = 'tab' + (tab.id === state.activeId ? ' active' : '');
    el.dataset.id = tab.id;
    el.title = tab.url || '';
    el.onclick = () => window.cometx.switchTab(tab.id);

    if (tab.loading) {
      const spinner = document.createElement('div');
      spinner.className = 'tab-spinner';
      el.appendChild(spinner);
    } else if (tab.favicon) {
      const img = document.createElement('img');
      img.className = 'tab-favicon';
      img.src = tab.favicon;
      img.onerror = () => img.remove();
      el.appendChild(img);
    } else {
      const icon = document.createElement('span');
      icon.className = 'tab-favicon';
      icon.textContent = '\uD83C\uDF10';
      el.appendChild(icon);
    }

    const title = document.createElement('span');
    title.className = 'tab-title';
    title.textContent = tab.title || tab.url || 'New Tab';
    el.appendChild(title);

    const closeBtn = document.createElement('button');
    closeBtn.className = 'tab-close';
    closeBtn.textContent = '\u00D7';
    closeBtn.title = 'Close tab';
    closeBtn.onclick = e => window.cometx.closeTab(tab.id, e);
    el.appendChild(closeBtn);

    tabsContainer.appendChild(el);
  });
}

function appendMsg(role, text, streaming = false) {
  const el = document.createElement('div');
  el.className = 'msg msg-' + role + (streaming ? ' msg-streaming' : '');
  el.textContent = text;
  chatLog.appendChild(el);
  chatLog.scrollTop = chatLog.scrollHeight;
  return el;
}

function logAction(icon, text, status = 'pending') {
  const entry = document.createElement('div');
  entry.className = 'action-entry ' + status;
  entry.innerHTML = `<span class="action-icon">${icon}</span><span>${text}</span>`;
  actionLog.appendChild(entry);
  actionLog.parentElement.scrollTop = actionLog.parentElement.scrollHeight;
  return entry;
}

async function tryExecuteAction(text, msgEl) {
  if (!text) return;
  const match = text.match(/\{[\s\S]*?"action"\s*:\s*"(click|fill|nav|scroll|snapshot)"[\s\S]*?\}/);
  if (!match) return;

  let cmd;
  try { cmd = JSON.parse(match[0]); } catch { return; }

  const icons = { click: '\uD83D\uDDB1', fill: '\u2328', nav: '\uD83D\uDD17', scroll: '\uD83D\uDCDC', snapshot: '\uD83D\uDCF8' };
  const entry = logAction(icons[cmd.action] || '\u2699', `${cmd.action}: ${cmd.selector || cmd.value || ''}`, 'pending');

  const result = await api.agentAction(cmd);
  entry.className = 'action-entry ' + (result.ok ? 'ok' : 'err');
  entry.querySelector('.action-icon').textContent = result.ok ? (icons[cmd.action] || '\u2713') : '\u2715';

  if (cmd.action === 'snapshot' && result.ok) {
    appendMsg('agent', '\uD83D\uDCF8 Page snapshot:\n' + result.result);
  }
}

function normalizeURL(input) {
  input = input.trim();
  if (!input) return 'https://www.google.com';
  if (input.startsWith('http://') || input.startsWith('https://')) return input;
  if (/^[a-zA-Z0-9-]+\.[a-zA-Z]{2,}(\/.*)?$/.test(input)) return 'https://' + input;
  return 'https://www.google.com/search?q=' + encodeURIComponent(input);
}

addressBar.addEventListener('focus', () => addressBar.select());
