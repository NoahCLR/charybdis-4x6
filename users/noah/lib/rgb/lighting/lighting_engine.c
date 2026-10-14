// ────────────────────────────────────────────────────────────────────────────
// Lighting Engine
// ────────────────────────────────────────────────────────────────────────────

#include "lighting_engine.h"

#include <string.h>
#ifdef NOAH_LIGHTING_TIMING_TEST_BACKEND
#    include <assert.h>
#endif

#ifdef RGB_MATRIX_ENABLE

enum {
    // Subjects one rule may yield; the most any fact yields is one per key.
    SUBJECT_MAX = 64u,
    // Sum of every registry entry's finite parameter signatures and full
    // subject domain. All accepted first conditions fit, regardless of load.
#define NOAH_LIGHTING_TRACKERS(id, name, thing, layout, slots, selector, signatures, live) +(signatures)
    TRACKER_MAX = 0u NOAH_LIGHTING_FACT_ROWS(NOAH_LIGHTING_TRACKERS),
#undef NOAH_LIGHTING_TRACKERS
#define NOAH_LIGHTING_CLOCKS(id, name, thing, layout, slots, selector, signatures, live) +((signatures) * (slots))
    CLOCK_MAX = 0u NOAH_LIGHTING_FACT_ROWS(NOAH_LIGHTING_CLOCKS),
#undef NOAH_LIGHTING_CLOCKS
#define NOAH_LIGHTING_LIVE_CLOCKS(id, name, thing, layout, slots, selector, signatures, live) +(live)
    LIVE_CLOCK_MAX = 0u NOAH_LIGHTING_FACT_ROWS(NOAH_LIGHTING_LIVE_CLOCKS),
#undef NOAH_LIGHTING_LIVE_CLOCKS
    RULE_MAX = 255u,
};
_Static_assert(LIVE_CLOCK_MAX <= UINT16_MAX, "joint live count fits u16");
#define NOAH_LIGHTING_SLOT_BOUND(id, name, thing, layout, slots, selector, signatures, live) _Static_assert((slots) <= 128u, "presence bitmap covers subject domain");
NOAH_LIGHTING_FACT_ROWS(NOAH_LIGHTING_SLOT_BOUND)
#undef NOAH_LIGHTING_SLOT_BOUND
_Static_assert(CLOCK_MAX <= UINT16_MAX, "registry-derived timer clock offsets fit u16");
_Static_assert(MATRIX_ROWS * MATRIX_COLS <= SUBJECT_MAX, "a rule's subjects must fit one per key");

typedef struct {
    const uint8_t *params;
    uint8_t        kind;
    uint8_t        slot;
} part_t;

typedef struct {
    uint8_t flags;
    uint8_t brightness;
    uint8_t condition_count;
    uint8_t timing_count;
    bool    has_background;
    part_t  conditions[NOAH_PROFILE_LIGHTING_V1_CONDITIONS_MAX];
    part_t  timings[NOAH_PROFILE_LIGHTING_V1_TIMINGS_MAX];
    part_t  target;
    part_t  paint;
    part_t  background;
} rule_t;

typedef struct {
    noah_lighting_leds_t leds;
    uint8_t              thing_kind;
    uint8_t              thing;
    uint8_t              slot;
    uint8_t              outcome;
    uint8_t              amount;
} placed_subject_t;

// Millisecond ages saturate at the largest accepted u16 timing. Sharing the
// first-condition presence clock is exact: a filtered rule's continuous age
// is min(subject presence age, filter-conjunction age).
typedef struct {
    uint16_t part_offset;
    uint16_t clock_offset;
} presence_tracker_t;

// Resolution scratch, kept off the reviewed render stack.
static placed_subject_t subjects[SUBJECT_MAX];
static presence_tracker_t trackers[TRACKER_MAX ? TRACKER_MAX : 1u];
static uint16_t clocks[LIVE_CLOCK_MAX ? LIVE_CLOCK_MAX : 1u];
static uint16_t live_clock_count;
static uint8_t clock_active[CLOCK_MAX ? (CLOCK_MAX + 7u) / 8u : 1u];
static uint16_t tracker_count;
static uint16_t filter_clocks[RULE_MAX];
static uint8_t filter_active[(RULE_MAX + 7u) / 8u];
#ifdef NOAH_LIGHTING_TIMING_TEST_BACKEND
uint16_t noah_lighting_test_live_clocks(void) { return live_clock_count; }
uint16_t noah_lighting_test_live_bound(void) { return LIVE_CLOCK_MAX; }
#endif
static uint32_t clock_last_time;
static bool clocks_current;
static bool timing_index_current;
static rgb_t            palette[NOAH_PROFILE_LIGHTING_V1_PALETTE_MAX];
static bool             palette_current;
static noah_lighting_frame_t fade_start;
static noah_lighting_frame_t fade_end;
static noah_lighting_facts_t fade_facts;

void noah_lighting_engine_program_changed(void) {
    palette_current = false;
    timing_index_current = false;
    clocks_current = false;
}

rgb_t noah_lighting_palette_rgb(const noah_lighting_scene_t *scene, uint8_t index) {
    const noah_profile_lighting_v1_program_t *program = scene->program;

    if (!palette_current) {
        for (uint8_t entry = 0u; entry < program->palette_count; entry++) {
            const uint8_t *hsv = &program->bytes[NOAH_PROFILE_LIGHTING_V1_HEADER_SIZE + 3u * entry];
            palette[entry]     = hsv_to_rgb((hsv_t){.h = hsv[0], .s = hsv[1], .v = hsv[2]});
        }
        palette_current = true;
    }
    return index < program->palette_count ? palette[index] : (rgb_t){0};
}

rgb_t noah_lighting_below(const noah_lighting_paint_context_t *context) {
    rgb_t effect = {0};

    if (context->below_painted) return context->below;
    *context->effect_used = true;
    (void)noah_lighting_effect_colour(context->led, &effect);
    return effect;
}

noah_lighting_leds_t noah_lighting_key_leds(const uint8_t *keys) {
    noah_lighting_leds_t leds = 0u;

    for (uint8_t row = 0u; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0u; col < MATRIX_COLS; col++) {
            keypos_t key = {.row = row, .col = col};
            uint8_t  found[NOAH_LIGHTING_LED_COUNT];
            uint8_t  count;

            if (!key_origin_bitmap_has_keypos(keys, key)) continue;
            count = rgb_matrix_map_row_column_to_led(row, col, found);
            for (uint8_t index = 0u; index < count; index++) {
                if (found[index] < NOAH_LIGHTING_LED_COUNT) leds |= NOAH_LIGHTING_LED_BIT(found[index]);
            }
        }
    }
    return leds;
}

static void parse_rule(const noah_profile_lighting_v1_program_t *program, uint8_t index, rule_t *rule) {
    const uint8_t *bytes = &program->bytes[program->rules[index]];
    const uint8_t *end   = bytes + 1u + bytes[0];
    const uint8_t *at    = bytes + 3u;

    memset(rule, 0, sizeof(*rule));
    rule->flags      = bytes[1];
    rule->brightness = bytes[2];
    while (at < end) {
        part_t part = {.slot = (uint8_t)(at[0] >> 5u), .kind = at[1], .params = at + 2u};
        switch (part.slot) {
            case NOAH_PROFILE_LIGHTING_V1_SLOT_CONDITION:
            case NOAH_PROFILE_LIGHTING_V1_SLOT_NEGATED:
                rule->conditions[rule->condition_count++] = part;
                break;
            case NOAH_PROFILE_LIGHTING_V1_SLOT_TARGET:
                rule->target = part;
                break;
            case NOAH_PROFILE_LIGHTING_V1_SLOT_PAINT:
                rule->paint = part;
                break;
            case NOAH_PROFILE_LIGHTING_V1_SLOT_BACKGROUND:
                rule->background     = part;
                rule->has_background = true;
                break;
            case NOAH_PROFILE_LIGHTING_V1_SLOT_TIMING:
                rule->timings[rule->timing_count++] = part;
                break;
            default:
                break;
        }
        at += 2u + (at[0] & 0x1fu);
    }
}

static bool fact_holds(const noah_lighting_facts_t *facts, const part_t *condition) {
    noah_lighting_subject_t subject;
    uint16_t                cursor = 0u;
    bool                    any    = noah_lighting_fact_next(condition->kind, facts, condition->params, &cursor, &subject);

    return condition->slot == NOAH_PROFILE_LIGHTING_V1_SLOT_NEGATED ? !any : any;
}

static uint16_t advance_clock(uint16_t value, uint32_t elapsed) {
    return elapsed >= 65535u - value ? 65535u : (uint16_t)(value + elapsed);
}

static bool same_condition(const uint8_t *a, const uint8_t *b, uint8_t selector) {
    if (a[0] != b[0] || a[1] != b[1]) return false;
    for (uint8_t i = 0u; i < (a[0] & 0x1fu); i++) {
        if (i != selector && a[2u + i] != b[2u + i]) return false;
    }
    return true;
}

static __attribute__((noinline)) void prepare_timing_index(const noah_profile_lighting_v1_program_t *program) {
    uint32_t clock_count = 0u;
    tracker_count = 0u;
    live_clock_count = 0u;
    memset(filter_clocks, 0, sizeof(filter_clocks));
    memset(filter_active, 0, sizeof(filter_active));
    memset(clock_active, 0, sizeof(clock_active));
    for (uint16_t i = 0u; i < program->rule_count; i++) {
        rule_t rule;
        parse_rule(program, i, &rule);
        if (!rule.timing_count || (rule.flags & NOAH_PROFILE_LIGHTING_V1_RULE_DISABLED)) continue;
        if (!rule.condition_count || rule.conditions[0].slot != NOAH_PROFILE_LIGHTING_V1_SLOT_CONDITION) continue;
        const uint8_t *part = rule.conditions[0].params - 2u;
        uint8_t selector = noah_lighting_fact_selector_parameter(rule.conditions[0].kind);
        uint16_t index;
        for (index = 0u; index < tracker_count; index++) {
            if (same_condition(part, program->bytes + trackers[index].part_offset, selector)) break;
        }
        if (index == tracker_count) {
            uint8_t slots = noah_lighting_fact_subject_slots(rule.conditions[0].kind);
            trackers[index] = (presence_tracker_t){.part_offset = part - program->bytes, .clock_offset = clock_count};
            clock_count += slots;
            tracker_count++;
        }
    }
    // Only normalized signatures selected by the accepted profile are indexed.
    timing_index_current = true;
}

// Keep timing seams linked for reviewed-path stack accounting.
// Rank in the compact age array, in registry-tracker/subject order.
static __attribute__((noinline)) uint16_t clock_rank(uint16_t first, uint16_t end) {
    static const uint8_t pop4[16] = {0u, 1u, 1u, 2u, 1u, 2u, 2u, 3u, 1u, 2u, 2u, 3u, 2u, 3u, 3u, 4u};
    uint16_t rank = 0u;
    while (first < end && (first % 8u)) {
        if (clock_active[first / 8u] & (1u << (first % 8u))) rank++;
        first++;
    }
    while (end - first >= 8u) {
        uint8_t byte = clock_active[first / 8u];
        rank += pop4[byte & 15u] + pop4[byte >> 4u];
        first += 8u;
    }
    while (first < end) {
        if (clock_active[first / 8u] & (1u << (first % 8u))) rank++;
        first++;
    }
    return rank;
}

static __attribute__((noinline)) void refresh_presence(const presence_tracker_t *tracker, const noah_lighting_facts_t *facts, const noah_profile_lighting_v1_program_t *program, uint32_t elapsed, bool remove, uint16_t *read, uint16_t *write, uint16_t *present_count) {
    const uint8_t *part = program->bytes + tracker->part_offset;
    uint8_t params[NOAH_PROFILE_LIGHTING_V1_PARAMETER_MAX];
    uint8_t present[16] = {0};
    uint16_t cursor = 0u;
    noah_lighting_subject_t subject;
    uint8_t selector = noah_lighting_fact_selector_parameter(part[1]);
    uint8_t slots = noah_lighting_fact_subject_slots(part[1]);
    memcpy(params, part + 2u, part[0] & 0x1fu);
    if (selector != 255u) params[selector] = 255u;
    while (noah_lighting_fact_next(part[1], facts, params, &cursor, &subject)) {
        uint8_t slot = slots == 1u ? 0u : subject.thing;
        if (slot < slots) present[slot / 8u] |= 1u << (slot % 8u);
    }
    for (uint16_t at = 0u; at < slots; at++) {
        uint8_t slot = remove ? at : slots - 1u - at;
        uint16_t index = tracker->clock_offset + slot;
        uint8_t bit = 1u << (index % 8u);
        bool active = (clock_active[index / 8u] & bit) != 0u;
        bool holds = (present[slot / 8u] & (1u << (slot % 8u))) != 0u;
        if (remove) {
            if (holds) (*present_count)++;
            if (active) {
                if (holds) clocks[(*write)++] = clocks[*read];
                else clock_active[index / 8u] &= ~bit;
                (*read)++;
            }
        } else if (holds) {
            // Inserting backwards follows complete forward removal. At every
            // write, the destination is at or after its unread old source.
            clocks[--(*write)] = active ? advance_clock(clocks[--(*read)], elapsed) : 0u;
            clock_active[index / 8u] |= bit;
        }
    }
}

static __attribute__((noinline)) uint16_t rule_presence_tracker(const noah_profile_lighting_v1_program_t *program, const rule_t *rule) {
    if (!rule->timing_count || !rule->condition_count || rule->conditions[0].slot != NOAH_PROFILE_LIGHTING_V1_SLOT_CONDITION) return UINT16_MAX;
    const uint8_t *part = rule->conditions[0].params - 2u;
    uint8_t selector = noah_lighting_fact_selector_parameter(rule->conditions[0].kind);
    for (uint16_t index = 0u; index < tracker_count; index++) {
        if (same_condition(part, program->bytes + trackers[index].part_offset, selector)) return index;
    }
    return UINT16_MAX;
}

static __attribute__((noinline)) void prepare_rule_clocks(const noah_lighting_scene_t *scene, uint32_t now) {
    if (!timing_index_current) prepare_timing_index(scene->program);
    uint32_t elapsed = clocks_current ? now - clock_last_time : 0u;
    // Remove forwards, insert backwards: linear age movement, no transient
    // union of old and new ids and no scratch copy of the age array.
    uint16_t read = 0u, write = 0u, present_count = 0u;
    for (uint16_t i = 0u; i < tracker_count; i++) refresh_presence(trackers + i, scene->facts, scene->program, elapsed, true, &read, &write, &present_count);
#ifdef NOAH_LIGHTING_TIMING_TEST_BACKEND
    assert(read == live_clock_count);
    assert(present_count <= LIVE_CLOCK_MAX);
#endif
    read = write;
    write = present_count;
    for (uint16_t i = tracker_count; i-- > 0u;) refresh_presence(trackers + i, scene->facts, scene->program, elapsed, false, &read, &write, &present_count);
#ifdef NOAH_LIGHTING_TIMING_TEST_BACKEND
    assert(read == 0u && write == 0u);
#endif
    live_clock_count = present_count;
    for (uint16_t i = 0u; i < scene->program->rule_count; i++) {
        rule_t rule;
        parse_rule(scene->program, i, &rule);
        if (!rule.timing_count || (rule.flags & NOAH_PROFILE_LIGHTING_V1_RULE_DISABLED)) continue;
        uint8_t filter = rule.condition_count && rule.conditions[0].slot == NOAH_PROFILE_LIGHTING_V1_SLOT_CONDITION ? 1u : 0u;
        bool holds = true;
        for (; filter < rule.condition_count; filter++) {
            if (!fact_holds(scene->facts, &rule.conditions[filter])) { holds = false; break; }
        }
        uint8_t bit = 1u << (i % 8u);
        if (holds) {
            filter_clocks[i] = filter_active[i / 8u] & bit ? advance_clock(filter_clocks[i], elapsed) : 0u;
            filter_active[i / 8u] |= bit;
        } else {
            filter_clocks[i] = 0u;
            filter_active[i / 8u] &= ~bit;
        }
    }
    clock_last_time = now;
    clocks_current = true;
}

static __attribute__((noinline)) uint8_t gate_amount(const rule_t *rule, uint8_t index, const noah_lighting_subject_t *subject, uint16_t tracker_index, uint16_t age_offset, uint32_t now, bool update, noah_lighting_resolve_info_t *info) {
    (void)update;
    if (!rule->timing_count) return UINT8_MAX;
    uint16_t age = filter_clocks[index];
    if (tracker_index != UINT16_MAX) {
        const presence_tracker_t *tracker = trackers + tracker_index;
        uint8_t slots = noah_lighting_fact_subject_slots(rule->conditions[0].kind);
        uint8_t slot = slots == 1u ? 0u : subject->thing;
        uint16_t clock = tracker->clock_offset + slot;
        bool active = slot < slots && (clock_active[clock / 8u] & (1u << (clock % 8u)));
        uint16_t presence = active ? clocks[age_offset + clock_rank(tracker->clock_offset, clock)] : 0u;
        if (presence < age) age = presence;
    }
    uint8_t amount = UINT8_MAX;
    for (uint8_t timing = 0u; timing < rule->timing_count; timing++) {
        uint32_t next_change;
        bool animating;
        // Ages saturate at the largest accepted u16 delay, never before it.
        uint32_t elapsed = age;
        uint8_t value = noah_lighting_timing(rule->timings[timing].kind, rule->timings[timing].params, elapsed, &next_change, &animating);
        if (value < amount) amount = value;
        if (animating) info->live = true;
        if (next_change && (!info->wake_set || (int32_t)(now + next_change - info->wake) < 0)) {
            info->wake = now + next_change;
            info->wake_set = true;
        }
    }
    return amount;
}

static uint8_t scale(uint8_t channel, uint8_t level) {
    return (uint8_t)(((uint16_t)channel * level + 127u) / 255u);
}

static rgb_t mix(rgb_t below, rgb_t colour, uint8_t amount) {
    return (rgb_t){
        .r = (uint8_t)(((uint32_t)below.r * (255u - amount) + (uint32_t)colour.r * amount + 127u) / 255u),
        .g = (uint8_t)(((uint32_t)below.g * (255u - amount) + (uint32_t)colour.g * amount + 127u) / 255u),
        .b = (uint8_t)(((uint32_t)below.b * (255u - amount) + (uint32_t)colour.b * amount + 127u) / 255u),
    };
}

static void paint_subject(const noah_lighting_scene_t *scene, const rule_t *rule, const placed_subject_t *placed, noah_lighting_leds_t leds, uint32_t now, noah_lighting_frame_t *frame, noah_lighting_resolve_info_t *info) {
    noah_lighting_subject_t       subject;
    noah_lighting_paint_context_t context = {.scene = scene, .subject = &subject, .time = now, .effect_used = &info->effect_used};

    noah_lighting_subject_init(&subject, placed->thing_kind, placed->thing, placed->slot);
    subject.outcome = placed->outcome;
    if (noah_lighting_paint_changes_with_time(rule->paint.kind) || (rule->has_background && noah_lighting_paint_changes_with_time(rule->background.kind))) info->live = true;
    for (uint8_t led = 0u; led < NOAH_LIGHTING_LED_COUNT; led++) {
        noah_lighting_leds_t bit = NOAH_LIGHTING_LED_BIT(led);
        rgb_t                colour;
        bool                 painted = false;

        if ((leds & bit) == 0u) continue;
        context.led           = led;
        context.below         = frame->colour[led];
        context.below_painted = (frame->painted & bit) != 0u;
        if (placed->leds & bit) {
            context.params = rule->paint.params;
            painted        = noah_lighting_paint(rule->paint.kind, &context, &colour);
        }
        if (!painted && rule->has_background) {
            context.params = rule->background.params;
            painted        = noah_lighting_paint(rule->background.kind, &context, &colour);
        }
        if (!painted) continue;
        if (rule->brightness != UINT8_MAX) colour = (rgb_t){.r = scale(colour.r, rule->brightness), .g = scale(colour.g, rule->brightness), .b = scale(colour.b, rule->brightness)};
        if (placed->amount != UINT8_MAX) colour = mix(noah_lighting_below(&context), colour, placed->amount);
        frame->colour[led] = colour;
        frame->painted |= bit;
    }
}

// Applies rules [first, end) bottom up over `leds` of `frame`.
static void apply_rules(const noah_lighting_scene_t *scene, uint8_t first, uint8_t end, noah_lighting_leds_t leds, uint32_t now, bool update, noah_lighting_frame_t *frame, noah_lighting_resolve_info_t *info) {
    for (uint8_t index = end; index-- > first;) {
        rule_t  rule;
        uint8_t count  = 0u;
        uint8_t filter = 0u;

        parse_rule(scene->program, index, &rule);
        if (rule.flags & NOAH_PROFILE_LIGHTING_V1_RULE_DISABLED) continue;
        if (rule.condition_count && rule.conditions[0].slot == NOAH_PROFILE_LIGHTING_V1_SLOT_CONDITION) filter = 1u;
        while (filter < rule.condition_count && fact_holds(scene->facts, &rule.conditions[filter])) filter++;
        if (filter < rule.condition_count) continue;
        uint16_t tracker_index = rule_presence_tracker(scene->program, &rule);
        uint16_t age_offset = tracker_index != UINT16_MAX ? clock_rank(0u, trackers[tracker_index].clock_offset) : 0u;

        if (rule.condition_count && rule.conditions[0].slot == NOAH_PROFILE_LIGHTING_V1_SLOT_CONDITION) {
            noah_lighting_subject_t subject;
            uint16_t                cursor = 0u;
            while (count < SUBJECT_MAX && noah_lighting_fact_next(rule.conditions[0].kind, scene->facts, rule.conditions[0].params, &cursor, &subject)) {
                subjects[count++] = (placed_subject_t){
                    .leds       = noah_lighting_target(rule.target.kind, scene, rule.target.params, &subject) & leds,
                    .thing_kind = subject.thing_kind,
                    .thing      = subject.thing,
                    .slot       = subject.slot,
                    .outcome    = subject.outcome,
                    .amount     = gate_amount(&rule, index, &subject, tracker_index, age_offset, now, update, info),
                };
            }
        } else {
            noah_lighting_subject_t board;
            noah_lighting_subject_init(&board, NOAH_LIGHTING_THING_BOARD, NOAH_PROFILE_LIGHTING_V1_ANY, 0u);
            subjects[count++] = (placed_subject_t){.leds = noah_lighting_target(rule.target.kind, scene, rule.target.params, &board) & leds, .thing_kind = NOAH_LIGHTING_THING_BOARD, .thing = NOAH_PROFILE_LIGHTING_V1_ANY, .amount = gate_amount(&rule, index, &board, tracker_index, age_offset, now, update, info)};
        }
        // The first subject is on top, so it paints last.
        while (count-- > 0u) {
            if (subjects[count].amount != 0u) paint_subject(scene, &rule, &subjects[count], leds, now, frame, info);
        }
    }
}

static void clear(noah_lighting_frame_t *frame, noah_lighting_leds_t leds) {
    frame->painted &= ~leds;
}

void noah_lighting_resolve(const noah_lighting_scene_t *scene, noah_lighting_leds_t leds, uint32_t now, bool update, noah_lighting_frame_t *frame, noah_lighting_resolve_info_t *info) {
    const noah_profile_lighting_v1_program_t *program = scene->program;
    noah_lighting_fade_t                      fade;
    uint8_t                                   rules = program->valid ? program->rule_count : 0u;
    uint8_t                                   scene_first;

    *info = (noah_lighting_resolve_info_t){0};
    clear(frame, leds);
    if (update && rules) prepare_rule_clocks(scene, now);
    if (!rules) return;
    if (!noah_lighting_fade_active(scene, &fade) || fade.base > rules) {
        apply_rules(scene, 0u, rules, leds, now, update, frame, info);
    } else {
        noah_lighting_scene_t end_scene = *scene;
        rgb_t                 end_colour = noah_lighting_palette_rgb(scene, fade.end);

        info->live  = true;
        scene_first = (uint8_t)(rules - fade.base);
        clear(&fade_start, leds);
        apply_rules(scene, scene_first, rules, leds, now, update, &fade_start, info);
        fade_facts = *scene->facts;
        noah_lighting_fade_end_facts(&fade_facts, fade.layer);
        end_scene.facts = &fade_facts;
        clear(&fade_end, leds);
        apply_rules(&end_scene, scene_first, rules, leds, now, false, &fade_end, info);
        for (uint8_t led = 0u; led < NOAH_LIGHTING_LED_COUNT; led++) {
            noah_lighting_leds_t bit = NOAH_LIGHTING_LED_BIT(led);
            bool                 start_visible = (fade_start.painted & bit) != 0u;
            bool                 end_visible;
            rgb_t                end;

            if ((leds & bit) == 0u) continue;
            if (fade.mode == 2u) {
                end         = end_colour;
                end_visible = true;
            } else {
                end         = fade_end.colour[led];
                end_visible = (fade_end.painted & bit) != 0u;
                if (!end_visible && fade.mode == 1u) {
                    end         = end_colour;
                    end_visible = true;
                } else if (!end_visible) {
                    info->effect_used = true;
                    end_visible       = noah_lighting_effect_colour(led, &end);
                }
            }
            if (start_visible && end_visible) {
                frame->colour[led] = mix(fade_start.colour[led], end, fade.blend);
            } else if (start_visible && fade.blend != UINT8_MAX) {
                frame->colour[led] = fade_start.colour[led];
            } else if (end_visible && !start_visible && fade.blend != 0u) {
                frame->colour[led] = end;
            } else {
                continue;
            }
            frame->painted |= bit;
        }
        apply_rules(scene, 0u, scene_first, leds, now, update, frame, info);
    }

}

#endif // RGB_MATRIX_ENABLE
