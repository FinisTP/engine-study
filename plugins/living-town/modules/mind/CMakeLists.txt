add_library(UmbraTownMind STATIC)
add_library(Umbra::TownMind ALIAS UmbraTownMind)

target_sources(
    UmbraTownMind
    PRIVATE
        src/LineGuard.cpp
        src/TownMind.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/Mind.hpp
            include/LineGuard.hpp
            include/TownMind.hpp
)
target_compile_features(UmbraTownMind PUBLIC cxx_std_20)

# The built-in model's side of the town (S23): prompts, checks, the worker. No model library here:
# a model is anything that implements town::Mind (tests use a scripted one).
find_package(Threads REQUIRED)
target_link_libraries(
    UmbraTownMind
    PUBLIC
        Umbra::Town
    PRIVATE
        Threads::Threads
        Umbra::Warnings
        Umbra::Sanitizers
)

# The real model: llama.cpp, when the build has it (vcpkg "llama-cpp"; Mind.md "The model").
find_package(llama CONFIG QUIET)
if(llama_FOUND)
    add_library(UmbraTownLlama STATIC src/LlamaMind.cpp)
    add_library(Umbra::TownLlama ALIAS UmbraTownLlama)
    target_include_directories(UmbraTownLlama PUBLIC include)
    target_compile_features(UmbraTownLlama PUBLIC cxx_std_20)
    target_compile_definitions(UmbraTownLlama PUBLIC UMBRA_TOWN_LLAMA=1)
    target_link_libraries(UmbraTownLlama PUBLIC Umbra::TownMind PRIVATE llama Umbra::Sanitizers)
    message(STATUS "Living town: llama.cpp found, the built-in model is available")
else()
    message(STATUS "Living town: llama.cpp not found, residents use rules and template lines only")
endif()
