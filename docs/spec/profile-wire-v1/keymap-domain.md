# Keymap domain `0x60`

Domain `0x60`, version 1, holds the keymap: the native QMK keycode at every
matrix position of every layer (D-F30,
[ADR](../../adr/0030-keymap-and-macros-belong-to-the-profile.md)). It is the
only copy of the keymap the firmware keeps; VIA is not supported (D-F31,
[ADR](../../adr/0031-via-is-not-supported.md)). The domain id, envelope and
byte order are [blob and digests](blob-and-digests.md)'s; how domains make a
profile is [the device-resident profile](../device-resident-profile.md)'s.
Refusal codes are named in [errors](errors.md).

## Layout

There is no inner header. The payload is one `u16` native QMK keycode per
position, layer 0 first, each layer row by row and each row column by column:
keycode `(layer, position)` sits at byte `2 × (layer × positions + position)`,
with `position = row × columns + column`. The length is exactly
`layers × positions × 2`: 1,920 bytes for this keyboard's 16 layers of 60
positions (10 rows of 6). The four matrix positions with no switch are
included. Any other length is refused.

Held by: `tests/host/run_profile_validator_v1_tests.sh` (`profile_validator_v1_test.c`).

## Keycodes a position may hold

A keycode means what it means in the connected firmware's QMK. Empty
(`KC_NO`) and transparent (`KC_TRNS`) positions are valid; every other keycode
must be one a key may hold, as the action-kind table behind
`noah_action_supported_at` (`users/noah/lib/action/action_kind.c`) says for the
key placement, the same table that checks the authored keymap at compile time.
So layer actions the firmware does not own (`DF()`), unassigned userspace
keycodes and layer targets outside 0–15 (`MO(16)`, `TG(16)`, `TO(16)`,
`TT(16)`, `OSL(16)`) are refused, while macro keys and custom keys are
accepted.

Validation reads ten keycodes (20 bytes) a step. A refusal is
`INVALID_DOMAIN`, and its location names the layer (table) and the position
(row) of the first refused keycode.

Held by: `tests/host/run_profile_validator_v1_tests.sh` (`profile_validator_v1_test.c`), `tests/host/run_profile_compiled_defaults_v1_tests.sh` (`profile_compiled_defaults_v1_test.c`).

## Runtime reads

`keymap_key_to_keycode` is the one keymap reader: QMK, combo-origin
reconstruction, transparent-key resolution and the lighting layer-coverage map
all use it. With a stored keymap active it reads two bytes per lookup from the
active profile; a failed read, a layer past the bank or a position past the
matrix answers `KC_NO`. With none active it reads the authored `keymaps[][][]`
directly. No lookup serialises the compiled profile. A newly active keymap
invalidates the lighting coverage map before its next render.

Held by: `tests/host/run_profile_keymap_macros_runtime_tests.sh` (`profile_keymap_macros_runtime_test.c`).

## Absent domain and compiled defaults

A profile without this domain, or whose domain range cannot be found, runs
the authored keymap. The compiled defaults encode the authored
`keymaps[][][]` in this layout.

Held by: `tests/host/run_profile_keymap_macros_runtime_tests.sh` (`profile_keymap_macros_runtime_test.c`), `tests/host/run_profile_domain_owner_round_trip_tests.sh` (`tests/fixtures/populated_profile_v4.fixture`, `tests/fixtures/maximum_profile_v4.fixture`).
