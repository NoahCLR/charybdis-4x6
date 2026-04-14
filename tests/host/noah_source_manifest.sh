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

noah_source_manifest_absolute_userspace_paths_excluding() {
    root="$1"
    variable_name="$2"
    excluded_paths="${3:-}"
    keymap_path="${4:-keyboards/bastardkb/charybdis/4x6/keymaps/noah}"
    value="$(noah_source_manifest_userspace_paths "$root" "$variable_name" "$keymap_path")"
    filtered_paths=""

    for src in $value; do
        rel_path="${src#users/noah/}"
        include_src=true

        for excluded in $excluded_paths; do
            if [ "$rel_path" = "$excluded" ]; then
                include_src=false
                break
            fi
        done

        if [ "$include_src" = true ]; then
            filtered_paths="$filtered_paths $root/$src"
        fi
    done

    printf '%s\n' "${filtered_paths# }"
}

noah_host_public_key_runtime_base_exclusions() {
    cat <<'EOF'
runtime_init.c
hooks.c
lib/compat/qmk_contract.c
lib/compat/qmk_mod_contract.c
lib/compat/qmk_via_contract.c
lib/compat/split_role.c
lib/action/action_dispatch.c
lib/action/action_kind_dispatch.c
lib/action/action_lifecycle.c
lib/action/synthetic_record.c
lib/key/interaction/key_behavior_lookup.c
lib/key/interaction/keymap_validation.c
lib/key/runtime/delayed_action.c
lib/macro/macro_dispatch.c
lib/macro/macro_slot_provider.c
lib/macro/macro_payload.c
lib/macro/macro_payload_decode_qmk.c
lib/macro/macro_payload_keycodes.c
lib/macro/macro_payload_parse.c
lib/macro/macro_payload_run.c
lib/macro/macro_payload_encode.c
lib/macro/via_macro_provider.c
lib/macro/via_macro_defaults.c
lib/state/runtime/keyboard_mod_state.c
lib/state/runtime/split_runtime_sync.c
lib/rgb/core/rgb_runtime.c
lib/rgb/stages/rgb_key_feedback_stage.c
lib/rgb/stages/rgb_layer_stage.c
lib/rgb/automouse/rgb_automouse_stage.c
lib/rgb/stages/rgb_pd_mode_stage.c
lib/rgb/stages/rgb_preview_stage.c
lib/rgb/core/rgb_config_defaults.c
lib/rgb/core/rgb_validation.c
EOF
}

noah_host_runtime_debug_support_paths() {
    root="$1"
    exclusions="$(noah_host_public_key_runtime_base_exclusions)
lib/action/owned_keycode.c
lib/key/interaction/handled_key_lookup.c
lib/key/runtime/key_runtime.c
lib/key/runtime/key_runtime_scan.c"
    common_paths="$(noah_source_manifest_absolute_userspace_paths_excluding "$root" NOAH_COMMON_SOURCES "$exclusions")"

    printf '%s %s %s\n' \
        "$common_paths" \
        "$root/users/noah/lib/pointing/runtime/pd_mode_snapshot.c" \
        "$root/users/noah/lib/pointing/runtime/pd_mode_state.c"
}

noah_host_key_runtime_modifier_hold_support_paths() {
    root="$1"
    exclusions="$(noah_host_public_key_runtime_base_exclusions)
lib/key/runtime/key_runtime_feedback.c
lib/state/ownership/layer_ownership.c
lib/state/runtime/runtime_trace.c"

    noah_source_manifest_absolute_userspace_paths_excluding "$root" NOAH_COMMON_SOURCES "$exclusions"
}

noah_host_key_runtime_scenario_support_paths() {
    root="$1"
    exclusions="$(noah_host_public_key_runtime_base_exclusions)
lib/action/owned_keycode.c
lib/key/ownership/held_action.c
lib/key/ownership/held_repeat.c
lib/key/runtime/key_runtime_feedback.c
lib/state/ownership/keyboard_mod_ownership.c
lib/state/ownership/layer_ownership.c"

    noah_source_manifest_absolute_userspace_paths_excluding "$root" NOAH_COMMON_SOURCES "$exclusions"
}
