add_library(UmbraAudio STATIC)
add_library(Umbra::Audio ALIAS UmbraAudio)

target_sources(
    UmbraAudio
    PRIVATE
        src/Wav.cpp
        src/Mixer.cpp
        src/SoundBank.cpp
        src/Ogg.cpp
        third_party/stb_vorbis.c
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/Wav.hpp
            include/Mixer.hpp
            include/SoundBank.hpp
            include/Ogg.hpp
)

target_compile_features(UmbraAudio PUBLIC cxx_std_20)

target_link_libraries(
    UmbraAudio
    PUBLIC
        Umbra::Core
        Umbra::Assets # AssetFolders in SoundBank.hpp
    PRIVATE
        Umbra::Warnings
        Umbra::Sanitizers
)

# stb_vorbis.c is compiled through src/Ogg.cpp (one copy, our warning pragmas), never on its own.
set_source_files_properties(third_party/stb_vorbis.c PROPERTIES HEADER_FILE_ONLY TRUE)
