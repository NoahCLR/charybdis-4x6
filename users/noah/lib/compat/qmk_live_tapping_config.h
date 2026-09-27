#pragma once

// QMK's tapping engine reads the compiled TAPPING_TERM unless a per-key hook
// is enabled. The live profile's dual-role setting has to reach LT(), MT(),
// TT() and one-shot keys too, so route both terms through the hooks in
// qmk_portable_profile.c; the compiled value stays the default until a
// profile's settings are live.
#ifdef NOAH_PORTABLE_PROFILE_ENABLE
#    define TAPPING_TERM_PER_KEY
#    define QUICK_TAP_TERM_PER_KEY
#endif
