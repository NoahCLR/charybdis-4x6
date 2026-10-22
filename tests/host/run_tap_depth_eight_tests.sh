#!/bin/sh
# The shared tap depth's second configuration (D-F14): the behaviour codec,
# lookup, key runtime, tap-branch colours and split feedback, rebuilt with
# KEY_BEHAVIOR_MAX_TAP_COUNT 8 through a compiler wrapper, so each runner
# keeps its own sources and flags.
set -eu
. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/in_build_image.sh"
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
REAL_CC="$(command -v cc)"
WRAP_DIR="$(mktemp -d)"
trap 'rm -rf "$WRAP_DIR"' EXIT INT TERM
printf '#!/bin/sh\nexec "%s" -DKEY_BEHAVIOR_MAX_TAP_COUNT=8u "$@"\n' "$REAL_CC" >"$WRAP_DIR/cc"
chmod +x "$WRAP_DIR/cc"
# The runners run side by side, one per CPU.
export ROOT WRAP_DIR
printf '%s\n' \
    run_key_behavior_domain_v1_tests.sh \
    run_key_behavior_lookup_tests.sh \
    run_key_behavior_validation_tests.sh \
    run_key_runtime_scenario_tests.sh \
    run_key_runtime_release_matrix_tests.sh \
    run_key_runtime_integration_harness_tests.sh \
    run_rgb_layer_render_tests.sh \
    run_effective_rgb_runtime_tests.sh \
    run_split_runtime_sync_tests.sh \
    run_runtime_debug_tests.sh |
    xargs -P "$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)" -I{} sh -c 'PATH="$WRAP_DIR:$PATH" sh "$ROOT/$1" >/dev/null' runner {}
echo "tap depth eight: behaviour, key runtime, RGB and split feedback runners passed"
