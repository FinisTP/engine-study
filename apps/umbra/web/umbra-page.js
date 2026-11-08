// The page around the browser build (roadmap P10, README.md): starts the game, draws its HUD as
// HTML, keeps saves in IndexedDB with export/import, and installs it for offline play.
'use strict';

const $ = (id) => document.getElementById(id);
const savesDir = '/libsdl/Umbra/UmbraEngine/saves'; // SDL_GetPrefPath in the browser + "saves"

const Page = {
  // 1. Messages ---------------------------------------------------------------------------------
  notice(text) { const n = $('notice'); n.textContent = text; n.hidden = false; },
  fatal(text) { $('start').style.display = 'flex'; $('play').hidden = true; $('start-text').textContent = text; },

  // 2. HUD from core::Hud (JSON from RendererWeb.cpp); only touch the page when it changed ------
  lastHud: '',
  hud(json) {
    if (json === Page.lastHud) return;
    Page.lastHud = json;
    const h = JSON.parse(json);
    $('status').textContent = h.status.join('\n');
    $('warnings').textContent = h.warnings.join('\n');
    $('message').textContent = h.message;
    $('prompt').textContent = h.prompt;
    $('stats').textContent = h.stats.join('\n');
    $('stats').style.display = h.diagnostics && Page.showStats ? 'block' : 'none';
    const menu = h.menuItems.length > 0;
    $('menu').style.display = menu ? 'block' : 'none';
    $('dim').style.display = menu && h.menuDim ? 'block' : 'none';
    if (menu) {
      $('menu-title').textContent = h.menuTitle;
      $('menu-text').textContent = h.menuText;
      $('menu-hint').textContent = h.menuHint || 'W/S choose, Enter';
      const list = $('menu-items');
      list.replaceChildren(...h.menuItems.map((item, i) => {
        const li = document.createElement('li');
        li.textContent = item;
        if (i === h.menuSelected) li.className = 'selected';
        return li;
      }));
    }
  },
  showStats: false,

  // 3. Saves: IndexedDB through Emscripten's IDBFS, plus export/import as a file --------------
  persist() {
    FS.syncfs(false, (err) => {
      if (err) Page.notice('Saving to this browser failed (' + err + '). Your game is saved only until you close the tab: use "Export saves".');
    });
  },
  exportSaves() {
    const files = {};
    try {
      for (const name of FS.readdir(savesDir)) {
        if (name === '.' || name === '..') continue;
        const bytes = FS.readFile(savesDir + '/' + name);
        files[name] = btoa(String.fromCharCode(...bytes));
      }
    } catch (e) { /* no saves yet */ }
    if (Object.keys(files).length === 0) { Page.notice('There are no saves to export yet.'); return; }
    const blob = new Blob([JSON.stringify({ format: 'umbra.web-saves', version: 1, files })], { type: 'application/json' });
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = 'umbra-saves.json';
    a.click();
    URL.revokeObjectURL(a.href);
  },
  async importSaves(file) {
    let data;
    try { data = JSON.parse(await file.text()); } catch (e) { data = null; }
    if (!data || data.format !== 'umbra.web-saves' || typeof data.files !== 'object') { Page.notice('That file is not an Umbra saves export.'); return; }
    if (!confirm('Replace the saves in this browser with the ones in ' + file.name + '?')) return;
    FS.mkdirTree(savesDir);
    for (const [name, base64] of Object.entries(data.files)) {
      if (!/^[\w.-]+$/.test(name)) continue; // plain file names only, nothing outside the folder
      FS.writeFile(savesDir + '/' + name, Uint8Array.from(atob(base64), (c) => c.charCodeAt(0)));
    }
    FS.syncfs(false, (err) => {
      if (err) Page.notice('Imported, but this browser could not store them (' + err + ').');
      else location.reload(); // the game reads saves at start ("Continue")
    });
  },

  // 4. Offline install: the service worker caches one complete version or reports not ready ----
  async offline() {
    const label = $('offline');
    if (!('serviceWorker' in navigator)) { label.textContent = 'Offline: not available here'; return; }
    try {
      const registration = await navigator.serviceWorker.register('sw.js');
      const ask = () => navigator.serviceWorker.controller && navigator.serviceWorker.controller.postMessage('status');
      navigator.serviceWorker.addEventListener('message', (event) => {
        const s = event.data;
        label.textContent = s.complete ? 'Ready offline (' + s.version + ')' : 'Offline: downloading...';
      });
      navigator.serviceWorker.addEventListener('controllerchange', ask);
      registration.addEventListener('updatefound', () => { label.textContent = 'Offline: downloading update...'; });
      setInterval(() => {
        if (registration.waiting) label.textContent = 'Update ready: reload the page to use it';
        else ask();
      }, 3000);
      ask();
    } catch (e) {
      label.textContent = 'Offline: not installed (' + e.message + ')';
    }
  },
};

// 5. The Emscripten module: canvas, saves mounted before main, callbacks from C++ -----------------
var Module = {
  canvas: $('canvas'),
  // A host that can't serve ".data" files may rename the asset package (set window.umbraDataFile).
  locateFile: (path, prefix) => prefix + (path === 'umbra.data' && window.umbraDataFile ? window.umbraDataFile : path),
  preRun: [() => {
    FS.mkdir('/libsdl');
    FS.mount(IDBFS, {}, '/libsdl');
    addRunDependency('saves');
    FS.syncfs(true, (err) => {
      if (err) Page.notice('This browser does not let the game read its storage (' + err + '); saves last only until the tab closes.');
      removeRunDependency('saves');
    });
  }],
  setStatus: (text) => { if (text) $('start-text').textContent = text; },
  monitorRunDependencies: (left) => { if (left === 0) { $('start-text').textContent = 'Ready.'; $('play').hidden = false; } },
  onHud: (json) => Page.hud(json),
  persistSaves: () => Page.persist(),
  onGpuLost: (why) => Page.fatal('The graphics device was lost (' + why + '). Reload the page; your last save is kept.'),
  onAbort: (what) => Page.fatal('The game stopped: ' + what),
  // SDL3's audio calls dynCall("ip", ...); this Emscripten (3.1.52, with Asyncify) only finds
  // per-signature exports, and "ip" (returns int, takes a pointer) is the same call as "ii".
  dynCall_ip: (fn, arg) => Module['dynCall_ii'](fn, arg),
  print: (text) => console.log(text),
  printErr: (text) => console.warn(text),
};

$('play').onclick = () => { $('start').style.display = 'none'; $('canvas').focus(); };
$('export').onclick = () => Page.exportSaves();
$('import').onchange = (event) => { if (event.target.files[0]) Page.importSaves(event.target.files[0]); event.target.value = ''; };
document.addEventListener('keydown', (event) => { if (event.key === 'F1') { Page.showStats = !Page.showStats; Page.lastHud = ''; } });
window.addEventListener('keydown', (event) => { if (event.key === 'Tab' || event.key === ' ') event.preventDefault(); });

// 6. Start: refuse clearly without WebGPU, otherwise load the game -------------------------------
if (!navigator.gpu) {
  Page.fatal('This browser has no WebGPU, which Umbra needs. Use Chrome or Edge (version 113 or newer) on a computer with a recent graphics driver.');
} else {
  Page.offline();
  const script = document.createElement('script');
  script.src = 'umbra.js';
  script.onerror = () => Page.fatal('Could not load the game files. Open the page through a web server (see README), not as a file.');
  document.body.appendChild(script);
}
