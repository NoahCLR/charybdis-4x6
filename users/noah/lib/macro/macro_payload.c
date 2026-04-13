#include "macro_payload_internal.h"

static bool macro_payload_visit_text_noop(char c, void *context) {
    (void)c;
    (void)context;
    return true;
}

static bool macro_payload_visit_command_noop(const macro_payload_command_t *command, void *context) {
    (void)command;
    (void)context;
    return true;
}

bool macro_payload_validate(const char *payload) {
    return macro_payload_visit(payload, macro_payload_visit_text_noop, macro_payload_visit_command_noop, NULL);
}

bool macro_payload_play(const char *payload) {
    return macro_payload_run(payload);
}
