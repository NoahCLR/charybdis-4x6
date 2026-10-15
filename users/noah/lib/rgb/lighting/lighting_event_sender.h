#pragma once
#include "lighting_event_receiver.h"

void noah_lighting_event_sender_begin(noah_lighting_event_link_t *, const uint8_t *profile);
size_t noah_lighting_event_sender_size(const noah_lighting_event_link_t *);
bool noah_lighting_event_sender_offer(noah_lighting_event_link_t *, const noah_lighting_event_group_t *);
// fresh_session seeds only currently LIVE qualified markers. ack clears
// only current records still equal to the captured occurrence/order/mask.
// Both callbacks are synchronous; borrowed packet bytes die on return.
typedef void (*noah_lighting_event_ack_fn)(void *, const uint8_t *group);
bool noah_lighting_event_sender_reply(noah_lighting_event_link_t *, const uint8_t *reply, size_t size, noah_lighting_event_reset_fn fresh_session, noah_lighting_event_ack_fn ack, void *);

void noah_lighting_snapshot_sender_begin(noah_lighting_event_link_t *, const uint8_t *profile);
void noah_lighting_snapshot_sender_prepare(noah_lighting_event_link_t *, const uint8_t *profile);
bool noah_lighting_snapshot_sender_offer(noah_lighting_event_link_t *, const uint8_t payload[55]);
