add_library(UmbraGameplay STATIC)
add_library(Umbra::Gameplay ALIAS UmbraGameplay)

target_sources(
    UmbraGameplay
    PRIVATE
        src/PhysicsBridge.cpp
        src/PlayerController.cpp
        src/StrideAnimator.cpp
        src/PauseMenu.cpp
        src/Interaction.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/PhysicsBridge.hpp
            include/PlayerController.hpp
            include/StrideAnimator.hpp
            include/PauseMenu.hpp
            include/Interaction.hpp
)

target_compile_features(UmbraGameplay PUBLIC cxx_std_20)

target_link_libraries(
    UmbraGameplay
    PUBLIC
        Umbra::World
        Umbra::Physics
    PRIVATE
        Umbra::Warnings
        Umbra::Sanitizers
)
