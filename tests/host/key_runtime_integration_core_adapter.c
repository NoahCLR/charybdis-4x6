#include "key_runtime_integration_harness.h"

void key_runtime_integration_shadow_core_apply_event(const runtime_event_t *event, uint16_t event_time) {
    key_runtime_core_apply_event(event, event_time);
}
