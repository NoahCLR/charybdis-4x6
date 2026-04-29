// ────────────────────────────────────────────────────────────────────────────
// Noah Keymap Authoring Surface
// ────────────────────────────────────────────────────────────────────────────
//
// Convenience authoring surface for keymap-owned translation units such as
// keymap.c and rgb_config.c.
// Runtime modules should include the narrower ids/runtime headers they
// actually consume instead of depending on this authoring bundle.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "noah_keymap_ids.h"                 // IWYU pragma: export
#include "lib/key/behavior/key_behavior.h"   // IWYU pragma: export
#include "keymap_materialize.h"              // IWYU pragma: export
#include "lib/pointing/defs/pd_mode_flags.h" // IWYU pragma: export
#include "lib/rgb/core/rgb_config_helpers.h" // IWYU pragma: export
#include "lib/rgb/core/rgb_helpers.h"        // IWYU pragma: export
