// ────────────────────────────────────────────────────────────────────────────
// Autocorrect Feedback
// ────────────────────────────────────────────────────────────────────────────
//
// What the typing stage last did that lighting shows (D-F70): a typed form
// replaced with text, replaced by its macro, or a replacement undone, and the
// key that caused it. It shows for the replacement-feedback flash length
// (settings scalar 31) from the moment the replacement or the undo began to be
// typed, and a newer one takes its place. Only the master half's typing stage
// notes one; the slave half draws what split sync mirrors (D-F58).
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

#include "../key/runtime/slot/origin_registry.h"

// The flash length before any settings are live, and the compiled default.
#ifdef RGB_AUTOCORRECT_FEEDBACK_FLASH_MS
#    define AUTOCORRECT_FEEDBACK_FLASH_MS RGB_AUTOCORRECT_FEEDBACK_FLASH_MS
#else
#    define AUTOCORRECT_FEEDBACK_FLASH_MS 200
#endif

// None, then the lighting format's replacement-feedback semantics plus one.
typedef enum {
    NOAH_AUTOCORRECT_FEEDBACK_NONE = 0,
    NOAH_AUTOCORRECT_FEEDBACK_TEXT,
    NOAH_AUTOCORRECT_FEEDBACK_MACRO,
    NOAH_AUTOCORRECT_FEEDBACK_UNDO,
} noah_autocorrect_feedback_kind_t;

// What shows now. `keys` is the footprint of the key that ended the word, or
// of the Backspace that undid the replacement: every key of a combo whose
// output it was, and no key when the stage does not know which key it was.
typedef struct __attribute__((packed)) {
    uint8_t kind; // noah_autocorrect_feedback_kind_t
    uint8_t keys[KEY_ORIGIN_BITMAP_SIZE];
} noah_autocorrect_feedback_t;

void noah_autocorrect_feedback_reset(void);

// The key that ended a word, or whose Backspace may undo a replacement, as
// the stage reads it: its footprint is taken now, while a combo that sent it
// is most likely still down. A position outside the matrix is a key the
// stage does not know.
void noah_autocorrect_feedback_key(keypos_t key_pos);

// A replacement or an undo began to be typed, for the key last given.
void noah_autocorrect_feedback_note(noah_autocorrect_feedback_kind_t kind);

// What shows now. Reading ends a flash whose time has passed, so it reads the
// clock only while one shows.
noah_autocorrect_feedback_t noah_autocorrect_feedback(void);
