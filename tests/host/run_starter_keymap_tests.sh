#!/bin/sh

# The starter keymap is what a release publishes (D-F78), and nothing else in
# the host suite compiles it: validate it as a real profile, the same checks
# the noah keymap's authored data gets, so a userspace change cannot break the
# released defaults unseen.

set -eu
. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/in_build_image.sh"

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
sh "$ROOT/tests/host/run_real_profile_validation_tests.sh" keyboards/bastardkb/charybdis/4x6/keymaps/starter
echo "starter keymap tests passed"
