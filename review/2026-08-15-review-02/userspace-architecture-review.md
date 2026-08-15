# VIA Macro Text and IR Preflight Review

This review continues the remediation program after the Finding 03 review was
closed. The closed folder remains immutable. Finding 04 changes the macro input
and playback trust boundary, so it uses the next sortable review folder.

## Review Scope

- Align authored and VIA macro text on QMK's supported ASCII byte domain.
- Preserve high byte values when they are operands of complete QMK key commands.
- Validate an entire macro IR before any output, delay, or key ownership change.
- Preserve negative caching and explicit repair invalidation for malformed VIA
  slots.
- Leave nonblocking execution and scheduler policy to Finding 07.

## Intended Design

`macro_payload_text_byte_is_supported()` is the single text-domain predicate.
It accepts bytes `0x01..0x7F`. The authored parser reaches it only for non-NUL
string bytes; the VIA decoder handles zero first as its stream terminator. A
byte above `0x7F` is rejected only in text grammar and remains available to a
complete `SS_TAP_CODE`, `SS_DOWN_CODE`, or `SS_UP_CODE` operand.

`macro_payload_ir_next()` is the structural authority for IR walking. Both
preflight and execution use it, so they cannot disagree about opcode widths,
text payload bounds, delay operands, or tap-list payloads. Preflight also checks
text bytes and final held-key balance. Execution starts only after the complete
IR passes.

Malformed VIA loads leave a zero-length IR in `MACRO_SLOT_CACHE_INVALID`.
Repeated plays return from the cache without rescanning NVM. An explicit macro
cache invalidation is required after storage changes before a repaired slot is
loaded again. Finding 05 must preserve that ordering on the receiving half.

## Contracts

- No high text byte can reach `send_char()` or `send_char_with_delay()`.
- A malformed operation after valid earlier IR cannot cause partial playback.
- Decoder or preflight rejection produces no text, wait, register, unregister,
  or tap side effect.
- Control bytes `0x01..0x1F` retain the existing QMK table behavior; this pass
  does not narrow the domain beyond QMK's 7-bit lookup contract.
- Keycode operands remain eight-bit protocol values and are not classified as
  text.
- No sibling QMK source change is required.

## Verification Strategy

- Reject every byte `0x80..0xFF` as direct and embedded text.
- Exercise high command operands at the lower and upper boundaries.
- Construct malformed IR directly, including valid leading output followed by
  truncation, and prove zero total effects.
- Count VIA NVM reads across rejection, cached retry, repair without
  invalidation, and repair after invalidation.
- Run macro payload tests under ASan/UBSan and retain macro/VIA, feature, full
  host, target firmware, and reviewed stack gates.

## Current Status

Verified and closed on 2026-08-15. The shared text predicate and IR iterator
match this design; exhaustive and sanitizer tests pass; invalid-slot caching
and repair are mechanically covered; the full host suite and target firmware
build pass; and explicit hardcoded/VIA macro preflight stack paths retain the
required target reserve.
