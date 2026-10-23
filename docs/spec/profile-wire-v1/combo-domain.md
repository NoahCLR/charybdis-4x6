# Combo domain `0x30` and native combo readback

Domain `0x30`, version 3, holds the profile's combos and the two combo-wide
timings; GET `0x06` reads back the native combo table the connected half
runs. The domain id, envelope, integer byte order and the four-byte action
encoding are [blob and digests](blob-and-digests.md)'s; the read envelope is
[the HID channel](hid-channel.md)'s; whether a press may join a combo and
whether a combo's output uses its behaviour are
[the participation policy](../participation-policy.md)'s. Refusal codes are
named in [errors](errors.md).

## Header

QMK keeps two values for all combos: the default combo window (`COMBO_TERM`),
which a combo without its own window uses, and the hold threshold
(`COMBO_HOLD_TERM`), one value for every combo. The header stores both, so
they exist with or without rows:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | row count, `0..128` |
| 1 | 3 | reserved, zero |
| 4 | 2 | default combo window, ms, `1..65535` |
| 6 | 2 | hold threshold, ms |

A zero default window is refused: every combo that follows it would never
fire. Zero rows is an explicit table with no combos; a profile without the
domain runs the compiled defaults' combos ([device-resident profile](../device-resident-profile.md)).
A payload shorter than `8 + 20 × row count` is refused before any row is read.

Held by: `tests/host/run_profile_validator_v1_tests.sh` (`profile_validator_v1_test.c`), `tests/host/run_effective_combo_runtime_tests.sh` (`effective_combo_runtime_test.c`), `tests/fixtures/combo_domain_v3.fixture`.

## Rows

Rows follow the header in priority order and end exactly at the domain's end
(`TRAILING_BYTES` otherwise). Each row is `12 + 4 × inputs` bytes, 20 to 76:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | input count, `2..16` |
| 1 | 1 | flags, below |
| 2 | 2 | combo window, ms; `0` follows the default window |
| 4 | 4 | allowed layers, `u32`, one bit per layer; bits at or above the bank's 16 layers zero; an empty mask is allowed |
| 8 | 4 | output action |
| 12 | `4 × inputs` | input actions, in declared order |

| Flag bit | Meaning |
| ---: | --- |
| 0 | must hold |
| 1 | must tap; never together with bit 0 |
| 2 | press in order |
| 3 | disabled |
| 4 | bypass output behaviour ([below](#combo-output-behaviour-permission-encoding)) |
| 5–7 | reserved, zero |

A row that follows the default window changes with it; a row with its own
window keeps it, even when it equals the default. A disabled combo keeps its
inputs, output, window and layers.

Row rules:

- The output is neither the empty action nor QMK keycode `0`; inputs are
  neither the empty action nor QMK keycodes `0` or `1` (`KC_NO`, `KC_TRNS`).
- Inputs are distinct as stored.
- Every action names something the build has (a bank layer, a supported
  pointing slot, a macro slot or a custom key), and the output may be a combo
  output (`noah_action_supported_at`, combo-output placement).
- Translated to native keycodes, the row still obeys
  `noah_profile_combo_v1_natives_valid`: a nonzero output and distinct inputs
  that are neither `KC_NO` nor `KC_TRNS`. This catches two actions that
  translate to one key.

A broken row refuses the domain as `INVALID_DOMAIN`, naming the row. The
validator and the native table ask the same native rule, so a profile the
keyboard accepts always materialises its combos.

Validation reads the header in one step, a row's fixed part in one, then its
inputs four at a time (16 bytes), never more than 20 bytes a step.

Held by: `tests/host/run_profile_validator_v1_tests.sh` (`profile_validator_v1_test.c`), `tests/host/run_effective_combo_runtime_tests.sh` (`effective_combo_runtime_test.c`), `tests/fixtures/combo_domain_v3.fixture`.

## The native table

At the safe boundary where a profile becomes active, the firmware walks the
header and rows once into a native table of 128 rows of up to sixteen
keycodes and a terminator, resolving each window that follows the default.
QMK's combo engine, combo-origin tracking and GET `0x06` all read that table;
typing and readback do no profile reads. An edit that leaves the combo domain
unchanged keeps the table. A table that fails to materialise exposes zero
combos and makes GET `0x06` answer status 3, never a partial table.

Without a stored table, the compiled combos run with their authored windows:
a `COMBO` row follows `COMBO_TERM`, a `COMBO_WINDOW` row has its own; every
compiled combo is enabled on every bank layer.

QMK runs combos in its long-combo representation (`EXTRA_LONG_COMBOS`): a
16-bit member state, sixteen-member combos and a sixteen-key press buffer.
Its queue of completed combos (`COMBO_BUFFER_LENGTH`, four) is a separate
limit.

Held by: `tests/host/run_effective_combo_runtime_tests.sh` (`effective_combo_runtime_test.c`), `tests/host/run_profile_domain_owner_round_trip_tests.sh` (`tests/fixtures/maximum_profile_v4.fixture`).

## Combo output behaviour permission encoding

Two stored switches decide whether a combo's output uses its keycode's
behaviour (D-F24, [ADR](../../adr/0024-combo-output-behaviour-permission.md)).
Both are stored inverted, so zero, the compiled default, means the output uses
its behaviour:

| Where | Bit | Set means |
| --- | --- | --- |
| [settings](settings-domain.md) header byte 5 | 0 | every combo output bypasses its behaviour |
| combo row flags | 4 | this combo's output bypasses its behaviour |
| GET `0x06` row page A flags | 5 | readback of the row bit |

Neither changes the chord: its inputs, output, window and allowed layers still
decide whether it fires. Settings readback carries header byte 5 unchanged.
The vetoes, the origin layer and release pairing are
[the participation policy](../participation-policy.md)'s.

Held by: `tests/host/run_participation_tests.sh` (`participation_test.c`), `tests/host/run_effective_combo_runtime_tests.sh` (`effective_combo_runtime_test.c`), `tests/host/run_profile_settings_v1_tests.sh` (`profile_settings_v1_test.c`), `tests/fixtures/combo_output_behaviors_v1.fixture`.

## Native combo readback — GET `0x06`

A read-only value reporting the connected half's native combo table, readout
version 3 (D-F36, [ADR](../../adr/0036-combo-readback-is-device-data.md)): the
active profile's combos, otherwise the compiled table. There is no SET or
save for it. The request takes a 16-bit page, low byte at request byte 4 and
high byte at byte 5; bytes 6 onward are zero. Every successful page carries
exactly 25 payload bytes. A table has `2 + 2 × row count` pages.

Page 0, metadata:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | readout version, `3` |
| 1 | 1 | row count, `0..128` |
| 2 | 1 | maximum inputs per row, `16` |
| 3 | 1 | layer count, `1..16` |
| 4 | 1 | QMK's combo enable now, `0` or `1` |
| 5 | 1 | build flags: bit 0 no timer, 1 strict timer, 2 custom trigger hook, 3 custom release hook, 4 custom repress hook, 5 fixed reference layer |
| 6 | 4 | FNV-1a 32-bit digest, below |
| 10 | 2 | default combo window, ms |
| 12 | 2 | hold threshold, ms |
| 14 | 1 | pages per row, `2` |
| 15 | 10 | reserved, zero |

Build flag bit 2 means a trigger hook of the user's own, which a client cannot
see into; the firmware's participation hook does not set it, since its
decisions are the profile's own controls.

Page 1 holds each layer's combo reference layer, one byte per layer for
`layer count` layers, the rest zero. With a fixed reference layer every entry
is that layer.

Row `n` occupies page A `2 + 2n` and page B `3 + 2n`:

| Page | Offset | Size | Field |
| --- | ---: | ---: | --- |
| A | 0 | 1 | row index |
| A | 1 | 1 | input count, `2..16` |
| A | 2 | 2 | native output keycode; `0` means a firmware callback |
| A | 4 | 2 | combo window in effect, ms, including a per-combo hook's answer |
| A | 6 | 1 | flags: bit 0 must hold, 1 must tap, 2 press in order, 3 follows the default window, 4 disabled, 5 bypass output behaviour |
| A | 7 | 4 | allowed layers, `u32`; bits at or above the layer count zero |
| A | 11 | 14 | native inputs 0–6, `u16` each, declared order; unused entries zero |
| B | 0 | 18 | native inputs 7–15, `u16` each; unused entries zero |
| B | 18 | 7 | reserved, zero |

Values are what QMK runs, not what a profile authored: inputs are distinct
nonzero native keycodes in the connected firmware's keycode numbering; must
hold follows QMK's precedence, including its suppression under no-timer
builds; transient per-combo engine state, callback bodies and custom
predicates are not reported. Without the profile owner, a build with a
per-combo window hook reports no row as following the default (the hook is
the user's own), and a build without one reports every row as following. A compiled
table reports every combo enabled on every bank layer. With a fixed reference
layer, QMK reads that layer's key directly; otherwise a reference layer equal
to the active layer uses the active stack's key.

The digest covers page-0 bytes 0–5 and 10–24, then page 1, then each row's two
pages in order. It covers QMK's combo enable, so it is not part of the
profile's identity. A consistent read is page 0, page 1, every row's pages,
then page 0 again with equal bytes and a digest matching the pages read; it
catches ordinary changes, not an atomic snapshot of the active layer stack.

| Status | When |
| ---: | --- |
| 1 | malformed request: zero request id, or a nonzero byte from 6 on |
| 2 | a page at or past `2 + 2 × row count` |
| 3 | the table cannot be read or represented: it failed to materialise, more than 128 rows, a row with fewer than 2 or more than 16 inputs or a repeated input, a layer count outside `1..16`, or a reference layer outside the bank |

Error replies carry an empty, zero payload. A build without combos answers a
valid empty table: zero rows, enable 0, no flags, both timings zero, and each
layer its own reference.

Held by: `tests/host/run_combo_readback_tests.sh` (`qmk_combo_readback_test.c`), `tests/host/run_effective_combo_runtime_tests.sh` (`effective_combo_runtime_test.c`), `tests/fixtures/combo_readback_v3.fixture`.
