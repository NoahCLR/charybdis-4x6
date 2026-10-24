# Pointing domain `0x50`

Domain `0x50`, version 3, holds the profile's 32 pointing slots: what each
pointing mode is, how its movement turns ball motion into scrolling or
directional actions, and what its mouse-button overrides do. This file also
states how the firmware runs a pointing mode. The domain id, envelope and the
four-byte action encoding are [blob and digests](profile-wire-v1/blob-and-digests.md)'s;
where the domain is stored and which domains a half admits is the
[device-resident profile](device-resident-profile.md)'s; each slot's colour is
a row of the [lighting domain](rgb-domain-v1.md); the mirrored mode state on
the slave half is the [split runtime state](split-activity-sync.md)'s.

## Pointing slots

The keyboard has 32 pointing slots, ids `0..31`. A slot holds a pointing mode
or is empty. Keys reach a slot by its id, so a slot keeps its keys when its
mode changes, and nothing depends on a slot's name. One pointing mode runs at
a time; outside a mode the ball moves the pointer. Auto-sniping is a layer and
DPI policy and uses no slot.

The pointing domain is required in every profile the keyboard validates
(`MISSING_DOMAIN` otherwise, [errors](profile-wire-v1/errors.md#validation-results)).
A profile stores a slot's colour only in the lighting domain, one row per slot
whether or not the slot is stored here (D-F09,
[ADR](../adr/0009-thirty-two-pointing-slots-stored-sparsely.md)).

Held by: `tests/host/run_pointing_mode_tests.sh` (`pointing_mode_profile_registry_test.c`), `tests/host/run_profile_validator_v1_tests.sh` (`profile_validator_v1_test.c`), `tests/host/run_profile_compiled_defaults_v1_tests.sh` (`profile_compiled_defaults_v1_test.c`, the required domain mask).

## Keycodes

Each userspace keycode family owns one fixed block in QMK's user range,
reserved beyond what is supported, so adding slots or layers renumbers nothing
(D-F51, [ADR](../adr/0051-userspace-keycodes-in-fixed-blocks.md)). A slot
keycode names its slot; the mode comes from the active profile (D-F50,
[ADR](../adr/0050-pointing-keycodes-name-slots.md)).

| Block | Keycodes | Supported |
| --- | --- | --- |
| `0x7e40..0x7e7f` | none: inert | — |
| `0x7e80..0x7e9f` | `PD_SLOT_n`, hold slot `n` | 32, the whole block |
| `0x7ea0..0x7ebf` | `PD_SLOT_n_LOCK`, toggle slot `n`'s lock | 32, the whole block |
| `0x7ec0..0x7edf` | `LOCK_LAYER(n)`, toggle layer `n`'s lock | 16 (`0x7ec0..0x7ecf`) |
| `0x7ee0..0x7eff` | unassigned: inert | — |
| `0x7f00..0x7f7f` | `CUSTOM_KEY_0..127` | 128 |
| `0x7f80..0x7fff` | unassigned: inert | — |
| `0x7700..0x777f` | `QK_MACRO_0..127`, QMK's macro block | 128 |

Slot `n`'s mode flag is bit `n` of a 32-bit mask; slot 31 is the top bit.
Profiles store slot keys as action kinds 4 (held) and 5 (locked)
([action encoding](profile-wire-v1/blob-and-digests.md#action-encoding)), so
a block move changes the action-ABI digest and nothing else.

Held by: `tests/host/run_pointing_mode_tests.sh` (`pointing_mode_profile_registry_test.c`), `tests/host/run_profile_compiled_defaults_v1_tests.sh` (`profile_compiled_defaults_v1_test.c`, the keycode assertions).

## Header

All integers are unsigned and little-endian. The payload is an eight-byte
header, then `n` 128-byte records in strictly ascending slot id: `8 + 128 × n`
bytes, at most 4,104.

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | version, `3` |
| 1 | 1 | slot capacity, `32` |
| 2 | 1 | record size, `128` |
| 3 | 1 | record count `n`, `0..32` |
| 4 | 4 | reserved, zero |

| Refusal | Offset | When |
| --- | ---: | --- |
| `INVALID_HEADER` | 0, 1, 2 | a byte other than `3`, `32`, `128` |
| `INVALID_HEADER` | 3 | a count above 32 |
| `RESERVED` | 4..7 | the first nonzero reserved byte |
| `INVALID_LENGTH` | 0 | a payload shorter than 8 bytes, or a length other than `8 + 128 × n` |

The empty domain is `03 20 80 00 00 00 00 00`: every slot empty with no name.

Held by: `tests/host/run_profile_pd_v1_tests.sh` (`profile_pd_v1_test.c`), `tests/host/run_pd_profile_integration_tests.sh` (`pd_profile_integration_test.c`), `tests/fixtures/pd_mode_domain_v3.json`.

## Records

A slot's record is present exactly when the slot holds a mode (kind not `0`)
or is empty with a name. An omitted slot is empty with no name. Records run in
strictly ascending id, each below 32.

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | slot id |
| 1 | 1 | kind: empty `0`, directional `1`, scrolling `2` |
| 2 | 1 | pointer layer: keep available `0`, prefer typing `1` |
| 3 | 1 | axis; directional: vertical `0`, horizontal `1`, dominant axis `2`, eight directions `3`; scrolling: both `0`, horizontal only `1`, vertical only `2` |
| 4 | 2 | DPI; `0` inherits |
| 6 | 1 | held modifiers (scrolling); zero in directional modes |
| 7 | 1 | reserved, zero |
| 8 | 1 | name length in bytes, `0..32` |
| 9 | 23 | reserved, zero |
| 32, 34 | 2 each | directional thresholds X and Y, in sensor counts |
| 36, 40, 44, 48 | 4 each | taps: left, right, up, down |
| 52, 58, 64 | 6 each | mouse buttons 1, 2, 3 |
| 70 | 20 | scrolling or directional fields, below |
| 90 | 6 | reserved, zero |
| 96 | 32 | name, UTF-8, `length` bytes, the rest zero |

Bytes 70..89 of a scrolling mode:

| Offset | Size | Field |
| ---: | ---: | --- |
| 70, 72 | 2 each | horizontal and vertical thresholds: sensor counts that start an axis |
| 74, 76 | 2 each | horizontal and vertical divisors: sensor counts per scroll step |
| 78 | 2 | output interval, ms; `0` sends every report |
| 80 | 2 | expiry, ms: still time that drops banked motion |
| 82 | 2 | axis timeout, ms: still time that releases the axis |
| 84, 85 | 1 each | start ratio numerator and denominator |
| 86, 87 | 1 each | sustain ratio numerator and denominator |
| 88 | 1 | cross-axis decay divisor |
| 89 | 1 | invert: bit 0 horizontal, bit 1 vertical |

Bytes 70..89 of a directional mode:

| Offset | Size | Field |
| ---: | ---: | --- |
| 70, 74, 78, 82 | 4 each | taps: up-left, up-right, down-left, down-right |
| 86 | 1 | empty direction: neighbours take its share `0`, both neighbours `1`, nothing `2` |
| 87 | 1 | output: once per threshold step `0`, once per movement `1` |
| 88 | 2 | reserved, zero |

The record is the C structure `noah_pd_config_t` byte for byte:
`profile_pd_v1.c` asserts every field's offset against this table, its
alignment and little-endian byte order.

Held by: `tests/host/run_profile_pd_v1_tests.sh` (`profile_pd_v1_test.c`), `tests/fixtures/pd_mode_domain_v3.json`.

## Record rules

A record is refused at the first rule it breaks, in this order; offsets are
record-relative inside a record and payload-relative for the whole domain:

| Refusal | Offset | When |
| --- | ---: | --- |
| `INVALID_ID` | 0 | an id at or past 32, or not above the previous record's |
| `RESERVED` | 7, 9 or 90 | a nonzero reserved byte |
| `INVALID_NAME` | 8 | a name that breaks [the name rule](#names) |
| `INVALID_POLICY` | 1 | kind above 2, pointer layer above 1, or axis above 3 |
| `INVALID_PARAMETER` | 2 | an empty slot with any nonzero byte in 2..6 or 32..89 |
| `INVALID_NAME` | 8 | a mode with an empty name |
| `INVALID_ACTION` | tap or button | a straight-direction tap or a button record that breaks [taps](#taps) or [mouse buttons](#mouse-buttons) |
| `NONCANONICAL` | 1 | a present record that is empty with no name |

Then, by kind:

- **Eight directions** (kind 1, axis 3): held modifiers zero, else
  `INVALID_PARAMETER` at 6; both thresholds nonzero, else
  `INVALID_PARAMETER` at 32; each diagonal tap valid, else `INVALID_ACTION`
  at its offset; byte 86 at most 2 and byte 87 at most 1, else
  `INVALID_POLICY` at that byte; bytes 88..89 zero, else `RESERVED` at 88.
- **Other directional** (kind 1, axis 0..2): held modifiers and diagonals
  zero, else `INVALID_PARAMETER` at 6; bytes 86 and 87 and 88..89 as above;
  threshold X nonzero unless vertical and threshold Y nonzero unless
  horizontal, else `INVALID_PARAMETER` at 32; a vertical mode has threshold X
  zero and no left or right tap, a horizontal mode threshold Y zero and no up
  or down tap, else `INVALID_PARAMETER` at 32. A dominant-axis mode needs both
  thresholds. A direction may have no tap.
- **Scrolling** (kind 2): axis at most 2 and bytes 32..51 zero, else
  `INVALID_PARAMETER` at 3. Else `INVALID_PARAMETER` at 70 unless: both
  thresholds, both divisors and the axis timeout are nonzero; expiry is at
  least the axis timeout; all four ratio terms are nonzero; start and sustain
  are each at least 1:1; sustain does not exceed start; decay is at least 2;
  invert is at most 3. Held modifiers are any modifier mask.

Integer bounds are otherwise the fields' widths. Validation reads the header
in one step, then each record in steps of at most 20 bytes, and holds no more
than one record.

Held by: `tests/host/run_profile_pd_v1_tests.sh` (`profile_pd_v1_test.c`, `check_sparse_rules`, `test_bounded_iterator`), `tests/host/run_pd_profile_integration_tests.sh` (`pd_profile_integration_test.c`), `tests/fixtures/pd_mode_domain_v3.json` (`invalid`).

## Names

A name follows the rule every domain shares: at most 32 bytes of well-formed
UTF-8 (no overlong or surrogate encodings, nothing past U+10FFFF), no ASCII C0
control or DEL, a length that does not cut a sequence, and zero bytes after
it. A full 32-byte name has no terminator. A mode needs a name; an empty slot
may keep one, and is then stored.

Held by: `tests/host/run_profile_pd_v1_tests.sh` (`profile_pd_v1_test.c`, `check_names`), `tests/fixtures/pd_mode_domain_v3.json` (`longest-name`, the `name-*` vectors).

## Taps

A tap is four bytes:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 2 | keycode; `0` sends nothing and all four bytes are zero |
| 2 | 1 | modifier policy: inherit `0`, hide the modifiers in the mask `1`, exact shortcut `2` |
| 3 | 1 | mask: the eight HID modifier bits; nonzero exactly for policy 1 |

A tap's keycode is one of:

| Keycodes | What |
| --- | --- |
| `0x0004..0x00c2` | keyboard `0x04..0xa4`, system `0xa5..0xa7`, consumer `0xa8..0xc2` |
| `0x00d1..0x00d8` | mouse buttons 1–8, a click (D-F22, [ADR](../adr/0022-pointing-modes-click-mouse-buttons.md)) |
| `0x7700..0x777f` | macro slots 0–127, a macro start, policy 0 and mask 0 only (D-F23, [ADR](../adr/0023-pointing-outputs-start-macros.md)) |
| high byte `0x01..0x1f` with a low-nibble modifier bit, low byte `0x04..0xa4` | a modified keyboard key |

Everything else is refused: standalone modifiers, mouse movement, wheel and
acceleration, modified mouse buttons, modified consumer or system codes,
layer and pointing keycodes, custom keys. No output of a mode can activate a
mode or leave a hold unbalanced.

Held by: `tests/host/run_profile_pd_v1_tests.sh` (`profile_pd_v1_test.c`), `tests/fixtures/pd_mode_domain_v3.json` (`mouse-buttons`, `macros`, `mouse-movement-tap`, `modified-mouse-button-tap`, `macro-*`).

## Mouse buttons

Each of buttons 1–3 has a six-byte record that applies while the mode runs:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | kind: pass through `0`, consume `1`, tap `2`, hold modifiers `3` |
| 1 | 1 | modifiers |
| 2 | 4 | tap |

Pass through and consume have bytes 1..5 zero. Tap has modifiers zero and a
nonempty valid tap. Hold modifiers has a nonzero modifier mask and the tap
zero. Kind above 3 is refused. Both movements use button records.

Held by: `tests/host/run_profile_pd_v1_tests.sh` (`profile_pd_v1_test.c`), `tests/fixtures/pd_mode_domain_v3.json` (`mouse-buttons`).

## Compiled defaults

The authored slots are `noah_pd_defaults[]` in the keymap's `pd_config.c`.
The compiled domain stores the slots that hold a mode or a name:

| Slot | Name | Movement | Outputs |
| ---: | --- | --- | --- |
| 0 | Dragscroll | scrolling, both axes | DPI 100 |
| 1 | Volume | vertical, threshold 60 | volume up / down, DPI 400 |
| 2 | Brightness | vertical, threshold 60 | brightness up / down, DPI 400 |
| 3 | Zoom | vertical, threshold 80 | Cmd = / Cmd −, DPI 400 |
| 4 | Arrow | dominant axis, thresholds 40 / 50 | arrows, up and down hiding both Alts; button 1 holds right Shift, buttons 2 and 3 Cmd C and Cmd V exact; prefers typing; DPI 400 |
| 5 | Pinch | scrolling, vertical only | holds left Cmd; DPI 100 |
| 6 | Undo / Redo | horizontal, threshold 40 | Cmd Z / Shift Cmd Z, DPI 100 |
| 7 | Back / Forward | horizontal, threshold 60 | mouse buttons 4 / 5, once per movement |
| 8–31 | — | empty | not stored |

Both scrolling slots take their tuning from the userspace `NOAH_DRAGSCROLL_*`
values. The compiled encoder and validators are the ones every profile uses;
the compiled domain is checked against the golden compiled profile.

Held by: `tests/host/run_profile_compiled_defaults_v1_tests.sh` (`profile_compiled_defaults_v1_test.c`, `tests/fixtures/compiled_profile_pd_v2.fixture`), `tests/fixtures/pd_mode_domain_v3.json` (`presets`), `tests/host/run_pd_profile_integration_tests.sh` (`pd_profile_integration_test.c`).

## Activation and locks

- Pressing `PD_SLOT_n` activates slot `n` while it is held; the mode ends
  when the last key holding it is released. Two keys may hold one mode; the
  first is its owner key, and a later key takes a released owner's place.
- `PD_SLOT_n_LOCK` toggles slot `n`'s lock. A locked mode stays on until it
  is unlocked; releasing a hold key does not end it.
- Pressing a slot's hold key while that slot is locked unlocks it; the mode
  stays on until the key is released.
- Activating or locking a slot first unlocks any other locked mode and ends
  any other held mode. The new mode's owner keys replace the old one's.
- A slot with no mode never activates: its keys are inert and own nothing.

Changing modes releases only the old mode's outputs and clears its banked
motion, axis and movement state.

Held by: `tests/host/run_pd_runtime_tests.sh` (`pd_runtime_test.c`, `test_an_empty_slot_hold_key_owns_nothing`, the owner tests), `tests/host/run_pointing_mode_key_runtime_integration_tests.sh` (`pointing_mode_key_runtime_integration_test.c`, `test_interrupted_locked_pointing_mode_press_unlocks_on_press_and_releases_momentary_hold`, `test_upper_slot_hold_lock_dpi_and_disabled_slot`, `test_last_slot_hold_lock_dpi_and_disabled_slot`).

## Pointer layer and DPI

Pointer layer `0` keeps the pointer layer on while the mode runs: a locked
mode anchors auto-mouse, and a locked scrolling mode turns QMK's auto-mouse
toggle on, then off again at unlock if the lock turned it on. Pointer layer
`1` prefers typing: activation turns the pointer layer off, and it stays off
while the mode runs.

The sensor's DPI follows, in order: a scrolling mode's DPI, or the normal DPI
when it inherits; the sniping DPI while sniping or on the auto-sniping layer;
a directional mode's DPI when it has one; otherwise the normal DPI. The normal
and sniping DPI are [settings](profile-wire-v1/settings-domain.md#scalars)
scalars 18 and 19.

Held by: `tests/host/run_pd_runtime_tests.sh` (`pd_runtime_test.c`, `test_layer_state_set_*`, `test_a_locked_modes_anchor_is_not_taken_back_after_a_qmk_reset`), `tests/host/run_pointer_layer_policy_tests.sh`, `tests/host/run_pointing_mode_key_runtime_integration_tests.sh` (`test_upper_slot_hold_lock_dpi_and_disabled_slot`).

## Directional movement

One engine runs every directional mode (D-F45,
[ADR](../adr/0045-one-directional-engine.md); D-F42,
[ADR](../adr/0042-directional-modes-read-eight-directions.md)). The axis says
which directions exist: vertical up and down, horizontal left and right,
dominant axis the four straight ones, eight directions all eight. A
directional mode never moves the pointer.

- Motion is counted in units of threshold X × threshold Y, so each axis moves
  in steps of its own threshold. Motion toward a direction that does not
  exist goes to the nearest one that does.
- A smoothed heading, moved half-way toward each report, picks the nearest
  available direction and keeps it until another is nearer by more than 15°.
  A report against the held direction releases it at once. After 150 ms
  without motion the next report chooses afresh.
- Only progress along the held direction counts; sideways drift is dropped.
  A diagonal step is one threshold step along the diagonal, and a newly held
  diagonal starts half a step back, so a turn between axes passes through it
  without firing.
- Each whole step is one output. A report sends at most 4 outputs; the
  backlog keeps at most 32 whole steps and the part of one, and a still report
  drains it under the same limit.

Byte 86, what a direction that exists but has no tap does: `0` its neighbours
take its share; `1` it taps both compass neighbours 45° either side, the one
the heading leans toward first, an empty neighbour sending nothing, and counts
as two outputs (only eight directions has such neighbours, so elsewhere `1`
acts as `0`); `2` its motion is consumed and sends nothing, each step still
counting as one output against the per-report limit.

Byte 87, how often a direction sends: `0` once per threshold step; `1` once
per movement (D-F07, [ADR](../adr/0007-directional-mode-can-send-once-per-movement.md)).
A movement starts with the first motion after the mode starts or after
150 ms without motion. Its first step that sends anything spends it; from then
on only a direction more than 90° from the one that sent may send, a whole
step of its own, and its sending spends the movement in turn. Other motion is
dropped, not banked. A step that sends nothing leaves the movement unspent.

Held by: `tests/host/run_pointing_mode_handlers_tests.sh` (`pointing_mode_handlers_test.c`, `test_configured_eight_directions`, `test_configured_empty_directions_and_single_axes`, `test_configured_dominant_axis`, `test_configured_once_per_movement`).

## Scrolling movement

A scrolling mode turns motion into wheel steps and never moves the pointer.
Inverted axes are negated first. Motion banks per axis; banked motion
saturates at ±2^24 counts.

- An axis starts when its banked motion reaches its threshold and beats the
  other axis by the start ratio; when both or neither qualify, nothing starts.
- A started axis holds while it stays above its threshold and beats the other
  by the sustain ratio. When neither axis is above its threshold it releases;
  when the other axis meets the start rule it takes over; otherwise the axis
  holds through the wobble.
- Each output takes whole divisor steps from the held axis, keeps the
  remainder, and divides the other axis's bank by the decay divisor, zeroing
  it below its threshold. Wheel values are clamped to the report's range.
- Outputs are at least the interval apart; motion still banks between them.
- Still for longer than the axis timeout releases the axis; longer than the
  expiry drops the banks too.
- Byte 3 limits which axes scroll (D-F08,
  [ADR](../adr/0008-scrolling-mode-can-scroll-one-axis.md)). A gesture held
  on an axis the mode does not scroll runs as above but sends nothing, so its
  motion neither scrolls later nor leaks into the other axis.
- Held modifiers are owned for as long as the mode runs. Keys typed meanwhile
  do not see a modifier only the mode holds.

Held by: `tests/host/run_pointing_mode_handlers_tests.sh` (`pointing_mode_handlers_test.c`, the `test_dragscroll_*` tests, `test_configured_scroll_one_axis`), `tests/host/run_pointing_mode_key_runtime_integration_tests.sh` (`test_pinch_masks_mode_owned_gui_*`, `test_pinch_keeps_physically_held_gui_visible_during_concurrent_plain_key_processing`).

## Outputs

- **Key taps** go out as one owned press and release, never through QMK's key
  processing, so they reach no behaviour. A usage another owner (a physical
  key, a behaviour, a macro) already holds stays down: the tap adds no edge
  and cannot release it. Policy 1 hides the masked modifiers for the tap and
  policy 2 hides all of them, each restoring them after; policy 0 keeps them.
- **Clicks** count as one output each, go through the owned-keycode ledger as
  one balanced press and release, and never release a button another owner
  holds. They reach neither the mode's button overrides nor the button's
  behaviour. The modifier policy applies; when hiding modifiers changes what
  the host sees, the click waits 8 ms after that report, and the modifiers
  return 8 ms after the button is up. A click drops the report's pending
  motion first.
- **Macro starts** begin playback through the profile macro provider after
  settling pending fallback holds; playback inherits the held modifiers and
  keeps the macro's own protection. A start counts against the per-report
  limit whether or not it plays, and spends a once-per-movement movement only
  when playback starts. An empty, unplayable or busy slot sends nothing; a
  refused start is not queued.

Held by: `tests/host/run_pointing_mode_handlers_tests.sh` (`pointing_mode_handlers_test.c`, `test_configured_mouse_button_clicks`, `test_configured_macro_start_accounting`; `pd_output_ownership_test.c`), `tests/host/run_owned_keycode_tests.sh` (`owned_keycode_test.c`, `test_literal_tap_never_releases_another_owners_usage`, `test_click_never_releases_a_physically_held_button`, `test_fresh_modifier_settles_before_a_mouse_button`).

## Button overrides at runtime

While a mode runs, a press of mouse button 1, 2 or 3 follows its record: pass
through runs the button as without the mode; consume does nothing; tap sends
its tap at the press; hold modifiers owns its modifiers until the release. A
press the mode takes never reaches the button's own behaviour, and its release
goes to the mode that took it, even after another mode replaced it. The
release of a press the mode did not take stays with the behaviour.

Held by: `tests/host/run_pointing_mode_key_runtime_integration_tests.sh` (`test_consumed_buttons_run_no_authored_behavior`, `test_button_release_follows_its_press_across_mode_changes`, `test_configured_consume_button_runs_nothing`, `test_configured_button_click_runs_no_behaviour`, `test_factory_arrow_paste_button_never_holds_itself`), `tests/host/run_pointing_mode_handlers_tests.sh` (`test_configured_release_ownership_and_custom_slot`).

## Effective slots and publication

The firmware keeps all 32 slots in RAM as validated records: stored records
copied in unchanged, every omitted slot empty with no name. It fills them at
activation of a profile, or when a working-copy edit changes this domain,
reading the header and each record in steps of at most 20 bytes; motion and
key handling do no storage read, decode or allocation. A build without the
live-profile owner loads the compiled slots at startup.

A failed read or validation leaves the slots unavailable, never partly filled
or mixed between generations: no mode activates, and profile status reports
the pending [active kind](profile-wire-v1/hid-channel.md#active-kind) 3.
Warming the slots from the compiled defaults reads the compiled pointing
payload directly, sorting no behaviour row.

A new generation or working copy activates only at the safe boundary: with a
mode active or locked, or a consumed button release pending, activation waits
and reports the persistent-intent reason. It never force-releases a key.

Held by: `tests/host/run_pd_profile_integration_tests.sh` (`pd_profile_integration_test.c`, `assert_cache_matches_domain`), `tests/host/run_profile_compiled_defaults_v1_tests.sh` (`profile_compiled_defaults_v1_test.c`, the cache warm-up), `tests/host/run_profile_activation_policy_tests.sh` (`profile_activation_policy_test.c`, `test_configured_pd_blocks_until_release_and_does_not_wrap`), `tests/host/run_profile_wire_v1_tests.sh` (`qmk_profile_channel_test.c`, the pending kind).

## Compatibility

The firmware reads only version 3 (D-F10,
[ADR](../adr/0010-firmware-accepts-only-current-formats.md)): another
envelope version is `UNKNOWN_DOMAIN_VERSION`, and a version-2 payload's
header is `INVALID_HEADER` at 0. Nothing is translated. A saved profile in an
older format or action vocabulary is not used; the
[device-resident profile](device-resident-profile.md) says what runs instead.

Held by: `tests/host/run_profile_blob_v1_tests.sh` (`profile_blob_v1_test.c`), `tests/fixtures/pd_mode_domain_v3.json` (`version-2-payload`, `wrong-record-size`).
