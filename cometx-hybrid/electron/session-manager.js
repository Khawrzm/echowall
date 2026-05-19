// electron/session-manager.js
const fs   = require('fs');
const path = require('path');
const { app } = require('electron');

const SESSION_PATH = path.join(app.getPath('userData'), 'session.json');

function saveSession(tabs, activeTabId) {
  try {
    const data = {
      savedAt: new Date().toISOString(),
      activeTabId,
      tabs: tabs.map(t => ({ id: t.id, url: t.url, title: t.title }))
    };
    fs.writeFileSync(SESSION_PATH, JSON.stringify(data, null, 2), 'utf8');
    console.log('[Session] Saved', data.tabs.length, 'tab(s) to', SESSION_PATH);
  } catch (err) {
    console.error('[Session] Save failed:', err.message);
  }
}

function loadSession() {
  try {
    if (!fs.existsSync(SESSION_PATH)) return null;
    const raw  = fs.readFileSync(SESSION_PATH, 'utf8');
    const data = JSON.parse(raw);
    if (!Array.isArray(data.tabs) || data.tabs.length === 0) return null;
    console.log('[Session] Loaded', data.tabs.length, 'tab(s) from', SESSION_PATH);
    return data;
  } catch (err) {
    console.error('[Session] Load failed:', err.message);
    return null;
  }
}

function clearSession() {
  try {
    if (fs.existsSync(SESSION_PATH)) fs.unlinkSync(SESSION_PATH);
  } catch (err) {
    console.error('[Session] Clear failed:', err.message);
  }
}

module.exports = { saveSession, loadSession, clearSession, SESSION_PATH };
