// ────────────────────────────────────────────────────────────────────────────
// Autocorrect
// ────────────────────────────────────────────────────────────────────────────
//
// Corrects the active profile's listed typos as they are typed (D-F69). It is
// the typing stage (macro/typing_stage.h): it reads every key the person's
// typing puts in a keyboard report, follows the word being typed through the
// host layout, and when a word ends as a typo holds the ending key back,
// types the correction through the macro engine and lets the held keys out in
// the order they were pressed. The rules are docs/spec/autocorrect-v1.md.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    // Characters of a word that are followed; a longer word is left alone.
    NOAH_AUTOCORRECT_WORD_MAX = 32u,
    // Key and modifier changes held back at once.
    NOAH_AUTOCORRECT_QUEUE_SIZE = 64u,
    // No correction starts with more than this many already waiting.
    NOAH_AUTOCORRECT_QUEUE_START_MAX = 16u,
    // The most key strokes one correction types, erasing included.
    NOAH_AUTOCORRECT_STROKES_MAX = 48u,
    // A correction's keys are each held this long and followed by as long a gap.
    NOAH_AUTOCORRECT_PACE_MS = 5u,
    // A key held this long is taken to repeat on the host.
    NOAH_AUTOCORRECT_HOLD_MS = 200u,
    // Part-of-word typos compared in one scan.
    NOAH_AUTOCORRECT_LOOKUP_STEP = 32u,
    // Regression policy for the 32-bit target's state, not an SRAM limit.
    NOAH_AUTOCORRECT_EMBEDDED_STATE_BUDGET = 304u,
};

void noah_autocorrect_init(void);
// Looks a finished word up, starts what the stage decided and lets held-back
// keys out, in bounded steps. The master half's scan.
void noah_autocorrect_scan(void);
