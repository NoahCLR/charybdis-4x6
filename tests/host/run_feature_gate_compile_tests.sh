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
REPO_OWNED_PRODUCTION_PATHS="users/noah keyboards/bastardkb/charybdis/4x6/keymaps/noah"
HOST_TEST_PATHS="tests/host"
REPO_OWNED_CODE_PATHS="$REPO_OWNED_PRODUCTION_PATHS $HOST_TEST_PATHS"

# Production boundary checks must scan REPO_OWNED_PRODUCTION_PATHS, never a
# hardcoded subset such as users/noah alone.
repo_owned_code_includes() {
    pattern="$1"

    (
        cd "$ROOT"
        # Intentional word splitting for repo-owned code path list.
        # shellcheck disable=SC2086
        rg -n "$pattern" $REPO_OWNED_CODE_PATHS
    )
}

host_test_includes() {
    pattern="$1"

    (
        cd "$ROOT"
        # Intentional word splitting for host test path list.
        # shellcheck disable=SC2086
        rg -n "$pattern" $HOST_TEST_PATHS
    )
}

repo_owned_production_include_violations() {
    pattern="$1"
    allowlist="$2"

    (
        cd "$ROOT"
        # Intentional word splitting for repo-owned production path list.
        # shellcheck disable=SC2086
        rg -n "$pattern" $REPO_OWNED_PRODUCTION_PATHS | grep -Ev "$allowlist" || true
    )
}

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

check_profile_build_validation_gate() {
    if ! rg -n 'run_real_profile_validation_tests\.sh' "$ROOT/users/noah/rules.mk" >/dev/null; then
        echo "users/noah/rules.mk must hard-fail firmware builds through real-profile validation" >&2
        exit 1
    fi
}

check_runtime_sealing_boundaries() {
    if host_test_includes '#include ".*host_runtime_fixture\.h"' >/dev/null; then
        echo "host tests must not include removed umbrella runtime fixture header" >&2
        host_test_includes '#include ".*host_runtime_fixture\.h"' >&2
        exit 1
    fi

    if repo_owned_code_includes '#include "((users/noah/lib/state/runtime/)?(.*/)?runtime_(context|shared_state)\.h)"' >/dev/null; then
        echo "repo-owned code must not include removed public runtime aggregate/context headers" >&2
        repo_owned_code_includes '#include "((users/noah/lib/state/runtime/)?(.*/)?runtime_(context|shared_state)\.h)"' >&2
        exit 1
    fi

    if host_test_includes '#include "(.*/)?runtime_(context|shared_state)_internal\.h"' >/dev/null; then
        echo "host tests must not include internal runtime storage headers" >&2
        host_test_includes '#include "(.*/)?runtime_(context|shared_state)_internal\.h"' >&2
        exit 1
    fi

    runtime_internal_prod_violations="$(
        repo_owned_production_include_violations \
            '#include "(.*/)?runtime_(context|shared_state)_internal\.h"' \
            '^(users/noah/lib/state/runtime/|users/noah/lib/state/ownership/|users/noah/lib/key/ownership/)'
    )"
    if [ -n "$runtime_internal_prod_violations" ]; then
        echo "only runtime owner modules may include internal runtime storage headers" >&2
        printf '%s\n' "$runtime_internal_prod_violations" >&2
        exit 1
    fi

    if repo_owned_code_includes '#include ".*pd_mode_runtime_shared_state\.h"' >/dev/null; then
        echo "repo-owned code must not include removed public pd runtime shared-state header" >&2
        repo_owned_code_includes '#include ".*pd_mode_runtime_shared_state\.h"' >&2
        exit 1
    fi

    if host_test_includes '#include ".*pd_mode_runtime_shared_state_internal\.h"' >/dev/null; then
        echo "host tests must not include internal pd runtime storage headers" >&2
        host_test_includes '#include ".*pd_mode_runtime_shared_state_internal\.h"' >&2
        exit 1
    fi

    pd_runtime_internal_prod_violations="$(
        repo_owned_production_include_violations \
            '#include ".*pd_mode_runtime_shared_state_internal\.h"' \
            '^(users/noah/lib/state/runtime/|users/noah/lib/pointing/runtime/)'
    )"
    if [ -n "$pd_runtime_internal_prod_violations" ]; then
        echo "only runtime owner and pd runtime modules may include internal pd runtime storage headers" >&2
        printf '%s\n' "$pd_runtime_internal_prod_violations" >&2
        exit 1
    fi

    if repo_owned_code_includes '#include ".*key_runtime_(state|process|index|admission)\.h"' >/dev/null; then
        echo "repo-owned code must not include removed key-runtime aggregate/index/admission headers" >&2
        repo_owned_code_includes '#include ".*key_runtime_(state|process|index|admission)\.h"' >&2
        exit 1
    fi

    if repo_owned_code_includes '#include ".*(key_runtime_internal\.h|key_runtime_index_internal\.h|key_runtime_shared_state\.h)"' >/dev/null; then
        echo "repo-owned code must not include deleted key-runtime internal/index/shared-state headers" >&2
        repo_owned_code_includes '#include ".*(key_runtime_internal\.h|key_runtime_index_internal\.h|key_runtime_shared_state\.h)"' >&2
        exit 1
    fi

    key_runtime_process_internal_prod_allowlist='^(users/noah/lib/key/runtime/)'
    key_runtime_process_internal_prod_violations="$(
        repo_owned_production_include_violations \
            '#include ".*process_internal\.h"' \
            "$key_runtime_process_internal_prod_allowlist"
    )"
    if [ -n "$key_runtime_process_internal_prod_violations" ]; then
        echo "only key-runtime owner modules may include process_internal.h in repo-owned production code" >&2
        printf '%s\n' "$key_runtime_process_internal_prod_violations" >&2
        exit 1
    fi

    if repo_owned_code_includes '#include ".*process_internal\.h"' | grep -Ev '^users/noah/lib/key/runtime/' >/dev/null; then
        echo "repo-owned code outside key-runtime owner modules must not include process_internal.h" >&2
        repo_owned_code_includes '#include ".*process_internal\.h"' | grep -Ev '^users/noah/lib/key/runtime/' >&2
        exit 1
    fi

    pd_mode_buffered_tap_internal_test_allowlist='^(tests/host/pd_mode_test\.c:)'
    pd_mode_buffered_tap_internal_test_violations="$(
        host_test_includes '#include ".*pd_mode_buffered_tap_internal\.h"' | grep -Ev "$pd_mode_buffered_tap_internal_test_allowlist" || true
    )"
    if [ -n "$pd_mode_buffered_tap_internal_test_violations" ]; then
        echo "only the pd-mode white-box host suite may include pd_mode_buffered_tap_internal.h" >&2
        printf '%s\n' "$pd_mode_buffered_tap_internal_test_violations" >&2
        exit 1
    fi

    pd_mode_buffered_tap_internal_prod_allowlist='^users/noah/lib/pointing/runtime/'
    pd_mode_buffered_tap_internal_prod_violations="$(
        repo_owned_production_include_violations \
            '#include ".*pd_mode_buffered_tap_internal\.h"' \
            "$pd_mode_buffered_tap_internal_prod_allowlist"
    )"
    if [ -n "$pd_mode_buffered_tap_internal_prod_violations" ]; then
        echo "only pd runtime owner modules may include pd_mode_buffered_tap_internal.h in repo-owned production code" >&2
        printf '%s\n' "$pd_mode_buffered_tap_internal_prod_violations" >&2
        exit 1
    fi

    pd_mode_keyboard_event_internal_test_allowlist='^(tests/host/pd_mode_test\.c:)'
    pd_mode_keyboard_event_internal_test_violations="$(
        host_test_includes '#include ".*pd_mode_keyboard_event_internal\.h"' | grep -Ev "$pd_mode_keyboard_event_internal_test_allowlist" || true
    )"
    if [ -n "$pd_mode_keyboard_event_internal_test_violations" ]; then
        echo "only the pd-mode white-box host suite may include pd_mode_keyboard_event_internal.h" >&2
        printf '%s\n' "$pd_mode_keyboard_event_internal_test_violations" >&2
        exit 1
    fi

    pd_mode_keyboard_event_internal_prod_allowlist='^(users/noah/lib/pointing/runtime/|users/noah/lib/key/runtime/process\.c:)'
    pd_mode_keyboard_event_internal_prod_violations="$(
        repo_owned_production_include_violations \
            '#include ".*pd_mode_keyboard_event_internal\.h"' \
            "$pd_mode_keyboard_event_internal_prod_allowlist"
    )"
    if [ -n "$pd_mode_keyboard_event_internal_prod_violations" ]; then
        echo "only pd runtime owner modules and process.c may include pd_mode_keyboard_event_internal.h in repo-owned production code" >&2
        printf '%s\n' "$pd_mode_keyboard_event_internal_prod_violations" >&2
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
compile_variant "tests/host/include/noah_compile_config.h" "-DCONSOLE_ENABLE -DNOAH_KEY_RUNTIME_TRACE_ENABLE -DNOAH_RUNTIME_TRACE_ENABLE" "$COMMON_SOURCES users/noah/lib/key/runtime/process.c users/noah/lib/key/runtime/preflight.c users/noah/lib/key/runtime/press.c users/noah/lib/key/runtime/release.c users/noah/lib/key/runtime/scan.c users/noah/lib/key/runtime/transition.c"
compile_variant "tests/host/include/noah_compile_config_no_rgb_feedback.h" "$RGB_TEST_FLAGS" "$COMMON_SOURCES $RGB_SOURCES"
compile_variant "tests/host/include/noah_compile_config.h" "$RGB_TEST_FLAGS" "$COMMON_SOURCES $RGB_SOURCES"
compile_variant "tests/host/include/noah_compile_config_no_pd_active_half.h" "-DSPLIT_KEYBOARD $POINTING_TEST_FLAGS $RGB_TEST_FLAGS" "$COMMON_SOURCES $POINTING_SOURCES $RGB_SOURCES"
compile_variant "tests/host/include/noah_compile_config_no_automouse.h" "-DNOAH_RUNTIME_TRACE_ENABLE $POINTING_TEST_FLAGS" "$COMMON_SOURCES $POINTING_SOURCES"
compile_variant "tests/host/include/noah_compile_config_no_automouse.h" "$POINTING_TEST_FLAGS" "$COMMON_SOURCES $POINTING_SOURCES"
compile_variant "tests/host/include/noah_compile_config_no_automouse.h" "$POINTING_TEST_FLAGS $RGB_TEST_FLAGS" "$COMMON_SOURCES $POINTING_SOURCES $RGB_SOURCES"
compile_variant "tests/host/include/noah_compile_config_no_automouse.h" "-DSPLIT_KEYBOARD $POINTING_TEST_FLAGS $RGB_TEST_FLAGS" "$COMMON_SOURCES $POINTING_SOURCES $RGB_SOURCES"
compile_variant "tests/host/include/noah_compile_config.h" "-DSPLIT_KEYBOARD $POINTING_TEST_FLAGS $RGB_TEST_FLAGS" "$COMMON_SOURCES $POINTING_SOURCES $RGB_SOURCES $AUTOMOUSE_SOURCES"

check_header_boundaries
check_profile_build_validation_gate
check_runtime_sealing_boundaries
