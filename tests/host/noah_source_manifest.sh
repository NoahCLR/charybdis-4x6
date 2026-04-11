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
