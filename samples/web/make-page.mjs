// Packs the sample into one self-contained HTML file (wasm, content, scene and shader inlined),
// so it can be opened anywhere without a web server: node samples/web/make-page.mjs
import { readFileSync, writeFileSync } from 'node:fs';

const here = new URL('.', import.meta.url);
const read = (path) => readFileSync(new URL(path, here));
const strip = (js) => js.replace(/^import .*$/m, '').replace(/^export /gm, '');
const script = `<script type="module">
${strip(read('umbra_sim.js').toString())}
${strip(read('main.js').toString())}
const bytes = Uint8Array.from(atob(${JSON.stringify(read('umbra_sim.wasm').toString('base64'))}), (c) => c.charCodeAt(0));
start({
  wasm: bytes,
  content: ${JSON.stringify(read('../../assets/rpg/reference.rpg.json').toString())},
  scene: ${read('../../assets/scenes/settlement.scene.json').toString().trim()},
  shader: ${JSON.stringify(read('scene.wgsl').toString())},
});
</script>`;
const html = read('page.html').toString().replace('<!--ASSETS-->', script);
writeFileSync(new URL('umbra-web.html', here), html);
console.log(`umbra-web.html: ${html.length} bytes`);
