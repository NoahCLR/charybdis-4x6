// ────────────────────────────────────────────────────────────────────────────
// Noah's Charybdis 4x6 authored key data
// ────────────────────────────────────────────────────────────────────────────
//
// This translation unit owns the non-layout authored data:
//   - key_behaviors[]
//   - macro_dispatch()
//
// The physical layout itself stays in ../keymap.c.
// ────────────────────────────────────────────────────────────────────────────

#define KEY_CONFIG_DEFINE_BEHAVIORS
#define KEY_CONFIG_DEFINE_MACROS
#include "../key_config.h"
