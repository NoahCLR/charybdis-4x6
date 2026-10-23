# Macros

The macro engine and what feeds it. How a macro starts, plays and reaches the
report is [macro-playback.md](../../../../docs/architecture/macro-playback.md);
the bank and its steps are the
[macro domain](../../../../docs/spec/profile-wire-v1/macro-domain.md).

## Layout

- `profile_macro_provider.c`: the 128 slots' source, the active profile's bank
  or the authored `VIA_MACROS()` strings, and the entry points
  `noah_profile_macro_start` and `noah_profile_macro_try_play`.
- `macro_slot_provider.c`: slot metadata and the one shared window; preflight
  over every window before a start.
- `macro_payload_decode_qmk.c`: bank bytes to IR, one window at a time.
- `macro_payload_parse.c`, `macro_payload_keycodes.c`: authored strings
  (`{KC_LGUI,KC_C}`, `{+KC_LSFT}`) to IR; `macro_payload_encode.c`: IR or an
  authored string to bank bytes, which real-profile validation uses to check
  `authored_macro_bank.h`.
- `macro_payload_run.c`: preflight, the engine, Unicode entry and the report
  filters BK's report hooks call.
- `macro_input_guard.c`: the press guard for uninterruptible playback.
- `host_layout.c`: layout lookup over the generated `host_layout_tables.c`.

## The IR

One window, `macro_payload_ir_t`, holds at most 512 bytes of steps, each an
opcode then its operands:

| Opcode | Operands |
| --- | --- |
| 1 text | a length (1–255), then that many ASCII bytes |
| 2 delay | milliseconds, two bytes little-endian |
| 3 press, 4 release | one basic keycode |
| 5 chord | a count (1–16), then the keycodes |
| 6 Unicode | one scalar, three bytes little-endian |

`protection` and `more` ride beside the bytes, outside the steps. A window
ends on a whole step; `macro_payload_stream_cursor_t` carries the bank offset
and the held-key balance into the next one.

## The engine

All state is static: `macro_payload_engine` and the suppressed-usage set in
`macro_payload_run.c`, the window (`macro_slot_active_ir`) in
`macro_slot_provider.c`, the slot metadata (`profile_macro_slots`) in
`profile_macro_provider.c`. The window and metadata are what the macro RAM
policy names ([resource-policy.md](../../../../docs/spec/resource-policy.md)).

`macro_payload_engine_scan` runs one phase per call. `READY` reads the next
step; text, chords and strokes take transient leases and pass through
`WAITING` and `TRANSIENT_RELEASE` (and `SECOND_STROKE` for a dead key), and
`TEXT_PRESS` starts each further character of a text step;
Unicode entry runs `UNICODE_PRESS`, `UNICODE_FINISH`, `UNICODE_RESTORE`;
`CLEANUP` releases one lease per call before `macro_payload_finish` resets the
engine and calls the provider back. Leases are of three kinds: held keys (at
most 16, matched to the balance), one transient tap or chord at a time, and
macOS's held Option.

`macro_payload_debug_snapshot` reports the phase as idle, ready, waiting,
output or cleanup, the held-lease count and counters; the activation policy
reads the first two to find the safe boundary.

## Host-test seams

`macro_payload_host_setting`, `macro_payload_host_os` and
`macro_payload_unicode_caps_lock` are weak, so a test fixes the host. A
direct start (`macro_payload_start_ir` with an IR, source `DIRECT`) plays only
in host tests; firmware calls it only for the busy answer.
