# VIA Split Replay Trust-Boundary Review

This review was opened for Finding 03 because the newest review folder covers
multi-profile tooling and the older runtime review covers loop performance and
stack safety. VIA split packet validation is a materially different external
binary-boundary topic, so neither open thread is reused.

## Review Scope

- Validate every slave-replayed VIA command before any storage or rendering
  side effect.
- Keep packet decoding pure and separate from command application.
- Preserve the current VIA wire format and its padded 32-byte report behavior.
- Centralize dynamic-keymap storage capacity in the QMK compatibility layer.
- Leave delivery, retry, reconnect, and durable persistence semantics to
  Finding 05.

## Intended Design

`qmk_via_split_sync.c` owns the untrusted RPC decoder. It first proves the
transport length, command shape, coordinate or destination range, and declared
payload length. Only a successfully decoded typed command may enter the apply
switch. RGB invalidation occurs after successful application.

Variable-length set-buffer decoding uses `size_t` for transport and payload
arithmetic. After proving the four-byte header exists, it compares the encoded
size against `length - 4`, the 28-byte RPC payload maximum, and
`capacity - offset`. No addition is needed for either safety check.

The local padding contract accepts trailing bytes for supported commands. This
matches QMK raw-HID reports, which may pass the full report length even when a
command's logical payload is shorter. Padding never expands the declared
set-buffer payload.

Zero-length set-buffer writes are valid at offsets from zero through the exact
end of the dynamic-keymap region. They apply command effects but skip the QMK
storage call and never construct a payload pointer. Nonzero writes at the exact
end, and all writes beyond it, are rejected.

`qmk_via_contract.c` derives the keymap byte capacity from layer, row, column,
and two-byte keycode dimensions. A compile-time assertion keeps that capacity
inside VIA's 16-bit offset contract. Raw EEPROM addresses remain private to the
compatibility layer.

## Contracts

- Malformed slave packets cause no NVM, RGB, reset, or partial side effect.
- Set-keycode and optional set-encoder commands validate both shape and target
  coordinates before calling QMK.
- Reset commands accept explicit trailing report padding but require the
  command byte.
- Unsupported commands are rejected without effects.
- The decoder never trusts QMK storage routines to bound source reads.
- No sibling QMK source change is required.

## Verification Strategy

- Exercise all 256 encoded set-buffer sizes for every transport length from
  zero through 32 bytes.
- Surround each RPC payload with canaries and run the matrix under ASan/UBSan.
- Cover offset zero, last valid byte, exact end, one past end, and `0xFFFF`.
- Compile and run an encoder-enabled variant to keep the optional command
  decoder live.
- Retain QMK contract, feature-gate, full host, firmware compile, and target
  stack gates before verification.

## Current Status

Verified and closed on 2026-08-15. `qmk_via_split_sync.c` now matches this
design, the storage capacity contract is centralized, normal/ASan/UBSan and
encoder-enabled boundary tests pass, the full host suite and target firmware
build pass, and the fresh reviewed-path stack gate preserves both main and
split-thread reserves.
