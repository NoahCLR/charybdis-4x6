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

noah_source_manifest_absolute_userspace_paths_selected() {
    root="$1"
    variable_name="$2"
    selected_paths="${3:-}"
    keymap_path="${4:-keyboards/bastardkb/charybdis/4x6/keymaps/noah}"
    manifest_paths="$(noah_source_manifest_raw_paths "$root" "$variable_name" "$keymap_path")"
    absolute_paths=""

    for src in $selected_paths; do
        found_in_manifest=false

        for manifest_src in $manifest_paths; do
            if [ "$src" = "$manifest_src" ]; then
                found_in_manifest=true
                break
            fi
        done

        if [ "$found_in_manifest" = false ]; then
            printf 'source not present in %s: %s\n' "$variable_name" "$src" >&2
            return 1
        fi

        absolute_paths="$absolute_paths $root/users/noah/$src"
    done

    printf '%s\n' "${absolute_paths# }"
}

noah_host_public_key_runtime_base_sources() {
    cat <<'EOF'
lib/action/action_kind.c
lib/key/interaction/handled_key_defaults.c
lib/key/interaction/handled_key_materialize.c
lib/key/interaction/handled_key_resolution_accessors.c
lib/key/interaction/handled_key_transparency.c
lib/key/runtime/debug.c
lib/key/runtime/preflight.c
lib/key/runtime/press.c
lib/key/runtime/process.c
lib/key/runtime/origin_registry.c
lib/key/runtime/release.c
lib/key/runtime/deferred_release.c
lib/key/runtime/scan.c
lib/key/runtime/trace.c
lib/key/runtime/transition.c
lib/compat/qmk_combo_origin.c
lib/key/runtime/core/runtime.c
lib/key/runtime/core/release_planner.c
lib/state/runtime/runtime_diag.c
lib/state/runtime/keyboard_mod_state.c
lib/state/runtime/runtime_shared_state.c
EOF
}

noah_host_runtime_debug_support_paths() {
    root="$1"
    base_sources="$(noah_host_public_key_runtime_base_sources)"
    common_additions='
lib/key/ownership/held_action.c
lib/key/ownership/held_repeat.c
lib/key/runtime/feedback.c
lib/key/runtime/core/trace.c
lib/state/ownership/keyboard_mod_ownership.c
lib/state/ownership/layer_ownership.c
lib/state/runtime/runtime_trace.c'
    pointing_additions='
lib/pointing/policy/pointer_layer_policy.c
lib/pointing/runtime/pd_mode_snapshot.c
lib/pointing/runtime/pd_mode_key_runtime_bridge.c
lib/pointing/runtime/pd_mode_state.c'
    base_paths="$(noah_source_manifest_absolute_userspace_paths_selected "$root" NOAH_COMMON_SOURCES "$base_sources")"
    common_paths="$(noah_source_manifest_absolute_userspace_paths_selected "$root" NOAH_COMMON_SOURCES "$common_additions")"
    pointing_paths="$(noah_source_manifest_absolute_userspace_paths_selected "$root" NOAH_POINTING_SOURCES "$pointing_additions")"

    printf '%s %s %s\n' "$base_paths" "$common_paths" "$pointing_paths"
}

noah_host_key_runtime_modifier_hold_support_paths() {
    root="$1"
    base_sources="$(noah_host_public_key_runtime_base_sources)"
    common_additions='
lib/action/owned_keycode.c
lib/key/interaction/handled_key_lookup.c
lib/key/ownership/held_action.c
lib/key/ownership/held_repeat.c
lib/key/runtime/api.c
lib/state/ownership/keyboard_mod_ownership.c'
    base_paths="$(noah_source_manifest_absolute_userspace_paths_selected "$root" NOAH_COMMON_SOURCES "$base_sources")"
    common_paths="$(noah_source_manifest_absolute_userspace_paths_selected "$root" NOAH_COMMON_SOURCES "$common_additions")"

    printf '%s %s\n' "$base_paths" "$common_paths"
}

noah_host_key_runtime_scenario_support_paths() {
    root="$1"
    base_sources="$(noah_host_public_key_runtime_base_sources)"
    common_additions='
lib/key/interaction/handled_key_lookup.c
lib/key/runtime/api.c
lib/state/runtime/runtime_trace.c'
    base_paths="$(noah_source_manifest_absolute_userspace_paths_selected "$root" NOAH_COMMON_SOURCES "$base_sources")"
    common_paths="$(noah_source_manifest_absolute_userspace_paths_selected "$root" NOAH_COMMON_SOURCES "$common_additions")"

    printf '%s %s\n' "$base_paths" "$common_paths"
}
