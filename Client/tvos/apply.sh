#!/bin/sh
# Turns a ghera/raylib-ios checkout into one that also builds for tvOS. Safe to re-run.
# Usage: ./apply.sh <raylib src dir>   e.g. ./apply.sh ~/Desktop/Code/raylib-ios/src
set -e
here=$(cd "$(dirname "$0")" && pwd)
src=${1:?usage: apply.sh <raylib src dir>}
[ -f "$src/rcore.c" ] || { echo "no rcore.c in $src" >&2; exit 1; }

cp "$here/rcore_tvos.c" "$src/platforms/rcore_tvos.c"

# rlSetDefaultFramebuffer only exists once tvos.patch is in
if grep -q rlSetDefaultFramebuffer "$src/rlgl.h"; then
    echo "rcore.c/rlgl.h already patched"
else
    patch -p1 -N -d "$src" < "$here/tvos.patch"
fi
