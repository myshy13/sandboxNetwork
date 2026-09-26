cmake_minimum_required(VERSION 3.20)
project(sandboxNetwork CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optimised unless told otherwise: with no build type everything compiles at -O0,
# which hid a 45 s load and made per-block scans ~15x slower. Use RelWithDebInfo for lldb.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

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
#
# Native only. Browsers cannot open raw UDP sockets, so the web build talks
# WebSocket to the server's --ws-port proxy instead (see src/Net/).
# ---------------------------------------------------------------------------
if(NOT BUILD_WEB)
    FetchContent_Declare(
        enet
        GIT_REPOSITORY https://github.com/lsalzman/enet.git
        GIT_TAG        v1.3.18
        GIT_SHALLOW    TRUE
    )

    FetchContent_MakeAvailable(enet)
endif()

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

# Exactly one transport backend gets compiled. This is deliberately the only
# place the native/web split lives, so no #ifdef leaks into the game code.
if(BUILD_WEB)
    list(REMOVE_ITEM PROJECT_SOURCES "${CMAKE_SOURCE_DIR}/src/Net/transport_enet.cpp")
else()
    list(REMOVE_ITEM PROJECT_SOURCES "${CMAKE_SOURCE_DIR}/src/Net/transport_ws.cpp")
endif()

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

# Native macOS: bundle into a .app. Emscripten sets its own .html suffix
# below, so keep this off for the web build.
if(APPLE AND NOT BUILD_WEB)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.sandboxnetwork.client"
        MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_NAME}"
    )

    # Opt-in signing: cmake -DSIGN_MACOS=ON -DSIGN_IDENTITY="Developer ID Application: Name (TEAMID)"
    # or SIGN_IDENTITY="-" for an ad-hoc seal (no account; can't be notarized)
    option(SIGN_MACOS "Sign the .app on Release builds" OFF)
    set(SIGN_IDENTITY "" CACHE STRING "codesign identity")
    set(NOTARY_PROFILE "notary" CACHE STRING "notarytool keychain profile")

    if(SIGN_MACOS AND CMAKE_BUILD_TYPE STREQUAL "Release")
        if(NOT SIGN_IDENTITY)
            message(FATAL_ERROR "SIGN_MACOS needs -DSIGN_IDENTITY=...")
        endif()
        set(APP_BUNDLE "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>")
        set(SIGN_FLAGS --options runtime --timestamp) # hardened runtime + timestamp: Developer ID only
        if(SIGN_IDENTITY STREQUAL "-")
            set(SIGN_FLAGS)
        endif()

        # Post-build runs after copy_assets (a dependency), so the seal covers them
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND codesign --force --deep ${SIGN_FLAGS}
                    --sign "${SIGN_IDENTITY}" "${APP_BUNDLE}"
            COMMAND codesign --verify --deep --strict "${APP_BUNDLE}"
            VERBATIM
        )

        # Uploads to Apple and waits, so it's a separate target: cmake --build . --target notarize
        if(NOT SIGN_IDENTITY STREQUAL "-")
            add_custom_target(notarize
                COMMAND ditto -c -k --keepParent "${APP_BUNDLE}" "${CMAKE_BINARY_DIR}/notarize.zip"
                COMMAND xcrun notarytool submit "${CMAKE_BINARY_DIR}/notarize.zip"
                        --keychain-profile "${NOTARY_PROFILE}" --wait
                COMMAND xcrun stapler staple "${APP_BUNDLE}"
                DEPENDS ${PROJECT_NAME}
                VERBATIM
            )
        endif()
    endif()
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/../Shared"
    "${cereal_SOURCE_DIR}/include"
)

if(NOT BUILD_WEB)
    target_include_directories(${PROJECT_NAME} PRIVATE "${enet_SOURCE_DIR}/include")
endif()

if(BUILD_WEB)
    target_link_libraries(${PROJECT_NAME} PRIVATE raylib)
else()
    # The ENet transport runs its network thread with std::thread.
    find_package(Threads REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE raylib enet Threads::Threads)
    # ENet is a static lib and does not pass its Windows socket/timer libs on to us
    if(WIN32)
        target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 winmm)
    endif()
endif()

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
# A target with no OUTPUT is always out of date, so this runs every build (even shader-only edits).
add_custom_target(copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            "${CMAKE_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
)
add_dependencies(${PROJECT_NAME} copy_assets)

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
        "-sEXPORTED_FUNCTIONS=['_main','_resize']"
        "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
        "-lwebsocket.js"
        "--preload-file=${CMAKE_SOURCE_DIR}/assets@assets"
        "--shell-file=${CMAKE_SOURCE_DIR}/minshell.html"
    )
endif()