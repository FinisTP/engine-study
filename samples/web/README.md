# W0 web experiment

A small browser sample answering roadmap W0: can Umbra's simulation run as WebAssembly, can the
scene be drawn with a browser graphics API, and can a value be stored persistently? It is an
experiment, not the web port (P10). The decision record is [`docs/adr/0001-web-backend.md`](../../docs/adr/0001-web-backend.md).

| File | What it is |
|---|---|
| `WebSim.cpp` | C exports around the **real** `Umbra::Rpg` + `Umbra::TurnCombat` sources (`TurnEnvironment`): load content, reset, step with the conventional AI, read the state as JSON |
| `build.sh` | Compiles those engine sources and `WebSim.cpp` to `umbra_sim.wasm` with wasi-sdk |
| `umbra_sim.js` | Loads the wasm in a browser or Node; supplies the six WASI functions it imports (all stubs: no files, no output) |
| `test-node.mjs` | Runs the reference turn fight in Node and compares it with the native regression numbers |
| `scene.wgsl` | WGSL version of `engine/render/shaders/Scene.hlsl` (uniform buffer instead of push constants, no texture) |
| `main.js`, `page.html` | The page: simulation results, WebGPU rendering of `settlement.scene.json`, a `localStorage` round trip |
| `make-page.mjs` | Packs everything into one self-contained `umbra-web.html` (wasm as base64), so no web server is needed |

## Build and run

```sh
# wasi-sdk 25: https://github.com/WebAssembly/wasi-sdk/releases (any folder with nlohmann/json.hpp for INCLUDE)
WASI_SDK_PATH=$HOME/wasi-sdk-25.0-x86_64-linux INCLUDE=$HOME/prefix/include sh samples/web/build.sh
node samples/web/test-node.mjs     # must print "OK: WebAssembly matches native"
node samples/web/make-page.mjs     # writes samples/web/umbra-web.html; open it in a WebGPU browser
```

On Windows use the wasi-sdk Windows release and run the same `clang++` line from `build.sh`
(or Git Bash). Node 18+ is needed for the test.

## How the pieces talk

The wasm module is a WASI "reactor": it has no `main`, JavaScript calls `_initialize` once (C++
static constructors) and then the exported functions. Strings cross the boundary as bytes in the
module's memory: JS asks for a buffer (`umbra_alloc`), copies the UTF-8 content text in and calls
`umbra_load`; `umbra_state` returns a pointer to a zero-terminated JSON string that JS decodes.
Exceptions are off (`-fno-exceptions`, `JSON_NOEXCEPTION`): the rules never rely on them, and
nlohmann-json is only used with `allow_exceptions = false` and checked types.
