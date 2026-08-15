# Scan-Driven Macro Playback Architecture Review

Review 06 is closed and immutable. Finding 07 changes the macro execution
lifecycle, so it continues in this next sortable review folder.

## Review Scope

- Remove blocking macro execution from physical key processing.
- Preserve validated hardcoded and VIA IR while an execution is active.
- Reuse owner-scoped literal-key leases for every retained key or modifier.
- Bound all scheduler and cleanup work performed by one matrix scan.
- Keep one shared lifecycle for hardcoded and VIA macro providers.

## Prior Finding Status

| Prior finding | Status | Required preservation |
| --- | --- | --- |
| Finding 07: blocking macro playback | resolved | `macro_payload_run.c` is scan-driven; provider/runtime integration and focused source, timer, lifecycle, full-host, firmware, and fresh-stack gates enforce the contract. |
| Finding 08: aggregate synthetic ownership | resolved | Macro holds and transient text/chord keys must use `owned_keycode_lease_t`; cleanup must not release another owner. |
| Finding 04: VIA macro validation | resolved | The whole IR remains validated before execution and invalid cache states remain fail-closed. |
| Finding 01: target stack safety | resolved | Start, scan, and cleanup paths need explicit fresh-linked target evidence. |

## Baseline

The focused macro, action, ownership, held-action, and runtime-init runners pass
before implementation. The synchronous interpreter still walks the complete IR
inside the triggering key event. Explicit delay, text output, owned taps, and
post-operation intervals all reach QMK helpers that call `wait_ms()`.

## Pinned QMK Timing Contract

`send_char()` delegates to `send_char_with_delay(..., TAP_CODE_DELAY)`. That
helper can wait around shift/AltGr acquisition, basic-key tapping, modifier
release, and dead-key completion. `owned_keycode_tap()` also waits between its
lease acquisition and release. Avoiding only the explicit delay opcode would
therefore leave blocking text and chord paths in firmware.

The engine must translate a text byte through QMK's exported ASCII lookup
tables, acquire its modifier/basic leases, wait through normal scan deadlines,
and release them in a later bounded transition. Tap-list chords require the
same press/wait/release treatment.

## Intended Architecture

One global engine owns at most one execution. Start validates a complete IR,
records its immutable pointer and provider identity, and returns a typed result:
started, busy, invalid, or empty. Busy triggers are consumed and counted; they
do not queue, restart, or cancel the active execution.

Provider cache entries are pinned for the execution lifetime. Invalidation of a
pinned entry marks it stale without clearing its bytes. Completion or
cancellation unpins it, then applies deferred invalidation before the slot can
be loaded again. This avoids a second 512-byte execution buffer while making a
VIA edit during a delay deterministic.

The scan engine samples `timer_read32()` once and performs at most one bounded
state transition. Waiting is represented by a start timestamp and a duration,
so a 65,535 ms opcode and 32-bit timer wrap remain well-defined. Text and chord
output use explicit lease-backed press and release phases; firmware playback
does not call `wait_ms()`, `send_char()`, `send_char_with_delay()`, or
`owned_keycode_tap()`.

Persistent key-down leases remain in acquisition order. Cancellation and
runtime failure enter cleanup, which releases at most one retained lease per
scan before notifying the provider and returning idle. Successful completion
requires no persistent holds.

## Concurrency and Lifecycle Decisions

- One active execution; no queue.
- Provider invalidation does not mutate or cancel active bytes.
- Runtime/eeconfig reset requests cancellation; cleanup runs from later scans.
- An empty macro is consumed without starting an execution.
- Compile/preflight failure is invalid; transient lease acquisition failure is
  a runtime error and does not make otherwise valid provider bytes invalid.
- Scan granularity may add lateness but never shortens a configured interval.

## Landed Boundaries

- `macro_payload_start_ir()` owns validation and typed start results;
  `macro_payload_engine_scan()` owns time and output advancement; and
  `macro_payload_engine_cancel()` enters bounded cleanup.
- `macro_slot_provider_start()` is the hardcoded/VIA lifetime boundary. A
  successful start pins the selected static cache entry, and its completion
  callback unpins or applies deferred stale invalidation.
- `noah_matrix_scan_user()` advances playback after key-runtime work and before
  split sync. `noah_eeconfig_init_user()` requests cancellation before reset
  stages, while post-init initializes the engine before VIA default loading.
- `owned_keycode_lease_t` is the only report-ownership mechanism used by macro
  persistent holds, text, modifiers, dead-key completion, and tap-list chords.
- Firmware macro output does not call `wait_ms()`, `send_char()`,
  `send_char_with_delay()`, or `owned_keycode_tap()`; the focused runner checks
  this directly against the source.

## Enforcement References

- `tests/host/macro_payload_engine_test.c` and
  `run_macro_payload_engine_tests.sh`: fake time, 32-bit wrap, source guards,
  busy, cancellation, runtime error, and exact lease cleanup.
- `tests/host/macro_slot_provider_test.c`: active-byte immutability and deferred
  invalidation.
- `via_macro_action_lifecycle_test.c`: QMK/VIA integration, real cache mutation,
  long payloads, and busy behavior.
- `runtime_init_order_test.c`: reset, initialization, and scan ordering.
- `tools/firmware_stack_budget.json`: fresh-linked start, scan-output,
  completion-callback, and preflight paths.

## Closure Verdict

**Closed — Finding 07 resolved on 2026-08-15.** The landed code matches the
intended design, focused and integration enforcement passes, user and internal
documentation agrees with the tree, the complete host suite passes, the
ordinary firmware build passes, and the fresh instrumented target stack gate
passes. The linked image reports 145,020 bytes of text and 245,592 bytes of BSS;
the engine and diagnostics occupy 188 and 24 B respectively. New macro paths
are at most 440 B, while the overall reviewed limits remain green at
1,816/1,920 B for the main process and 328/768 B for the split thread.

## Closure Bar

Closure requires fake-timer deadline/wrap tests, bounded work assertions,
busy/invalidation/cancellation tests, owner-clean cleanup, hardcoded/VIA
integration, source and order gates, full host, ordinary firmware, fresh linked
stack/resource evidence, and reconciled user/review/Sol documentation.
