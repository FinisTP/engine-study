find_package(nlohmann_json CONFIG REQUIRED)

add_library(UmbraAgent STATIC)
add_library(Umbra::Agent ALIAS UmbraAgent)

target_sources(
    UmbraAgent
    PRIVATE
        src/AgentTools.cpp
        src/Agent.cpp
        src/Mcp.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/AgentTools.hpp
            include/Agent.hpp
            include/Mcp.hpp
            include/Agent.md
)

target_compile_features(UmbraAgent PUBLIC cxx_std_20)

# The agent prompt (S5): tools over the authoring documents, a local Ollama planner and the
# runner. Used by the editor's Agent window and "UmbraTool mcp"; the game never links it.
target_link_libraries(
    UmbraAgent
    PUBLIC
        Umbra::Authoring
        Umbra::LocalAi
    PRIVATE
        nlohmann_json::nlohmann_json
        Umbra::Warnings
        Umbra::Sanitizers
)
