#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_source_manifest.sh"
COMMON_SOURCES="$(noah_source_manifest_userspace_paths "$ROOT" NOAH_COMMON_SOURCES)"
POINTING_SOURCES="$(noah_source_manifest_userspace_paths "$ROOT" NOAH_POINTING_SOURCES)"
RGB_SOURCES="$(noah_source_manifest_raw_paths "$ROOT" NOAH_RGB_KEYMAP_SOURCES)"
AUTOMOUSE_SOURCES="$(noah_source_manifest_userspace_paths "$ROOT" NOAH_AUTOMOUSE_SOURCES)"

POINTING_TEST_FLAGS="-DPOINTING_DEVICE_ENABLE -DDPI_MOD=0x5201u -DDPI_RMOD=0x5202u -DS_D_MOD=0x5203u -DS_D_RMOD=0x5204u"
RGB_TEST_FLAGS="-DRGB_MATRIX_ENABLE -DRGB_MATRIX_WS2812"

check_header_boundaries() {
    if rg -n '#include "(users/noah/)?noah_keymap.h"' "$ROOT/users/noah" --glob '!noah_keymap.h' >/dev/null; then
        echo "runtime modules must not include noah_keymap.h directly" >&2
        rg -n '#include "(users/noah/)?noah_keymap.h"' "$ROOT/users/noah" --glob '!noah_keymap.h' >&2
        exit 1
    fi

    if rg -n '#include "(users/noah/)?noah_runtime.h"' "$ROOT/keyboards/bastardkb/charybdis/4x6/keymaps/noah" >/dev/null; then
        echo "keymap-owned translation units must not include noah_runtime.h directly" >&2
        rg -n '#include "(users/noah/)?noah_runtime.h"' "$ROOT/keyboards/bastardkb/charybdis/4x6/keymaps/noah" >&2
        exit 1
    fi
}

check_runtime_sealing_boundaries() {
    if rg -n '#include ".*host_runtime_fixture\.h"' "$ROOT/tests/host" >/dev/null; then
        echo "host tests must not include removed umbrella runtime fixture header" >&2
        rg -n '#include ".*host_runtime_fixture\.h"' "$ROOT/tests/host" >&2
        exit 1
    fi

    if rg -n '#include "((users/noah/lib/state/runtime/)?(.*/)?runtime_(context|shared_state)\.h)"' "$ROOT/users/noah" "$ROOT/tests/host" >/dev/null; then
        echo "repo-owned code must not include removed public runtime aggregate/context headers" >&2
        rg -n '#include "((users/noah/lib/state/runtime/)?(.*/)?runtime_(context|shared_state)\.h)"' "$ROOT/users/noah" "$ROOT/tests/host" >&2
        exit 1
    fi

    if rg -n '#include "(.*/)?runtime_(context|shared_state)_internal\.h"' "$ROOT/tests/host" >/dev/null; then
        echo "host tests must not include internal runtime storage headers" >&2
        rg -n '#include "(.*/)?runtime_(context|shared_state)_internal\.h"' "$ROOT/tests/host" >&2
        exit 1
    fi

    if (
        cd "$ROOT/users/noah"
        rg -n '#include "(.*/)?runtime_(context|shared_state)_internal\.h"' . --glob '!lib/state/runtime/**' --glob '!lib/state/ownership/**' --glob '!lib/key/ownership/**'
    ) >/dev/null; then
        echo "only runtime owner modules may include internal runtime storage headers" >&2
        (
            cd "$ROOT/users/noah"
            rg -n '#include "(.*/)?runtime_(context|shared_state)_internal\.h"' . --glob '!lib/state/runtime/**' --glob '!lib/state/ownership/**' --glob '!lib/key/ownership/**'
        ) >&2
        exit 1
    fi

    if rg -n '#include ".*pd_mode_runtime_shared_state\.h"' "$ROOT/users/noah" "$ROOT/tests/host" >/dev/null; then
        echo "repo-owned code must not include removed public pd runtime shared-state header" >&2
        rg -n '#include ".*pd_mode_runtime_shared_state\.h"' "$ROOT/users/noah" "$ROOT/tests/host" >&2
        exit 1
    fi

    if rg -n '#include ".*pd_mode_runtime_shared_state_internal\.h"' "$ROOT/tests/host" >/dev/null; then
        echo "host tests must not include internal pd runtime storage headers" >&2
        rg -n '#include ".*pd_mode_runtime_shared_state_internal\.h"' "$ROOT/tests/host" >&2
        exit 1
    fi

    if (
        cd "$ROOT/users/noah"
        rg -n '#include ".*pd_mode_runtime_shared_state_internal\.h"' . --glob '!lib/state/runtime/**' --glob '!lib/pointing/runtime/**'
    ) >/dev/null; then
        echo "only runtime owner and pd runtime modules may include internal pd runtime storage headers" >&2
        (
            cd "$ROOT/users/noah"
            rg -n '#include ".*pd_mode_runtime_shared_state_internal\.h"' . --glob '!lib/state/runtime/**' --glob '!lib/pointing/runtime/**'
        ) >&2
        exit 1
    fi

    if (
        cd "$ROOT"
        rg -n '#include ".*key_runtime_(state|process)\.h"' users/noah tests/host
    ) >/dev/null; then
        echo "repo-owned code must not include removed key-runtime aggregate headers" >&2
        (
            cd "$ROOT"
            rg -n '#include ".*key_runtime_(state|process)\.h"' users/noah tests/host
        ) >&2
        exit 1
    fi

    key_runtime_internal_test_allowlist='^(tests/host/(key_runtime_feedback_test|key_runtime_index_test|key_runtime_preflight_test|key_runtime_slot_test|key_runtime_transition_test)\.c:|tests/host/key_runtime_admission_test\.c:)'
    key_runtime_internal_test_violations="$(
        cd "$ROOT"
        rg -n '#include ".*(key_runtime_internal\.h|key_runtime_process_internal\.h|key_runtime_shared_state\.h)"' tests/host | grep -Ev "$key_runtime_internal_test_allowlist" || true
    )"
    if [ -n "$key_runtime_internal_test_violations" ]; then
        echo "only low-level white-box host suites may include key-runtime internal headers" >&2
        printf '%s\n' "$key_runtime_internal_test_violations" >&2
        exit 1
    fi

    key_runtime_internal_prod_allowlist='^(users/noah/lib/key/runtime/|users/noah/lib/state/runtime/runtime_shared_state_internal\.h:)'
    key_runtime_internal_prod_violations="$(
        cd "$ROOT"
        rg -n '#include ".*(key_runtime_internal\.h|key_runtime_process_internal\.h|key_runtime_shared_state\.h)"' users/noah | grep -Ev "$key_runtime_internal_prod_allowlist" || true
    )"
    if [ -n "$key_runtime_internal_prod_violations" ]; then
        echo "only key-runtime owner modules and the aggregate runtime storage wrapper may include key-runtime internal headers" >&2
        printf '%s\n' "$key_runtime_internal_prod_violations" >&2
        exit 1
    fi

    key_runtime_index_test_allowlist='^(tests/host/(key_runtime_admission_test|key_runtime_feedback_test|key_runtime_index_test|key_runtime_preflight_test|key_runtime_transition_test)\.c:)'
    key_runtime_index_test_violations="$(
        cd "$ROOT"
        rg -n '#include ".*key_runtime_index\.h"' tests/host | grep -Ev "$key_runtime_index_test_allowlist" || true
    )"
    if [ -n "$key_runtime_index_test_violations" ]; then
        echo "only low-level white-box host suites may include key_runtime_index.h" >&2
        printf '%s\n' "$key_runtime_index_test_violations" >&2
        exit 1
    fi
}

compile_variant() {
    config_header="$1"
    extra_flags="$2"
    sources="$3"

    # Intentional word splitting for flag and source lists.
    # shellcheck disable=SC2086
    for src in $sources; do
        cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic -fsyntax-only \
            -DQMK_KEYBOARD_H='"qmk_stub.h"' \
            -DQMK_STUB_SUPPRESS_LAYER_COUNT \
            $extra_flags \
            -I"$ROOT" \
            -I"$ROOT/users/noah" \
            -I"$ROOT/tests/host/include" \
            -include "$ROOT/$config_header" \
            "$ROOT/$src"
    done
}

compile_variant "tests/host/include/noah_compile_config.h" "" "$COMMON_SOURCES"
compile_variant "tests/host/include/noah_compile_config.h" "-DCONSOLE_ENABLE -DNOAH_KEY_RUNTIME_TRACE_ENABLE -DNOAH_RUNTIME_TRACE_ENABLE" "$COMMON_SOURCES users/noah/lib/key/runtime/key_runtime_process.c users/noah/lib/key/runtime/key_runtime_preflight.c users/noah/lib/key/runtime/key_runtime_press.c users/noah/lib/key/runtime/key_runtime_release.c users/noah/lib/key/runtime/key_runtime_scan.c users/noah/lib/key/runtime/key_runtime_transition.c"
compile_variant "tests/host/include/noah_compile_config_no_rgb_feedback.h" "$RGB_TEST_FLAGS" "$COMMON_SOURCES $RGB_SOURCES"
compile_variant "tests/host/include/noah_compile_config.h" "$RGB_TEST_FLAGS" "$COMMON_SOURCES $RGB_SOURCES"
compile_variant "tests/host/include/noah_compile_config_no_automouse.h" "-DNOAH_RUNTIME_TRACE_ENABLE $POINTING_TEST_FLAGS" "$COMMON_SOURCES $POINTING_SOURCES"
compile_variant "tests/host/include/noah_compile_config_no_automouse.h" "$POINTING_TEST_FLAGS" "$COMMON_SOURCES $POINTING_SOURCES"
compile_variant "tests/host/include/noah_compile_config_no_automouse.h" "$POINTING_TEST_FLAGS $RGB_TEST_FLAGS" "$COMMON_SOURCES $POINTING_SOURCES $RGB_SOURCES"
compile_variant "tests/host/include/noah_compile_config_no_automouse.h" "-DSPLIT_KEYBOARD $POINTING_TEST_FLAGS $RGB_TEST_FLAGS" "$COMMON_SOURCES $POINTING_SOURCES $RGB_SOURCES"
compile_variant "tests/host/include/noah_compile_config.h" "-DSPLIT_KEYBOARD $POINTING_TEST_FLAGS $RGB_TEST_FLAGS" "$COMMON_SOURCES $POINTING_SOURCES $RGB_SOURCES $AUTOMOUSE_SOURCES"

check_header_boundaries
check_runtime_sealing_boundaries
