#include "macro_payload_internal.h"

bool macro_payload_validate(const char *payload) {
    macro_payload_ir_t ir = {0};

    return macro_payload_compile(payload, &ir);
}

bool macro_payload_play(const char *payload) {
    macro_payload_ir_t ir = {0};

    return macro_payload_compile(payload, &ir) && macro_payload_play_ir(&ir);
}
