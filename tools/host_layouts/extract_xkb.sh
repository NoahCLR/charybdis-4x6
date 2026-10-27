#!/bin/sh
# Builds and runs extract_xkb.c against Homebrew's libxkbcommon,
# xkeyboard-config and libx11 Compose data, printing the raw Linux source:
#
#   sh tools/host_layouts/extract_xkb.sh > tools/host_layouts/sources/linux.json
#
# Developer tool: brew install libxkbcommon xkeyboard-config libx11

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
XKBCOMMON="$(brew --prefix libxkbcommon)"
XKB_CONFIG="$(brew --prefix xkeyboard-config)"
X11="$(brew --prefix libx11)"
VERSION="$(brew list --versions xkeyboard-config | awk '{print $2}')"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -I"$XKBCOMMON/include" "$ROOT/extract_xkb.c" \
    -L"$XKBCOMMON/lib" -lxkbcommon -o "$BUILD_DIR/extract_xkb"
XLOCALEDIR="$X11/share/X11/locale" "$BUILD_DIR/extract_xkb" "$XKB_CONFIG/share/X11/xkb" "$VERSION"
