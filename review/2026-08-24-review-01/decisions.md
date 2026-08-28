# Architecture Decisions

This is the durable decision log for the live-profile project. Agents may add
detail, but must not silently reverse an accepted decision.

## Accepted

### D-001 — Live Means Data And Policy Within Compiled Ceilings

Status: accepted

The system live-edits every behavior already representable by the compiled
profile schema and advertised capacities. Arbitrary executable C, new hardware
support, USB topology, and capacity increases still require flashing.

Reason: a bounded interpreter and profile store can be validated and recovered;
general C hot loading cannot preserve the firmware safety model.

### D-002 — The Three C Files Remain Compiled Defaults

Status: accepted

keymap.c, config.h, and rgb_config.c remain the human-readable authored default
profile. A valid persistent device profile is the active deployed state.
Profile Studio exposes and reconciles both instead of pretending they are
always identical.

### D-003 — Use The Existing VIA Raw HID Endpoint

Status: accepted

Standard VIA commands remain authoritative for dynamic keymap and VIA macro
storage. Custom live-profile operations use a versioned keyboard-specific
channel on the existing 32-byte VIA Raw HID endpoint.

Reason: this avoids another USB interface and builds on existing host, EEPROM,
and split integration.

### D-004 — Use A Canonical Wire Schema

Status: accepted

The persisted and transported format uses fixed-width integers, explicit byte
order, explicit lengths, stable ids, a schema version, and checksums. It never
stores pointers or raw compiler structs.

Desktop and firmware implementations share golden byte fixtures and
round-trip tests.

### D-005 — Activate Candidates Transactionally

Status: accepted

A candidate is staged, completely validated, and then activated as one logical
generation. A rejected or interrupted candidate leaves the previous valid
generation active.

### D-006 — Behavior Changes Require A Safe Boundary

Status: accepted

Changes that can affect key output wait until the runtime quiescence contract is
satisfied. RGB-only preview may use a documented RGB frame boundary when it
does not alter behavior-affecting fields.

### D-007 — Both Halves Persist And Reconcile

Status: accepted

The USB half accepts host operations. The existing durable split
reconciliation model is extended or generalized so both halves converge,
survive role swaps, and recover after reconnect.

### D-008 — Milestone A Is RGB Plus Key Behaviors

Status: accepted

The first user-significant completion point is not layout-only live editing.
It is the complete Milestone A definition in README.md: live, persistent,
split-safe, source-aware RGB and key behaviors.

## Stage 00 Decisions

### D-009 — Persistent Storage Layout

Status: accepted on 2026-08-25

Reserve the upper 8 KiB of the existing 16 KiB logical EEPROM as two 4 KiB
live-profile slots. Slot A is `0x2000–0x2FFF`; slot B is
`0x3000–0x3FFF`. Cap standard VIA storage at `0x1FFF`, leaving 7,551 bytes for
VIA macros after existing config and keymap regions.

Each profile slot has a 32-byte canonical header and at most 4,064 bytes of
payload. An inactive slot is invalidated, written, read back, checksummed, and
committed by a final two-byte marker. The previous valid slot remains the
last-known-good record until that marker succeeds.

Do not grow logical EEPROM and do not use `VIA_EEPROM_CUSTOM_CONFIG_SIZE`.
Exact addresses, measured budgets, and the header contract are in
`stage-00-baseline.md`.

### D-010 — Fixed Capacity Ceilings

Status: accepted on 2026-08-25

Profile Wire v1 supports at most 8 logical layers, 64 behavior rows, 5 tap
steps per behavior, 128 populated behavior steps, 32 combos with 4 keys each,
16 reusable RGB groups, 32 aggregate RGB stage-group rows, 58 LEDs per group,
16 hardcoded macro slots, 1,024 hardcoded macro bytes, and a 4,064-byte
canonical payload.

The first firmware advertises its compiled 5 layers and 58 LEDs. The aggregate
payload ceiling still applies when individual domain maxima would sum past one
slot. `stage-00-baseline.md` is the canonical capacity table.

### D-011 — Split Integration Shape

Status: accepted on 2026-08-25

Use a sibling live-profile reconciler with its own split transaction and
generation metadata. Share or extract the existing bounded-frame, checksum,
chunk, retry, backoff, dirty, and recovery primitives; do not add the blob to
the current contiguous VIA region enum.

Ordinary VIA mutations must not trigger a 4 KiB profile transfer. The new
reconciler still requires a mechanically defined descriptor table tying every
advertised operation to size, read, write, digest, commit, invalidate, reset,
and recovery behavior.

### D-012 — Profile Studio HID Adapter

Status: accepted on 2026-08-25; real-board packaging evidence remains open

Start with `node-hid` 3.x async/N-API, lazy-loaded by a concrete adapter inside
the local VS Code extension host. Keep an injected, mockable device-adapter
contract and a helper-process escape hatch if the VS Code-host or packaging
spike fails.

One extension-scoped coordinator serializes requests per device, supports
cancellation and timeouts, rejects queued work on disconnect, and treats VIA
and Studio as contending protocol clients even when macOS permits a
nonexclusive open. No webview module imports HID.

This architectural choice is frozen; Stage 00 remains open until the concrete
adapter enumerates and repeats a protocol request from the actual extension
host on a real board.

### D-013 — Preview And Apply Semantics

Status: accepted on 2026-08-25

Keep existing Apply actions source-only. Add explicit Preview Live, Deploy
Live, Apply Source + Device, Pull Device, Push Source, Roll Back Preview, and
Reset Device operations.

Compound Apply prepares and validates the device candidate first, writes
source second, and commits the prepared device candidate third. This avoids a
source write the connected firmware cannot represent while ensuring a source
failure can still abort before device activation. Every partial outcome stays
visible and retryable.

Only behavior-independent RGB fields use volatile frame-boundary preview. The
preview rolls back on cancel, reload, profile switch, panel disposal, timeout,
disconnect, reboot, or explicit rollback. Full rules are in
`authority-state-table.md`.

### D-014 — Generation Authority And Drift Resolution

Status: accepted on 2026-08-25

Committed device authority is `{counter, origin_half}` plus canonical payload
digest. Only a durable device commit advances the counter. Source, draft, and
compiled-default digests are comparison identities, not deployment
generations.

Higher valid counters win after reconnect. Equal tuple and digest means
converged. Equal tuple with different digest is corruption. Equal counter with
different origins is a disconnected concurrent commit and requires explicit
Studio resolution. Reset commits an override-disabled generation; unchanged
push is a no-op. See `authority-state-table.md`.

### D-015 — Semantic Actions And ABI Compatibility

Status: accepted on 2026-08-25

Profile Wire v1 uses tagged semantic actions. Standard QMK keycodes may use a
16-bit operand under an exact action-ABI digest. Layer, PD-mode, VIA-macro,
hardcoded-macro, and userspace-owned actions use stable kinds plus bounded
operands rather than raw custom-keycode enum values.

Unknown kinds, incompatible action ABI, and invalid cross-references reject the
whole candidate. `profile-wire-v1.md` owns the byte contract.

### D-016 — Resource Truth Separates Hardware, Linker Accounting, Policy, And Runtime Evidence

Status: accepted on 2026-08-25

Report RP2040 resources per half and by physical/linker bank. The target has
270,336 bytes of physical SRAM: 262,144 bytes in the word-striped SRAM0–3
`ram0` region and 4,096 bytes each in SRAM4 and SRAM5. The overlapping
256-byte `ram7` boot region is part of SRAM5 and is never added to that total.

The `.data + .bss` value is an SRAM0–3 regression metric, not total static RAM
or physical capacity. The `__heap_base__` to `__heap_end__` value is the
SRAM0–3 linker/core-memory span at boot. It backs the ChibiOS core allocator
and linked newlib allocation path, so it is neither a second pool nor proof of
runtime-free memory. Runtime allocator high-water remains hardware evidence.

The existing 26,000-byte BSS maximum, 51,000-byte `.data + .bss` maximum, and
204,800-byte minimum core-memory span remain conservative regression policies
until deliberately replaced. A design may revise them with bank-aware linked
accounting, explicit rationale, regression tests, fresh target evidence, and
hardware high-water measurements; it must not present policy slack as hardware
headroom.

Stack reporting also separates the reviewed-path policy from physical stack
allocation. The current 1,880-byte worst reviewed main path has 40 bytes to the
1,920-byte policy budget and 680 bytes to the 2,560-byte process-stack boundary.
The reviewed-path gate is not proof of a global or interrupt-stack maximum.

Keeping candidate payloads in inactive EEPROM remains the accepted
power-loss-safe and memory-efficient design. It is not justified by a claim
that a nominal 4 KiB RAM buffer is physically impossible.

### D-017 — Dedicated Profile Split Protocol And Fail-Closed Authority

Status: accepted on 2026-08-27

The live profile uses its own versioned 32-byte sibling protocol rather than a
new VIA reconciliation region. Metadata carries the complete durable record
identity and both firmware compatibility digests. Payload transfer is bounded
to 14 bytes per frame and correlated by generation plus payload digest. Every
frame has canonical zero padding and a CRC8.

The authority comparator implements D-014 directly and does not use current
USB role as a tie-breaker. Unreadable metadata, unsupported schema, firmware
incompatibility, an active transfer, equal-counter/different-origin commits,
and equal-tuple record disagreement all block activation. Only two compiled
defaults with matching firmware identity or two exact committed records are
converged. The exact byte contract is in `profile-split-v1.md`.

The isolated exact peer-store backend now preserves the sender's full durable
identity, validates the declared domain mask against the canonical payload,
and reports success only after marker readback plus exact committed-record
comparison. Store compatibility now includes the compiled-default digest, and
ambiguous marker durability blocks all later prepares until boot selection
reconciles the slots.

This still does not make the foundation a production reconciler. QMK RPC
registration, protocol acknowledgement emission, retry/reconnect/role-change
state, boot whole-profile validation, and production owner installation remain
required before peer capability or live mutation is advertised.
