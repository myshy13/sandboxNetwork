cmake_minimum_required(VERSION 3.20)

# tvOS: run on 26.0 and later (the SDK default would be its own version); must come before project()
if(CMAKE_SYSTEM_NAME STREQUAL "tvOS")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "26.0" CACHE STRING "Minimum tvOS version")
endif()

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

# iOS: cmake -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DRAYLIB_IOS_DIR=<ghera/raylib-ios clone with PR #1> [-DIOS_TEAM=<team id>]
# tvOS: same, with -DCMAKE_SYSTEM_NAME=tvOS [-DCMAKE_OSX_SYSROOT=appletvsimulator]; the backend is patched in from tvos/ at configure time
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(BUILD_IOS ON)
endif()
set(RAYLIB_IOS_DIR "" CACHE PATH "ghera/raylib-ios checkout (iOS 27 scene fix; tvOS patch is made against d1aedbd6)")
set(IOS_TEAM "" CACHE STRING "Apple development team id (device builds only)")
set(IOS_BUNDLE_ID "com.sandboxnetwork.client" CACHE STRING "Bundle id for iOS (must be unique to your team)")

if(CMAKE_SYSTEM_NAME STREQUAL "tvOS")
    set(BUILD_IOS ON)
    set(BUILD_TVOS ON)
    # Xcode's Debug configuration is -O0 (the generator derives it from these flags), far too slow on an Apple TV
    set(CMAKE_C_FLAGS_DEBUG "-g -O3")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O3")
    set(CMAKE_OBJC_FLAGS_DEBUG "-g -O3")
endif()

# ---------------------------------------------------------------------------
# raylib (fetched at configure time, built from source)
# ---------------------------------------------------------------------------
include(FetchContent)

set(FETCHCONTENT_QUIET FALSE)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)

# Don't build raylib's examples/games, we only want the library.
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

if(BUILD_WEB)
    set(PLATFORM "Web" CACHE STRING "" FORCE)
    # Web defaults to GLES2 rlgl, whose instancing/VAO extension pointers are null in a WebGL2 context
    set(OPENGL_VERSION "ES 3.0" CACHE STRING "" FORCE)
endif()

if(BUILD_IOS)
    # The fork has no CMake iOS support (only an Xcode project), so build its sources the way that project does.
    if(NOT RAYLIB_IOS_DIR)
        message(FATAL_ERROR "iOS needs -DRAYLIB_IOS_DIR=<path to ghera/raylib-ios>")
    endif()
    enable_language(C OBJC)
    set(raylib_SOURCE_DIR "${RAYLIB_IOS_DIR}")
    if(BUILD_TVOS)
        # tvOS: build a patched copy of the fork's src (backend and rlgl hook live in tvos/), so the checkout stays untouched
        set(RAYLIB_TVOS_DIR "${CMAKE_BINARY_DIR}/raylib-tvos")
        set(RAYLIB_TVOS_STAMP "${RAYLIB_TVOS_DIR}/.patched")
        set(TVOS_INPUTS "${CMAKE_SOURCE_DIR}/tvos/rcore_tvos.c" "${CMAKE_SOURCE_DIR}/tvos/tvos.patch" "${CMAKE_SOURCE_DIR}/tvos/apply.sh")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${TVOS_INPUTS})
        set(TVOS_STALE OFF)
        foreach(f ${TVOS_INPUTS})
            if(NOT EXISTS "${RAYLIB_TVOS_STAMP}" OR "${f}" IS_NEWER_THAN "${RAYLIB_TVOS_STAMP}")
                set(TVOS_STALE ON)
            endif()
        endforeach()
        if(TVOS_STALE)
            file(REMOVE_RECURSE "${RAYLIB_TVOS_DIR}")
            file(COPY "${RAYLIB_IOS_DIR}/src" DESTINATION "${RAYLIB_TVOS_DIR}")
            execute_process(COMMAND sh "${CMAKE_SOURCE_DIR}/tvos/apply.sh" "${RAYLIB_TVOS_DIR}/src"
                RESULT_VARIABLE TVOS_RESULT OUTPUT_VARIABLE TVOS_OUT ERROR_VARIABLE TVOS_OUT)
            if(NOT TVOS_RESULT EQUAL 0)
                message(FATAL_ERROR "tvos/apply.sh failed (the fork must be ghera/raylib-ios at d1aedbd6 or compatible):\n${TVOS_OUT}")
            endif()
            file(TOUCH "${RAYLIB_TVOS_STAMP}")
        endif()
        set(raylib_SOURCE_DIR "${RAYLIB_TVOS_DIR}")
    endif()
    foreach(m rcore rshapes rtextures rtext rmodels raudio)
        list(APPEND RAYLIB_IOS_SOURCES "${raylib_SOURCE_DIR}/src/${m}.c")
    endforeach()
    add_library(raylib STATIC ${RAYLIB_IOS_SOURCES})
    # rcore.c #includes rcore_ios.c and raudio.c (miniaudio) pulls in Foundation: both are Objective-C, as in the fork's project
    set_source_files_properties("${raylib_SOURCE_DIR}/src/rcore.c" "${raylib_SOURCE_DIR}/src/raudio.c" PROPERTIES LANGUAGE OBJC)
    target_compile_options(raylib PRIVATE -fobjc-arc)
    # Clang modules: ANGLE's glext.h must precede gl2.h (iOS), and rcore.c's Point/Size clash with Foundation's MacTypes.h without them
    set_target_properties(raylib PROPERTIES XCODE_ATTRIBUTE_CLANG_ENABLE_MODULES YES)
    target_include_directories(raylib PUBLIC "${raylib_SOURCE_DIR}/src")
    # PLATFORM_IOS and PLATFORM_TVOS must never both be set: rcore.c would pick the ANGLE backend
    if(BUILD_TVOS)
        # OpenGLES is deprecated on tvOS but still shipped, so there is no ANGLE
        target_compile_definitions(raylib PUBLIC PLATFORM_TVOS GRAPHICS_API_OPENGL_ES3 GL_GLEXT_PROTOTYPES GLES_SILENCE_DEPRECATION)
        target_link_libraries(raylib PUBLIC "-framework OpenGLES" "-framework GameController")
    else()
        target_compile_definitions(raylib PUBLIC PLATFORM_IOS GRAPHICS_API_OPENGL_ES3 GL_GLEXT_PROTOTYPES)
        # ANGLE (GLES on Metal), shipped as xcframeworks; rlgl.h includes them as <libGLESv2/...>
        set(ANGLE_FRAMEWORKS
            "${raylib_SOURCE_DIR}/deps/ANGLE/libEGL.xcframework"
            "${raylib_SOURCE_DIR}/deps/ANGLE/libGLESv2.xcframework")
        target_link_libraries(raylib PUBLIC ${ANGLE_FRAMEWORKS})
    endif()
    target_link_libraries(raylib PUBLIC
        "-framework UIKit" "-framework Foundation" "-framework QuartzCore"
        "-framework AudioToolbox" "-framework CoreAudio" "-framework AVFoundation")
else()
    FetchContent_MakeAvailable(raylib)
endif()

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
        GIT_TAG v1.3.18
        GIT_SHALLOW TRUE
    )

    FetchContent_MakeAvailable(enet)
endif()

# ---------------------------------------------------------------------------
# cpp-httplib + Mbed TLS (HTTPS to the auth service, fetched at configure time)
#
# Native only: the web build uses the browser's fetch (TLS included). httplib is >= 0.47 because
# earlier Mbed TLS builds skipped chain validation for IP-literal hosts. Mbed TLS is excluded from
# "all" so it only builds once a target links it.
# ---------------------------------------------------------------------------
if(NOT BUILD_WEB)
    FetchContent_Declare(
        mbedtls
        GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
        GIT_TAG v3.6.7
        GIT_SHALLOW TRUE
        GIT_SUBMODULES framework
        EXCLUDE_FROM_ALL
    )
    set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
    set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "" FORCE)
    set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "" FORCE)

    # Header only: SOURCE_SUBDIR points at a folder with no CMakeLists.txt, so httplib is downloaded but
    # not built; the backend adds ${httplib_SOURCE_DIR} to its include path.
    FetchContent_Declare(
        httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG v0.59.0
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR header-only
    )

    FetchContent_MakeAvailable(mbedtls httplib)
endif()

# ---------------------------------------------------------------------------
# Cereal
# ---------------------------------------------------------------------------

FetchContent_Declare(
    cereal
    GIT_REPOSITORY https://github.com/USCiLab/cereal.git
    GIT_TAG v1.3.2
    GIT_SHALLOW TRUE
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

# # Exactly one InputSource backend gets compiled.
# Testing: -DUSE_GAMEPAD_INPUT=ON swaps in the gamepad backend on any platform (keyboard and mouse stop working).
option(USE_GAMEPAD_INPUT "Use input_gamepad.cpp instead of the platform's input backend" OFF)
if(BUILD_TVOS OR USE_GAMEPAD_INPUT)
    set(INPUT_BACKEND input_gamepad.cpp)
elseif(BUILD_IOS)
    set(INPUT_BACKEND input_touch.cpp)
else()
    set(INPUT_BACKEND input_kbm.cpp)
endif()
foreach(f input_kbm.cpp input_touch.cpp input_gamepad.cpp)
    if(NOT f STREQUAL INPUT_BACKEND)
        list(REMOVE_ITEM PROJECT_SOURCES "${CMAKE_SOURCE_DIR}/src/Input/InputSources/${f}")
    endif()
endforeach()

# Compile exactly one backend per platform; keep the native/web split here.
if(BUILD_WEB)
    list(REMOVE_ITEM PROJECT_SOURCES "${CMAKE_SOURCE_DIR}/src/Net/transport_enet.cpp")
    list(REMOVE_ITEM PROJECT_SOURCES "${CMAKE_SOURCE_DIR}/src/Net/httpClient_native.cpp")
    list(REMOVE_ITEM PROJECT_SOURCES "${CMAKE_SOURCE_DIR}/src/Auth/tokenStore_file.cpp")
else()
    list(REMOVE_ITEM PROJECT_SOURCES "${CMAKE_SOURCE_DIR}/src/Net/transport_ws.cpp")
    list(REMOVE_ITEM PROJECT_SOURCES "${CMAKE_SOURCE_DIR}/src/Net/httpClient_web.cpp")
    list(REMOVE_ITEM PROJECT_SOURCES "${CMAKE_SOURCE_DIR}/src/Auth/tokenStore_web.cpp")
endif()

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

# Shadows start on natively and off in the browser and on phones (the settings screen can
# still change it); game code reads this as a plain value, not an #ifdef.
if(BUILD_WEB OR BUILD_IOS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SHADOWS_DEFAULT=0)
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE SHADOWS_DEFAULT=1)
endif()

if(BUILD_TVOS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE RENDER_SCALE_DEFAULT=1.0f)
elseif(BUILD_WEB)
    target_compile_definitions(${PROJECT_NAME} PRIVATE RENDER_SCALE_DEFAULT=1.5f)
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE RENDER_SCALE_DEFAULT=1.0f)

endif()

if(BUILD_TVOS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE BUILD_TVOS=1)
endif()

# Auto-jump (hop up one-block ledges while walking into them) starts on for phones, which have too few
# fingers for a jump button as well as a stick and a look drag; the settings screen can still change it.
if(BUILD_IOS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE AUTO_JUMP_DEFAULT=1)
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE AUTO_JUMP_DEFAULT=0)
endif()

# Frame-rate cap: 0 means uncapped (the browser's loop paces the web build, iOS's display link
# paces the phone, and the settings slider is hidden there); native starts at 120.
if(BUILD_WEB OR BUILD_IOS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE TARGET_FPS_DEFAULT=0)
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE TARGET_FPS_DEFAULT=120)
endif()

# Native macOS: bundle into a .app. Emscripten sets its own .html suffix
# below, so keep this off for the web build.
if(APPLE AND NOT BUILD_WEB)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.sandboxnetwork.client"
        MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_NAME}"
    )

    target_compile_definitions(${PROJECT_NAME} PRIVATE DESKTOP)

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

# iOS + tvOS: signing and the generated Info.plist
if(BUILD_IOS)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${IOS_BUNDLE_ID}"
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${IOS_TEAM}"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE Automatic
        XCODE_ATTRIBUTE_GENERATE_INFOPLIST_FILE YES
        XCODE_ATTRIBUTE_INFOPLIST_KEY_NSLocalNetworkUsageDescription "Connects to the game server on your network."
    )
endif()

if(BUILD_TVOS)
    set_target_properties(${PROJECT_NAME} PROPERTIES XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "3")
elseif(BUILD_IOS)
    # Embed + sign ANGLE; landscape, full screen, no launch screen = letterboxed (as the fork's example)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        XCODE_EMBED_FRAMEWORKS "${ANGLE_FRAMEWORKS}"
        XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY ON
        XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "@executable_path/Frameworks"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        XCODE_ATTRIBUTE_INFOPLIST_KEY_UILaunchScreen_Generation YES
        XCODE_ATTRIBUTE_INFOPLIST_KEY_UIRequiresFullScreen YES
        XCODE_ATTRIBUTE_INFOPLIST_KEY_UIStatusBarHidden YES
        XCODE_ATTRIBUTE_INFOPLIST_KEY_UISupportedInterfaceOrientations "UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight"
    )
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/../Shared"
    "${cereal_SOURCE_DIR}/include"
)

if(NOT BUILD_WEB)
    target_include_directories(${PROJECT_NAME} PRIVATE "${enet_SOURCE_DIR}/include" "${httplib_SOURCE_DIR}")
    # Use only our bundled CA file on every platform: no macOS keychain (needs Security.framework) or Windows root store.
    target_compile_definitions(${PROJECT_NAME} PRIVATE CPPHTTPLIB_MBEDTLS_SUPPORT
        CPPHTTPLIB_DISABLE_MACOSX_AUTOMATIC_ROOT_CERTIFICATES
        CPPHTTPLIB_DISABLE_WINDOWS_AUTOMATIC_ROOT_CERTIFICATES_UPDATE)
endif()

if(BUILD_WEB)
    target_link_libraries(${PROJECT_NAME} PRIVATE raylib)
else()
    # The ENet transport runs its network thread with std::thread.
    find_package(Threads REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE raylib enet Threads::Threads mbedtls mbedx509 mbedcrypto)
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

    target_compile_options(${PROJECT_NAME} PRIVATE
        "-O3"
    )

    target_link_options(${PROJECT_NAME} PRIVATE
        "-O3"
        "-sUSE_GLFW=3"
        "-sWASM=1"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sFETCH=1"
        # TEMP debugging: function names in stack traces + runtime checks; remove once the web freeze is fixed
        "-sASSERTIONS=2"
        "--profiling-funcs"
        "-sMIN_WEBGL_VERSION=2"
        "-sMAX_WEBGL_VERSION=2"
        "-sEXPORTED_FUNCTIONS=['_main','_resize']"
        "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
        "-lwebsocket.js"
        "--preload-file=${CMAKE_SOURCE_DIR}/assets@assets"
        "--shell-file=${CMAKE_SOURCE_DIR}/minshell.html"
    )
endif()
