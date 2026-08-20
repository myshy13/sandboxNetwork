cmake_minimum_required(VERSION 3.20)
project(sandboxNetwork CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(BUILD_WEB "Build for the web (Emscripten)" OFF)

if(DEFINED EMSCRIPTEN)
    set(BUILD_WEB ON)
endif()

# ---------------------------------------------------------------------------
# raylib (fetched at configure time, built from source)
# ---------------------------------------------------------------------------
include(FetchContent)

set(FETCHCONTENT_QUIET FALSE)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.5
    GIT_SHALLOW    TRUE
)

# Don't build raylib's examples/games, we only want the library.
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

if(BUILD_WEB)
    set(PLATFORM "Web" CACHE STRING "" FORCE)
endif()

FetchContent_MakeAvailable(raylib)

# ---------------------------------------------------------------------------
# ENet (reliable UDP, fetched at configure time, built from source)
# ---------------------------------------------------------------------------
FetchContent_Declare(
    enet
    GIT_REPOSITORY https://github.com/lsalzman/enet.git
    GIT_TAG        v1.3.18
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(enet)

# ---------------------------------------------------------------------------
# Cereal
# ---------------------------------------------------------------------------

FetchContent_Declare(
  cereal
  GIT_REPOSITORY https://github.com/USCiLab/cereal.git
  GIT_TAG        v1.3.2
  GIT_SHALLOW    TRUE
)
set(JUST_INSTALL_CEREAL ON CACHE BOOL "" FORCE) # skip building cereal's tests/docs
FetchContent_MakeAvailable(cereal)

# ---------------------------------------------------------------------------
# Source files
# ---------------------------------------------------------------------------
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/../Shared/Protocol/protocol.cpp"
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/../Shared"
    "${enet_SOURCE_DIR}/include"
    "${cereal_SOURCE_DIR}/include"
)

target_link_libraries(${PROJECT_NAME} PRIVATE raylib enet)

# Treat raylib's headers as system headers so its own -Wall/-Wextra
# warnings (e.g. raymath.h's missing-field-initializer noise) don't
# show up in our build output - it's fetched third-party code, not ours to fix.
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
    "${raylib_SOURCE_DIR}/src"
)

# Warnings - helps a lot when learning a systems-heavy codebase
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# ---------------------------------------------------------------------------
# Assets
# ---------------------------------------------------------------------------
# Keep assets next to the binary for native builds so relative paths
# (e.g. "assets/models/foo.glb") work the same in-editor and when run.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
)

# ---------------------------------------------------------------------------
# Emscripten (web) specifics
# ---------------------------------------------------------------------------
if(BUILD_WEB)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")

    target_link_options(${PROJECT_NAME} PRIVATE
        "-sUSE_GLFW=3"
        "-sWASM=1"
        "-sASYNCIFY"
        "-sALLOW_MEMORY_GROWTH=1"
        "--preload-file=${CMAKE_SOURCE_DIR}/assets@assets"
        "--shell-file=${raylib_SOURCE_DIR}/src/minshell.html"
    )
endif()