#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
cc -std=c11 -Wall -Wextra -Werror -pedantic -I"$ROOT" "$ROOT/tests/host/host_detection_test.c" -o "$BUILD_DIR/test"
"$BUILD_DIR/test"
