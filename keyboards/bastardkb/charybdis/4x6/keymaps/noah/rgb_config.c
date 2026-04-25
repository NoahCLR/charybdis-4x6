// ────────────────────────────────────────────────────────────────────────────
// RGB Color Configuration
// ────────────────────────────────────────────────────────────────────────────
//
// Read this file in render order:
//   1. base layer render surfaces
//   2. the auto-mouse base-stage transition
//   3. later overlay surfaces
//
// Want to change layer, LED-group, automouse, pd-mode, or key-behavior
// feedback colors? Edit this file.
// Want to change the rendering logic?  Edit users/noah/lib/rgb/core/rgb_runtime.c
// and the stage modules under users/noah/lib/rgb/.
// For shared RGB config types and the HSV helper, see
// users/noah/lib/rgb/core/rgb_config_helpers.h.
// For split-safe LED helper functions, see users/noah/lib/rgb/core/rgb_helpers.h.
//
// ────────────────────────────────────────────────────────────────────────────

#include "lib/rgb/core/rgb_helpers.h"
#include "noah_keymap.h" // layer enum, PD_MODE_* constants, hsv_t, rgb config types, RGB config helpers

#if defined(RGB_MATRIX_ENABLE)

// ─── LED Map ────────────────────────────────────────────────────────────────
//
// Reference map for LED group authoring:
//
//   ╭───────────────────────                ╭────────────────────────╮
//     0   7   8  15  16  20                   49  45  44  37  36  29
//   ├───────────────────────┤               ├────────────────────────┤
//     1   6   9  14  17  21                   50  46  43  38  35  30
//   ├───────────────────────┤               ├────────────────────────┤
//     2   5  10  13  18  22                   51  47  42  39  34  31
//   ├───────────────────────┤               ├────────────────────────┤
//     3   4  11  12  19  23                   52  48  41  40  33  32
//   ╰───────────────────────╯               ╰────────────────────────╯
//                       26  27  28     53  54
//                           25  24     55      (56)
//                     ╰────────────╯ ╰────────╯
//
// LED 56 is the custom trackball LED soldered on the right half, not part of
// the standard key matrix.
//
// Define reusable physical groups here, then reference them from any
// per-stage LED group table below.
#define RGB_LED_GROUP_LEFT_THUMB  RGB_LED_GROUP(26, 27, 28, 25, 24)
#define RGB_LED_GROUP_RIGHT_THUMB RGB_LED_GROUP(53, 54, 55)
#define RGB_LED_GROUP_THUMBS      RGB_LED_GROUP(26, 27, 28, 25, 24, 53, 54, 55)
#define RGB_LED_GROUP_TRACKBALL   RGB_LED_GROUP(56)

// ─── Layer colors ───────────────────────────────────────────────────────────
//
// Layer indicator colors and render modes, indexed by layer enum.
// LAYER_BASE is the persistent layer underlay: a non-black color paints below
// higher layers, while HSV(0, 0, 0) lets the default RGB Matrix effect show.
// The configured auto-mouse target layer uses its authored layer color as the
// timeout fade start state.
// .mode:
//   - ALL_KEYS = paint the whole layer color wash
//   - KEYS_MAPPED_ON_THIS_LAYER_ONLY = paint only keys that have a real key
//     assigned on that layer; lower active layers remain visible underneath
//     transparent positions
//                       { .color = HSV(hue, sat, val), .mode = ... }
const layer_color_config_t layer_colors[LAYER_COUNT] = {
    [LAYER_BASE] =
        {
            .color = HSV(0, 0, 0),
            .mode  = ALL_KEYS,
        },
    [LAYER_NUM] =
        {
            .color = HSV(85, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
            .mode  = KEYS_MAPPED_ON_THIS_LAYER_ONLY,
        },
    [LAYER_SYM] =
        {
            .color = HSV(169, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
            .mode  = KEYS_MAPPED_ON_THIS_LAYER_ONLY,
        },
    [LAYER_NAV] =
        {
            .color = HSV(180, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
            .mode  = KEYS_MAPPED_ON_THIS_LAYER_ONLY,
        },
    [LAYER_POINTER] =
        {
            .color = HSV(0, 0, 150),
            .mode  = KEYS_MAPPED_ON_THIS_LAYER_ONLY,
        },
};

// ─── Layer LED Groups ───────────────────────────────────────────────────────
//
// Paint specific LEDs a different color when a layer is active.
//
// Layer LED groups repaint after normal layer colors. Rows keyed to LAYER_BASE
// act as persistent underlay accents for the layer scene.
//
// Uncomment or add rows inside this table to enable layer-specific LED
// highlights.
static const layer_led_group_t layer_led_groups_data[] = RGB_LED_GROUP_TABLE(
    // { .layer = LAYER_NAV, .color = HSV(0, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS), .led_group = RGB_LED_GROUP_RIGHT_THUMB },
    // { .layer = LAYER_SYM, .color = HSV(43, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS), .led_group = RGB_LED_GROUP_LEFT_THUMB },
);

//
// ─── Auto-mouse timeout fade ────────────────────────────────────────────────
//
// Auto-mouse starts from the authored auto-mouse layer rendering above and
// fades toward the destination configured below. The first AUTOMOUSE_RGB_DEAD_TIME
// ms are dead time; only the remaining portion of AUTO_MOUSE_TIME animates.
//
// .mode chooses how the fade picks its destination:
//   - FOLLOW_REAL_DESTINATION = land on the real rendered board state after
//     the auto-mouse layer drops out
//   - END_COLOR_WHERE_BASE_EFFECT_WOULD_SHOW = keep the real destination where
//     layers paint, but use end_color for keys that would otherwise reveal the
//     base RGB effect
//   - END_COLOR_ON_ALL_KEYS = use end_color as the fade destination on every
//     key while the automouse renderer is active
//
// .end_color is used only by the END_COLOR_* modes above. Later overlays such
// as pd-mode color or key feedback can still repaint on top of the visible
// result.
#    ifdef RGB_AUTOMOUSE_GRADIENT_ENABLE
const automouse_fade_end_config_t automouse_fade_end_config = {
    .mode      = FOLLOW_REAL_DESTINATION,
    .end_color = HSV(0, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
};
#    endif // RGB_AUTOMOUSE_GRADIENT_ENABLE

// ─── Pointing device mode colors ────────────────────────────────────────────
//
// Overlay colors for the active trackball mode.
// Each entry is tagged with its pointing mode so the order doesn't need to
// match pd_modes[] — adding or reordering modes won't silently break colors.
//
// Locality decides where the overlay paints:
//   - RGB_BOTH_HALVES = mirror the PD color across both halves
//   - RGB_LEFT_HALF = always paint the left half
//   - RGB_RIGHT_HALF = always paint the right half
//   - RGB_KEY_HALF = paint the half or halves containing the key footprint
//     that triggered the current effective PD mode
//   - RGB_KEYS_ONLY = paint the key footprint that triggered the current
//     effective PD mode; combo-driven triggers paint every combo key
//
// { .pointing_mode = ..., .color = HSV(hue, sat, val), .locality = ... }
#    if defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)
const pd_mode_color_t pd_mode_colors[] = {
    {
        .pointing_mode = PD_MODE_DRAGSCROLL,
        .color         = HSV(21, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
        .locality      = RGB_RIGHT_HALF,
    },
    {
        .pointing_mode = PD_MODE_VOLUME,
        .color         = HSV(43, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
        .locality      = RGB_RIGHT_HALF,
    },
    {
        .pointing_mode = PD_MODE_BRIGHTNESS,
        .color         = HSV(213, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
        .locality      = RGB_RIGHT_HALF,
    },
    {
        .pointing_mode = PD_MODE_ARROW,
        .color         = HSV(127, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
        .locality      = RGB_RIGHT_HALF,
    },
    {
        .pointing_mode = PD_MODE_PINCH,
        .color         = HSV(55, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
        .locality      = RGB_RIGHT_HALF,
    },
    {
        .pointing_mode = PD_MODE_ZOOM,
        .color         = HSV(70, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
        .locality      = RGB_RIGHT_HALF,
    },
};

// ─── Pointing-Device Mode LED Groups ────────────────────────────────────────
//
// Paint specific LEDs a different color while a pointing mode is active.
// These groups repaint after the active pd-mode locality render.
//
// Uncomment or add rows inside this table to enable per-mode LED highlights.
static const pd_mode_led_group_t pd_mode_led_groups_data[] = RGB_LED_GROUP_TABLE(
    // { .pointing_mode = PD_MODE_VOLUME, .color = HSV(85, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS), .led_group = RGB_LED_GROUP_TRACKBALL },
);
#    endif // POINTING_DEVICE_ENABLE && RGB_PD_MODE_FEEDBACK_ENABLE

// ─── Combo feedback ────────────────────────────────────────────────────────
//
// Visual identity for any currently active combo chord.
//
// This is a persistent combo layer:
//   - if a combo is held, its combo color stays active
//   - combo-owned preview / PD state can route that combo underneath those
//     state indicators
//   - authored key-behavior feedback can still repaint above the combo color
//     on the same keys
//
// Locality decides where the combo layer paints:
//   - RGB_BOTH_HALVES = mirror the combo color across both halves
//   - RGB_LEFT_HALF = always paint the left half
//   - RGB_RIGHT_HALF = always paint the right half
//   - RGB_KEY_HALF = paint the half or halves touched by the live combo
//     footprint
//   - RGB_KEYS_ONLY = paint the exact combo keys
//
#    if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
const combo_feedback_color_config_t combo_feedback_colors = {
    .color    = HSV(191, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
    .locality = RGB_KEY_HALF,
};

// ─── Combo Feedback LED Groups ──────────────────────────────────────────────
//
// Paint specific LEDs a different color while combo feedback is active.
// These groups repaint after the combo locality render inside whichever combo
// underlay/overlay substage is live.
//
// Uncomment or add rows inside this table to enable persistent combo accents.
static const combo_feedback_led_group_t combo_feedback_led_groups_data[] = RGB_LED_GROUP_TABLE(
    // { .color = HSV(191, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS), .led_group = RGB_LED_GROUP_TRACKBALL },
);
#    endif // COMBO_ENABLE && RGB_COMBO_FEEDBACK_ENABLE

//
// ─── Key behavior feedback ──────────────────────────────────────────────────
//
// Visual feedback for the custom key behavior engine state.
//
// Terms used below:
//   - tap index = tap_counts[n] branch (single press, double press, ...)
//   - hold tier = .hold on the winning tap index
//   - long-hold tier = .long_hold on the winning tap index
//
// Feedback categories:
//   - RGB_TAP_PENDING_COLORS(...) = colors available while a selected tap
//     index is still unresolved, including terminal tap-only branches for the
//     normal pending window and tap-hold branches until the hold tier resolves
//   - tap_pending_mode chooses how pending tap branches pick from that list:
//     KEY_FEEDBACK_TAP_PENDING_SINGLE_COLOR uses the first color for every
//     pending branch; KEY_FEEDBACK_TAP_PENDING_BRANCH_COLORS uses the selected
//     tap index and clamps higher tap counts to the last configured color
//   - tap_committed_color = tap branch resolved and committed; layer and
//     PD-mode state actions stay quiet because their state overlays own that
//     feedback
//   - tap_commit_mode chooses which committed tap branches pulse:
//     KEY_FEEDBACK_TAP_COMMIT_OFF, KEY_FEEDBACK_TAP_COMMIT_NON_BASE_TAPS, or
//     KEY_FEEDBACK_TAP_COMMIT_ALL_TAPS
//   - hold_active_color = authored hold-tier pending / active states and
//     hold-tier commit pulses
//   - long_hold_active_color = authored long-hold-tier active states and
//     long-hold-tier commit pulses
//   - missing tiers stay quiet; e.g. a long-hold-only surface does not show
//     hold feedback before the long-hold tier commits
//   - the active tier picks the color: .hold always uses hold_active_color,
//     .long_hold always uses long_hold_active_color, regardless of which
//     helper authored that tier
//   - TAP_AT_HOLD_THRESHOLD(...) pulses once when that tier commits
//   - TAP_ON_RELEASE_AFTER_HOLD(...) stays steady while that tier is pending
//     release
//   - PRESS_AND_HOLD_UNTIL_RELEASE(...) and REPEAT_WHILE_HELD(...) flash while
//     that tier stays active
//
// Locality decides where the overlay paints:
//   - RGB_BOTH_HALVES = mirror the feedback color across both halves
//   - RGB_LEFT_HALF = always paint the left half
//   - RGB_RIGHT_HALF = always paint the right half
//   - RGB_KEY_HALF = paint only the half that owns the key or tap series
//     currently driving the feedback state; combo-driven feedback can broaden
//     this to both halves
//   - RGB_KEYS_ONLY = paint only the key footprint currently driving the
//     feedback state; combo-driven feedback paints every combo key
#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
const key_behavior_feedback_color_config_t key_behavior_feedback_colors = {
    RGB_TAP_PENDING_COLORS(
        HSV(0, 0, 150),
        HSV(169, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
        HSV(213, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
        HSV(0, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
        HSV(43, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
    ),

    // Choose SINGLE_COLOR to always use the first pending color.
    .tap_pending_mode = KEY_FEEDBACK_TAP_PENDING_BRANCH_COLORS,

    // Used for committed tap branches that do not already have state feedback.
    .tap_committed_color = HSV(85, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),

    // Commit feedback is most useful for double-tap and higher tap branches.
    .tap_commit_mode = KEY_FEEDBACK_TAP_COMMIT_NON_BASE_TAPS,

    // Used for authored hold-tier pending / active states and commit pulses.
    .hold_active_color = HSV(18, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),

    // Used for authored long-hold-tier active states and commit pulses.
    .long_hold_active_color = HSV(148, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),

    .locality = RGB_KEY_HALF,
};

// ─── Key-Behavior Feedback LED Groups ───────────────────────────────────────
//
// Paint specific LEDs a different color while key-behavior feedback is active.
// These groups repaint after the feedback locality render when a matching
// visible semantic category is live.
//
// Uncomment or add rows inside this table to enable feedback accents.
static const key_behavior_feedback_led_group_t key_behavior_feedback_led_groups_data[] = RGB_LED_GROUP_TABLE(
    // { .semantic = KEY_FEEDBACK_GROUP_MULTI_TAP_PENDING, .color = HSV(0, 0, 150), .led_group = RGB_LED_GROUP_TRACKBALL },
    // { .semantic = KEY_FEEDBACK_GROUP_TAP_COMMITTED, .color = HSV(85, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS), .led_group = RGB_LED_GROUP_TRACKBALL },
    // { .semantic = KEY_FEEDBACK_GROUP_HOLD_ACTIVE, .color = HSV(18, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS), .led_group = RGB_LED_GROUP_TRACKBALL },
    // { .semantic = KEY_FEEDBACK_GROUP_LONG_HOLD_ACTIVE, .color = HSV(148, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS), .led_group = RGB_LED_GROUP_TRACKBALL },
);
#    endif // RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE

// Expand the authored RGB tables, derived counts, and RGB feedback policy
// bridges above into the runtime symbols expected by the userspace runtime.
MATERIALIZE_RGB_CONFIG();

#endif // RGB_MATRIX_ENABLE
