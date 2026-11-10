find_package(nlohmann_json CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(Vorbis CONFIG REQUIRED) # libvorbisenc: generated music -> .ogg (tool only)

add_library(UmbraLocalAi STATIC)
add_library(Umbra::LocalAi ALIAS UmbraLocalAi)

target_sources(
    UmbraLocalAi
    PRIVATE
        src/Ollama.cpp
        src/QuestAssistant.cpp
        src/ComfyUi.cpp
        src/TextureAssistant.cpp
        src/ModelAssistant.cpp
        src/SoundAssistant.cpp
        src/Generate.cpp
        src/AnimationGen.cpp
        third_party/dr_flac.h
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/Ollama.hpp
            include/QuestAssistant.hpp
            include/ComfyUi.hpp
            include/TextureAssistant.hpp
            include/ModelAssistant.hpp
            include/SoundAssistant.hpp
            include/Generate.hpp
            include/AnimationGen.hpp
)

target_compile_features(UmbraLocalAi PUBLIC cxx_std_20)

# AI-assisted authoring through a local Ollama server (P8B). Used by the editor and UmbraTool
# only; the game never links it (Umbra.GameExcludesEditor).
target_link_libraries(
    UmbraLocalAi
    PUBLIC
        Umbra::Authoring
        Umbra::QuestDraft
        Umbra::Assets
        Umbra::Audio
        Threads::Threads
    PRIVATE
        httplib::httplib
        Vorbis::vorbisenc
        nlohmann_json::nlohmann_json
        Umbra::Warnings
        Umbra::Sanitizers
)
