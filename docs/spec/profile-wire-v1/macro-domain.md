# Macro domain `0x70`

Domain `0x70`, version 1, holds the 128 macros' steps in one bank (D-F30,
[ADR](../../adr/0030-keymap-and-macros-belong-to-the-profile.md)). It is the
only copy of the macros the firmware keeps; VIA is not supported (D-F31,
[ADR](../../adr/0031-via-is-not-supported.md)). Macro names live in the
[settings domain](settings-domain.md#names) (D-F43,
[ADR](../../adr/0043-macros-have-names-user-macros-retired.md)). The domain
id, envelope and byte order are [blob and digests](blob-and-digests.md)'s; how
domains make a profile is [the device-resident profile](../device-resident-profile.md)'s;
how a macro plays is [macro playback](../../architecture/macro-playback.md)'s.
Refusal codes are named in [errors](errors.md).

## Bank layout

The bank is the 128 macros back to back, slot 0 first, each ended by a zero
byte; an empty macro is its terminator alone. Unused capacity may follow the
128th terminator and may hold stale bytes, for instance after a macro was
shortened; playback never reads it. The domain's last byte must be zero. The
domain is at most 34,903 bytes, so at least 128 (every macro empty). This is
the encoding VIA's dynamic macro bank used, so any bank VIA could hold fits.

Macro slot `n` is played by keycode `QK_MACRO + n`.

Held by: `tests/host/run_profile_validator_v1_tests.sh` (`profile_validator_v1_test.c`), `tests/host/run_profile_keymap_macros_runtime_tests.sh` (`profile_keymap_macros_runtime_test.c`).

## Macro steps

Inside a macro, a byte other than `0x00` and `0x01` is text; `0x01` starts a
coded step:

| Bytes | Step |
| --- | --- |
| `0x02`–`0x7f` | one ASCII text byte |
| `0xc2`–`0xf4` and continuation bytes | one UTF-8 character, below |
| `01 01 k` | tap key `k` |
| `01 02 k` | press key `k` |
| `01 03 k` | release key `k` |
| `01 04` digits | delay of the decimal digits in ms, ended by the next byte, which the delay consumes, or by the macro's terminator |
| `01 05 p` | protection, only as the macro's first three bytes, below |

`k` is a one-byte basic keycode, nonzero. Any other code after `0x01`, or a
code cut off by the terminator, is refused. A UTF-8 character must be
canonical: no overlong form, no surrogate, nothing past U+10FFFF and no C1
control (U+0080–U+009F), and not cut off by the terminator. Playback
saturates a delay at 65,535 ms.

The protection step (D-F17,
[ADR](../../adr/0017-per-macro-protection-isolates-keyboard-output.md)) sets
the macro's uninterruptible playback: no prefix is automatic, `p = 1` on and
`p = 2` off; any other `p` is refused. What each policy does is
[macro playback](../../architecture/macro-playback.md#uninterruptible-playback)'s.

Held by: `tests/host/run_profile_validator_v1_tests.sh` (`profile_validator_v1_test.c`), `tests/host/run_macro_payload_tests.sh` (`macro_payload_test.c`).

## Validation and playback checks

Validation consumes the bank one byte at a time from reads of at most 20
bytes and checks framing, the step grammar, canonical UTF-8 and the
protection prefix. A refusal is `INVALID_DOMAIN`, and its location names the
macro (row) it falls in.

What a well-formed macro needs to play is checked when it plays, over every
window of the macro before any output: presses balanced by releases, at most
16 keys held at once, and its text typeable through the host layout in
[settings](settings-domain.md#scalars) scalar 27
([host layouts](../host-layouts-v1.md)). A macro that fails plays nothing.

Held by: `tests/host/run_profile_validator_v1_tests.sh` (`profile_validator_v1_test.c`), `tests/host/run_macro_payload_tests.sh` (`macro_payload_test.c`), `tests/host/run_macro_payload_engine_tests.sh` (`macro_payload_engine_test.c`).

## Runtime reads

With a stored bank active, playback reads it from the active profile: starting
a macro walks the bank to the slot's start, and each window of the macro is
decoded from there; neither a key lookup nor a refill reads the whole bank
into RAM. A newly active profile drops the cached slot positions. The safe
boundary excludes a running macro, so no playback spans two profiles.

Held by: `tests/host/run_profile_keymap_macros_runtime_tests.sh` (`profile_keymap_macros_runtime_test.c`).

## Absent domain and compiled defaults

A profile without this domain, or whose domain range cannot be found, plays
the authored macros: each `VIA_MACROS()` row's payload in `keymap.c`, compiled
by the firmware's macro compiler. The compiled defaults' domain is the
authored bank in this encoding (`authored_macro_bank.h`), which real-profile
validation refuses when it is stale.

Held by: `tests/host/run_profile_keymap_macros_runtime_tests.sh` (`profile_keymap_macros_runtime_test.c`), `tests/host/run_real_profile_validation_tests.sh`, `tests/host/run_profile_domain_owner_round_trip_tests.sh` (`tests/fixtures/populated_profile_v4.fixture`, `tests/fixtures/maximum_profile_v4.fixture`).
