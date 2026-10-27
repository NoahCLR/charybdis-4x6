#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// The one name rule every domain shares (D-F14): a length byte, then at most
// NOAH_PROFILE_NAME_MAX bytes of well-formed UTF-8 without control characters.
// A caller starts a name with its declared length and feeds its bytes in
// order; settings and pointing names validate through this alone.
enum { NOAH_PROFILE_NAME_MAX = 32 };
typedef struct {
    uint8_t remaining, utf8_remaining, utf8_min, utf8_max;
} noah_profile_name_v1_t;

static inline bool noah_profile_name_v1_begin(noah_profile_name_v1_t *name, uint8_t length) {
    if (!name || length > NOAH_PROFILE_NAME_MAX) return false;
    memset(name, 0, sizeof(*name));
    name->remaining = length;
    return true;
}

// Well-formed UTF-8 (no overlong forms, surrogates or code points past
// U+10FFFF) without C0 controls or DEL. Names are counted, so NUL is refused.
static inline bool noah_profile_name_v1_byte(noah_profile_name_v1_t *n, uint8_t b) {
    if (!n || !n->remaining) return false;
    n->remaining--;
    if (n->utf8_remaining) {
        if (b < n->utf8_min || b > n->utf8_max) return false;
        n->utf8_remaining--;
        n->utf8_min = 0x80;
        n->utf8_max = 0xbf;
        return true;
    }
    if (b < 0x20 || b == 0x7f) return false;
    if (b < 0x80) return true;
    n->utf8_min = 0x80;
    n->utf8_max = 0xbf;
    if (b >= 0xc2 && b <= 0xdf)
        n->utf8_remaining = 1;
    else if (b >= 0xe0 && b <= 0xef) {
        n->utf8_remaining = 2;
        if (b == 0xe0) n->utf8_min = 0xa0;
        if (b == 0xed) n->utf8_max = 0x9f;
    } else if (b >= 0xf0 && b <= 0xf4) {
        n->utf8_remaining = 3;
        if (b == 0xf0) n->utf8_min = 0x90;
        if (b == 0xf4) n->utf8_max = 0x8f;
    } else
        return false;
    // A sequence cannot run past the declared length.
    return n->utf8_remaining <= n->remaining;
}

static inline bool noah_profile_name_v1_complete(const noah_profile_name_v1_t *name) {
    return name && !name->remaining && !name->utf8_remaining;
}
