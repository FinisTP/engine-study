find_package(httplib CONFIG REQUIRED)
find_package(Threads REQUIRED)

add_library(UmbraLocalAi STATIC)
add_library(Umbra::LocalAi ALIAS UmbraLocalAi)

target_sources(
    UmbraLocalAi
    PRIVATE
        src/Ollama.cpp
        src/QuestAssistant.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/Ollama.hpp
            include/QuestAssistant.hpp
)

target_compile_features(UmbraLocalAi PUBLIC cxx_std_20)

# AI-assisted authoring through a local Ollama server (P8B). Used by the editor and UmbraTool
# only; the game never links it (Umbra.GameExcludesEditor).
target_link_libraries(
    UmbraLocalAi
    PUBLIC
        Umbra::Authoring
        Threads::Threads
    PRIVATE
        httplib::httplib
        Umbra::Warnings
        Umbra::Sanitizers
)
