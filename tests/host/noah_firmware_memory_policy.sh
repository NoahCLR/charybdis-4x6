# Artifact policy comes from the recorded compiler configuration, not the
# target filename. The query increment applies only to the D-F26 portable RGB
# profile configuration; compiling both query consumers out keeps its old limit.
# Argument3 is yes only after the artifact compiler proves the reviewed retained
# GCC15.2.0/60-key/four-combo configuration from its recorded flags/headers.
noah_firmware_static_ram_policy() {
    flags_file="$1"
    if [ ! -f "$flags_file" ] || ! grep -Eq '(^|[[:space:]])-DNOAH_PD_PROFILE_ENABLE(=[^[:space:]]+)?([[:space:]]|$)' "$flags_file"; then
        echo 57344
    elif [ "${2:-no}" = yes ] &&
         grep -Eq '(^|[[:space:]])-DNOAH_PORTABLE_PROFILE_ENABLE(=[^[:space:]]+)?([[:space:]]|$)' "$flags_file" &&
         grep -Eq '(^|[[:space:]])-DRGB_MATRIX_ENABLE(=[^[:space:]]+)?([[:space:]]|$)' "$flags_file" &&
         { ! grep -Eq '(^|[[:space:]])-DNOAH_LIGHTING_TARGET_KEYS_THAT=0[uUlL]*([[:space:]]|$)' "$flags_file" ||
           ! grep -Eq '(^|[[:space:]])-DNOAH_LIGHTING_PAINT_WHAT_IT_DOES=0[uUlL]*([[:space:]]|$)' "$flags_file"; }; then
        if [ "${3:-no}" = yes ] &&
           grep -Eq '(^|[[:space:]])-DNOAH_LIGHTING_EVENT_CAPTURE_ENABLE=1([[:space:]]|$)' "$flags_file"; then
            echo 247808
        else
            echo 229376
        fi
    else
        echo 226304
    fi
}
