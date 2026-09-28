// ──────────────────────────────────────────────────────────────────────────
// QMK Split Frame CRC Contract
// ──────────────────────────────────────────────────────────────────────────
//
// The frame CRC lives in the fork's serial transport; userspace only turns it
// on (qmk_split_transport.mk) and checks here that the fork provides it, so a
// build against a fork without it fails instead of silently sending
// unchecked frames.
// ──────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#ifdef SPLIT_TRANSPORT_CRC
#    include "transactions.h"

#    ifndef QMK_SPLIT_TRANSPORT_CRC_VERSION
#        error "The split frame CRC needs the fork's serial transport CRC (sol at f4f77a2aaf or later); NOAH_SPLIT_CRC=no builds without it"
#    endif
_Static_assert(QMK_SPLIT_TRANSPORT_CRC_VERSION == 1, "QMK split frame CRC contract changed");
#endif
