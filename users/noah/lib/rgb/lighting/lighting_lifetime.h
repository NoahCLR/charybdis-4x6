#pragma once
#include <stdbool.h>
#include <stdint.h>

// Allocation-free transition logic. The history owner stores ordered batches
// and delivery references separately; this cell never owns a pointer or queue.
// Equal cells may be interned without changing subsequent transitions.
typedef struct { uint8_t bytes[3]; } noah_lighting_lifetime_t;
_Static_assert(sizeof(noah_lighting_lifetime_t) == 3u, "packed lifetime cell");
typedef struct { uint16_t show_after, linger, flash; } noah_lighting_lifetime_parameters_t;
enum { NOAH_LIGHTING_LIFETIME_IDLE, NOAH_LIGHTING_LIFETIME_MATCH, NOAH_LIGHTING_LIFETIME_DEPART, NOAH_LIGHTING_LIFETIME_EVENT, NOAH_LIGHTING_LIFETIME_NONQUALIFYING };
enum { NOAH_LIGHTING_BATCH_KEEP, NOAH_LIGHTING_BATCH_REPLACE, NOAH_LIGHTING_BATCH_RELEASE_RENDER };

// elapsed is the forward local observation delta, after the runtime's existing
// wrap/backward clock guard. Expiry releases only the rendering reference:
// an unacknowledged delivery reference must survive ordinary link failure.
uint8_t noah_lighting_lifetime_observe(noah_lighting_lifetime_t *cell, const noah_lighting_lifetime_parameters_t *parameters, uint32_t elapsed, uint8_t observation);
bool noah_lighting_lifetime_visible(const noah_lighting_lifetime_t *cell, const noah_lighting_lifetime_parameters_t *parameters);
uint32_t noah_lighting_lifetime_wake(const noah_lighting_lifetime_t *cell, const noah_lighting_lifetime_parameters_t *parameters);
