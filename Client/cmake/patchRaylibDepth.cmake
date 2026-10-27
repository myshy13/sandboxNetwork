# Run as: cmake -DRLGL=<raylib>/src/rlgl.h -P patchRaylibDepth.cmake
# raylib's ES3 path claims WebGL1's unsized depth format, which WebGL2 rejects (0x1902);
# turning texDepthWebGL off makes rlLoadTextureDepth pick GL_DEPTH_COMPONENT24.

if(NOT EXISTS "${RLGL}")
    message(FATAL_ERROR "patchRaylibDepth: RLGL not set or missing: '${RLGL}'")
endif()

file(READ "${RLGL}" text)

# Two lines, so only the ES3 block matches (the WebGL1 extension scan sets the same flag); any indent (the fork uses 2)
set(TAIL "(\n *RLGL\\.ExtSupported\\.maxDepthBits = 24;)")
set(OLD "RLGL\\.ExtSupported\\.texDepthWebGL = true;${TAIL}")
set(NEW "RLGL\\.ExtSupported\\.texDepthWebGL = false;${TAIL}")

# Already patched: don't rewrite, or the new timestamp rebuilds raylib every configure
if(text MATCHES "${NEW}")
    return()
endif()

# Counted on a copy without ';': MATCHALL returns a list, which would split each match at its semicolons
string(REPLACE ";" "" flat "${text}")
string(REGEX MATCHALL "texDepthWebGL = true\n *RLGL\\.ExtSupported\\.maxDepthBits = 24" found "${flat}")
list(LENGTH found count)
if(NOT count EQUAL 1)
    message(FATAL_ERROR "patchRaylibDepth: expected the ES3 depth lines once in ${RLGL}, found ${count}; raylib changed?")
endif()

string(REGEX REPLACE "${OLD}" "RLGL.ExtSupported.texDepthWebGL = false;\\1" text "${text}")
file(WRITE "${RLGL}" "${text}")
message(STATUS "Patched rlgl.h: sized depth texture for ES3")
