#!/bin/sh
# Builds umbra_sim.wasm from the real engine sources with wasi-sdk (see README.md).
# Usage: WASI_SDK_PATH=/path/to/wasi-sdk-25.0 INCLUDE=/path/with/nlohmann sh samples/web/build.sh
set -e
cd "$(dirname "$0")/../.."
: "${WASI_SDK_PATH:?set WASI_SDK_PATH to the wasi-sdk folder}"
: "${INCLUDE:?set INCLUDE to a folder that contains nlohmann/json.hpp}"

"$WASI_SDK_PATH/bin/clang++" --target=wasm32-wasip1 -std=c++20 -O2 \
    -mexec-model=reactor -fno-exceptions -DJSON_NOEXCEPTION \
    -Iengine/core/include -Igame/rpg/include -Igame/turncombat/include -isystem "$INCLUDE" \
    game/rpg/src/RpgContent.cpp game/rpg/src/StoryContent.cpp game/rpg/src/Character.cpp game/rpg/src/Inventory.cpp \
    game/rpg/src/SkillTree.cpp game/rpg/src/Rules.cpp game/rpg/src/Progression.cpp \
    game/turncombat/src/TurnEncounter.cpp game/turncombat/src/TurnEnvironment.cpp \
    samples/web/WebSim.cpp \
    -Wl,--strip-all -o samples/web/umbra_sim.wasm
ls -l samples/web/umbra_sim.wasm
