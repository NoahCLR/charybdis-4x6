#pragma once
#include "lighting_occurrence.h"
#include "lighting_batch.h"

#ifdef RGB_MATRIX_ENABLE
typedef struct {
    noah_lighting_leds_t leds;
    uint8_t              thing_kind, thing, slot, outcome, amount;
} noah_lighting_placed_subject_t;
enum { NOAH_LIGHTING_EVENT_BATCH_BYTES = 1u + 3u + KEY_ORIGIN_BITMAP_SIZE };
// Render placement is dead when synchronous resolution returns. Occurrence
// collection/qualification/retention cannot call the renderer or input
// producers; split callbacks stage into their separate mailbox. In particular
// resolved RGB facts/frame and the immutable outgoing packet are NOT arms.
// Facts and encoded event output deliberately coexist: a qualified event has
// exactly ONE captured subject, unlike a state thing's up-to-64-member batch.
typedef union {
    noah_lighting_placed_subject_t render[NOAH_LIGHTING_BATCH_SUBJECTS];
    struct {
        noah_lighting_event_group_t group;
        noah_lighting_facts_t       facts;
        uint8_t                     encoded[NOAH_LIGHTING_EVENT_BATCH_BYTES];
    } capture;
    uint8_t ordered_batch[NOAH_LIGHTING_BATCH_SNAPSHOT_BYTES];
} noah_lighting_workspace_t;
// Other compiler geometries use the larger arm and must be measured against
// their own policy; this assertion protects the recorded default configuration.
_Static_assert(MATRIX_ROWS *MATRIX_COLS != 60u || COMBO_BUFFER_LENGTH != 4u || sizeof(((noah_lighting_workspace_t *)0)->capture) <= sizeof(((noah_lighting_workspace_t *)0)->render), "default capture scratch fits existing render placement");
_Static_assert(NOAH_LIGHTING_BATCH_SUBJECTS == 64u, "matches existing per-rule placement bound");
extern noah_lighting_workspace_t noah_lighting_workspace;
#endif
