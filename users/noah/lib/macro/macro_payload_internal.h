#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "macro_payload.h"

#define MACRO_PAYLOAD_MAX_TAP_KEYS 16

typedef enum {
    MACRO_PAYLOAD_COMMAND_DELAY,
    MACRO_PAYLOAD_COMMAND_KEY_DOWN,
    MACRO_PAYLOAD_COMMAND_KEY_UP,
    MACRO_PAYLOAD_COMMAND_TAP_LIST,
} macro_payload_command_kind_t;

typedef struct {
    uint8_t keycodes[MACRO_PAYLOAD_MAX_TAP_KEYS];
    uint8_t count;
} macro_payload_tap_list_t;

typedef struct {
    macro_payload_command_kind_t kind;
    uint16_t                     delay_ms;
    uint8_t                      keycode;
    macro_payload_tap_list_t     tap_list;
} macro_payload_command_t;

typedef bool (*macro_payload_text_visitor_t)(char c, void *context);
typedef bool (*macro_payload_command_visitor_t)(const macro_payload_command_t *command, void *context);

bool macro_payload_lookup_keycode(const char *start, size_t length, uint8_t *keycode);
bool macro_payload_parse_command(const char *start, const char *end, macro_payload_command_t *command);
bool macro_payload_visit(const char *payload, macro_payload_text_visitor_t visit_text, macro_payload_command_visitor_t visit_command, void *context);
bool macro_payload_run(const char *payload);
