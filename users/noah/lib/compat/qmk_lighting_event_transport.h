#pragma once
#include QMK_KEYBOARD_H
#include "../rgb/lighting/lighting_events.h"
#include "../rgb/lighting/lighting_event_sender.h"

#if NOAH_LIGHTING_EVENT_CAPTURE_ENABLE && defined(SPLIT_TRANSACTION_IDS_USER)
// Main-context init/reinit. History owner resets its local bank first and
// supplies the exact22-byte effective-profile identity. The selected role's
// packet arm is initialized before registration/readiness publication.
void noah_qmk_lighting_event_transport_init(const uint8_t profile[NOAH_LIGHTING_EVENT_PROFILE_BYTES]);
// One synchronous bounded RPC per master call; slave calls consume in main.
// next() offers one fair dirty group; it must not clear it before committed ACK.
typedef bool (*noah_lighting_event_next_fn)(void *context, noah_lighting_event_group_t *group);
bool noah_qmk_lighting_event_transport_step(uint32_t now, noah_lighting_event_next_fn next, noah_lighting_event_consume_fn consume, noah_lighting_event_reset_fn reset, noah_lighting_event_ack_fn ack, void *context);
#endif
