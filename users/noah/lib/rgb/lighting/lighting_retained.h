#pragma once
#include "lighting_batch_arena.h"
#include "lighting_lifetime.h"

// Independent rule/thing metadata, not a target reservation or fixed bank.
// Payloads can share full equality; qualification, age, order and dirty state
// never share merely because their final pixels or batch happen to coincide.
typedef struct {
    uint8_t bytes[18];
} noah_lighting_retained_t;
_Static_assert(sizeof(noah_lighting_retained_t) == 18u, "explicit retained record layout");
void                      noah_lighting_retained_init(noah_lighting_retained_t *cell, uint8_t rule, uint8_t thing);
bool                      noah_lighting_retained_observe(noah_lighting_retained_t *cell, noah_lighting_batch_arena_t *arena, const noah_lighting_lifetime_parameters_t *parameters, uint32_t elapsed, uint8_t observation, uint8_t kind, const uint8_t *batch, uint16_t size, uint64_t order, bool source_delivery);
bool                      noah_lighting_retained_visible(const noah_lighting_retained_t *cell, const noah_lighting_lifetime_parameters_t *parameters);
uint32_t                  noah_lighting_retained_wake(const noah_lighting_retained_t *cell, const noah_lighting_lifetime_parameters_t *parameters);
noah_lighting_batch_ref_t noah_lighting_retained_batch(const noah_lighting_retained_t *cell);
uint8_t                   noah_lighting_retained_rule(const noah_lighting_retained_t *cell);
uint8_t                   noah_lighting_retained_thing(const noah_lighting_retained_t *cell);
bool                      noah_lighting_retained_needed(const noah_lighting_retained_t *cell);
uint64_t                  noah_lighting_retained_order(const noah_lighting_retained_t *cell);
bool                      noah_lighting_retained_rendering(const noah_lighting_retained_t *cell);
bool                      noah_lighting_retained_dirty(const noah_lighting_retained_t *cell);
// Consumes one already owned reference, with no allocation or fallible callback
// after the group's commit boundary. Same-order retries release the transferred
// reference and preserve the existing age. Caller preflights every group cell.
void noah_lighting_retained_install_event(noah_lighting_retained_t *cell, noah_lighting_batch_arena_t *arena, const noah_lighting_lifetime_parameters_t *parameters, noah_lighting_batch_ref_t owned_reference, uint64_t order, bool source_delivery);
// Fresh same-profile transport session: after advancing local calendars, seed
// every still-live event (including show-after waiting), discard expired dirty
// delivery ownership, and preserve captured filters/order/age. Not a profile reset.
void noah_lighting_retained_seed(noah_lighting_retained_t *cell, noah_lighting_batch_arena_t *arena);
// An older packet ACK never clears a newer qualifying repeat. Expired dirty
// markers retain their payload until this ACK or a coordinated fresh reset.
bool noah_lighting_retained_ack(noah_lighting_retained_t *cell, noah_lighting_batch_arena_t *arena, uint64_t captured_order);
// Frozen frame metadata owns an independent payload reference, with no dirty
// delivery ownership. Its age/order survive current repeats, remaps and ACKs.
bool noah_lighting_retained_pin(const noah_lighting_retained_t *cell, noah_lighting_batch_arena_t *arena, noah_lighting_retained_t *snapshot);
void noah_lighting_retained_discard(noah_lighting_retained_t *cell, noah_lighting_batch_arena_t *arena);
