add_library(UmbraAudio STATIC)
add_library(Umbra::Audio ALIAS UmbraAudio)

target_sources(
    UmbraAudio
    PRIVATE
        src/Wav.cpp
        src/Mixer.cpp
        src/SoundBank.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/Wav.hpp
            include/Mixer.hpp
            include/SoundBank.hpp
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
