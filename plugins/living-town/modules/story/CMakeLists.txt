add_library(UmbraTownStory STATIC)
add_library(Umbra::TownStory ALIAS UmbraTownStory)

target_sources(
    UmbraTownStory
    PRIVATE
        src/Plan.cpp
        src/Storyteller.cpp
        src/Soak.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/Plan.hpp
            include/Storyteller.hpp
            include/Soak.hpp
)
target_compile_features(UmbraTownStory PUBLIC cxx_std_20)

# The storyteller (S24): the developer's outcome plan and the code that keeps a town to it.
# Rules only, like the town itself: no world, renderer or model.
target_link_libraries(
    UmbraTownStory
    PUBLIC
        Umbra::Town
    PRIVATE
        Umbra::Warnings
        Umbra::Sanitizers
)
