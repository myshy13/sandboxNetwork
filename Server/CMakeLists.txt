cmake_minimum_required(VERSION 3.20)
project(sandboxNetworkServer CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optimised unless told otherwise: with no build type everything compiles at -O0,
# which hid a 45 s load and made per-block scans ~15x slower. Use RelWithDebInfo for lldb.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# ENet (reliable UDP, fetched at configure time, built from source)
# ---------------------------------------------------------------------------
include(FetchContent)

set(FETCHCONTENT_QUIET FALSE)

# ---------------------------------------------------------------------------
# raylib (for shared math/model types, fetched at configure time)
# ---------------------------------------------------------------------------
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.5
    GIT_SHALLOW    TRUE
)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(raylib)

FetchContent_Declare(
    enet
    GIT_REPOSITORY https://github.com/lsalzman/enet.git
    GIT_TAG        v1.3.18
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(enet)

# Cereal

FetchContent_Declare(
  cereal
  GIT_REPOSITORY https://github.com/USCiLab/cereal.git
  GIT_TAG        v1.3.2
  GIT_SHALLOW    TRUE
)
set(JUST_INSTALL_CEREAL ON CACHE BOOL "" FORCE) # skip building cereal's tests/docs
FetchContent_MakeAvailable(cereal)

# ---------------------------------------------------------------------------
# IXWebSocket
#
# Browsers can't open raw UDP sockets, so the Emscripten client can't speak
# ENet. This backs the optional --ws-port proxy that bridges browser clients
# into the same game loop.
# ---------------------------------------------------------------------------
set(USE_TLS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  ixwebsocket
  GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
  GIT_TAG        v11.4.5
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(ixwebsocket)

# ---------------------------------------------------------------------------
# Source files
# ---------------------------------------------------------------------------
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/../Shared/Protocol/protocol.cpp"
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/../Shared"
    "${enet_SOURCE_DIR}/include"
    "${cereal_SOURCE_DIR}/include"
)

find_package(Threads REQUIRED) # std::async for the background world save
target_link_libraries(${PROJECT_NAME} PRIVATE raylib enet ixwebsocket Threads::Threads)

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
