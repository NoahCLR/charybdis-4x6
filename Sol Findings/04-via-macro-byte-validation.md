# Finding 04: Reject Non-ASCII VIA Macro Text Before QMK Playback

## Plan metadata

- **Severity:** Must fix — target out-of-bounds lookup during macro playback
- **Status:** Planned; no remediation has landed
- **Affected surfaces:** QMK-stream macro decoder, macro-slot cache state, VIA macro provider, IR playback defense, macro payload tests
- **Primary files:** [`macro_payload_decode_qmk.c`](../users/noah/lib/macro/macro_payload_decode_qmk.c), [`macro_payload_run.c`](../users/noah/lib/macro/macro_payload_run.c), [`macro_payload_parse.c`](../users/noah/lib/macro/macro_payload_parse.c), [`macro_slot_provider.c`](../users/noah/lib/macro/macro_slot_provider.c), [`via_macro_provider.c`](../users/noah/lib/macro/via_macro_provider.c)
- **Prerequisites:** none; coordinate any playback API changes with Finding 07 but do not delay decoder hardening
- **Recommended phase:** Phase 1 boundary hardening

## Problem statement

The VIA/QMK-stream decoder accepts any nonzero, non-prefix byte as a text byte, including `0x80` through `0xFF`. Playback later passes that byte to QMK `send_char*()`. Upstream send-string code indexes a 128-entry ASCII-to-keycode table and 16-byte bit tables using the full `uint8_t` value, so a high byte reads out of bounds.

Hardcoded text parsing already rejects non-ASCII input. The VIA decoder must enforce the same text-domain contract, and playback should defensively reject an invalid IR rather than assuming every provider is perfect.

## Current evidence and failure scenario

- [`macro_payload_decode_qmk_stream()`](../users/noah/lib/macro/macro_payload_decode_qmk.c#L102) loops over NVM bytes.
- Its non-prefix path at [`macro_payload_decode_qmk.c`](../users/noah/lib/macro/macro_payload_decode_qmk.c#L134) flushes pending QMK sequences and appends `byte` as text without checking `byte <= 0x7F`.
- [`macro_payload_play_ir_with_text_output()`](../users/noah/lib/macro/macro_payload_run.c#L88) dispatches IR text bytes through `macro_payload_send_text_char()` at lines 65–72 and 118–124.
- Upstream QMK's `ascii_to_keycode_lut` contains 128 entries, while [`send_char_with_delay()`](../../bastardkb-qmk/quantum/send_string/send_string.c#L216) indexes that table and packed modifier tables with the full `(uint8_t)ascii_code`.
- The authored-string parser already rejects bytes above `0x7F` in [`macro_payload_parse.c`](../users/noah/lib/macro/macro_payload_parse.c#L229), so provider behavior is inconsistent.
- [`macro_slot_provider_load()`](../users/noah/lib/macro/macro_slot_provider.c#L7) can retain a failed load as `MACRO_SLOT_CACHE_INVALID`, which is the correct basis for fail-closed VIA behavior if tested carefully.

Representative failure: a VIA macro slot contains `0x80, 0x00`. The decoder creates a one-byte text IR. Pressing its macro key calls QMK text playback, which reads beyond the ASCII and bit lookup tables and may derive arbitrary keycodes/modifier flags from adjacent flash.

## Required invariants

1. Every IR text byte is in the QMK-supported ASCII domain `0x01..0x7F`; zero remains the stream terminator and is not emitted as text.
2. High bytes are rejected only in text position. Valid QMK prefix command operands retain their separately defined 8-bit keycode grammar.
3. Decoder failure resets IR length/state and marks the slot invalid without sending text, registering/unregistering keys, or waiting.
4. Repeated playback of a cached-invalid slot remains side-effect free and does not repeatedly scan NVM until an explicit VIA mutation invalidates the cache.
5. After cache invalidation and repair of the NVM payload, the slot can load and play normally.
6. Playback validates IR structure/text before the first side effect, providing defense against corrupt or manually constructed IR.

## Scope

### In scope

- Reject `0x80..0xFF` in QMK-stream text position.
- Align VIA-decoded text with the hardcoded parser's ASCII contract.
- Add preflight/structural validation before IR playback side effects.
- Test invalid cache lifecycle, repair, and complete high-byte coverage.

### Non-goals

- Adding UTF-8 or arbitrary Unicode macro output.
- Rejecting high-valued keycode operands merely because they are not ASCII.
- Solving blocking macro execution; Finding 07 owns scheduling.
- Changing upstream QMK lookup tables.

## Implementation plan

### Step 1 — Add the decoder boundary check

1. In the non-prefix text branch, reject a byte greater than `0x7F` before appending it to a text chunk.
2. On rejection, use the decoder's existing fail path to set `ir->length = 0` and return false.
3. Keep the check after recognizing `SS_QMK_PREFIX` so encoded command bytes are parsed by their opcode/operand grammar.
4. Add a named helper such as `macro_payload_text_byte_is_supported()` and reuse it in the authored parser and IR validator to avoid future drift.
5. Explicitly document control-character policy. The current QMK table contains mappings/`KC_NO` entries below ASCII space; preserve current valid behavior unless tests and user-facing policy intentionally narrow it.

### Step 2 — Validate complete IR before playback

1. Add a pure IR preflight that walks every opcode, length, text byte, key operand, and hold-balance transition without emitting output.
2. Require preflight success before `macro_payload_play_ir_with_text_output()` performs its first `send_char`, key registration, or delay.
3. Reuse shared opcode decoding helpers so preflight and execution cannot disagree on operand lengths.
4. Keep final cleanup for failures that occur after execution starts, but do not rely on cleanup to make malformed text safe.
5. Ensure the nonblocking scheduler planned in Finding 07 consumes only preflighted IR and retains this validation boundary.

### Step 3 — Specify invalid-slot cache lifecycle

1. Confirm decoder false results make `macro_slot_provider_load()` store `MACRO_SLOT_CACHE_INVALID` with zero IR length.
2. Ensure `macro_slot_provider_play()` performs no playback call for invalid slots.
3. Preserve invalid state until `via_macro_provider_invalidate_all()` or a future slot-specific invalidation is triggered by a VIA mutation.
4. When Finding 05 adds complete macro mutation mirroring, invalidate the receiving half's macro cache only after the validated storage write commits.
5. Add debug-only rejection reason/slot reporting without echoing macro contents.

### Step 4 — Add exhaustive tests

1. Iterate all bytes `0x80..0xFF` as the first text byte followed by NUL; every decode must fail with zero IR length.
2. Repeat with valid ASCII before and after the high byte to exercise text-chunk flushing.
3. Test each high byte adjacent to a complete and incomplete `SS_QMK_PREFIX` sequence so text/operand classification cannot drift.
4. Include valid high-valued keycode operands for supported tap/down/up commands and verify they are governed by keycode grammar, not rejected as text.
5. Instrument send-char, owned-keycode, and wait stubs. Assert all counts stay zero for malformed text.
6. Exercise cache lifecycle:
   - first play reads and marks invalid;
   - second play performs no new NVM reads or output;
   - a simulated VIA write invalidates cache;
   - repaired ASCII payload loads and plays once.
7. Construct malformed IR directly to prove playback preflight catches high text and truncated operands before side effects.

## Test and verification plan

### Targeted existing runners

```sh
sh tests/host/run_macro_payload_tests.sh
sh tests/host/run_macro_dispatch_tests.sh
sh tests/host/run_via_macro_defaults_tests.sh
sh tests/host/run_via_macro_action_lifecycle_tests.sh
sh tests/host/run_action_lifecycle_tests.sh
sh tests/host/run_qmk_contract_checks.sh
```

Add sanitizer coverage to the concrete payload runner if supported.

If macro interfaces or feature-gated compile surfaces change:

```sh
sh tests/host/run_feature_gate_compile_tests.sh
```

### Closure gates

```sh
sh tests/host/run_all_host_tests.sh
qmk compile -kb bastardkb/charybdis/4x6 -km noah
```

## Measurements and observability

- Track invalid VIA macro slots and rejection reason in existing debug diagnostics, with saturating counters.
- Measure decoder/preflight target stack and flash delta; the safety fix must not compromise Finding 01.
- In tests, count NVM reads to enforce negative caching and count every output primitive to enforce no-side-effect rejection.

## Risks, tradeoffs, and fallbacks

- **Signed `char` behavior can obscure high bytes.** Validate as `uint8_t` before any cast to `char`.
- **A blanket high-byte rejection could reject valid QMK command operands.** Apply the ASCII predicate only in text grammar positions and cover valid operands.
- **Execution-time validation can release unrelated holds.** Preflight the entire IR before output rather than discovering malformed text mid-playback.
- **Negative caching can make a repaired macro appear stuck.** Tie invalidation mechanically to every macro-buffer mutation/reset in Finding 05 and test repair.
- **Supporting UTF-8 later needs a different output engine.** Do not pass raw UTF-8 bytes to QMK send-string tables.

## Documentation and review-note updates

- Record the macro text-domain and preflight invariant in the active open firmware architecture review, or create the next sortable review folder if the prior review is closed.
- Update macro/VIA documentation to state that macro text is QMK ASCII, while QMK prefix command operands follow their keycode grammar.
- If a diagnostic surface is added, document how invalid slots are cleared by a subsequent VIA edit/reset.
- Keep default macro validation aligned with the same shared predicate.

## Acceptance checklist

- [ ] Every direct text byte `0x80..0xFF` is rejected by the QMK-stream decoder.
- [ ] Valid high-valued command operands follow explicit keycode rules and have tests.
- [ ] Decoder failure resets the IR and performs no output/wait/key side effects.
- [ ] Playback preflights the entire IR before its first side effect.
- [ ] Invalid slots are negatively cached until an explicit mutation invalidates them.
- [ ] A repaired slot plays after invalidation.
- [ ] Authored and VIA providers share one documented text-byte predicate.
- [ ] Targeted macro/VIA runners pass.
- [ ] `sh tests/host/run_qmk_contract_checks.sh` passes.
- [ ] `sh tests/host/run_all_host_tests.sh` passes.
- [ ] `qmk compile -kb bastardkb/charybdis/4x6 -km noah` passes.
- [ ] Review and macro documentation match the landed contract.

## Next action

Add an exhaustive `0x80..0xFF` decoder test with output-call counters, then introduce the shared ASCII predicate at the append-text boundary.
