#pragma once
#include "../../profile/schema/profile_lighting_v1.h"
#include "lighting_lifetime.h"

// Describes an accepted rule without keeping source pointers or doing profile
// reader I/O. Fields are selected by registry metadata and timing modules.
typedef struct {
    noah_lighting_lifetime_parameters_t timing;
    uint8_t                             kind, thing_kind, domain, selected;
    bool                                retained, event;
} noah_lighting_history_rule_t;
// Prepare and lookup belong to the same validated program publication; reset
// the owning histories on publication before rebuilding this index.
// Prefix sums address every selected rule/thing. This is an index, not a
// reservation of that many retained records or an admission/storage policy.
typedef struct {
    uint16_t offsets[256];
} noah_lighting_history_index_t;
_Static_assert(sizeof(noah_lighting_history_index_t) == 512u, "u16 logical history index");
bool noah_lighting_history_rule(const noah_profile_lighting_v1_program_t *program, uint8_t rule, noah_lighting_history_rule_t *description);
bool noah_lighting_history_index_prepare(const noah_profile_lighting_v1_program_t *program, noah_lighting_history_index_t *index);
bool noah_lighting_history_index_lookup(const noah_profile_lighting_v1_program_t *program, const noah_lighting_history_index_t *index, uint8_t rule, uint8_t thing, uint16_t *slot, noah_lighting_lifetime_parameters_t *timing);
#define NOAH_LIGHTING_DESCRIBE_TIMING(id, name, layout, timed, role) void noah_lighting_timing_##name##_lifetime(const uint8_t *parameters, noah_lighting_lifetime_parameters_t *timing);
NOAH_LIGHTING_TIMING_ROWS(NOAH_LIGHTING_DESCRIBE_TIMING)
#undef NOAH_LIGHTING_DESCRIBE_TIMING
