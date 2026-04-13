#include <stdint.h>
#include <string.h>

#include "macro_payload_internal.h"

static bool macro_payload_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static void macro_payload_trim(const char **start, const char **end) {
    while (*start < *end && macro_payload_is_space(**start)) {
        (*start)++;
    }
    while (*start < *end && macro_payload_is_space((*end)[-1])) {
        (*end)--;
    }
}

static bool macro_payload_parse_delay(const char *start, const char *end, uint16_t *delay_ms) {
    uint32_t delay = 0;

    if (start == end) {
        return false;
    }

    for (const char *cursor = start; cursor < end; cursor++) {
        if (*cursor < '0' || *cursor > '9') {
            return false;
        }
        delay = (delay * 10) + (uint32_t)(*cursor - '0');
        if (delay > UINT16_MAX) {
            delay = UINT16_MAX;
        }
    }

    *delay_ms = (uint16_t)delay;
    return true;
}

static bool macro_payload_parse_tap_list(const char *start, const char *end, uint8_t *keycodes, size_t capacity, size_t *keycode_count) {
    size_t      count      = 0;
    const char *item_start = start;

    for (const char *cursor = start; cursor <= end; cursor++) {
        if (cursor != end && *cursor != ',') {
            continue;
        }

        const char *trimmed_start = item_start;
        const char *trimmed_end   = cursor;
        uint8_t     keycode       = 0;

        macro_payload_trim(&trimmed_start, &trimmed_end);
        if (trimmed_start == trimmed_end) {
            return false;
        }
        if (count == capacity) {
            return false;
        }
        if (!macro_payload_lookup_keycode(trimmed_start, (size_t)(trimmed_end - trimmed_start), &keycode)) {
            return false;
        }

        keycodes[count++] = keycode;
        item_start        = cursor + 1;
    }

    if (count == 0) {
        return false;
    }

    *keycode_count = count;
    return true;
}

bool macro_payload_parse_command(const char *start, const char *end, macro_payload_command_t *command) {
    const char *trimmed_start = start;
    const char *trimmed_end   = end;
    uint8_t     keycode       = 0;
    uint16_t    delay_ms      = 0;
    size_t      keycode_count = 0;

    macro_payload_trim(&trimmed_start, &trimmed_end);
    if (trimmed_start == trimmed_end) {
        return false;
    }

    if (macro_payload_parse_delay(trimmed_start, trimmed_end, &delay_ms)) {
        command->kind     = MACRO_PAYLOAD_COMMAND_DELAY;
        command->delay_ms = delay_ms;
        return true;
    }

    if (*trimmed_start == '+' || *trimmed_start == '-') {
        bool        is_keydown = *trimmed_start == '+';
        const char *key_start  = trimmed_start + 1;
        const char *key_end    = trimmed_end;

        macro_payload_trim(&key_start, &key_end);
        if (key_start == key_end) {
            return false;
        }
        if (memchr(key_start, ',', (size_t)(key_end - key_start)) != NULL) {
            return false;
        }
        if (!macro_payload_lookup_keycode(key_start, (size_t)(key_end - key_start), &keycode)) {
            return false;
        }

        command->kind    = is_keydown ? MACRO_PAYLOAD_COMMAND_KEY_DOWN : MACRO_PAYLOAD_COMMAND_KEY_UP;
        command->keycode = keycode;
        return true;
    }

    if (!macro_payload_parse_tap_list(trimmed_start, trimmed_end, command->tap_list.keycodes, sizeof(command->tap_list.keycodes) / sizeof(command->tap_list.keycodes[0]), &keycode_count)) {
        return false;
    }

    command->kind           = MACRO_PAYLOAD_COMMAND_TAP_LIST;
    command->tap_list.count = (uint8_t)keycode_count;
    return true;
}

bool macro_payload_visit(const char *payload, macro_payload_text_visitor_t visit_text, macro_payload_command_visitor_t visit_command, void *context) {
    const char *cursor = payload;

    if (!payload || !visit_text || !visit_command) {
        return false;
    }

    while (*cursor) {
        if (*cursor == '{') {
            const char             *command_start = cursor + 1;
            const char             *command_end   = command_start;
            macro_payload_command_t command       = {0};

            while (*command_end && *command_end != '}') {
                command_end++;
            }
            if (*command_end != '}') {
                return false;
            }
            if (!macro_payload_parse_command(command_start, command_end, &command)) {
                return false;
            }
            if (!visit_command(&command, context)) {
                return false;
            }

            cursor = command_end + 1;
            continue;
        }

        if (*cursor == '}') {
            return false;
        }
        if ((uint8_t)*cursor > 0x7F) {
            return false;
        }
        if (!visit_text(*cursor, context)) {
            return false;
        }

        cursor++;
    }

    return true;
}
