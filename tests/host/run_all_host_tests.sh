#!/bin/sh
# The whole host suite. Each check builds and runs on its own (every runner
# compiles into its own mktemp directory), so they run side by side,
# NOAH_HOST_TEST_JOBS at a time (default below), in the build image
# (in_build_image.sh). Each check's output is kept and printed in list order
# once all have finished; the suite fails if any check failed, repeating each
# failed check's output under its name. NOAH_HOST_TEST_JOBS=1 runs them one at
# a time, in order.

set -eu
. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/in_build_image.sh"

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
JOBS="${NOAH_HOST_TEST_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"
LOGS="$(mktemp -d)"
trap 'rm -rf "$LOGS"' EXIT HUP INT TERM
export ROOT LOGS

# One check per line: its interpreter and its script in tests/host. The
# slowest come first, so none starts last and holds the suite up.
# run_contract_probe.sh checks that the contract Ark is checked against always
# builds and states itself; its statement on stdout is not kept.
CHECKS='sh run_feature_gate_compile_tests.sh
sh run_tap_depth_eight_tests.sh
sh run_qmk_gesture_pipeline_tests.sh
sh run_profile_domain_owner_round_trip_tests.sh
sh run_rgb_layer_render_tests.sh
sh run_qmk_usb_product_name_owner_tests.sh
sh run_runtime_debug_tests.sh
sh run_profile_compiled_defaults_v1_tests.sh
python3 pair_build_script_test.py
python3 weak_const_default_test.py
sh run_firmware_client_independence_tests.sh
sh run_qmk_contract_checks.sh
sh run_qmk_usb_product_name_tests.sh
sh run_qmk_report_filter_tests.sh
sh run_split_transport_build_tests.sh
sh run_split_activity_tests.sh
sh run_qmk_durable_io_tests.sh
sh run_qmk_physical_half_tests.sh
sh run_qmk_combo_origin_tests.sh
sh run_hook_chaining_tests.sh
sh run_runtime_init_order_tests.sh
sh run_keyboard_mod_ownership_tests.sh
sh run_synthetic_record_tests.sh
sh run_owned_keycode_tests.sh
sh run_held_action_tests.sh
sh run_layer_ownership_tests.sh
sh run_pd_mode_tests.sh
sh run_pd_mode_handlers_tests.sh
sh run_pd_runtime_tests.sh
sh run_pointer_layer_policy_tests.sh
sh run_split_runtime_sync_tests.sh
sh run_profile_storage_layout_tests.sh
sh run_profile_blob_v1_tests.sh
sh run_key_behavior_domain_v1_tests.sh
sh run_profile_rgb_v1_tests.sh
sh run_profile_rgb_v4_tests.sh
sh run_profile_settings_v1_tests.sh
sh run_host_detection_tests.sh
sh run_profile_pd_v1_tests.sh
sh run_pd_profile_integration_tests.sh
sh run_qmk_portable_editor_tests.sh
sh run_qmk_portable_profile_tests.sh
sh run_profile_validator_v1_tests.sh
sh run_profile_keymap_macros_runtime_tests.sh
sh run_profile_validator_work_budget_tests.sh
sh run_effective_profile_provider_tests.sh
sh run_profile_working_copy_tests.sh
sh run_profile_working_sync_tests.sh
sh run_effective_rgb_runtime_tests.sh
sh run_profile_activation_policy_tests.sh
sh run_profile_split_foundation_tests.sh
sh run_profile_split_reconciler_tests.sh
sh run_qmk_profile_split_transport_tests.sh
sh run_profile_store_tests.sh
sh run_profile_flash_store_tests.sh
sh run_qmk_profile_flash_tests.sh
sh run_profile_store_runtime_tests.sh
sh run_profile_store_runtime_owner_tests.sh
sh run_profile_owner_tests.sh
sh run_profile_wire_v1_tests.sh
sh run_qmk_profile_edit_channel_tests.sh
sh run_qmk_keyboard_restart_tests.sh
sh run_qmk_profile_adjustments_tests.sh
sh run_combo_readback_tests.sh
sh run_effective_combo_runtime_tests.sh
sh run_profile_candidate_transaction_tests.sh
sh run_profile_candidate_store_backend_tests.sh
sh run_profile_peer_store_backend_tests.sh
sh run_runtime_diag_tests.sh
sh run_pending_release_queue_tests.sh
sh run_runtime_trace_tests.sh
sh run_key_behavior_lookup_tests.sh
sh run_participation_tests.sh
sh run_key_behavior_validation_tests.sh
sh run_keymap_validation_tests.sh
sh run_all_profile_validation_tests.sh
sh run_firmware_stack_budget_tool_tests.sh
sh run_firmware_memory_budget_tool_tests.sh
sh run_pd_mode_key_runtime_integration_tests.sh
sh run_action_dispatch_tests.sh
sh run_delayed_action_tests.sh
sh run_action_lifecycle_tests.sh
sh run_macro_slot_provider_tests.sh
sh run_macro_payload_tests.sh
sh run_macro_program_size_tests.sh
sh run_macro_payload_engine_tests.sh
sh run_macro_output_ownership_tests.sh
sh run_host_layout_tests.sh
sh run_key_runtime_release_matrix_tests.sh
sh run_key_runtime_integration_harness_tests.sh
sh run_key_runtime_scenario_tests.sh
sh run_key_runtime_layer_lock_integration_tests.sh
sh run_real_profile_thumb_layer_lock_integration_tests.sh
sh run_key_runtime_modifier_hold_integration_tests.sh
sh run_key_runtime_physical_ownership_integration_tests.sh
sh run_rgb_validation_tests.sh
sh run_contract_probe.sh'

# Checks that time a real thread against the wall clock run after the others,
# one at a time, so the pool's load cannot starve them: the split frame CRC
# harness waits 20 ms for its peer, as the link does.
ALONE='sh run_split_frame_crc_tests.sh'

run_checks() { # run_checks FIRST_NUMBER JOBS CHECKS
    printf '%s\n' "$3" | awk -v first="$1" '{ printf "%03d %s %s\n", first + NR - 1, $1, $2 }' |
        xargs -L 1 -P "$2" sh -c '
            start=$(date +%s)
            if [ "$3" = run_contract_probe.sh ]; then
                "$2" "$ROOT/$3" > /dev/null 2> "$LOGS/$1.log"
            else
                "$2" "$ROOT/$3" > "$LOGS/$1.log" 2>&1
            fi || echo "$2 tests/host/$3" > "$LOGS/$1.failed"
            echo "$(( $(date +%s) - start )) $3" > "$LOGS/$1.time"
        ' check
}

run_checks 1 "$JOBS" "$CHECKS"
run_checks 900 1 "$ALONE"

for log in "$LOGS"/*.log; do
    cat "$log"
done
[ -z "${NOAH_HOST_TEST_TIMES:-}" ] || { echo "slowest checks (seconds):"; cat "$LOGS"/*.time | sort -rn | head -15; }
failed=0
for mark in "$LOGS"/*.failed; do
    [ -f "$mark" ] || continue
    failed=$((failed + 1))
    echo >&2
    echo "== $(cat "$mark") failed:" >&2
    cat "${mark%.failed}.log" >&2
done
checks=$(ls "$LOGS"/*.log | wc -l | tr -d ' ')
if [ "$failed" -gt 0 ]; then
    echo >&2
    echo "host suite failed: $failed of $checks checks" >&2
    cat "$LOGS"/*.failed | sed 's/^/  /' >&2
    exit 1
fi
echo "host suite passed: $checks checks, $JOBS at a time"
