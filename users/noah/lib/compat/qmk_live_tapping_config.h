#pragma once

// QMK's tapping engine reads the compiled TAPPING_TERM unless a per-key hook
// is enabled, so both terms go through the hooks in qmk_portable_profile.c.
// The key runtime decides every LT(), MT(), OSM(), TT() and OSL() key (D-F27).
// QMK would still decide TT() and OSL() to layers 16-31, which no keymap the
// firmware accepts can hold since VIA left (D-F31; is_tap_keycode_user in
// key/runtime/process.c), and keeps the dual-role setting for them.
#ifdef NOAH_PORTABLE_PROFILE_ENABLE
#    define TAPPING_TERM_PER_KEY
#    define QUICK_TAP_TERM_PER_KEY
#endif
