# Finding 03: Harden VIA Split Buffer Replay at the RPC Trust Boundary

## Plan metadata

- **Severity:** Must fix — out-of-bounds read and unintended NVM-write risk
- **Status:** Planned; no remediation has landed
- **Affected surfaces:** VIA command mirroring, slave RPC decoder, dynamic-keymap storage compatibility, host packet tests
- **Primary files:** [`qmk_via_split_sync.c`](../users/noah/lib/compat/qmk_via_split_sync.c), [`qmk_via_contract.c`](../users/noah/lib/compat/qmk_via_contract.c), [`qmk_via_split_sync_test.c`](../tests/host/qmk_via_split_sync_test.c)
- **Prerequisites:** define storage-region capacity helpers shared with Finding 05; the arithmetic fix itself should land immediately and must not wait for the persistence redesign
- **Recommended phase:** Phase 1 boundary hardening

## Problem statement

The slave-side decoder validates a variable-length `id_dynamic_keymap_set_buffer` packet by casting `4 + size` back to `uint8_t`. Values from 252 through 255 wrap the required length to zero through three. A normal 32-byte RPC packet can therefore pass validation while declaring hundreds of payload bytes. The decoder then hands a pointer to at most 28 bytes to `dynamic_keymap_set_buffer()`, which reads beyond the RPC buffer and writes the resulting bytes to NVM.

The decoder is an untrusted binary boundary even when the normal sender is local firmware. It must validate packet shape and destination range before invoking any QMK storage API.

## Current evidence and failure scenario

- The split RPC payload limit is 32 bytes (`RPC_M2S_BUFFER_SIZE`).
- [`noah_qmk_via_split_sync_apply_command()`](../users/noah/lib/compat/qmk_via_split_sync.c#L23) decodes packets on the slave.
- For set-buffer packets, [`qmk_via_split_sync.c`](../users/noah/lib/compat/qmk_via_split_sync.c#L36) reads `size = data[3]` and checks `length < (uint8_t)(4u + size)`.
- With `size = 252`, the cast converts 256 to zero; sizes 253–255 produce one through three. Any nonempty 32-byte request passes.
- The subsequent call at line 48 passes `&data[4]` and the unchecked size to `dynamic_keymap_set_buffer()`.
- Upstream [`dynamic_keymap_set_buffer()`](../../bastardkb-qmk/quantum/dynamic_keymap.c#L81) forwards the requested size to NVM and does not validate the source-buffer length.
- Existing [`qmk_via_split_sync_test.c`](../tests/host/qmk_via_split_sync_test.c#L189) covers a valid three-byte buffer and only checks oversized master sends; it does not exercise malformed packets delivered to the registered slave callback.

Representative failure: a 32-byte slave request begins `{id_dynamic_keymap_set_buffer, 0x00, 0x00, 0xFC, ...}`. The wrapped check passes, and the NVM layer consumes 252 bytes starting at a 28-byte payload. Upstream bounds destination writes to its configured region, but it still reads far beyond the source packet and can persist unrelated adjacent RAM across the valid dynamic-keymap bytes.

## Required invariants

1. All packet-size arithmetic is performed in a type wider than every encoded size and transport length.
2. For set-buffer, `size <= length - header_size` is proven before computing or dereferencing `data + header_size`.
3. The encoded size is also bounded by the protocol payload maximum: `RPC_M2S_BUFFER_SIZE - header_size`.
4. `offset + size` is checked without integer overflow and cannot exceed the configured dynamic-keymap storage region.
5. Zero-length writes have one explicit policy, matching upstream VIA behavior, and never dereference payload data unnecessarily.
6. A malformed packet produces no NVM call, RGB invalidation, cache invalidation, acknowledgment of application, or partial side effect.
7. Every supported command has a centralized minimum/exact/variable packet-shape definition; future command additions do not copy unsafe arithmetic.

## Scope

### In scope

- Immediate widened length/range validation for all existing slave-replayed VIA commands.
- A small decoder/validator boundary with typed decoded fields.
- Dynamic-keymap destination-capacity validation through the compatibility layer.
- Exhaustive malformed-length tests and sanitizer-friendly fuzz/property coverage.
- Protocol validation reusable by the durable split protocol in Finding 05.

### Non-goals

- Trusting QMK's NVM layer to clamp invalid input.
- Changing valid VIA packet encoding or reducing the valid 28-byte payload.
- Completing the delivery/retry/rejoin design; that is Finding 05.
- Editing the sibling QMK tree to compensate for a userspace decoder bug.

## Implementation plan

### Step 1 — Land the minimal arithmetic containment

1. In the set-buffer case, reject `length < 4u` first.
2. Convert `length` and `data[3]` to `size_t` or `uint16_t` before arithmetic.
3. Validate using subtraction after the header check:
   - `payload_available = (size_t)length - 4u`;
   - reject when `(size_t)size > payload_available`;
   - reject when `(size_t)size > RPC_M2S_BUFFER_SIZE - 4u`.
4. Do not cast the computed required length back to `uint8_t`.
5. Add regression tests for encoded sizes 252, 253, 254, and 255 in the same change. Confirm the NVM stub and RGB invalidation remain untouched.

### Step 2 — Centralize packet-shape decoding

1. Introduce an internal, pure decoder that returns a typed command view only after validation.
2. Define a descriptor or explicit validator per command:
   - set keycode: exact/minimum six-byte shape and coordinate fields;
   - set buffer: four-byte header plus bounded payload;
   - reset commands: exact one-byte shape unless upstream compatibility requires tolerated trailing transport bytes;
   - encoder command when enabled: six-byte shape.
3. Decide and test whether trailing bytes are accepted. Prefer exact logical length for locally framed packets; if 32-byte VIA reports must be tolerated, require the declared payload to fit and ignore only explicitly defined padding.
4. Keep side-effect application in a separate function that accepts the validated view. No switch arm should parse and write simultaneously.
5. Apply RGB/cache effects only after a command was successfully decoded and applied.

### Step 3 — Validate destination storage ranges

1. Add compatibility helpers that expose the dynamic-keymap byte capacity from configured layer, matrix, and encoder layout without leaking raw EEPROM addresses into the sync module.
2. Validate `offset <= capacity` and `size <= capacity - offset`; use subtraction so `offset + size` cannot overflow.
3. Treat `offset == capacity && size == 0` according to the explicit zero-length policy; reject every nonzero write there.
4. Add compile-time assertions where configured capacities must fit the 16-bit VIA offset contract.
5. Keep macro-buffer capacity separate. Finding 05 will use the same pattern for `id_dynamic_keymap_macro_set_buffer`.

### Step 4 — Build malformed-input coverage

1. Extend the registered-callback host test so the RPC payload is surrounded by canaries and all side-effect stubs record calls.
2. Iterate all 256 encoded size bytes for actual packet lengths 0 through 32. Assert a write occurs only when the complete header/payload and destination range are valid.
3. Cover offsets around zero, the last valid byte, exact end, one past end, and `0xFFFF`.
4. Cover truncated set-keycode/encoder packets and reset packets with the chosen padding policy.
5. Add a deterministic fuzz loop over command ID, length, offset, and size. Compile/run this target under AddressSanitizer and UndefinedBehaviorSanitizer when available.
6. Assert malformed packets do not invalidate RGB; this catches the current pattern of effects being applied after an unsafe or partial path.

### Step 5 — Make the contract hard to regress

1. Add a compile-only check that the decoder uses a width capable of representing `RPC_M2S_BUFFER_SIZE + header_size`.
2. Add a code-contract test or narrow static check that forbids casts of required packet length to `uint8_t` in the compatibility decoder.
3. Reuse the decoder for both current command replay and Finding 05's versioned frames, rather than maintaining two parsers.

## Test and verification plan

### Targeted existing runners

```sh
sh tests/host/run_qmk_via_split_sync_tests.sh
sh tests/host/run_qmk_contract_checks.sh
sh tests/host/run_via_macro_defaults_tests.sh
sh tests/host/run_action_lifecycle_tests.sh
```

Because compatibility surfaces and likely headers change:

```sh
sh tests/host/run_feature_gate_compile_tests.sh
```

Run the new malformed-input target with sanitizers in addition to the normal runner. The exact sanitizer command should be added to `run_qmk_via_split_sync_tests.sh` or a dedicated concrete runner so it becomes repeatable.

### Closure gates

```sh
sh tests/host/run_all_host_tests.sh
qmk compile -kb bastardkb/charybdis/4x6 -km noah
```

## Measurements and observability

- Count rejected packets by reason in debug builds: truncated header, payload-size mismatch, unsupported command, and destination-range violation.
- Keep counters saturating and out of release hot paths unless diagnostics are enabled.
- Report validator code/stack delta in the target map because compatibility hardening must not worsen Finding 01.

## Risks, tradeoffs, and fallbacks

- **Over-strict exact lengths can reject padded VIA reports.** Establish the actual upstream hook length and encode the padding rule explicitly in tests.
- **A capacity formula can drift from QMK's EEPROM layout.** Centralize it beside existing storage compatibility macros and cover it with compile gates against configured constants.
- **Validation after pointer creation is too late.** Produce payload pointers only from a validated decoded view.
- **Sanitizers may not be available in the ARM build.** Run the pure host decoder under sanitizers and still verify the compiled firmware path.
- **The persistence redesign may replace packet formats.** Land the arithmetic containment now; carry the validator forward rather than postponing the safety fix.

## Documentation and review-note updates

- Document the slave RPC trust-boundary invariants in the active open firmware architecture review, or open the next sortable review folder if the relevant one is closed.
- Update compatibility documentation if a packet padding or zero-length policy becomes an explicit local contract.
- Note that no upstream QMK change is required; the userspace compatibility layer owns validation before calling QMK storage APIs.
- User-facing documentation is unnecessary unless invalid packets become externally reported.

## Acceptance checklist

- [ ] The set-buffer length calculation contains no narrowing cast.
- [ ] Encoded sizes 252–255 are rejected for every transport-valid short packet.
- [ ] All 256 sizes and lengths 0–32 are covered by a deterministic test matrix.
- [ ] Offset/capacity bounds are checked without overflow.
- [ ] Malformed packets cause zero NVM, RGB, cache, and acknowledgment side effects.
- [ ] The valid 28-byte payload boundary remains supported.
- [ ] The pure decoder passes ASan/UBSan coverage where available.
- [ ] `sh tests/host/run_qmk_via_split_sync_tests.sh` passes.
- [ ] `sh tests/host/run_qmk_contract_checks.sh` passes.
- [ ] `sh tests/host/run_feature_gate_compile_tests.sh` passes.
- [ ] `sh tests/host/run_all_host_tests.sh` passes.
- [ ] `qmk compile -kb bastardkb/charybdis/4x6 -km noah` passes.

## Next action

Add the four 252–255 regression packets to `qmk_via_split_sync_test.c`, then replace the narrowing check with subtraction-based validation before expanding the decoder contract.
