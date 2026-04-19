#include "key_runtime_integration_harness.h"

void key_runtime_integration_shadow_runtime_v2_apply_event(const runtime_event_t *event, uint16_t event_time) {
    runtime_v2_apply_event(event, event_time);
}
