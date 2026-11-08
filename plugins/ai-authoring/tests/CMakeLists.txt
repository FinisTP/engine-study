find_package(Catch2 3 CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)

# The Ollama bridge against a stand-in server on 127.0.0.1 (no real model needed).
add_executable(UmbraLocalAiTests)
target_sources(UmbraLocalAiTests PRIVATE OllamaTests.cpp ComfyUiTests.cpp QuestDraftTests.cpp SoundGenTests.cpp AgentTests.cpp)
target_include_directories(UmbraLocalAiTests PRIVATE ${PROJECT_SOURCE_DIR}/tests)
target_compile_definitions(UmbraLocalAiTests PRIVATE UMBRA_TEST_ASSET_DIRECTORY="${UMBRA_REFERENCE_ASSETS}" UMBRA_TEST_DATA_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/data" UMBRA_TEST_LOCALAI_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data")
target_compile_features(UmbraLocalAiTests PRIVATE cxx_std_20)
target_link_libraries(
    UmbraLocalAiTests
    PRIVATE
        Umbra::LocalAi
        Umbra::Agent
        httplib::httplib
        Umbra::Warnings
        Umbra::Sanitizers
        Catch2::Catch2WithMain
)

umbra_copy_asan_runtime(UmbraLocalAiTests)

add_test(NAME Umbra.LocalAi COMMAND UmbraLocalAiTests)


# AI quest drafts (P8B) through the tool, without a model: a saved draft passes, an unplayable one
# fails, and a non-local address is refused before any connection.
add_test(
    NAME Umbra.Tool.DraftFromFile
    COMMAND UmbraTool draft ${UMBRA_REFERENCE_ASSETS}/rpg/reference.rpg.json --npc elder
        --from ${CMAKE_CURRENT_SOURCE_DIR}/data/good.draft.json
)
set_tests_properties(Umbra.Tool.DraftFromFile PROPERTIES PASS_REGULAR_EXPRESSION "ok   The quest completes")
add_test(
    NAME Umbra.Tool.DraftUnplayable
    COMMAND UmbraTool draft ${UMBRA_REFERENCE_ASSETS}/rpg/reference.rpg.json --npc elder
        --from ${CMAKE_CURRENT_SOURCE_DIR}/data/unplayable.draft.json
)
set_tests_properties(Umbra.Tool.DraftUnplayable PROPERTIES WILL_FAIL TRUE)
add_test(
    NAME Umbra.Tool.DraftLocalOnly
    COMMAND UmbraTool draft ${UMBRA_REFERENCE_ASSETS}/rpg/reference.rpg.json --npc elder --idea "anything"
        --host http://api.example.com --json
)
set_tests_properties(Umbra.Tool.DraftLocalOnly PROPERTIES PASS_REGULAR_EXPRESSION "ai.host")

# AI textures (P8C): a non-local image generator is refused before any connection (T64).
add_test(
    NAME Umbra.Tool.TextureLocalOnly
    COMMAND UmbraTool texture "mossy stone" --assets ${CMAKE_CURRENT_BINARY_DIR}/texture-assets --host http://images.example.com:8188 --json
)
set_tests_properties(Umbra.Tool.TextureLocalOnly PROPERTIES PASS_REGULAR_EXPRESSION "ai.host")

# AI sound (S4): only a ComfyUI on this computer is ever asked.
add_test(
    NAME Umbra.Tool.SoundLocalOnly
    COMMAND UmbraTool sound "battle drums" --kind music --assets ${CMAKE_CURRENT_BINARY_DIR}/sound-assets --host http://audio.example.com:8188 --json
)
set_tests_properties(Umbra.Tool.SoundLocalOnly PROPERTIES PASS_REGULAR_EXPRESSION "ai.host")

# AI models (P8D): without an exported workflow the tool says how to make one.
add_test(
    NAME Umbra.Tool.ModelNeedsWorkflow
    COMMAND UmbraTool model --idea "barrel" --assets ${CMAKE_CURRENT_BINARY_DIR}/model-assets --workflow ${CMAKE_CURRENT_BINARY_DIR}/none.api.json
)
set_tests_properties(Umbra.Tool.ModelNeedsWorkflow PROPERTIES PASS_REGULAR_EXPRESSION "Export \\(API\\)")

# The agent prompt (S5): only an Ollama on this computer is ever asked.
add_test(
    NAME Umbra.Tool.AgentLocalOnly
    COMMAND UmbraTool agent "add a bench" --assets ${UMBRA_REFERENCE_ASSETS} --scene scenes/settlement.scene.json --host http://api.example.com:11434 --json
)
set_tests_properties(Umbra.Tool.AgentLocalOnly PROPERTIES PASS_REGULAR_EXPRESSION "ai.host")
