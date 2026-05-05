noah_host_expand_path() {
    case "$1" in
        "~")
            printf '%s\n' "$HOME"
            ;;
        "~/"*)
            printf '%s/%s\n' "$HOME" "${1#"~/"}"
            ;;
        *)
            printf '%s\n' "$1"
            ;;
    esac
}

noah_host_qmk_config_home() {
    if command -v qmk >/dev/null 2>&1; then
        qmk config user.qmk_home 2>/dev/null | sed -n 's/^user\.qmk_home=//p' | tail -n 1
    fi
}

noah_host_find_qmk_root() {
    root="$1"
    configured_home="$(noah_host_qmk_config_home || true)"

    for candidate in \
        "${QMK_ROOT:-}" \
        "${QMK_HOME:-}" \
        "$configured_home" \
        "$root/qmk_firmware" \
        "$root/../bastardkb-qmk" \
        "$root/../qmk_firmware"
    do
        if [ -z "$candidate" ]; then
            continue
        fi

        candidate="$(noah_host_expand_path "$candidate")"
        if [ -f "$candidate/quantum/quantum_keycodes.h" ] && [ -f "$candidate/quantum/keycodes.h" ]; then
            (cd "$candidate" && pwd)
            return 0
        fi
    done

    echo "Unable to find a QMK checkout. Set QMK_ROOT or QMK_HOME, or place QMK at ./qmk_firmware, ../bastardkb-qmk, or ../qmk_firmware." >&2
    return 1
}

noah_host_prepend_cpath() {
    include_path="$1"

    case ":${CPATH:-}:" in
        *":$include_path:"*)
            ;;
        *)
            CPATH="$include_path${CPATH:+:$CPATH}"
            ;;
    esac
}

noah_host_export_qmk_cpath() {
    root="$1"
    QMK_ROOT="$(noah_host_find_qmk_root "$root")"
    export QMK_ROOT

    for include_path in \
        "$QMK_ROOT" \
        "$QMK_ROOT/quantum" \
        "$QMK_ROOT/quantum/keymap_extras" \
        "$QMK_ROOT/quantum/send_string" \
        "$QMK_ROOT/quantum/sequencer" \
        "$QMK_ROOT/tmk_core/protocol" \
        "$QMK_ROOT/tmk_core/common" \
        "$QMK_ROOT/platforms"
    do
        noah_host_prepend_cpath "$include_path"
    done

    export CPATH
}
