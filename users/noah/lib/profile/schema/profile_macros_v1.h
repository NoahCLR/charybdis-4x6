#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "profile_versions.h"

// Domain 0x70 version 1: the macro bank, in the encoding VIA's dynamic macro
// bank uses (what macro_payload_decode_qmk.c reads and Ark's codec writes).
// Every macro, slot 0 first, ends with a zero byte; the domain is the 128
// macros back to back, so an empty macro is its terminator alone and the
// domain holds 128 terminators followed by optional unused capacity. The final
// bank byte must be zero, as it is in the existing bank. Inside a
// macro: text bytes 0x02..0x7F, canonical UTF-8, or 0x01 followed by a code:
// 1, 2 or 3 (tap, press, release) and a nonzero keycode; 4 and decimal digits
// (a delay) ended by any other byte, which the delay consumes, or by the
// macro's terminator; 5 and a protection level of 1 or 2, only as the macro's
// first two bytes. The capacity is today's VIA bank, so every bank VIA could
// hold fits. Whether a macro can play (its presses released, every tap in one
// window, its text typeable on the host) is checked when it plays, as it is
// for VIA's bank.
enum {
    NOAH_PROFILE_MACROS_V1_VERSION  = NOAH_PROFILE_DOMAIN_VERSION_MACROS,
    NOAH_PROFILE_MACROS_V1_SLOTS    = 128u,
    NOAH_PROFILE_MACROS_V1_MAX_SIZE = 34903u,
    NOAH_PROFILE_MACROS_V1_PREFIX   = 0x01u,
    NOAH_PROFILE_MACROS_V1_TAP      = 0x01u,
    NOAH_PROFILE_MACROS_V1_DOWN     = 0x02u,
    NOAH_PROFILE_MACROS_V1_UP       = 0x03u,
    NOAH_PROFILE_MACROS_V1_DELAY    = 0x04u,
    NOAH_PROFILE_MACROS_V1_PROTECT  = 0x05u,
};

// One byte per call, so the whole-profile validator keeps its 20-byte step.
typedef struct {
    uint32_t offset;  // domain bytes consumed, kept by the caller
    uint8_t macros;   // macros ended so far
    uint8_t lexeme;   // where in a macro's grammar the next byte falls
    uint8_t position; // bytes into the current macro, saturating at 255
    uint8_t utf8_need, utf8_have;
    uint8_t last_byte;
    uint8_t utf8[4];
} noah_profile_macros_v1_validation_t;

bool noah_profile_macros_v1_length_valid(uint32_t length, uint8_t slots);
bool noah_profile_macros_v1_consume(noah_profile_macros_v1_validation_t *state, uint8_t byte, uint8_t slots);
bool noah_profile_macros_v1_complete(const noah_profile_macros_v1_validation_t *state, uint8_t slots);
