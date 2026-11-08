#!/bin/bash
# Builds the browser version and its Node test suites on Linux (roadmap P10; README.md here).
# Needs: Emscripten (EM_CONFIG set; 3.1.52 was used with the system LLVM 18), and the game's
# libraries built for WebAssembly into $WEB_PREFIX (SDL3, spdlog, Jolt, glm, nlohmann-json,
# tinygltf, Catch2 for the tests).
set -e
SRC=$(cd "$(dirname "$0")/../.." && pwd)
OUT=${OUT:-$SRC/out/web-build}
emcmake cmake -S "$SRC" -B "$OUT" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$WEB_PREFIX" -DCMAKE_FIND_ROOT_PATH="$WEB_PREFIX" -DUMBRA_WEB_TESTS=${TESTS:-ON}
cmake --build "$OUT"
# The page loads its assets from assets.bin/assets.json: pack them with a native UmbraTool.
TOOL=${UMBRA_TOOL:-$(ls "$SRC"/build*/tools/umbratool/UmbraTool 2>/dev/null | head -1)}
[ -x "$TOOL" ] || { echo "Set UMBRA_TOOL to a native UmbraTool (needed for web-pack)"; exit 1; }
"$TOOL" web-pack "$SRC/projects/${PROJECT:-reference}/assets" "$OUT/apps/umbra"
[ "${TESTS:-ON}" = ON ] && ctest --test-dir "$OUT" -E "Tool|SceneCheck" --output-on-failure
echo "Page: $OUT/apps/umbra (index.html, umbra.js/.wasm, assets.bin/.json, sw.js). Serve it, e.g. python3 -m http.server -d $OUT/apps/umbra"
