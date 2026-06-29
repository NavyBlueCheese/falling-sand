cmake_minimum_required(VERSION 3.16)
project(falling_sand LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Optimize the hot simulation loop in Release builds.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options($<$<CONFIG:Release>:-O2>)
endif()

# ---------------------------------------------------------------------------
# Dependencies: SDL2 + SDL2_ttf.
# We try the modern CMake config packages first and fall back to pkg-config /
# find_library so this works across Windows (vcpkg), macOS (brew) and Linux.
# ---------------------------------------------------------------------------
find_package(SDL2 REQUIRED)

# SDL2_ttf: prefer a config package, otherwise locate it manually.
find_package(SDL2_ttf QUIET)

set(SRC
    src/main.cpp
    src/Simulation.cpp
    src/Renderer.cpp
    src/UI.cpp
    src/Chunk.cpp
    src/Explosion.cpp
    src/SaveLoad.cpp
)

add_executable(sand_sim ${SRC})

# --- link SDL2 -------------------------------------------------------------
if(TARGET SDL2::SDL2)
    target_link_libraries(sand_sim PRIVATE SDL2::SDL2)
    if(TARGET SDL2::SDL2main)
        target_link_libraries(sand_sim PRIVATE SDL2::SDL2main)
    endif()
else()
    target_include_directories(sand_sim PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(sand_sim PRIVATE ${SDL2_LIBRARIES})
endif()

# --- link SDL2_ttf ---------------------------------------------------------
if(TARGET SDL2_ttf::SDL2_ttf)
    target_link_libraries(sand_sim PRIVATE SDL2_ttf::SDL2_ttf)
elseif(TARGET SDL2_ttf::SDL2_ttf-static)
    target_link_libraries(sand_sim PRIVATE SDL2_ttf::SDL2_ttf-static)
else()
    # Manual fallback (e.g. older distros without a CMake config for ttf).
    find_path(SDL2_TTF_INCLUDE_DIR SDL_ttf.h PATH_SUFFIXES SDL2)
    find_library(SDL2_TTF_LIBRARY NAMES SDL2_ttf)
    if(NOT SDL2_TTF_INCLUDE_DIR OR NOT SDL2_TTF_LIBRARY)
        message(FATAL_ERROR
            "SDL2_ttf not found. Install it (e.g. 'apt install libsdl2-ttf-dev', "
            "'brew install sdl2_ttf', or vcpkg 'sdl2-ttf').")
    endif()
    target_include_directories(sand_sim PRIVATE ${SDL2_TTF_INCLUDE_DIR})
    target_link_libraries(sand_sim PRIVATE ${SDL2_TTF_LIBRARY})
endif()

# Copy the assets folder next to the binary so 'assets/font.ttf' resolves when
# running from the build directory.
add_custom_command(TARGET sand_sim POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:sand_sim>/assets)
