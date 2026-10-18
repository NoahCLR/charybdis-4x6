#pragma once

// QMK's tapping engine reads the compiled TAPPING_TERM unless a per-key hook
// is enabled, so both terms go through the hooks in qmk_portable_profile.c.
// The key runtime now decides every LT(), MT(), OSM(), TT() and OSL() key
// (D-F27) and QMK's tapping engine classifies none, so the hooks decide no
// key; removing them is later work.
#ifdef NOAH_PORTABLE_PROFILE_ENABLE
#    define TAPPING_TERM_PER_KEY
#    define QUICK_TAP_TERM_PER_KEY
#endif
