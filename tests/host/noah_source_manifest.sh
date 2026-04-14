noah_source_manifest_value() {
    root="$1"
    variable_name="$2"
    keymap_path="${3:-keyboards/bastardkb/charybdis/4x6/keymaps/noah}"

    make -s -f - "print-$variable_name" ROOT="$root" USER_PATH=users/noah KEYMAP_PATH="$keymap_path" <<'EOF'
include $(ROOT)/users/noah/source_manifest.mk
print-%: ; @printf '%s\n' '$($*)'
EOF
}

noah_source_manifest_userspace_paths() {
    root="$1"
    variable_name="$2"
    keymap_path="${3:-keyboards/bastardkb/charybdis/4x6/keymaps/noah}"
    value="$(noah_source_manifest_value "$root" "$variable_name" "$keymap_path")"
    prefixed_paths=""

    for src in $value; do
        prefixed_paths="$prefixed_paths users/noah/$src"
    done

    printf '%s\n' "${prefixed_paths# }"
}

noah_source_manifest_raw_paths() {
    root="$1"
    variable_name="$2"
    keymap_path="${3:-keyboards/bastardkb/charybdis/4x6/keymaps/noah}"

    noah_source_manifest_value "$root" "$variable_name" "$keymap_path"
}

noah_host_public_key_runtime_support_paths() {
    root="$1"

    cat <<EOF
$root/users/noah/lib/action/action_kind.c
$root/users/noah/lib/key/interaction/handled_key_defaults.c
$root/users/noah/lib/key/interaction/handled_key_materialize.c
$root/users/noah/lib/key/interaction/handled_key_resolution_accessors.c
$root/users/noah/lib/key/interaction/handled_key_transparency.c
$root/users/noah/lib/key/interaction/multi_tap_engine.c
$root/users/noah/lib/key/ownership/held_action.c
$root/users/noah/lib/key/ownership/held_repeat.c
$root/users/noah/lib/key/runtime/key_runtime_admission.c
$root/users/noah/lib/key/runtime/key_runtime_debug.c
$root/users/noah/lib/key/runtime/key_runtime_index.c
$root/users/noah/lib/key/runtime/key_runtime_preflight.c
$root/users/noah/lib/key/runtime/key_runtime_press.c
$root/users/noah/lib/key/runtime/key_runtime_process.c
$root/users/noah/lib/key/runtime/key_runtime_release.c
$root/users/noah/lib/key/runtime/key_runtime_trace.c
$root/users/noah/lib/key/runtime/key_runtime_transition.c
$root/users/noah/lib/key/runtime/slot/key_runtime_slot.c
$root/users/noah/lib/key/runtime/slot/key_runtime_slot_pending_multi_tap.c
$root/users/noah/lib/key/runtime/slot/key_runtime_slot_policy.c
$root/users/noah/lib/key/runtime/slot/key_runtime_slot_press_reduce.c
$root/users/noah/lib/key/runtime/slot/key_runtime_slot_release_active.c
$root/users/noah/lib/key/runtime/slot/key_runtime_slot_release_reduce.c
$root/users/noah/lib/key/runtime/slot/key_runtime_slot_result.c
$root/users/noah/lib/key/runtime/slot/key_runtime_slot_scan_reduce.c
$root/users/noah/lib/key/runtime/slot/key_runtime_slot_step.c
$root/users/noah/lib/state/ownership/keyboard_mod_ownership.c
$root/users/noah/lib/state/runtime/runtime_shared_state.c
EOF
}
