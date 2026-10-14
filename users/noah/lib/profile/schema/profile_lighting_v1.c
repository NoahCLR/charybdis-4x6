// ────────────────────────────────────────────────────────────────────────────
// Lighting Rules — RGB domain 0x10 version 5: strict validation
// ────────────────────────────────────────────────────────────────────────────
//
// Checks follow docs/architecture/lighting-rules-v1.md in payload order and
// report the offset tools/lighting_reference.py reports. Each step reads at
// most one bounded chunk, so a scan-context owner can validate a candidate
// without blocking.

#include "profile_lighting_v1.h"

#include <string.h>

enum {
    PHASE_UNINITIALIZED = 0u,
    PHASE_HEADER,
    PHASE_PALETTE,
    PHASE_THING_HEADER,
    PHASE_THING_RECORD,
    PHASE_KEY_SETS,
    PHASE_MAP_HEADER,
    PHASE_MAP_DENSE,
    PHASE_MAP_SPARSE,
    PHASE_RULE_HEADER,
    PHASE_RULE_PART,
    PHASE_EXTENSION_COUNT,
    PHASE_EXTENSION,
    PHASE_END,
    PHASE_COMPLETE,
    PHASE_REJECTED,
};

enum {
    COLOURS_PER_STEP = 6u,
    KEY_SETS_PER_STEP = 2u,
    DENSE_VALUES_PER_STEP = 20u,
    SPARSE_ENTRIES_PER_STEP = 10u,
    PART_READ_MAX = 2u + NOAH_PROFILE_LIGHTING_V1_PARAMETER_MAX,
    SINGLE_TARGET = 1u << 0,
    SINGLE_PAINT = 1u << 1,
    SINGLE_BACKGROUND = 1u << 2,
    // Ranks: conditions share one, then target, paint, background, timing, extension.
    RANK_NONE = 0u,
};

_Static_assert((unsigned)PART_READ_MAX <= (unsigned)NOAH_PROFILE_LIGHTING_V1_VALIDATION_READ_MAX, "a part must fit one validation read");
_Static_assert((unsigned)COLOURS_PER_STEP * 3u <= (unsigned)NOAH_PROFILE_LIGHTING_V1_VALIDATION_READ_MAX, "palette chunk exceeds a validation read");
_Static_assert((unsigned)KEY_SETS_PER_STEP * (unsigned)NOAH_PROFILE_LIGHTING_V1_KEY_SET_SIZE <= (unsigned)NOAH_PROFILE_LIGHTING_V1_VALIDATION_READ_MAX, "key-set chunk exceeds a validation read");
_Static_assert((unsigned)SPARSE_ENTRIES_PER_STEP * 2u <= (unsigned)NOAH_PROFILE_LIGHTING_V1_VALIDATION_READ_MAX, "sparse chunk exceeds a validation read");
_Static_assert((unsigned)NOAH_PROFILE_LIGHTING_V1_RECORD_MAX <= (unsigned)NOAH_PROFILE_LIGHTING_V1_VALIDATION_READ_MAX, "a thing record must fit one validation read");

// ─── Registry facts ─────────────────────────────────────────────────────────

static bool fact_layout(uint8_t id, uint32_t *layout) {
    (void)layout;
    switch (id) {
#define NOAH_LIGHTING_FACT_LAYOUT(fid, name, thing, params) \
    case fid:                                              \
        *layout = params;                                  \
        return true;
        NOAH_LIGHTING_FACT_ROWS(NOAH_LIGHTING_FACT_LAYOUT)
#undef NOAH_LIGHTING_FACT_LAYOUT
        default:
            return false;
    }
}

static bool target_layout(uint8_t id, uint32_t *layout) {
    (void)layout;
    switch (id) {
#define NOAH_LIGHTING_TARGET_LAYOUT(tid, name, params, place) \
    case tid:                                                 \
        *layout = params;                                     \
        return true;
        NOAH_LIGHTING_TARGET_ROWS(NOAH_LIGHTING_TARGET_LAYOUT)
#undef NOAH_LIGHTING_TARGET_LAYOUT
        default:
            return false;
    }
}

static bool paint_layout(uint8_t id, uint32_t *layout) {
    (void)layout;
    switch (id) {
#define NOAH_LIGHTING_PAINT_LAYOUT(pid, name, params, below, timed) \
    case pid:                                                       \
        *layout = params;                                           \
        return true;
        NOAH_LIGHTING_PAINT_ROWS(NOAH_LIGHTING_PAINT_LAYOUT)
#undef NOAH_LIGHTING_PAINT_LAYOUT
        default:
            return false;
    }
}

static bool timing_layout(uint8_t id, uint32_t *layout) {
    (void)layout;
    switch (id) {
#define NOAH_LIGHTING_TIMING_LAYOUT(gid, name, params, timed) \
    case gid:                                                 \
        *layout = params;                                     \
        return true;
        NOAH_LIGHTING_TIMING_ROWS(NOAH_LIGHTING_TIMING_LAYOUT)
#undef NOAH_LIGHTING_TIMING_LAYOUT
        default:
            return false;
    }
}

static bool extension_length(uint8_t id, uint8_t *length) {
    (void)length;
    switch (id) {
#define NOAH_LIGHTING_EXTENSION_LENGTH(xid, name, bytes) \
    case xid:                                            \
        *length = bytes;                                 \
        return true;
        NOAH_LIGHTING_DOMAIN_EXTENSION_ROWS(NOAH_LIGHTING_EXTENSION_LENGTH)
#undef NOAH_LIGHTING_EXTENSION_LENGTH
        default:
            return false;
    }
}

static bool supported(uint32_t mask, uint8_t id) {
    return id < 32u && (mask & (UINT32_C(1) << id)) != 0u;
}

bool noah_profile_lighting_v1_place_capable(const noah_profile_lighting_v1_limits_t *limits, uint8_t target) {
    return limits && supported(limits->targets, target) && supported(NOAH_LIGHTING_COMPILED_PLACES, target);
}

uint8_t noah_profile_lighting_v1_capability_page(const noah_profile_lighting_v1_limits_t *limits, uint8_t page, uint8_t *payload) {
    uint32_t registry;

    if (!limits || !payload || page >= NOAH_PROFILE_LIGHTING_V1_CAPABILITY_PAGES) return 0u;
    if (page == 0u) {
        payload[0]  = NOAH_PROFILE_LIGHTING_V1_CAPABILITY_LAYOUT;
        payload[1]  = NOAH_PROFILE_LIGHTING_V1_FORMAT_VERSION;
        payload[2]  = (uint8_t)(limits->max_payload_size & 0xffu);
        payload[3]  = (uint8_t)(limits->max_payload_size >> 8u);
        payload[4]  = NOAH_PROFILE_LIGHTING_V1_PALETTE_MAX;
        payload[5]  = NOAH_PROFILE_LIGHTING_V1_CONDITIONS_MAX;
        payload[6]  = NOAH_PROFILE_LIGHTING_V1_PARAMETER_MAX;
        payload[7]  = limits->tap_depth;
        payload[8]  = NOAH_PROFILE_LIGHTING_V1_LED_COUNT;
        payload[9]  = NOAH_PROFILE_LIGHTING_V1_CAPABILITY_REGISTRIES;
        return 10u;
    }
    page = (uint8_t)(page - 1u);
    switch (page / 2u) {
        case 0u: registry = limits->facts; break;
        case 1u: registry = limits->targets; break;
        case 2u: registry = limits->paints; break;
        case 3u: registry = limits->timings; break;
        case 4u: registry = limits->rule_extensions; break;
        case 5u: registry = limits->domain_extensions; break;
        default: registry = limits->things; break;
    }
    payload[0] = (uint8_t)(page / 2u);
    payload[1] = (uint8_t)((page % 2u) * 128u);
    for (uint8_t i = 0u; i < 16u; i++) payload[2u + i] = (page % 2u) == 0u && i < 4u ? (uint8_t)(registry >> (8u * i)) : 0u;
    return 18u;
}

uint8_t noah_profile_lighting_v1_colour_slots(uint8_t tap_depth, uint8_t thing) {
    return thing == NOAH_LIGHTING_THING_BEHAVIOUR ? (uint8_t)(3u + tap_depth - 1u) : 1u;
}

uint8_t noah_profile_lighting_v1_thing_count(const noah_profile_lighting_v1_limits_t *limits, uint8_t thing) {
    switch (thing) {
        case NOAH_LIGHTING_THING_LAYER:
            return limits->logical_layer_count;
        case NOAH_LIGHTING_THING_POINTING_MODE:
            return 32u;
        case NOAH_LIGHTING_THING_COMBO:
        case NOAH_LIGHTING_THING_BEHAVIOUR:
            return 128u;
        case NOAH_LIGHTING_THING_MACRO: return 128u;
        case NOAH_LIGHTING_THING_MODIFIER: return 8u;
        case NOAH_LIGHTING_THING_KEY: return 60u;
        default:
            return 0u;
    }
}

noah_profile_lighting_v1_limits_t noah_profile_lighting_v1_default_limits(void) {
    return (noah_profile_lighting_v1_limits_t){
        .max_payload_size    = NOAH_PROFILE_LIGHTING_V1_PAYLOAD_MAX,
        .tap_depth           = NOAH_PROFILE_TAP_DEPTH,
        .logical_layer_count = 16u,
        .maximum_brightness  = UINT8_MAX,
        .supported_mode_mask = UINT32_MAX,
        .facts               = NOAH_LIGHTING_COMPILED_FACTS,
        .targets             = NOAH_LIGHTING_COMPILED_TARGETS,
        .paints              = NOAH_LIGHTING_COMPILED_PAINTS,
        .timings             = NOAH_LIGHTING_COMPILED_TIMINGS,
        .rule_extensions     = NOAH_LIGHTING_COMPILED_RULE_EXTENSIONS,
        .domain_extensions   = NOAH_LIGHTING_COMPILED_DOMAIN_EXTENSIONS,
        .things              = NOAH_LIGHTING_COMPILED_THINGS,
    };
}

// ─── Steps ──────────────────────────────────────────────────────────────────

typedef noah_profile_lighting_v1_validation_t validation_t;

static noah_profile_lighting_v1_validation_result_t reject(validation_t *v, noah_profile_lighting_v1_error_t *error, noah_profile_lighting_v1_result_t code, size_t offset) {
    v->rejection = (noah_profile_lighting_v1_error_t){.code = code, .offset = offset};
    v->phase     = PHASE_REJECTED;
    v->result    = NOAH_PROFILE_LIGHTING_V1_VALIDATION_REJECTED;
    if (error) *error = v->rejection;
    return NOAH_PROFILE_LIGHTING_V1_VALIDATION_REJECTED;
}

static noah_profile_lighting_v1_validation_result_t progress(validation_t *v, uint8_t phase) {
    v->phase = phase;
    return NOAH_PROFILE_LIGHTING_V1_VALIDATION_IN_PROGRESS;
}

static bool read_at(const validation_t *v, uint16_t offset, uint8_t *target, size_t length) {
    return noah_profile_reader_read(&v->candidate.reader, v->candidate.base_offset + offset, target, length);
}

static uint16_t available(const validation_t *v, uint16_t offset) {
    return offset < v->candidate.byte_length ? (uint16_t)(v->candidate.byte_length - offset) : 0u;
}

// The section after the current one, entered with its item count.
static noah_profile_lighting_v1_validation_result_t enter_things(validation_t *v);
static noah_profile_lighting_v1_validation_result_t enter_key_sets(validation_t *v);
static noah_profile_lighting_v1_validation_result_t enter_maps(validation_t *v);
static noah_profile_lighting_v1_validation_result_t enter_rules(validation_t *v);

static noah_profile_lighting_v1_validation_result_t enter_things(validation_t *v) {
    v->candidate.things_offset = v->offset;
    v->remaining               = v->candidate.thing_table_count;
    v->previous                = 0u;
    v->rank                    = 0u; // no table yet
    return v->remaining ? progress(v, PHASE_THING_HEADER) : enter_key_sets(v);
}

static noah_profile_lighting_v1_validation_result_t enter_key_sets(validation_t *v) {
    v->candidate.key_sets_offset = v->offset;
    v->remaining                 = v->candidate.key_set_count;
    return v->remaining ? progress(v, PHASE_KEY_SETS) : enter_maps(v);
}

static noah_profile_lighting_v1_validation_result_t enter_maps(validation_t *v) {
    v->candidate.maps_offset = v->offset;
    v->remaining             = v->candidate.map_count;
    return v->remaining ? progress(v, PHASE_MAP_HEADER) : enter_rules(v);
}

static noah_profile_lighting_v1_validation_result_t enter_rules(validation_t *v) {
    v->candidate.rules_offset = v->offset;
    v->remaining              = v->candidate.rule_count;
    return progress(v, v->remaining ? PHASE_RULE_HEADER : PHASE_EXTENSION_COUNT);
}

static noah_profile_lighting_v1_result_t check_param(const validation_t *v, uint8_t type, const uint8_t *bytes) {
    const noah_profile_lighting_v1_limits_t *limits = v->limits;
    uint8_t                                  value  = bytes[0];

    switch (type) {
        case NOAH_LIGHTING_PARAM_LAYER_STATUS: return value <= 3u ? NOAH_PROFILE_LIGHTING_V1_OK : NOAH_PROFILE_LIGHTING_V1_INVALID_PARAMETER;
        case NOAH_LIGHTING_PARAM_MODE_STATUS: return value <= 1u ? NOAH_PROFILE_LIGHTING_V1_OK : NOAH_PROFILE_LIGHTING_V1_INVALID_PARAMETER;
        case NOAH_LIGHTING_PARAM_BEHAVIOUR_PHASE: return value <= 4u ? NOAH_PROFILE_LIGHTING_V1_OK : NOAH_PROFILE_LIGHTING_V1_INVALID_PARAMETER;
        case NOAH_LIGHTING_PARAM_TAP_SELECTOR: return value == 255u || (value > 0u && value <= limits->tap_depth) ? NOAH_PROFILE_LIGHTING_V1_OK : NOAH_PROFILE_LIGHTING_V1_INVALID_PARAMETER;
        case NOAH_LIGHTING_PARAM_KEY_SELECTOR: return value == 255u || value < 60u ? NOAH_PROFILE_LIGHTING_V1_OK : NOAH_PROFILE_LIGHTING_V1_INVALID_PARAMETER;
        case NOAH_LIGHTING_PARAM_MODIFIER: return value < 4u ? NOAH_PROFILE_LIGHTING_V1_OK : NOAH_PROFILE_LIGHTING_V1_INVALID_PARAMETER;
        case NOAH_LIGHTING_PARAM_SIDE: return value < 3u ? NOAH_PROFILE_LIGHTING_V1_OK : NOAH_PROFILE_LIGHTING_V1_INVALID_PARAMETER;
        case NOAH_LIGHTING_PARAM_MODIFIER_STATE: return value < 2u ? NOAH_PROFILE_LIGHTING_V1_OK : NOAH_PROFILE_LIGHTING_V1_INVALID_PARAMETER;
        case NOAH_LIGHTING_PARAM_HOST_LOCK: return value < 3u ? NOAH_PROFILE_LIGHTING_V1_OK : NOAH_PROFILE_LIGHTING_V1_INVALID_PARAMETER;
        case NOAH_LIGHTING_PARAM_POINTER_STATE: return value < 4u ? NOAH_PROFILE_LIGHTING_V1_OK : NOAH_PROFILE_LIGHTING_V1_INVALID_PARAMETER;
        case NOAH_LIGHTING_PARAM_LAYER_SELECTOR:
            return value == NOAH_PROFILE_LIGHTING_V1_ANY || value < limits->logical_layer_count ? NOAH_PROFILE_LIGHTING_V1_OK : NOAH_PROFILE_LIGHTING_V1_INVALID_PARAMETER;
        case NOAH_LIGHTING_PARAM_MODE_SELECTOR:
            return value == NOAH_PROFILE_LIGHTING_V1_ANY || (value < 32u && (limits->supported_mode_mask & (UINT32_C(1) << value)) != 0u) ? NOAH_PROFILE_LIGHTING_V1_OK : NOAH_PROFILE_LIGHTING_V1_INVALID_PARAMETER;
        case NOAH_LIGHTING_PARAM_THING_SELECTOR:
            return value == NOAH_PROFILE_LIGHTING_V1_ANY || value < 128u ? NOAH_PROFILE_LIGHTING_V1_OK : NOAH_PROFILE_LIGHTING_V1_INVALID_PARAMETER;
        case NOAH_LIGHTING_PARAM_COMBO_ROLE:
            return value == 0u || value == 1u || value == NOAH_PROFILE_LIGHTING_V1_ANY ? NOAH_PROFILE_LIGHTING_V1_OK : NOAH_PROFILE_LIGHTING_V1_INVALID_PARAMETER;
        case NOAH_LIGHTING_PARAM_BEHAVIOUR_STATES:
            return value != 0u && (value & 0xf0u) == 0u ? NOAH_PROFILE_LIGHTING_V1_OK : NOAH_PROFILE_LIGHTING_V1_INVALID_PARAMETER;
        case NOAH_LIGHTING_PARAM_BEHAVIOUR_SCOPE:
            return value <= NOAH_LIGHTING_SCOPE_PER_STATE ? NOAH_PROFILE_LIGHTING_V1_OK : NOAH_PROFILE_LIGHTING_V1_INVALID_PARAMETER;
        case NOAH_LIGHTING_PARAM_KEY_SET:
            return value < v->candidate.key_set_count ? NOAH_PROFILE_LIGHTING_V1_OK : NOAH_PROFILE_LIGHTING_V1_INVALID_REFERENCE;
        case NOAH_LIGHTING_PARAM_PALETTE_INDEX:
            return value < v->candidate.palette_count ? NOAH_PROFILE_LIGHTING_V1_OK : NOAH_PROFILE_LIGHTING_V1_INVALID_REFERENCE;
        case NOAH_LIGHTING_PARAM_MAP:
            return value < v->candidate.map_count ? NOAH_PROFILE_LIGHTING_V1_OK : NOAH_PROFILE_LIGHTING_V1_INVALID_REFERENCE;
        case NOAH_LIGHTING_PARAM_LEVEL:
            return NOAH_PROFILE_LIGHTING_V1_OK;
        case NOAH_LIGHTING_PARAM_MILLISECONDS:
            return (bytes[0] | bytes[1]) != 0u ? NOAH_PROFILE_LIGHTING_V1_OK : NOAH_PROFILE_LIGHTING_V1_INVALID_PARAMETER;
        default:
            return NOAH_PROFILE_LIGHTING_V1_INVALID_PARAMETER;
    }
}

static bool map_value_valid(const validation_t *v, uint8_t value) {
    uint32_t layout;
    uint8_t  kind;

    if (value < NOAH_PROFILE_LIGHTING_V1_MAP_PAINT) return value < v->candidate.palette_count;
    kind = (uint8_t)(value - NOAH_PROFILE_LIGHTING_V1_MAP_PAINT);
    return kind != NOAH_LIGHTING_PAINT_ID_MAP && supported(v->limits->paints, kind) && paint_layout(kind, &layout) && layout == NOAH_LIGHTING_PARAMS0;
}

static noah_profile_lighting_v1_validation_result_t step_header(validation_t *v, noah_profile_lighting_v1_error_t *error) {
    uint8_t bytes[NOAH_PROFILE_LIGHTING_V1_HEADER_SIZE];

    if (!read_at(v, 0u, bytes, sizeof(bytes))) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_READ_ERROR, 0u);
    if (bytes[0] != NOAH_PROFILE_LIGHTING_V1_FORMAT_VERSION) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_INVALID_VERSION, 0u);
    if (bytes[1] != 0u) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_RESERVED_BITS, 1u);
    if (bytes[2] != NOAH_PROFILE_LIGHTING_V1_LED_COUNT) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_INCOMPATIBLE_GEOMETRY, 2u);
    if (bytes[3] > NOAH_PROFILE_LIGHTING_V1_PALETTE_MAX) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_CAPACITY_EXCEEDED, 3u);
    v->candidate.palette_count     = bytes[3];
    v->candidate.thing_table_count = bytes[4];
    v->candidate.key_set_count     = bytes[5];
    v->candidate.map_count         = bytes[6];
    v->candidate.rule_count        = bytes[7];
    v->offset                      = NOAH_PROFILE_LIGHTING_V1_HEADER_SIZE;
    v->remaining                   = v->candidate.palette_count;
    return v->remaining ? progress(v, PHASE_PALETTE) : enter_things(v);
}

static noah_profile_lighting_v1_validation_result_t step_palette(validation_t *v, noah_profile_lighting_v1_error_t *error) {
    uint8_t bytes[COLOURS_PER_STEP * 3u];
    uint8_t count = v->remaining < COLOURS_PER_STEP ? v->remaining : COLOURS_PER_STEP;
    uint8_t whole = (uint8_t)(available(v, v->offset) / 3u);

    if (whole < count) count = whole;
    if (count && !read_at(v, v->offset, bytes, (size_t)count * 3u)) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_READ_ERROR, v->offset);
    for (uint8_t index = 0u; index < count; index++) {
        if (bytes[index * 3u + 2u] > v->limits->maximum_brightness) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_BRIGHTNESS_EXCEEDED, (size_t)v->offset + 2u);
        v->offset = (uint16_t)(v->offset + 3u);
        v->remaining--;
    }
    if (v->remaining && count < COLOURS_PER_STEP && available(v, v->offset) < 3u) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_TRUNCATED, v->offset);
    return v->remaining ? progress(v, PHASE_PALETTE) : enter_things(v);
}

static noah_profile_lighting_v1_validation_result_t step_thing_header(validation_t *v, noah_profile_lighting_v1_error_t *error) {
    uint8_t bytes[2];
    uint8_t kind;

    if (available(v, v->offset) < 2u) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_TRUNCATED, v->offset);
    if (!read_at(v, v->offset, bytes, sizeof(bytes))) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_READ_ERROR, v->offset);
    kind = bytes[0];
    if (kind == NOAH_LIGHTING_THING_BOARD || kind >= NOAH_LIGHTING_THING_KIND_LIMIT || !supported(v->limits->things, kind)) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_UNSUPPORTED_KIND, v->offset);
    if (v->rank && kind <= v->previous) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_NONCANONICAL_ORDER, v->offset);
    if (bytes[1] > noah_profile_lighting_v1_thing_count(v->limits, kind)) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_CAPACITY_EXCEEDED, (size_t)v->offset + 1u);
    v->rank        = 1u;
    v->previous    = kind;
    v->record_size = (uint8_t)(1u + noah_profile_lighting_v1_colour_slots(v->limits->tap_depth, kind));
    v->sub         = bytes[1];
    v->encoding    = 1u; // the next record is the default
    v->offset      = (uint16_t)(v->offset + 2u);
    return progress(v, PHASE_THING_RECORD);
}

// As many whole records as one read holds, each checked in order.
static noah_profile_lighting_v1_validation_result_t step_thing_record(validation_t *v, noah_profile_lighting_v1_error_t *error) {
    uint8_t  bytes[NOAH_PROFILE_LIGHTING_V1_VALIDATION_READ_MAX];
    uint16_t records = (uint16_t)(v->sub + (v->encoding != 0u ? 1u : 0u));
    uint16_t fit     = (uint16_t)(sizeof(bytes) / v->record_size);
    uint16_t whole   = (uint16_t)(available(v, v->offset) / v->record_size);

    if (v->record_size > NOAH_PROFILE_LIGHTING_V1_RECORD_MAX) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_INVALID_ARGUMENT, v->offset);
    if (whole == 0u) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_TRUNCATED, v->offset);
    if (records > fit) records = fit;
    if (records > whole) records = whole;
    if (!read_at(v, v->offset, bytes, (size_t)records * v->record_size)) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_READ_ERROR, v->offset);
    for (uint16_t record = 0u; record < records; record++) {
        const uint8_t *fields     = &bytes[record * v->record_size];
        bool           is_default = v->encoding != 0u;

        if (!noah_profile_lighting_v1_place_capable(v->limits, fields[0]) && !(fields[0] == NOAH_PROFILE_LIGHTING_V1_INHERIT && !is_default)) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_INVALID_REFERENCE, v->offset);
        for (uint8_t field = 1u; field < v->record_size; field++) {
            uint8_t colour = fields[field];
            if (colour == NOAH_PROFILE_LIGHTING_V1_NONE || (colour == NOAH_PROFILE_LIGHTING_V1_INHERIT && !is_default) || colour < v->candidate.palette_count) continue;
            return reject(v, error, NOAH_PROFILE_LIGHTING_V1_INVALID_REFERENCE, (size_t)v->offset + field);
        }
        v->offset = (uint16_t)(v->offset + v->record_size);
        if (is_default) {
            v->encoding = 0u;
        } else {
            v->sub--;
        }
    }
    if (v->sub || v->encoding) return progress(v, PHASE_THING_RECORD);
    v->remaining--;
    return v->remaining ? progress(v, PHASE_THING_HEADER) : enter_key_sets(v);
}

static noah_profile_lighting_v1_validation_result_t step_key_sets(validation_t *v, noah_profile_lighting_v1_error_t *error) {
    uint8_t bytes[KEY_SETS_PER_STEP * NOAH_PROFILE_LIGHTING_V1_KEY_SET_SIZE];
    uint8_t count = v->remaining < KEY_SETS_PER_STEP ? v->remaining : KEY_SETS_PER_STEP;
    uint8_t whole = (uint8_t)(available(v, v->offset) / NOAH_PROFILE_LIGHTING_V1_KEY_SET_SIZE);

    if (whole < count) count = whole;
    if (count && !read_at(v, v->offset, bytes, (size_t)count * NOAH_PROFILE_LIGHTING_V1_KEY_SET_SIZE)) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_READ_ERROR, v->offset);
    for (uint8_t index = 0u; index < count; index++) {
        if ((bytes[index * NOAH_PROFILE_LIGHTING_V1_KEY_SET_SIZE + 7u] & 0xfcu) != 0u) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_RESERVED_BITS, (size_t)v->offset + 7u);
        v->offset = (uint16_t)(v->offset + NOAH_PROFILE_LIGHTING_V1_KEY_SET_SIZE);
        v->remaining--;
    }
    if (v->remaining && count < KEY_SETS_PER_STEP && available(v, v->offset) < NOAH_PROFILE_LIGHTING_V1_KEY_SET_SIZE) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_TRUNCATED, v->offset);
    return v->remaining ? progress(v, PHASE_KEY_SETS) : enter_maps(v);
}

static noah_profile_lighting_v1_validation_result_t next_map(validation_t *v) {
    v->remaining--;
    return v->remaining ? progress(v, PHASE_MAP_HEADER) : enter_rules(v);
}

// Checks `count` dense values read at v->offset and moves past them.
static noah_profile_lighting_v1_validation_result_t check_dense(validation_t *v, noah_profile_lighting_v1_error_t *error, const uint8_t *bytes, uint8_t count) {
    for (uint8_t index = 0u; index < count; index++) {
        if (!map_value_valid(v, bytes[index])) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_INVALID_REFERENCE, (size_t)v->offset + index);
        if (bytes[index] != NOAH_PROFILE_LIGHTING_V1_MAP_CLEAR) v->painted++;
    }
    v->offset = (uint16_t)(v->offset + count);
    v->sub    = (uint8_t)(v->sub - count);
    if (v->sub) return progress(v, PHASE_MAP_DENSE);
    if (v->painted <= NOAH_PROFILE_LIGHTING_V1_SPARSE_MAX) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_NONCANONICAL_ENCODING, v->item_end);
    return next_map(v);
}

// Checks `count` sparse entries read at v->offset and moves past them.
static noah_profile_lighting_v1_validation_result_t check_sparse(validation_t *v, noah_profile_lighting_v1_error_t *error, const uint8_t *bytes, uint8_t count) {
    for (uint8_t index = 0u; index < count; index++) {
        uint8_t  led   = bytes[index * 2u];
        uint8_t  value = bytes[index * 2u + 1u];
        uint16_t at    = (uint16_t)(v->offset + index * 2u);

        if (led >= NOAH_PROFILE_LIGHTING_V1_LED_COUNT) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_INVALID_REFERENCE, at);
        if (v->rank && led <= v->previous) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_NONCANONICAL_ORDER, at);
        v->rank     = 1u;
        v->previous = led;
        if (value == NOAH_PROFILE_LIGHTING_V1_MAP_CLEAR) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_NONCANONICAL_ENCODING, (size_t)at + 1u);
        if (!map_value_valid(v, value)) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_INVALID_REFERENCE, (size_t)at + 1u);
    }
    v->offset = (uint16_t)(v->offset + count * 2u);
    v->sub    = (uint8_t)(v->sub - count);
    return v->sub ? progress(v, PHASE_MAP_SPARSE) : next_map(v);
}

// The encoding byte, read with the map's first values.
static noah_profile_lighting_v1_validation_result_t step_map_header(validation_t *v, noah_profile_lighting_v1_error_t *error) {
    uint8_t  bytes[NOAH_PROFILE_LIGHTING_V1_VALIDATION_READ_MAX];
    uint16_t left = available(v, v->offset);
    uint8_t  count;

    if (left < 1u) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_TRUNCATED, v->offset);
    count = left < sizeof(bytes) ? (uint8_t)left : (uint8_t)sizeof(bytes);
    if (!read_at(v, v->offset, bytes, count)) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_READ_ERROR, v->offset);
    if (bytes[0] == 0u) {
        if (left < NOAH_PROFILE_LIGHTING_V1_DENSE_MAP_SIZE) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_TRUNCATED, v->offset);
        v->item_end = v->offset; // the map's start, for the canonical check
        v->sub      = NOAH_PROFILE_LIGHTING_V1_LED_COUNT;
        v->painted  = 0u;
        v->offset   = (uint16_t)(v->offset + 1u);
        return check_dense(v, error, &bytes[1], (uint8_t)(count - 1u));
    }
    if (bytes[0] == 1u) {
        uint8_t entries;

        if (count < 2u) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_TRUNCATED, v->offset);
        if (bytes[1] > NOAH_PROFILE_LIGHTING_V1_SPARSE_MAX) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_NONCANONICAL_ENCODING, (size_t)v->offset + 1u);
        if (left < 2u + 2u * (uint16_t)bytes[1]) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_TRUNCATED, v->offset);
        v->sub      = bytes[1];
        v->previous = 0u;
        v->rank     = 0u; // no LED yet
        v->offset   = (uint16_t)(v->offset + 2u);
        if (!v->sub) return next_map(v);
        entries = (uint8_t)((count - 2u) / 2u);
        return check_sparse(v, error, &bytes[2], entries < v->sub ? entries : v->sub);
    }
    return reject(v, error, NOAH_PROFILE_LIGHTING_V1_INVALID_ENUM, v->offset);
}

static noah_profile_lighting_v1_validation_result_t step_map_dense(validation_t *v, noah_profile_lighting_v1_error_t *error) {
    uint8_t bytes[DENSE_VALUES_PER_STEP];
    uint8_t count = v->sub < DENSE_VALUES_PER_STEP ? v->sub : DENSE_VALUES_PER_STEP;

    if (!read_at(v, v->offset, bytes, count)) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_READ_ERROR, v->offset);
    return check_dense(v, error, bytes, count);
}

static noah_profile_lighting_v1_validation_result_t step_map_sparse(validation_t *v, noah_profile_lighting_v1_error_t *error) {
    uint8_t bytes[SPARSE_ENTRIES_PER_STEP * 2u];
    uint8_t count = v->sub < SPARSE_ENTRIES_PER_STEP ? v->sub : SPARSE_ENTRIES_PER_STEP;

    if (!read_at(v, v->offset, bytes, (size_t)count * 2u)) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_READ_ERROR, v->offset);
    return check_sparse(v, error, bytes, count);
}

static uint8_t slot_rank(uint8_t slot) {
    return slot <= NOAH_PROFILE_LIGHTING_V1_SLOT_NEGATED ? 0u : (uint8_t)(slot - 1u);
}

static noah_profile_lighting_v1_validation_result_t finish_rule(validation_t *v, noah_profile_lighting_v1_error_t *error) {
    uint16_t start = (uint16_t)(v->item_end - v->sub - 1u);

    if ((v->singles & (SINGLE_TARGET | SINGLE_PAINT)) != (SINGLE_TARGET | SINGLE_PAINT)) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_INVALID_STRUCTURE, start);
    v->remaining--;
    return progress(v, v->remaining ? PHASE_RULE_HEADER : PHASE_EXTENSION_COUNT);
}

// Checks the part at v->offset, whose bytes (at least the whole part, or
// PART_READ_MAX of them) start at `bytes`, and moves past it.
static noah_profile_lighting_v1_validation_result_t check_part(validation_t *v, noah_profile_lighting_v1_error_t *error, const uint8_t *bytes) {
    uint16_t left = (uint16_t)(v->item_end - v->offset);
    uint16_t at   = v->offset;
    uint32_t layout;
    uint8_t  slot, length, kind;
    bool     known;
    uint32_t mask;

    slot   = (uint8_t)(bytes[0] >> 5u);
    length = (uint8_t)(bytes[0] & 0x1fu);
    kind   = bytes[1];
    if (slot == NOAH_PROFILE_LIGHTING_V1_SLOT_RESERVED) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_INVALID_STRUCTURE, at);
    if (slot_rank(slot) < v->rank) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_NONCANONICAL_ORDER, at);
    v->rank = slot_rank(slot);
    switch (slot) {
        case NOAH_PROFILE_LIGHTING_V1_SLOT_CONDITION:
        case NOAH_PROFILE_LIGHTING_V1_SLOT_NEGATED:
            if (++v->conditions > NOAH_PROFILE_LIGHTING_V1_CONDITIONS_MAX) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_CAPACITY_EXCEEDED, at);
            mask  = v->limits->facts;
            known = fact_layout(kind, &layout);
            break;
        case NOAH_PROFILE_LIGHTING_V1_SLOT_TARGET:
            if (v->singles & SINGLE_TARGET) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_INVALID_STRUCTURE, at);
            v->singles |= SINGLE_TARGET;
            mask  = v->limits->targets;
            known = target_layout(kind, &layout);
            break;
        case NOAH_PROFILE_LIGHTING_V1_SLOT_PAINT:
        case NOAH_PROFILE_LIGHTING_V1_SLOT_BACKGROUND: {
            uint8_t single = slot == NOAH_PROFILE_LIGHTING_V1_SLOT_PAINT ? SINGLE_PAINT : SINGLE_BACKGROUND;
            if (v->singles & single) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_INVALID_STRUCTURE, at);
            v->singles |= single;
            mask  = v->limits->paints;
            known = paint_layout(kind, &layout);
            break;
        }
        case NOAH_PROFILE_LIGHTING_V1_SLOT_TIMING:
            if (++v->timings > NOAH_PROFILE_LIGHTING_V1_TIMINGS_MAX) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_CAPACITY_EXCEEDED, at);
            if ((v->painted & 1u) && kind <= v->previous_timing) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_NONCANONICAL_ORDER, (size_t)at + 1u);
            v->painted |= 1u;
            v->previous_timing = kind;
            mask               = v->limits->timings;
            known              = timing_layout(kind, &layout);
            break;
        default: // extension
            if ((v->painted & 2u) && kind <= v->previous_extension) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_NONCANONICAL_ORDER, (size_t)at + 1u);
            v->painted |= 2u;
            v->previous_extension = kind;
            mask                  = v->limits->rule_extensions;
            known                 = false; // version 5 defines no rule extension
            layout                = 0u;
            break;
    }
    if (!known || !supported(mask, kind)) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_UNSUPPORTED_KIND, (size_t)at + 1u);
    if (length != noah_lighting_param_layout_length(layout)) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_INVALID_LENGTH, at);
    if (2u + (uint16_t)length > left) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_INVALID_LENGTH, at);
    {
        uint8_t param_at = 2u;
        for (; layout != 0u; layout >>= 5u) {
            uint8_t                           type   = (uint8_t)(layout & 0x1fu);
            noah_profile_lighting_v1_result_t result = check_param(v, type, &bytes[param_at]);
            if (result != NOAH_PROFILE_LIGHTING_V1_OK) return reject(v, error, result, (size_t)at + param_at);
            param_at = (uint8_t)(param_at + noah_lighting_param_size(type));
        }
    }
    v->offset = (uint16_t)(at + 2u + length);
    return NOAH_PROFILE_LIGHTING_V1_VALIDATION_IN_PROGRESS;
}

// Checks every whole part of the current rule that `count` bytes read at
// `base` hold, from v->offset on, then finishes the rule or waits for the
// next step. A part cut off by the end of the read is read again whole.
static noah_profile_lighting_v1_validation_result_t check_parts(validation_t *v, noah_profile_lighting_v1_error_t *error, const uint8_t *bytes, uint16_t base, uint16_t count) {
    while (v->offset < v->item_end) {
        uint16_t pos  = (uint16_t)(v->offset - base);
        uint16_t left = (uint16_t)(v->item_end - v->offset);

        if (left < 2u) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_INVALID_LENGTH, v->offset);
        if (pos + 2u > count) break;
        {
            uint16_t whole = (uint16_t)(2u + (bytes[pos] & 0x1fu));
            // Without the whole part in hand, check it from a fresh read,
            // unless this read already holds all it could.
            if (pos + whole > count && pos != 0u) break;
        }
        if (check_part(v, error, &bytes[pos]) == NOAH_PROFILE_LIGHTING_V1_VALIDATION_REJECTED) return NOAH_PROFILE_LIGHTING_V1_VALIDATION_REJECTED;
    }
    return v->offset >= v->item_end ? finish_rule(v, error) : progress(v, PHASE_RULE_PART);
}

static noah_profile_lighting_v1_validation_result_t step_rule_part(validation_t *v, noah_profile_lighting_v1_error_t *error) {
    uint8_t  bytes[NOAH_PROFILE_LIGHTING_V1_VALIDATION_READ_MAX];
    uint16_t left = (uint16_t)(v->item_end - v->offset);
    uint16_t count;

    if (v->offset >= v->item_end) return finish_rule(v, error);
    if (left < 2u) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_INVALID_LENGTH, v->offset);
    count = left < sizeof(bytes) ? left : (uint16_t)sizeof(bytes);
    if (!read_at(v, v->offset, bytes, count)) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_READ_ERROR, v->offset);
    return check_parts(v, error, bytes, v->offset, count);
}

static noah_profile_lighting_v1_validation_result_t step_rule_header(validation_t *v, noah_profile_lighting_v1_error_t *error) {
    uint8_t  bytes[NOAH_PROFILE_LIGHTING_V1_VALIDATION_READ_MAX];
    uint16_t left = available(v, v->offset);
    uint16_t base = v->offset;
    uint16_t count;

    if (left < 1u) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_TRUNCATED, v->offset);
    count = left < sizeof(bytes) ? left : (uint16_t)sizeof(bytes);
    if (!read_at(v, v->offset, bytes, count)) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_READ_ERROR, v->offset);
    if (bytes[0] < NOAH_PROFILE_LIGHTING_V1_RULE_MIN_SIZE) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_INVALID_LENGTH, v->offset);
    if (left < 1u + (uint16_t)bytes[0]) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_TRUNCATED, v->offset);
    if ((bytes[1] & (uint8_t)~NOAH_PROFILE_LIGHTING_V1_RULE_DISABLED) != 0u) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_RESERVED_BITS, (size_t)v->offset + 1u);
    v->item_end           = (uint16_t)(v->offset + 1u + bytes[0]);
    v->previous           = (uint8_t)0u; // rule start is item_end - size - 1; kept below
    v->rank               = 0u;
    v->conditions         = 0u;
    v->timings            = 0u;
    v->singles            = 0u;
    v->previous_timing    = 0u;
    v->previous_extension = 0u;
    v->painted            = 0u; // bit 0: a timing seen, bit 1: an extension seen
    v->sub                = bytes[0];
    v->offset             = (uint16_t)(v->offset + 3u);
    // The same read carries the rule's first parts.
    return check_parts(v, error, bytes, base, (uint16_t)(count < 1u + (uint16_t)bytes[0] ? count : 1u + bytes[0]));
}

static noah_profile_lighting_v1_validation_result_t step_extension_count(validation_t *v, noah_profile_lighting_v1_error_t *error) {
    uint8_t count;

    v->candidate.extensions_offset = v->offset;
    if (available(v, v->offset) < 1u) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_TRUNCATED, v->offset);
    if (!read_at(v, v->offset, &count, 1u)) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_READ_ERROR, v->offset);
    v->offset    = (uint16_t)(v->offset + 1u);
    v->remaining = count;
    v->rank      = 0u;
    v->previous  = 0u;
    return progress(v, count ? PHASE_EXTENSION : PHASE_END);
}

static noah_profile_lighting_v1_validation_result_t step_extension(validation_t *v, noah_profile_lighting_v1_error_t *error) {
    uint8_t  bytes[PART_READ_MAX];
    uint16_t left = available(v, v->offset);
    uint16_t at   = v->offset;
    uint8_t  count, kind, declared;

    if (left < 2u) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_TRUNCATED, at);
    count = left < sizeof(bytes) ? (uint8_t)left : (uint8_t)sizeof(bytes);
    if (!read_at(v, at, bytes, count)) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_READ_ERROR, at);
    kind = bytes[0];
    if (!supported(v->limits->domain_extensions, kind) || !extension_length(kind, &declared)) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_UNSUPPORTED_KIND, at);
    if (v->rank && kind <= v->previous) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_NONCANONICAL_ORDER, at);
    v->rank     = 1u;
    v->previous = kind;
    if (bytes[1] != declared) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_INVALID_LENGTH, (size_t)at + 1u);
    if (left < 2u + (uint16_t)declared) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_TRUNCATED, at);
    if (kind == NOAH_LIGHTING_EXTENSION_ID_AUTOMOUSE_FADE) {
        if ((bytes[2] & (uint8_t)~NOAH_PROFILE_LIGHTING_V1_FADE_ON) != 0u) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_RESERVED_BITS, (size_t)at + 2u);
        if (bytes[3] > 2u) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_INVALID_PARAMETER, (size_t)at + 3u);
        if (bytes[4] >= v->candidate.palette_count) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_INVALID_REFERENCE, (size_t)at + 4u);
        if (bytes[5] > v->candidate.rule_count) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_INVALID_REFERENCE, (size_t)at + 5u);
    } else if (kind == NOAH_LIGHTING_EXTENSION_ID_TAP_COMMIT_FLASH) {
        if (bytes[2] != 1u) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_INVALID_PARAMETER, (size_t)at + 2u);
    }
    v->offset = (uint16_t)(at + 2u + declared);
    v->remaining--;
    return progress(v, v->remaining ? PHASE_EXTENSION : PHASE_END);
}

// ─── Public interface ───────────────────────────────────────────────────────

noah_profile_lighting_v1_validation_result_t noah_profile_lighting_v1_validation_begin(validation_t *validation, const noah_profile_reader_t *reader, size_t base_offset, size_t length, const noah_profile_lighting_v1_limits_t *limits, noah_profile_lighting_v1_error_t *error) {
    if (!validation) return NOAH_PROFILE_LIGHTING_V1_VALIDATION_REJECTED;
    memset(validation, 0, sizeof(*validation));
    if (!reader || !limits || limits->tap_depth == 0u || limits->max_payload_size == 0u || limits->max_payload_size > NOAH_PROFILE_LIGHTING_V1_PAYLOAD_MAX) return reject(validation, error, NOAH_PROFILE_LIGHTING_V1_INVALID_ARGUMENT, 0u);
    validation->candidate.reader      = *reader;
    validation->candidate.base_offset = base_offset;
    validation->limits                = limits;
    validation->candidate.tap_depth   = limits->tap_depth;
    if (length > limits->max_payload_size) return reject(validation, error, NOAH_PROFILE_LIGHTING_V1_CAPACITY_EXCEEDED, limits->max_payload_size);
    if (length < NOAH_PROFILE_LIGHTING_V1_HEADER_SIZE) return reject(validation, error, NOAH_PROFILE_LIGHTING_V1_TRUNCATED, 0u);
    validation->candidate.byte_length = (uint16_t)length;
    validation->phase                 = PHASE_HEADER;
    validation->result                = NOAH_PROFILE_LIGHTING_V1_VALIDATION_IN_PROGRESS;
    return NOAH_PROFILE_LIGHTING_V1_VALIDATION_IN_PROGRESS;
}

noah_profile_lighting_v1_validation_result_t noah_profile_lighting_v1_validation_step(validation_t *v, noah_profile_lighting_v1_error_t *error) {
    if (!v) return NOAH_PROFILE_LIGHTING_V1_VALIDATION_REJECTED;
    switch (v->phase) {
        case PHASE_HEADER:
            return step_header(v, error);
        case PHASE_PALETTE:
            return step_palette(v, error);
        case PHASE_THING_HEADER:
            return step_thing_header(v, error);
        case PHASE_THING_RECORD:
            return step_thing_record(v, error);
        case PHASE_KEY_SETS:
            return step_key_sets(v, error);
        case PHASE_MAP_HEADER:
            return step_map_header(v, error);
        case PHASE_MAP_DENSE:
            return step_map_dense(v, error);
        case PHASE_MAP_SPARSE:
            return step_map_sparse(v, error);
        case PHASE_RULE_HEADER:
            return step_rule_header(v, error);
        case PHASE_RULE_PART:
            return step_rule_part(v, error);
        case PHASE_EXTENSION_COUNT:
            return step_extension_count(v, error);
        case PHASE_EXTENSION:
            return step_extension(v, error);
        case PHASE_END:
            if (v->offset != v->candidate.byte_length) return reject(v, error, NOAH_PROFILE_LIGHTING_V1_TRAILING_BYTES, v->offset);
            v->phase  = PHASE_COMPLETE;
            v->result = NOAH_PROFILE_LIGHTING_V1_VALIDATION_VALID;
            return NOAH_PROFILE_LIGHTING_V1_VALIDATION_VALID;
        case PHASE_COMPLETE:
            return NOAH_PROFILE_LIGHTING_V1_VALIDATION_VALID;
        case PHASE_REJECTED:
            if (error) *error = v->rejection;
            return NOAH_PROFILE_LIGHTING_V1_VALIDATION_REJECTED;
        default:
            return reject(v, error, NOAH_PROFILE_LIGHTING_V1_INVALID_ARGUMENT, 0u);
    }
}

noah_profile_lighting_v1_validation_result_t noah_profile_lighting_v1_validation_view(const validation_t *v, noah_profile_lighting_v1_view_t *view, noah_profile_lighting_v1_error_t *error) {
    if (!v || !view) return NOAH_PROFILE_LIGHTING_V1_VALIDATION_REJECTED;
    if (v->phase == PHASE_REJECTED) {
        if (error) *error = v->rejection;
        return NOAH_PROFILE_LIGHTING_V1_VALIDATION_REJECTED;
    }
    if (v->phase != PHASE_COMPLETE) return NOAH_PROFILE_LIGHTING_V1_VALIDATION_IN_PROGRESS;
    *view = v->candidate;
    return NOAH_PROFILE_LIGHTING_V1_VALIDATION_VALID;
}

noah_profile_lighting_v1_result_t noah_profile_lighting_v1_decode_reader(const noah_profile_reader_t *reader, size_t base_offset, size_t length, const noah_profile_lighting_v1_limits_t *limits, noah_profile_lighting_v1_view_t *view, noah_profile_lighting_v1_error_t *error) {
    noah_profile_lighting_v1_validation_t        validation;
    noah_profile_lighting_v1_error_t             local = {0};
    noah_profile_lighting_v1_validation_result_t result;

    if (!view) return NOAH_PROFILE_LIGHTING_V1_INVALID_ARGUMENT;
    result = noah_profile_lighting_v1_validation_begin(&validation, reader, base_offset, length, limits, &local);
    while (result == NOAH_PROFILE_LIGHTING_V1_VALIDATION_IN_PROGRESS) {
        result = noah_profile_lighting_v1_validation_step(&validation, &local);
    }
    if (result != NOAH_PROFILE_LIGHTING_V1_VALIDATION_VALID) {
        if (error) *error = local;
        return local.code == NOAH_PROFILE_LIGHTING_V1_OK ? NOAH_PROFILE_LIGHTING_V1_INVALID_ARGUMENT : local.code;
    }
    (void)noah_profile_lighting_v1_validation_view(&validation, view, &local);
    if (error) *error = (noah_profile_lighting_v1_error_t){0};
    return NOAH_PROFILE_LIGHTING_V1_OK;
}

noah_profile_lighting_v1_result_t noah_profile_lighting_v1_decode(const uint8_t *bytes, size_t length, const noah_profile_lighting_v1_limits_t *limits, noah_profile_lighting_v1_view_t *view, noah_profile_lighting_v1_error_t *error) {
    noah_profile_reader_t reader = noah_profile_reader_from_memory(bytes, length);
    return noah_profile_lighting_v1_decode_reader(&reader, 0u, length, limits, view, error);
}

// ─── The decoded program ────────────────────────────────────────────────────

enum { NOAH_PROFILE_LIGHTING_V1_LOAD_CHUNK = 32u };

bool noah_profile_lighting_v1_program_load(const noah_profile_lighting_v1_view_t *view, noah_profile_lighting_v1_program_t *program) {
    uint16_t offset;
    uint8_t *bytes;

    if (!view || !program) return false;
    program->valid = false;
    if (view->byte_length > sizeof(program->bytes)) return false;
    // Storage readers bound each read, so copy 32 bytes at a time.
    for (uint16_t at = 0u; at < view->byte_length; at = (uint16_t)(at + NOAH_PROFILE_LIGHTING_V1_LOAD_CHUNK)) {
        uint16_t left  = (uint16_t)(view->byte_length - at);
        uint16_t chunk = left < NOAH_PROFILE_LIGHTING_V1_LOAD_CHUNK ? left : (uint16_t)NOAH_PROFILE_LIGHTING_V1_LOAD_CHUNK;
        if (!noah_profile_reader_read(&view->reader, view->base_offset + at, &program->bytes[at], chunk)) return false;
    }
    bytes                  = program->bytes;
    program->length        = view->byte_length;
    program->palette_count = view->palette_count;
    program->key_set_count = view->key_set_count;
    program->map_count     = view->map_count;
    program->rule_count    = view->rule_count;
    program->tap_depth     = view->tap_depth;
    memset(program->thing_records, 0, sizeof(program->thing_records));
    memset(program->thing_record_size, 0, sizeof(program->thing_record_size));
    memset(program->thing_table, 0, sizeof(program->thing_table));
    offset = view->things_offset;
    for (uint8_t table = 0u; table < view->thing_table_count; table++) {
        uint8_t kind = bytes[offset];
        uint8_t size = (uint8_t)(1u + noah_profile_lighting_v1_colour_slots(view->tap_depth, kind));
        program->thing_records[kind]     = bytes[offset + 1u];
        program->thing_record_size[kind] = size;
        program->thing_table[kind]       = (uint16_t)(offset + 2u);
        offset                           = (uint16_t)(offset + 2u + (uint16_t)size * (1u + bytes[offset + 1u]));
    }
    program->key_sets = view->key_sets_offset;
    offset            = view->maps_offset;
    for (uint8_t map = 0u; map < view->map_count; map++) {
        program->maps[map] = offset;
        offset             = (uint16_t)(offset + (bytes[offset] == 0u ? NOAH_PROFILE_LIGHTING_V1_DENSE_MAP_SIZE : 2u + 2u * bytes[offset + 1u]));
    }
    offset = view->rules_offset;
    for (uint16_t rule = 0u; rule < view->rule_count; rule++) {
        program->rules[rule] = offset;
        offset               = (uint16_t)(offset + 1u + bytes[offset]);
    }
    program->rules[view->rule_count] = offset;
    program->fade_present            = false;
    program->tap_commit_flash        = false;
    offset                           = view->extensions_offset;
    for (uint8_t extension = 0u, count = bytes[offset++]; extension < count; extension++) {
        if (bytes[offset] == NOAH_LIGHTING_EXTENSION_ID_AUTOMOUSE_FADE) {
            memcpy(program->fade, &bytes[offset + 2u], sizeof(program->fade));
            program->fade_present = true;
        } else if (bytes[offset] == NOAH_LIGHTING_EXTENSION_ID_TAP_COMMIT_FLASH) {
            program->tap_commit_flash = bytes[offset + 2u] == 1u;
        }
        offset = (uint16_t)(offset + 2u + bytes[offset + 1u]);
    }
    program->valid = true;
    return true;
}

bool noah_profile_lighting_v1_program_thing_field(const noah_profile_lighting_v1_program_t *program, uint8_t kind, uint8_t thing, uint8_t field, uint8_t *value) {
    uint16_t       table;
    uint8_t        size;
    const uint8_t *record;

    if (!program || kind >= NOAH_LIGHTING_THING_KIND_LIMIT || (table = program->thing_table[kind]) == 0u) return false;
    size = program->thing_record_size[kind];
    if (field >= size) return false;
    record = &program->bytes[table];
    if (thing < program->thing_records[kind] && record[size * (1u + (uint16_t)thing) + field] != NOAH_PROFILE_LIGHTING_V1_INHERIT) {
        *value = record[size * (1u + (uint16_t)thing) + field];
    } else {
        *value = record[field];
    }
    return true;
}
