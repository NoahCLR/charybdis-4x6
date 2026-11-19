#include "lib/profile/runtime/active_profile_settings_runtime.h"
// ────────────────────────────────────────────────────────────────────────────
// RGB Behaviour Feedback Stage
// ────────────────────────────────────────────────────────────────────────────
//
// Each case (a tap branch pending, tap committed, hold, long hold) has its own
// colour and locality, and the localities' areas stack, widest first (D-F73).
// Which cases show on which half is read once a frame from the semantic,
// tap-branch and flash-visibility maps; which key is newest comes from the
// broad owners the key runtime keeps.

#include "rgb_key_feedback_stage.h"

#if defined(RGB_MATRIX_ENABLE) && defined(RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE)

#    include "../../key/runtime/feedback.h"

#    if RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS <= 0
#        error "RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS must be greater than zero"
#    endif

// One bit per case, in case order.
typedef uint16_t key_feedback_case_mask_t;
_Static_assert(RGB_ACTIVE_PROFILE_KEY_MAX_CASES <= 16u, "a case mask holds every behaviour-feedback case");
// A stored tier semantic less tap committed's is its tier; a stored tap
// branch semantic names case 0 (tap count 2) or, from tap count 3's on, the
// case its distance from tap count 3's plus one.
_Static_assert(NOAH_PROFILE_RGB_V1_KEY_SEMANTIC_HOLD - NOAH_PROFILE_RGB_V1_KEY_SEMANTIC_TAP_COMMITTED == RGB_ACTIVE_PROFILE_KEY_TIER_HOLD && NOAH_PROFILE_RGB_V1_KEY_SEMANTIC_LONG_HOLD - NOAH_PROFILE_RGB_V1_KEY_SEMANTIC_TAP_COMMITTED == RGB_ACTIVE_PROFILE_KEY_TIER_LONG_HOLD && RGB_ACTIVE_PROFILE_KEY_TIER_TAP_COMMITTED == 0, "stored tier semantics are contiguous, in tier order");
_Static_assert(NOAH_PROFILE_RGB_V1_KEY_SEMANTIC_TAP_BRANCH_2 == 0 && NOAH_PROFILE_RGB_V1_KEY_SEMANTIC_TAP_BRANCH_4 == NOAH_PROFILE_RGB_V1_KEY_SEMANTIC_TAP_BRANCH_3 + 1 && NOAH_PROFILE_RGB_V1_KEY_SEMANTIC_TAP_BRANCH_5 == NOAH_PROFILE_RGB_V1_KEY_SEMANTIC_TAP_BRANCH_3 + 2, "stored tap branch semantics for tap counts 3 to 5 are contiguous");

// A case index, or what an owner slot names when it holds no case.
#    define KEY_FEEDBACK_STAGE_NO_CASE UINT8_MAX
#    define KEY_FEEDBACK_STAGE_NO_OWNER (UINT8_MAX - 1u)

typedef struct {
    rgb_active_profile_key_feedback_t config;
    rgb_runtime_frame_t              *frame;
    const uint8_t                    *semantic_map;
    const uint8_t                    *tap_branch_map;
    const uint8_t                    *flash_visibility_bitmap;
    const uint8_t                    *broad_owner_map;
    // The cases some key visibly shows, on the left half and the right.
    key_feedback_case_mask_t shown[2];
    // The cases whose accents paint this frame: rows naming one tap branch
    // or a tier, rows naming any tap branch, and 0xff rows.
    key_feedback_case_mask_t accented;
    key_feedback_case_mask_t accented_by_any_branch;
    key_feedback_case_mask_t accented_by_all;
    uint8_t                  led_min;
    uint8_t                  led_max;
} key_feedback_render_t;

static uint8_t rgb_runtime_key_feedback_stage_case_count(const key_feedback_render_t *render) {
    return rgb_active_profile_key_tier_case(&render->config, RGB_ACTIVE_PROFILE_KEY_TIER_COUNT);
}

static uint8_t rgb_runtime_key_feedback_stage_tier_case(const key_feedback_render_t *render, rgb_active_profile_key_tier_t tier) {
    return rgb_active_profile_key_tier_case(&render->config, tier);
}

static key_feedback_case_mask_t rgb_runtime_key_feedback_stage_case_bit(uint8_t case_index) {
    return (key_feedback_case_mask_t)(1u << case_index);
}

static key_feedback_case_mask_t rgb_runtime_key_feedback_stage_tap_branch_cases(const key_feedback_render_t *render) {
    return (key_feedback_case_mask_t)(rgb_runtime_key_feedback_stage_case_bit(render->config.tap_branch_count) - 1u);
}

// A deeper tap count takes the last tap branch's case.
static uint8_t rgb_runtime_key_feedback_stage_tap_branch_case(const key_feedback_render_t *render, uint8_t tap_branch) {
    uint8_t index;

    if (render->config.tap_branch_count == 0u) {
        return KEY_FEEDBACK_STAGE_NO_CASE;
    }
    if (tap_branch <= 2u) {
        return 0u;
    }
    index = (uint8_t)(tap_branch - 2u);
    if (index >= render->config.tap_branch_count) {
        index = (uint8_t)(render->config.tap_branch_count - 1u);
    }
    return index;
}

static uint8_t rgb_runtime_key_feedback_stage_case(const key_feedback_render_t *render, key_feedback_semantic_t semantic, keypos_t key_pos) {
    switch (semantic) {
        case KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING:
            return rgb_runtime_key_feedback_stage_tap_branch_case(render, key_feedback_tap_branch_map_get(render->tap_branch_map, key_pos));
        case KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED:
            return rgb_runtime_key_feedback_stage_tier_case(render, RGB_ACTIVE_PROFILE_KEY_TIER_TAP_COMMITTED);
        case KEY_FEEDBACK_SEMANTIC_HOLD_PENDING:
        case KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_FLASHING:
            return rgb_runtime_key_feedback_stage_tier_case(render, RGB_ACTIVE_PROFILE_KEY_TIER_HOLD);
        case KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_STEADY:
        case KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_FLASHING:
            return rgb_runtime_key_feedback_stage_tier_case(render, RGB_ACTIVE_PROFILE_KEY_TIER_LONG_HOLD);
        case KEY_FEEDBACK_SEMANTIC_NONE:
        default:
            return KEY_FEEDBACK_STAGE_NO_CASE;
    }
}

// The case a key shows, and whether it shows it now: a flashing case is
// dark in its off phase.
static uint8_t rgb_runtime_key_feedback_stage_key_case(const key_feedback_render_t *render, keypos_t key_pos, bool *visible) {
    key_feedback_semantic_t semantic = key_feedback_semantic_map_get(render->semantic_map, key_pos);

    *visible = !key_feedback_semantic_is_flashing(semantic) || key_origin_bitmap_has_keypos(render->flash_visibility_bitmap, key_pos);
    return rgb_runtime_key_feedback_stage_case(render, semantic, key_pos);
}

// The case the newest key in an owner slot shows: NO_OWNER when the slot
// names no key, NO_CASE when its key shows none.
static uint8_t rgb_runtime_key_feedback_stage_owner_case(const key_feedback_render_t *render, key_feedback_broad_owner_slot_t slot, bool *visible) {
    keypos_t owner_key_pos = key_feedback_broad_owner_map_get(render->broad_owner_map, slot);

    *visible = false;
    if (!key_origin_keypos_valid(owner_key_pos)) {
        return KEY_FEEDBACK_STAGE_NO_OWNER;
    }
    return rgb_runtime_key_feedback_stage_key_case(render, owner_key_pos, visible);
}

static rgb_t rgb_runtime_key_feedback_stage_case_rgb(const key_feedback_render_t *render, uint8_t case_index) {
    return hsv_to_rgb(render->config.case_colors[case_index]);
}

// A passthrough case paints nothing, nor do its black rows (D-F74).
static bool rgb_runtime_key_feedback_stage_case_paints(const key_feedback_render_t *render, uint8_t case_index) {
    return !rgb_hsv_is_passthrough(render->config.case_colors[case_index]);
}

// Whether no key shows any case: every packed semantic is none, so no area,
// key or accent has anything to paint this frame.
_Static_assert(KEY_FEEDBACK_SEMANTIC_NONE == 0, "a semantic map of zero bytes shows nothing");
static bool rgb_runtime_key_feedback_stage_nothing_shows(const uint8_t *semantic_map) {
    for (uint8_t index = 0u; index < KEY_FEEDBACK_SEMANTIC_MAP_SIZE; index++) {
        if (semantic_map[index] != 0u) {
            return false;
        }
    }
    return true;
}

static void rgb_runtime_key_feedback_stage_scan(key_feedback_render_t *render) {
    for (uint8_t row = 0u; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0u; col < MATRIX_COLS; col++) {
            keypos_t key_pos = {.row = row, .col = col};
            bool     visible;
            uint8_t  case_index = rgb_runtime_key_feedback_stage_key_case(render, key_pos, &visible);

            if (case_index != KEY_FEEDBACK_STAGE_NO_CASE && visible) {
                render->shown[split_half_from_keypos(key_pos) == SPLIT_HALF_RIGHT ? 1u : 0u] |= rgb_runtime_key_feedback_stage_case_bit(case_index);
            }
        }
    }
}

static bool rgb_runtime_key_feedback_stage_paint_range(key_feedback_render_t *render, rgb_t color, uint8_t from, uint8_t to) {
    bool painted = false;

    if (from < render->led_min) {
        from = render->led_min;
    }
    if (to > render->led_max) {
        to = render->led_max;
    }
    for (uint8_t led = from; led < to; led++) {
        render->frame->colors[led]  = color;
        render->frame->painted[led] = true;
        painted                     = true;
    }
    return painted;
}

// One area: both halves, a fixed half, or the half of the key (D-F73). The
// newest key decides the area when its case's locality is this one, through
// its flash's dark phase too and with a passthrough case (D-F74), and when it
// shows no case the area stays dark. Otherwise each case of this locality
// that some key shows paints it, a later case over an earlier one, a
// passthrough one not at all.
static bool rgb_runtime_key_feedback_stage_paint_area(key_feedback_render_t *render, rgb_locality_t locality, key_feedback_broad_owner_slot_t slot, key_feedback_case_mask_t shown, uint8_t from, uint8_t to) {
    bool    visible;
    bool    painted = false;
    uint8_t newest  = rgb_runtime_key_feedback_stage_owner_case(render, slot, &visible);

    if (newest == KEY_FEEDBACK_STAGE_NO_CASE) {
        return false;
    }
    if (newest != KEY_FEEDBACK_STAGE_NO_OWNER && render->config.case_localities[newest] == locality) {
        return visible && rgb_runtime_key_feedback_stage_case_paints(render, newest) && rgb_runtime_key_feedback_stage_paint_range(render, rgb_runtime_key_feedback_stage_case_rgb(render, newest), from, to);
    }
    for (uint8_t case_index = 0u; case_index < rgb_runtime_key_feedback_stage_case_count(render); case_index++) {
        if (render->config.case_localities[case_index] == locality && (shown & rgb_runtime_key_feedback_stage_case_bit(case_index)) != 0u && rgb_runtime_key_feedback_stage_case_paints(render, case_index)) {
            painted |= rgb_runtime_key_feedback_stage_paint_range(render, rgb_runtime_key_feedback_stage_case_rgb(render, case_index), from, to);
        }
    }
    return painted;
}

static bool rgb_runtime_key_feedback_stage_paint_key(key_feedback_render_t *render, rgb_t color, keypos_t key_pos) {
    uint8_t leds[RGB_MATRIX_LED_COUNT];
    uint8_t led_count = rgb_matrix_map_row_column_to_led(key_pos.row, key_pos.col, leds);
    bool    painted   = false;

    for (uint8_t index = 0u; index < led_count; index++) {
        uint8_t led = leds[index];

        if (led >= render->led_min && led < render->led_max) {
            render->frame->colors[led]  = color;
            render->frame->painted[led] = true;
            painted                     = true;
        }
    }
    return painted;
}

// Under the keys locality each key showing such a case paints its own key.
// With no case in that locality there is nothing to look up.
static bool rgb_runtime_key_feedback_stage_paint_keys(key_feedback_render_t *render) {
    bool painted  = false;
    bool any_keys = false;

    for (uint8_t case_index = 0u; case_index < rgb_runtime_key_feedback_stage_case_count(render); case_index++) {
        any_keys |= render->config.case_localities[case_index] == RGB_KEYS_ONLY;
    }
    if (!any_keys) {
        return false;
    }

    for (uint8_t row = 0u; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0u; col < MATRIX_COLS; col++) {
            keypos_t key_pos = {.row = row, .col = col};
            bool     visible;
            uint8_t  case_index = rgb_runtime_key_feedback_stage_key_case(render, key_pos, &visible);

            if (case_index != KEY_FEEDBACK_STAGE_NO_CASE && visible && render->config.case_localities[case_index] == RGB_KEYS_ONLY && rgb_runtime_key_feedback_stage_case_paints(render, case_index)) {
                painted |= rgb_runtime_key_feedback_stage_paint_key(render, rgb_runtime_key_feedback_stage_case_rgb(render, case_index), key_pos);
            }
        }
    }
    return painted;
}

// Widest area first, so a narrower one paints over it: both halves, one half,
// the half of the key, then the keys alone (D-F73).
static bool rgb_runtime_key_feedback_stage_render_localities(key_feedback_render_t *render) {
    key_feedback_case_mask_t anywhere = (key_feedback_case_mask_t)(render->shown[0] | render->shown[1]);
    bool                     painted  = false;

    painted |= rgb_runtime_key_feedback_stage_paint_area(render, RGB_BOTH_HALVES, KEY_FEEDBACK_BROAD_OWNER_GLOBAL, anywhere, 0u, RGB_MATRIX_LED_COUNT);
    painted |= rgb_runtime_key_feedback_stage_paint_area(render, RGB_LEFT_HALF, KEY_FEEDBACK_BROAD_OWNER_GLOBAL, anywhere, 0u, RGB_LEFT_LED_COUNT);
    painted |= rgb_runtime_key_feedback_stage_paint_area(render, RGB_RIGHT_HALF, KEY_FEEDBACK_BROAD_OWNER_GLOBAL, anywhere, RGB_LEFT_LED_COUNT, RGB_MATRIX_LED_COUNT);
    painted |= rgb_runtime_key_feedback_stage_paint_area(render, RGB_KEY_HALF, KEY_FEEDBACK_BROAD_OWNER_LEFT_HALF, render->shown[0], 0u, RGB_LEFT_LED_COUNT);
    painted |= rgb_runtime_key_feedback_stage_paint_area(render, RGB_KEY_HALF, KEY_FEEDBACK_BROAD_OWNER_RIGHT_HALF, render->shown[1], RGB_LEFT_LED_COUNT, RGB_MATRIX_LED_COUNT);
    painted |= rgb_runtime_key_feedback_stage_paint_keys(render);
    return painted;
}

// The cases a row naming one semantic names: one tap branch, the tap count 5
// semantic also every deeper one, or one tier (D-F73).
static key_feedback_case_mask_t rgb_runtime_key_feedback_stage_semantic_cases(const key_feedback_render_t *render, uint8_t semantic) {
    key_feedback_case_mask_t branches = rgb_runtime_key_feedback_stage_tap_branch_cases(render);

    switch (semantic) {
        case NOAH_PROFILE_RGB_V1_KEY_SEMANTIC_TAP_BRANCH_2:
            return (key_feedback_case_mask_t)(branches & rgb_runtime_key_feedback_stage_case_bit(0u));
        case NOAH_PROFILE_RGB_V1_KEY_SEMANTIC_TAP_BRANCH_3:
        case NOAH_PROFILE_RGB_V1_KEY_SEMANTIC_TAP_BRANCH_4:
            return (key_feedback_case_mask_t)(branches & rgb_runtime_key_feedback_stage_case_bit((uint8_t)(semantic - NOAH_PROFILE_RGB_V1_KEY_SEMANTIC_TAP_BRANCH_3 + 1u)));
        case NOAH_PROFILE_RGB_V1_KEY_SEMANTIC_TAP_BRANCH_5:
            return (key_feedback_case_mask_t)(branches & ~(rgb_runtime_key_feedback_stage_case_bit((uint8_t)(semantic - NOAH_PROFILE_RGB_V1_KEY_SEMANTIC_TAP_BRANCH_3 + 1u)) - 1u));
        case NOAH_PROFILE_RGB_V1_KEY_SEMANTIC_TAP_COMMITTED:
        case NOAH_PROFILE_RGB_V1_KEY_SEMANTIC_HOLD:
        case NOAH_PROFILE_RGB_V1_KEY_SEMANTIC_LONG_HOLD:
            return rgb_runtime_key_feedback_stage_case_bit(rgb_runtime_key_feedback_stage_tier_case(render, (rgb_active_profile_key_tier_t)(semantic - NOAH_PROFILE_RGB_V1_KEY_SEMANTIC_TAP_COMMITTED)));
        default:
            return 0u;
    }
}

// Whether the newest key in an owner slot visibly shows a case among these;
// that case's bit, or none.
static key_feedback_case_mask_t rgb_runtime_key_feedback_stage_owner_shows(const key_feedback_render_t *render, key_feedback_broad_owner_slot_t slot, key_feedback_case_mask_t among) {
    bool    visible;
    uint8_t shown_case = rgb_runtime_key_feedback_stage_owner_case(render, slot, &visible);

    if (!visible || shown_case >= rgb_runtime_key_feedback_stage_case_count(render)) {
        return 0u;
    }
    return (key_feedback_case_mask_t)(rgb_runtime_key_feedback_stage_case_bit(shown_case) & among);
}

// Accents of a tier follow the newest key showing that tier. A row naming one
// tap branch paints while any key visibly shows that branch. A row naming any
// tap branch takes the tap branches as one semantic, the newest key showing
// any of them (D-F75); a 0xff row takes them so too, and the tiers (D-F73).
static void rgb_runtime_key_feedback_stage_accent(key_feedback_render_t *render) {
    static const key_feedback_broad_owner_slot_t tier_slots[RGB_ACTIVE_PROFILE_KEY_TIER_COUNT] = {
        [RGB_ACTIVE_PROFILE_KEY_TIER_TAP_COMMITTED] = KEY_FEEDBACK_BROAD_OWNER_GROUP_TAP_COMMITTED,
        [RGB_ACTIVE_PROFILE_KEY_TIER_HOLD]          = KEY_FEEDBACK_BROAD_OWNER_GROUP_HOLD_ACTIVE,
        [RGB_ACTIVE_PROFILE_KEY_TIER_LONG_HOLD]     = KEY_FEEDBACK_BROAD_OWNER_GROUP_LONG_HOLD_ACTIVE,
    };
    key_feedback_case_mask_t branches = rgb_runtime_key_feedback_stage_tap_branch_cases(render);
    key_feedback_case_mask_t tiers    = 0u;

    for (uint8_t tier = 0u; tier < RGB_ACTIVE_PROFILE_KEY_TIER_COUNT; tier++) {
        tiers |= rgb_runtime_key_feedback_stage_owner_shows(render, tier_slots[tier], rgb_runtime_key_feedback_stage_case_bit(rgb_runtime_key_feedback_stage_tier_case(render, (rgb_active_profile_key_tier_t)tier)));
    }
    render->accented               = (key_feedback_case_mask_t)(((render->shown[0] | render->shown[1]) & branches) | tiers);
    render->accented_by_any_branch = rgb_runtime_key_feedback_stage_owner_shows(render, KEY_FEEDBACK_BROAD_OWNER_GROUP_TAP_BRANCH_PENDING, branches);
    render->accented_by_all        = (key_feedback_case_mask_t)(render->accented_by_any_branch | tiers);
}

static bool rgb_runtime_key_feedback_stage_paint_group(key_feedback_render_t *render, const rgb_active_profile_key_group_t *group, rgb_t color) {
    bool painted = false;

    for (uint8_t led = render->led_min; led < render->led_max; led++) {
        if (led >= NOAH_PROFILE_RGB_V1_PHYSICAL_LED_COUNT || (group->bitmap[led / 8u] & (uint8_t)(1u << (led % 8u))) == 0u) {
            continue;
        }
        render->frame->colors[led]  = color;
        render->frame->painted[led] = true;
        painted                     = true;
    }
    return painted;
}

// A black row takes the colour of each case it names that shows, in case
// order, and paints nothing for a passthrough one; a coloured row paints its
// own colour.
static bool rgb_runtime_key_feedback_stage_render_group(key_feedback_render_t *render, const rgb_active_profile_key_group_t *group, key_feedback_case_mask_t cases) {
    bool painted = false;

    if (!rgb_hsv_is_inherit_color(group->color)) {
        return rgb_runtime_key_feedback_stage_paint_group(render, group, hsv_to_rgb(group->color));
    }
    for (uint8_t case_index = 0u; case_index < rgb_runtime_key_feedback_stage_case_count(render); case_index++) {
        if ((cases & rgb_runtime_key_feedback_stage_case_bit(case_index)) != 0u && rgb_runtime_key_feedback_stage_case_paints(render, case_index)) {
            painted |= rgb_runtime_key_feedback_stage_paint_group(render, group, rgb_runtime_key_feedback_stage_case_rgb(render, case_index));
        }
    }
    return painted;
}

static bool rgb_runtime_key_feedback_stage_render_groups(key_feedback_render_t *render, const noah_active_profile_rgb_frame_t *profile_frame) {
    rgb_active_profile_group_selectors_t selectors;
    bool                                 painted = false;

    if (!rgb_active_profile_config_group_selectors(profile_frame, &selectors)) {
        rgb_runtime_frame_clear(render->frame, render->led_min, render->led_max);
        return false;
    }
    // Widest first, so a narrower row repaints a wider one: all-semantics
    // rows, then rows naming any tap branch (D-F75), then the rows naming one
    // case. Each pass reads only the rows whose semantic shows (D-F63), so a
    // row is read at most once.
    for (uint8_t pass = 0u; pass < 3u; pass++) {
        for (uint8_t group_index = 0u; group_index < selectors.key_count; group_index++) {
            rgb_active_profile_key_group_t group;
            uint8_t                        selector  = selectors.key[group_index];
            uint8_t                        row_pass  = selector == NOAH_PROFILE_RGB_V1_SELECTOR_ALL ? 0u : selector == NOAH_PROFILE_RGB_V1_KEY_SEMANTIC_TAP_BRANCH_ANY ? 1u : 2u;
            key_feedback_case_mask_t       cases;

            if (row_pass != pass) {
                continue;
            }
            cases = row_pass == 0u ? render->accented_by_all : row_pass == 1u ? render->accented_by_any_branch : (key_feedback_case_mask_t)(rgb_runtime_key_feedback_stage_semantic_cases(render, selector) & render->accented);
            if (cases == 0u) {
                continue;
            }
            if (!rgb_active_profile_config_key_group_at(profile_frame, group_index, &group)) {
                rgb_runtime_frame_clear(render->frame, render->led_min, render->led_max);
                return false;
            }
            painted |= rgb_runtime_key_feedback_stage_render_group(render, &group, cases);
        }
    }
    return painted;
}

bool rgb_runtime_key_feedback_stage_render_active_profile_frame(rgb_runtime_frame_t *frame, const noah_active_profile_rgb_frame_t *profile_frame, const uint8_t *semantic_map, const uint8_t *tap_branch_map, const uint8_t *flash_visibility_bitmap, const uint8_t *broad_owner_map, uint8_t led_min, uint8_t led_max) {
    key_feedback_render_t render;
    bool                  painted = false;

    if (!frame) {
        return false;
    }
    rgb_runtime_frame_clear(frame, led_min, led_max);
    if (!(semantic_map && tap_branch_map && flash_visibility_bitmap && broad_owner_map) || rgb_runtime_key_feedback_stage_nothing_shows(semantic_map) || !rgb_active_profile_config_key_stage_enabled(profile_frame) || !rgb_active_profile_config_key_feedback(profile_frame, &render.config)) {
        return false;
    }
    render.frame                   = frame;
    render.semantic_map            = semantic_map;
    render.tap_branch_map          = tap_branch_map;
    render.flash_visibility_bitmap = flash_visibility_bitmap;
    render.broad_owner_map         = broad_owner_map;
    render.shown[0]                = 0u;
    render.shown[1]                = 0u;
    render.led_min                 = led_min;
    render.led_max                 = led_max;

    rgb_runtime_key_feedback_stage_scan(&render);
    rgb_runtime_key_feedback_stage_accent(&render);
    painted |= rgb_runtime_key_feedback_stage_render_localities(&render);
    painted |= rgb_runtime_key_feedback_stage_render_groups(&render, profile_frame);
    if (!rgb_active_profile_config_frame_current(profile_frame)) {
        rgb_runtime_frame_clear(frame, led_min, led_max);
        return false;
    }
    return painted;
}

#endif
