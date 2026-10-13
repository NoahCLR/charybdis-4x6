// ────────────────────────────────────────────────────────────────────────────
// Lighting Engine
// ────────────────────────────────────────────────────────────────────────────

#include "lighting_engine.h"

#include <string.h>

#ifdef RGB_MATRIX_ENABLE

enum {
    // Subjects one rule may yield; the most any fact yields is one per key.
    SUBJECT_MAX = 64u,
    GATE_MAX    = 32u,
};
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
    uint8_t              amount;
} placed_subject_t;

typedef struct {
    uint32_t since;
    uint8_t  rule;
    uint8_t  thing_kind;
    uint8_t  thing;
    bool     used;
    bool     seen;
} gate_t;

// Resolution scratch, kept off the reviewed render stack.
static placed_subject_t subjects[SUBJECT_MAX];
static gate_t           gates[GATE_MAX];
static rgb_t            palette[NOAH_PROFILE_LIGHTING_V1_PALETTE_MAX];
static bool             palette_current;
static noah_lighting_frame_t fade_start;
static noah_lighting_frame_t fade_end;
static noah_lighting_facts_t fade_facts;

void noah_lighting_engine_program_changed(void) {
    palette_current = false;
    memset(gates, 0, sizeof(gates));
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

static gate_t *gate_for(uint8_t rule, uint8_t thing_kind, uint8_t thing, uint32_t now, bool update) {
    gate_t *free_gate = NULL;

    for (uint8_t index = 0u; index < GATE_MAX; index++) {
        gate_t *gate = &gates[index];
        if (gate->used && gate->rule == rule && gate->thing_kind == thing_kind && gate->thing == thing) return gate;
        if (!gate->used && !free_gate) free_gate = gate;
    }
    if (!update || !free_gate) return NULL;
    *free_gate = (gate_t){.since = now, .rule = rule, .thing_kind = thing_kind, .thing = thing, .used = true};
    return free_gate;
}

static uint8_t gate_amount(const rule_t *rule, uint8_t index, const noah_lighting_subject_t *subject, uint32_t now, bool update, noah_lighting_resolve_info_t *info) {
    gate_t  *gate;
    uint32_t since;
    uint8_t  amount = UINT8_MAX;

    if (rule->timing_count == 0u) return UINT8_MAX;
    gate  = gate_for(index, subject->thing_kind, subject->thing, now, update);
    since = gate ? gate->since : now;
    if (gate) gate->seen = true;
    for (uint8_t timing = 0u; timing < rule->timing_count; timing++) {
        uint32_t next_change;
        bool     animating;
        uint8_t  value = noah_lighting_timing(rule->timings[timing].kind, rule->timings[timing].params, now - since, &next_change, &animating);

        if (value < amount) amount = value;
        if (animating) info->live = true;
        if (next_change && (!info->wake_set || (int32_t)(now + next_change - info->wake) < 0)) {
            info->wake     = now + next_change;
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

        if (rule.condition_count && rule.conditions[0].slot == NOAH_PROFILE_LIGHTING_V1_SLOT_CONDITION) {
            noah_lighting_subject_t subject;
            uint16_t                cursor = 0u;
            while (count < SUBJECT_MAX && noah_lighting_fact_next(rule.conditions[0].kind, scene->facts, rule.conditions[0].params, &cursor, &subject)) {
                subjects[count++] = (placed_subject_t){
                    .leds = noah_lighting_target(rule.target.kind, scene, rule.target.params, &subject) & leds, .thing_kind = subject.thing_kind, .thing = subject.thing, .slot = subject.slot,
                    .amount = gate_amount(&rule, index, &subject, now, update, info),
                };
            }
        } else {
            noah_lighting_subject_t board;
            noah_lighting_subject_init(&board, NOAH_LIGHTING_THING_BOARD, NOAH_PROFILE_LIGHTING_V1_ANY, 0u);
            subjects[count++] = (placed_subject_t){.leds = noah_lighting_target(rule.target.kind, scene, rule.target.params, &board) & leds, .thing_kind = NOAH_LIGHTING_THING_BOARD, .thing = NOAH_PROFILE_LIGHTING_V1_ANY, .amount = gate_amount(&rule, index, &board, now, update, info)};
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
    if (update) {
        for (uint8_t index = 0u; index < GATE_MAX; index++) gates[index].seen = false;
    }
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
    if (update) {
        for (uint8_t index = 0u; index < GATE_MAX; index++) {
            if (!gates[index].seen) gates[index].used = false;
        }
    }
}

#endif // RGB_MATRIX_ENABLE
