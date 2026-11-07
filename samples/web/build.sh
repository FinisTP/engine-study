#!/bin/sh
# Builds umbra_sim.wasm from the real engine sources with wasi-sdk (see README.md).
# Usage: WASI_SDK_PATH=/path/to/wasi-sdk-25.0 INCLUDE=/path/with/nlohmann sh samples/web/build.sh
set -e
cd "$(dirname "$0")/../.."
: "${WASI_SDK_PATH:?set WASI_SDK_PATH to the wasi-sdk folder}"
: "${INCLUDE:?set INCLUDE to a folder that contains nlohmann/json.hpp}"

"$WASI_SDK_PATH/bin/clang++" --target=wasm32-wasip1 -std=c++20 -O2 \
    -mexec-model=reactor -fno-exceptions -DJSON_NOEXCEPTION \
    -Iengine/core/include -Iengine/rpg/include -Iengine/turncombat/include -isystem "$INCLUDE" \
    engine/rpg/src/RpgContent.cpp engine/rpg/src/StoryContent.cpp engine/rpg/src/Character.cpp engine/rpg/src/Inventory.cpp \
    engine/rpg/src/SkillTree.cpp engine/rpg/src/Rules.cpp engine/rpg/src/Progression.cpp \
    engine/turncombat/src/TurnEncounter.cpp engine/turncombat/src/TurnEnvironment.cpp \
    samples/web/WebSim.cpp \
    -Wl,--strip-all -o samples/web/umbra_sim.wasm
ls -l samples/web/umbra_sim.wasm
