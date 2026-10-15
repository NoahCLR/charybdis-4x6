// ────────────────────────────────────────────────────────────────────────────
// Lighting Engine — the contract between the engine and its modules
// ────────────────────────────────────────────────────────────────────────────
//
// docs/architecture/lighting-rules-v1.md. The engine walks the published rule
// list; every fact, target, paint and timing is a module under kinds/ that
// implements one of the function shapes declared here, generated from its
// registry row. The engine reaches modules only through lighting_dispatch.c.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

#include "../../key/runtime/feedback.h"
#include "../../key/runtime/slot/origin_registry.h"
#include "../../profile/schema/profile_lighting_v1.h"

#if __has_include("color.h")
#    include "color.h" // QMK
#endif
#ifdef RGB_MATRIX_ENABLE
#    include "rgb_matrix.h" // QMK
#endif

#ifndef RGB_LEFT_LED_COUNT
#    define RGB_LEFT_LED_COUNT 29
#endif

#ifdef RGB_MATRIX_LED_COUNT
#    define NOAH_LIGHTING_LED_COUNT RGB_MATRIX_LED_COUNT
#else
#    define NOAH_LIGHTING_LED_COUNT NOAH_PROFILE_LIGHTING_V1_LED_COUNT
#endif
_Static_assert(NOAH_LIGHTING_LED_COUNT <= 64, "an LED mask is one 64-bit word");

// One bit per LED.
typedef uint64_t noah_lighting_leds_t;

#define NOAH_LIGHTING_LED_BIT(led) ((noah_lighting_leds_t)1u << (led))
#define NOAH_LIGHTING_ALL_LEDS ((NOAH_LIGHTING_LED_COUNT >= 64) ? UINT64_MAX : ((noah_lighting_leds_t)1u << NOAH_LIGHTING_LED_COUNT) - 1u)
#define NOAH_LIGHTING_LEFT_LEDS (((noah_lighting_leds_t)1u << RGB_LEFT_LED_COUNT) - 1u)
#define NOAH_LIGHTING_RIGHT_LEDS (NOAH_LIGHTING_ALL_LEDS & ~NOAH_LIGHTING_LEFT_LEDS)

// What a rule paints for: a thing (or the board) and the keys and halves it
// concerns. A fact yields subjects in its natural order, highest first.
typedef struct {
    uint8_t thing_kind; // NOAH_LIGHTING_THING_*
    uint8_t thing;      // the thing's id, or NOAH_PROFILE_LIGHTING_V1_ANY for every thing of the kind
    uint8_t slot;       // identity colour slot
    uint8_t outcome;    // feedback outcome encoding (feedback.h)
    uint8_t sides;      // split_side_mask_t of the halves it concerns
    bool    has_keys;
    uint8_t keys[KEY_ORIGIN_BITMAP_SIZE];
} noah_lighting_subject_t;

// ─── The fact frame ─────────────────────────────────────────────────────────
//
// Each fact module owns a slice: X(name, bytes, synced, gated). A local slice
// is collected on each half; a synced one on the USB half, which sends it in
// the lighting facts frame. A gated slice is rebuilt for the link only when
// its owner marks it dirty, it was active, or a heartbeat is due.

#ifndef COMBO_BUFFER_LENGTH
#    define COMBO_BUFFER_LENGTH 4
#endif
#define NOAH_LIGHTING_COMBO_RECORD_BYTES (2u + KEY_ORIGIN_BITMAP_SIZE)
#define NOAH_LIGHTING_COMBO_SLICE_BYTES (COMBO_BUFFER_LENGTH * NOAH_LIGHTING_COMBO_RECORD_BYTES)

// Shared producer/consumer layouts. These sizes follow the compiled matrix
// and combo buffer; the default wire layout remains byte-identical.
#define NOAH_LIGHTING_MATRIX_KEYS (MATRIX_ROWS * MATRIX_COLS)
#define NOAH_LIGHTING_LAYER_LOCK_OFFSET NOAH_LIGHTING_MATRIX_KEYS
#define NOAH_LIGHTING_LAYER_ONESHOT_OFFSET (NOAH_LIGHTING_LAYER_LOCK_OFFSET + 2u)
#define NOAH_LIGHTING_LAYER_TOP_OFFSET (NOAH_LIGHTING_LAYER_ONESHOT_OFFSET + 1u)
#define NOAH_LIGHTING_LAYER_STATUS_BYTES (NOAH_LIGHTING_LAYER_TOP_OFFSET + 1u)
#define NOAH_LIGHTING_MODE_STATUS_BYTES (3u + KEY_ORIGIN_BITMAP_SIZE)
#define NOAH_LIGHTING_BEHAVIOUR_PHASE_RECORD_BYTES KEY_FEEDBACK_LIGHTING_PHASE_RECORD_BYTES
#define NOAH_LIGHTING_BEHAVIOUR_PHASE_BYTES KEY_FEEDBACK_LIGHTING_PHASE_BYTES
_Static_assert(LAYER_COUNT <= 16u, "lighting layer masks contain sixteen layers");
_Static_assert(NOAH_LIGHTING_MATRIX_KEYS <= 64u, "lighting subjects fit one per key");

#define NOAH_LIGHTING_BEHAVIOUR_SLICE_BYTES (KEY_ORIGIN_BITMAP_SIZE + KEY_FEEDBACK_SEMANTIC_MAP_SIZE + KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE + KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE)

#if NOAH_LIGHTING_FACT_LAYER_ACTIVE
#    define NOAH_LIGHTING_SLICE_LAYER_ACTIVE(X) X(layer_active, 2u, 0, 0)
#else
#    define NOAH_LIGHTING_SLICE_LAYER_ACTIVE(X)
#endif
#if NOAH_LIGHTING_FACT_LAYER_PREVIEWING
#    define NOAH_LIGHTING_SLICE_LAYER_PREVIEWING(X) X(layer_previewing, 1u, 1, 0)
#else
#    define NOAH_LIGHTING_SLICE_LAYER_PREVIEWING(X)
#endif
#if NOAH_LIGHTING_FACT_POINTING_MODE_ACTIVE
#    define NOAH_LIGHTING_SLICE_POINTING_MODE_ACTIVE(X) X(pointing_mode_active, 3u + KEY_ORIGIN_BITMAP_SIZE, 0, 0)
#else
#    define NOAH_LIGHTING_SLICE_POINTING_MODE_ACTIVE(X)
#endif
#if NOAH_LIGHTING_FACT_COMBOS_HELD
#    define NOAH_LIGHTING_SLICE_COMBOS_HELD(X) X(combos_held, 2u * KEY_ORIGIN_BITMAP_SIZE, 1, 0)
#else
#    define NOAH_LIGHTING_SLICE_COMBOS_HELD(X)
#endif
#if NOAH_LIGHTING_FACT_BEHAVIOUR_STATE
#    define NOAH_LIGHTING_SLICE_BEHAVIOUR_STATE(X) X(behaviour_state, NOAH_LIGHTING_BEHAVIOUR_SLICE_BYTES, 1, 1)
#else
#    define NOAH_LIGHTING_SLICE_BEHAVIOUR_STATE(X)
#endif

#if NOAH_LIGHTING_FACT_COMBO_HELD
#    define NOAH_LIGHTING_SLICE_COMBO_HELD(X) X(combo_held, NOAH_LIGHTING_COMBO_SLICE_BYTES, 1, 0)
#else
#    define NOAH_LIGHTING_SLICE_COMBO_HELD(X)
#endif
#if NOAH_LIGHTING_FACT_BEHAVIOUR
#    define NOAH_LIGHTING_SLICE_BEHAVIOUR(X) X(behaviour, KEY_FEEDBACK_LIGHTING_BYTES, 1, 1)
#else
#    define NOAH_LIGHTING_SLICE_BEHAVIOUR(X)
#endif

#if NOAH_LIGHTING_FACT_LAYER_STATUS
#    define NOAH_LIGHTING_SLICE_LAYER_STATUS(X) X(layer_status, NOAH_LIGHTING_LAYER_STATUS_BYTES, 1, 0)
#else
#    define NOAH_LIGHTING_SLICE_LAYER_STATUS(X)
#endif
#if NOAH_LIGHTING_FACT_MODE_STATUS
#    define NOAH_LIGHTING_SLICE_MODE_STATUS(X) X(mode_status, NOAH_LIGHTING_MODE_STATUS_BYTES, 1, 0)
#else
#    define NOAH_LIGHTING_SLICE_MODE_STATUS(X)
#endif
#if NOAH_LIGHTING_FACT_COMBO_LAYER
#    define NOAH_LIGHTING_SLICE_COMBO_LAYER(X) X(combo_layer, NOAH_LIGHTING_COMBO_SLICE_BYTES, 1, 0)
#else
#    define NOAH_LIGHTING_SLICE_COMBO_LAYER(X)
#endif
#if NOAH_LIGHTING_FACT_BEHAVIOUR_PHASE
#    define NOAH_LIGHTING_SLICE_BEHAVIOUR_PHASE(X) X(behaviour_phase, NOAH_LIGHTING_BEHAVIOUR_PHASE_BYTES, 1, 0)
#else
#    define NOAH_LIGHTING_SLICE_BEHAVIOUR_PHASE(X)
#endif
#if NOAH_LIGHTING_FACT_MACRO_PLAYING
#    define NOAH_LIGHTING_SLICE_MACRO_PLAYING(X) X(macro_playing, 1u, 1, 0)
#else
#    define NOAH_LIGHTING_SLICE_MACRO_PLAYING(X)
#endif
#if NOAH_LIGHTING_FACT_KEY_PRESSED
#    define NOAH_LIGHTING_SLICE_KEY_PRESSED(X) X(key_pressed, KEY_ORIGIN_BITMAP_SIZE, 1, 0)
#else
#    define NOAH_LIGHTING_SLICE_KEY_PRESSED(X)
#endif
#if NOAH_LIGHTING_FACT_MODIFIER
#    define NOAH_LIGHTING_SLICE_MODIFIER(X) X(modifier, 2u, 1, 0)
#else
#    define NOAH_LIGHTING_SLICE_MODIFIER(X)
#endif
#if NOAH_LIGHTING_FACT_HOST_LOCK
#    define NOAH_LIGHTING_SLICE_HOST_LOCK(X) X(host_lock, 1u, 1, 0)
#else
#    define NOAH_LIGHTING_SLICE_HOST_LOCK(X)
#endif
#if NOAH_LIGHTING_FACT_POINTER
#    define NOAH_LIGHTING_SLICE_POINTER(X) X(pointer, 1u, 1, 0)
#else
#    define NOAH_LIGHTING_SLICE_POINTER(X)
#endif
#if NOAH_LIGHTING_FACT_IDLE
#    define NOAH_LIGHTING_SLICE_IDLE_FACT(X) X(idle, 1u, 1, 0)
#else
#    define NOAH_LIGHTING_SLICE_IDLE_FACT(X)
#endif

#define NOAH_LIGHTING_SLICE_ROWS(X)             \
    NOAH_LIGHTING_SLICE_LAYER_ACTIVE(X)         \
    NOAH_LIGHTING_SLICE_LAYER_PREVIEWING(X)     \
    NOAH_LIGHTING_SLICE_POINTING_MODE_ACTIVE(X) \
    NOAH_LIGHTING_SLICE_COMBOS_HELD(X)          \
    NOAH_LIGHTING_SLICE_BEHAVIOUR_STATE(X)      \
    NOAH_LIGHTING_SLICE_COMBO_HELD(X)           \
    NOAH_LIGHTING_SLICE_BEHAVIOUR(X) \
    NOAH_LIGHTING_SLICE_LAYER_STATUS(X) \
    NOAH_LIGHTING_SLICE_MODE_STATUS(X) \
    NOAH_LIGHTING_SLICE_COMBO_LAYER(X) \
    NOAH_LIGHTING_SLICE_BEHAVIOUR_PHASE(X) \
    NOAH_LIGHTING_SLICE_MACRO_PLAYING(X) \
    NOAH_LIGHTING_SLICE_KEY_PRESSED(X) \
    NOAH_LIGHTING_SLICE_MODIFIER(X) \
    NOAH_LIGHTING_SLICE_HOST_LOCK(X) \
    NOAH_LIGHTING_SLICE_POINTER(X) \
    NOAH_LIGHTING_SLICE_IDLE_FACT(X)

typedef struct {
#define NOAH_LIGHTING_SLICE_FIELD(name, bytes, synced, gated) uint8_t name[bytes];
    NOAH_LIGHTING_SLICE_ROWS(NOAH_LIGHTING_SLICE_FIELD)
#undef NOAH_LIGHTING_SLICE_FIELD
    uint8_t terminator; // keeps the struct non-empty in a build without facts
} noah_lighting_facts_t;

#define NOAH_LIGHTING_SLICE_SYNCED_SIZE(name, bytes, synced, gated) +((synced) ? (bytes) : 0u)
enum { NOAH_LIGHTING_SYNCED_BYTES = 0u NOAH_LIGHTING_SLICE_ROWS(NOAH_LIGHTING_SLICE_SYNCED_SIZE) };
#undef NOAH_LIGHTING_SLICE_SYNCED_SIZE

// Whether this build sends any synced slice; the link carries none otherwise.
#if NOAH_LIGHTING_FACT_LAYER_STATUS || NOAH_LIGHTING_FACT_MODE_STATUS || NOAH_LIGHTING_FACT_COMBO_LAYER || NOAH_LIGHTING_FACT_BEHAVIOUR_PHASE || NOAH_LIGHTING_FACT_MACRO_PLAYING || NOAH_LIGHTING_FACT_KEY_PRESSED || NOAH_LIGHTING_FACT_MODIFIER || NOAH_LIGHTING_FACT_HOST_LOCK || NOAH_LIGHTING_FACT_POINTER || NOAH_LIGHTING_FACT_IDLE || NOAH_LIGHTING_FACT_LAYER_PREVIEWING || NOAH_LIGHTING_FACT_COMBOS_HELD || NOAH_LIGHTING_FACT_BEHAVIOUR_STATE || NOAH_LIGHTING_FACT_COMBO_HELD || NOAH_LIGHTING_FACT_BEHAVIOUR
#    define NOAH_LIGHTING_HAS_SYNCED_FACTS 1
#else
#    define NOAH_LIGHTING_HAS_SYNCED_FACTS 0
#endif

static inline void noah_lighting_subject_init(noah_lighting_subject_t *subject, uint8_t thing_kind, uint8_t thing, uint8_t slot) {
    *subject = (noah_lighting_subject_t){.thing_kind = thing_kind, .thing = thing, .slot = slot};
}

// ─── Contexts ───────────────────────────────────────────────────────────────

// What a target may consult: the program, the facts and the keymap.
typedef struct {
    const noah_profile_lighting_v1_program_t *program;
    noah_lighting_facts_t                    *facts; // main-context owned; fade overlay is restored before return
    uint32_t                                  paint_time; // synced animation time, separate from local gate time
    uint32_t                                  keymap_generation;
} noah_lighting_scene_t;

// What a paint receives: the scene, its parameters, the subject's identity,
// the LED, the time and what is below.
typedef struct {
    const noah_lighting_scene_t   *scene;
    const uint8_t                 *params;
    const noah_lighting_subject_t *subject;
    uint8_t                        led;
    uint32_t                       time;
    rgb_t                          below;
    bool                           below_painted;
    bool                          *effect_used;
} noah_lighting_paint_context_t;

// The colour below this paint: what the rules below painted, or the matrix
// effect's colour where nothing below paints.
rgb_t noah_lighting_below(const noah_lighting_paint_context_t *context);

// A palette colour as RGB.
rgb_t noah_lighting_palette_rgb(const noah_lighting_scene_t *scene, uint8_t index);

// The LEDs of a key bitmap.
noah_lighting_leds_t noah_lighting_key_leds(const uint8_t *keys);

// Where the board runs the matrix effect: false where the driver cannot say.
bool noah_lighting_effect_colour(uint8_t led, rgb_t *colour);

// Bumped whenever the keymap changes, so keymap-derived caches refresh.
uint32_t noah_lighting_keymap_generation(void);

// ─── Module shapes, one prototype per registry row ──────────────────────────

#define NOAH_LIGHTING_DECLARE_FACT(id, name, thing, layout, slots, selector, signatures, live, role) bool noah_lighting_fact_##name##_next(const noah_lighting_facts_t *facts, const uint8_t *params, uint16_t *cursor, noah_lighting_subject_t *subject);
NOAH_LIGHTING_STATE_FACT_ROWS(NOAH_LIGHTING_DECLARE_FACT)
#undef NOAH_LIGHTING_DECLARE_FACT

// A synced slice also states its bytes at rest (no gesture, combo or
// preview), which the link compares against to tell an active frame.
#define NOAH_LIGHTING_DECLARE_SLICE(name, bytes, synced, gated) \
    void noah_lighting_slice_##name##_collect(uint8_t *slice);  \
    void noah_lighting_slice_##name##_idle(uint8_t *slice);
NOAH_LIGHTING_SLICE_ROWS(NOAH_LIGHTING_DECLARE_SLICE)
#undef NOAH_LIGHTING_DECLARE_SLICE

#define NOAH_LIGHTING_DECLARE_TARGET(id, name, layout, place) noah_lighting_leds_t noah_lighting_target_##name(const noah_lighting_scene_t *scene, const uint8_t *params, const noah_lighting_subject_t *subject);
NOAH_LIGHTING_TARGET_ROWS(NOAH_LIGHTING_DECLARE_TARGET)
#undef NOAH_LIGHTING_DECLARE_TARGET

#define NOAH_LIGHTING_DECLARE_PAINT(id, name, layout, below, timed) bool noah_lighting_paint_##name(const noah_lighting_paint_context_t *context, rgb_t *colour);
NOAH_LIGHTING_PAINT_ROWS(NOAH_LIGHTING_DECLARE_PAINT)
#undef NOAH_LIGHTING_DECLARE_PAINT

// A timing returns the gate amount for a subject that has existed for
// `elapsed` ms, and the ms until its amount next changes (0: never).
#define NOAH_LIGHTING_DECLARE_TIMING(id, name, layout, timed, role) uint8_t noah_lighting_timing_##name(const uint8_t *params, uint32_t elapsed, uint32_t *next_change);
NOAH_LIGHTING_TIMING_ROWS(NOAH_LIGHTING_DECLARE_TIMING)
#undef NOAH_LIGHTING_DECLARE_TIMING

// ─── Dispatch (lighting_dispatch.c) ─────────────────────────────────────────

bool                 noah_lighting_fact_next(uint8_t kind, const noah_lighting_facts_t *facts, const uint8_t *params, uint16_t *cursor, noah_lighting_subject_t *subject);
noah_lighting_leds_t noah_lighting_target(uint8_t kind, const noah_lighting_scene_t *scene, const uint8_t *params, const noah_lighting_subject_t *subject);
bool                 noah_lighting_paint(uint8_t kind, const noah_lighting_paint_context_t *context, rgb_t *colour);
bool                 noah_lighting_paint_changes_with_time(uint8_t kind);
uint8_t              noah_lighting_timing(uint8_t kind, const uint8_t *params, uint32_t elapsed, uint32_t *next_change, bool *animating);
void                 noah_lighting_facts_collect(noah_lighting_facts_t *facts, bool local, bool synced);
void                 noah_lighting_facts_pack_synced(const noah_lighting_facts_t *facts, uint8_t *frame);
void                 noah_lighting_facts_unpack_synced(noah_lighting_facts_t *facts, const uint8_t *frame);
// Rebuilds the synced frame for the link; gated slices keep their bytes in
// `frame` unless `refresh_gated` is set.
void                 noah_lighting_facts_build_synced(uint8_t *frame, bool refresh_gated);
// The synced frame at rest.
void                 noah_lighting_facts_build_idle(uint8_t *frame);
void                 noah_lighting_facts_init_synced(noah_lighting_facts_t *facts);
bool                 noah_lighting_facts_collect_changed(noah_lighting_facts_t *facts, bool local, bool synced);
bool                 noah_lighting_facts_unpack_synced_changed(noah_lighting_facts_t *facts, const uint8_t *frame);
bool                 noah_lighting_facts_active(const uint8_t *frame, bool gated_only);
// Module-specific idle comparison; scratch never exceeds one slice.

// Capacity comes from each registry entry's subject domain, not runtime load.
uint8_t noah_lighting_fact_subject_slots(uint8_t kind);
uint8_t noah_lighting_fact_selector_parameter(uint8_t kind);
