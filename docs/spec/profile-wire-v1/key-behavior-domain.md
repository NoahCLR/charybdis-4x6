# Key-behaviour domain `0x20`

Domain `0x20`, version 2, holds the profile's behaviours: for each keycode that
has one, what it does on each tap count and tier, its own thresholds, and where
it may act. The domain id, envelope, integer byte order and the four-byte
action encoding are [blob and digests](blob-and-digests.md)'s; how domains
make a profile is [the device-resident profile](../device-resident-profile.md)'s;
whether a press uses its behaviour is [the participation policy](../participation-policy.md)'s.
Refusal codes are named in [errors](errors.md).

## Ceilings

The build's tap depth `d` (five unless the build sets `KEY_BEHAVIOR_MAX_TAP_COUNT`;
the capability pages in [the HID channel](hid-channel.md) report it) sets the
ceilings (D-F14, [ADR](../../adr/0014-bigger-profiles.md)):

| Limit | Value |
| --- | --- |
| Behaviours (rows) | 128 |
| Branches (steps) in one row | `d` |
| Steps in the whole domain | `128 × d` (640 at depth five) |
| Repeat rate | 1–100 Hz |
| Payload | the blob's payload ceiling, and never more than 65,535 bytes, because the lookup index stores 16-bit row offsets |

A count over its ceiling refuses the domain with `CAPACITY_EXCEEDED`.

Held by: `tests/host/run_key_behavior_domain_v1_tests.sh` (`key_behavior_domain_v1_test.c`), `tests/host/run_tap_depth_eight_tests.sh`.

## Header

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | row count, at most 128 |
| 1 | 1 | reserved, zero |
| 2 | 2 | populated step count, `u16`, at most `128 × d`; must equal the steps the rows hold |

An empty domain is the four bytes `00 00 00 00`. Rows follow the header
directly and must end exactly at the domain's end.

Held by: `tests/host/run_key_behavior_domain_v1_tests.sh` (`key_behavior_domain_v1_test.c`), `tests/fixtures/key_behavior_domain_v1.fixture`.

## Rows

Each row is length-delimited:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 2 | row-body length, `u16`, excluding this field; at least 16 |
| 2 | 4 | target action: the keycode the behaviour belongs to |
| 6 | 2 | tap / hold threshold, ms; `0` follows the setting |
| 8 | 2 | long hold threshold, ms; `0` follows the setting |
| 10 | 2 | multi tap window, ms; `0` follows the setting |
| 12 | 1 | flags: bit 0 keeps auto-mouse anchored, bit 1 disabled; other bits zero |
| 13 | 1 | step count for this row, at most `d` |
| 14 | 4 | allowed layers, `u32`, one bit per layer; bits at or above the bank's 16 layers zero; an empty mask is allowed |
| 18 | … | the row's steps |

Rows are sorted by the target action's four canonical bytes, compared as
unsigned bytes; a repeated target is `DUPLICATE_TARGET`, a target out of order
`ROW_ORDER`. The steps must end exactly at the row's declared end
(`ROW_LENGTH`).

A threshold of zero follows the keyboard-wide setting in the
[settings domain](settings-domain.md): the tap / hold threshold follows
scalar 1, or scalar 0 (the tapping term, D-F47,
[ADR](../../adr/0047-dual-role-setting-is-the-tapping-term.md)) when the target
is a dual-role keycode; the long hold threshold follows scalar 2 and the multi
tap window scalar 3.

A disabled row keeps its steps, thresholds and allowed layers. A press takes
the key's normal action while its row is disabled or its source layer is not
allowed; the [participation policy](../participation-policy.md) states the
whole rule.

Held by: `tests/host/run_key_behavior_domain_v1_tests.sh` (`key_behavior_domain_v1_test.c`), `tests/host/run_key_behavior_lookup_tests.sh` (`key_behavior_lookup_test.c`), `tests/host/run_pd_mode_key_runtime_integration_tests.sh` (`pd_mode_key_runtime_integration_test.c`), `tests/host/run_participation_tests.sh`.

## Steps and branches

A step is one tap count's branch. Steps sit in strictly ascending tap index;
a repeated index is `DUPLICATE_STEP`, a lower one `STEP_ORDER`.

| Size | Field |
| ---: | --- |
| 1 | zero-based tap index, below `d` (`INVALID_TAP_INDEX` otherwise) |
| 1 | presence mask: bit 0 tap, bit 1 hold, bit 2 long hold; nonzero (`EMPTY_STEP`), other bits zero |
| 4 | tap action, only when bit 0 is set |
| 6 | hold branch, only when bit 1 is set |
| 6 | long-hold branch, only when bit 2 is set |

Present branches appear in tap, hold, long-hold order. A hold or long-hold
branch is:

| Size | Field |
| ---: | --- |
| 1 | hold style id |
| 1 | repeat rate, Hz: `1..100` for repeat while held, `0` for every other style |
| 4 | action |

Hold style ids are wire values and do not follow the firmware's C enum:

| Id | Hold style |
| ---: | --- |
| 1 | held until release |
| 2 | tapped at the threshold |
| 3 | repeated while held |
| 4 | tapped on release after the hold |

Any other id is `INVALID_HOLD_MODE`; a wrong repeat rate is
`INVALID_REPEAT_RATE`. The runtime's immediate-press style is derived, never
stored, and has no id.

Held by: `tests/host/run_key_behavior_domain_v1_tests.sh` (`key_behavior_domain_v1_test.c`), `tests/fixtures/key_behavior_domain_v1.fixture`.

## Actions and placement

No target, tap or hold action may be the empty action (`INVALID_TARGET` for a
target, `INVALID_ACTION` otherwise). Every action must name something the
build has: a bank layer, a supported pointing slot, a macro slot or a custom
key. The firmware then asks whether the action may sit where it is: as a key
(the target), as a tap, as a held-until-release hold, or as any other hold;
the action-kind table behind `noah_action_supported_at`
(`users/noah/lib/action/action_kind.c`) answers, the same table that checks
the authored profile at compile time. A step may send a custom key; it sends
that key's shortcut, never its behaviour (D-F26,
[ADR](../../adr/0026-custom-key-carries-a-shortcut.md)).

A structural fault is reported first. The first action that names something
missing or sits where it may not is recorded and reported as
`INVALID_REFERENCE` only once the whole profile's structure has validated,
with its row, step and field.

Held by: `tests/host/run_profile_validator_v1_tests.sh` (`profile_validator_v1_test.c`), `tests/host/run_profile_compiled_defaults_v1_tests.sh`.

## Validation and lookup

Validation is incremental: each step makes at most one read of at most 20
bytes (the header, a row length, a row's 16 fixed bytes, a step header, an
action, or a hold branch's two fixed bytes), so a whole-profile check never
holds the domain in RAM.

When a profile becomes active, the firmware walks the row lengths once (one
two-byte read a row) into an index of row offsets. A key press then
binary-searches the index, reading one four-byte target a probe, and decodes
only the matching row's fixed fields and the step it needs; earlier steps are
skipped by their presence masks.

Held by: `tests/host/run_key_behavior_domain_v1_tests.sh` (`key_behavior_domain_v1_test.c`), `tests/host/run_profile_domain_owner_round_trip_tests.sh`.

## Absent domain and compiled defaults

A profile without this domain runs the compiled defaults' behaviours. The
compiled defaults encode the authored `key_behaviors[]` in this format, sorted
by target, every row enabled and allowed on every bank layer.

Held by: `tests/host/run_profile_compiled_defaults_v1_tests.sh` (`profile_compiled_defaults_v1_test.c`), `tests/host/run_profile_domain_owner_round_trip_tests.sh` (`tests/fixtures/populated_profile_v4.fixture`, `tests/fixtures/maximum_profile_v4.fixture`).
