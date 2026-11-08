// Offline install (roadmap P10.4). stamp.cmake writes the version (a hash of the build) and the
// file list; "UmbraTool web-pack" adds the assets' hash to the version. One complete version at
// a time: cache.addAll is all-or-nothing, so a partial download is never reported ready.
const VERSION = '@UMBRA_VERSION@';
const FILES = [@UMBRA_FILES@];

self.addEventListener('install', (event) => {
  event.waitUntil(caches.open('umbra-' + VERSION).then((cache) => cache.addAll(FILES)));
});

self.addEventListener('activate', (event) => {
  // Old versions go once this one is in control.
  event.waitUntil(caches.keys().then((keys) => Promise.all(keys.filter((k) => k.startsWith('umbra-') && k !== 'umbra-' + VERSION).map((k) => caches.delete(k))))
    .then(() => self.clients.claim()));
});

// Network first: a freshly packed assets.bin (editor changes) shows up on the next load; the
// cached version is only used when the server can't be reached.
self.addEventListener('fetch', (event) => {
  event.respondWith(fetch(event.request).catch(() => caches.match(event.request, { ignoreSearch: true })
    .then((hit) => hit || Response.error())));
});

self.addEventListener('message', async (event) => {
  // "status": is every file of this version in the cache?
  const cache = await caches.open('umbra-' + VERSION);
  const present = await Promise.all(FILES.map((f) => cache.match(f)));
  event.source.postMessage({ version: VERSION, complete: present.every(Boolean) });
});
