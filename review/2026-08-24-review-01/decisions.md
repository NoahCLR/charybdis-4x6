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

The isolated scan reconciler and QMK adapter now implement acknowledgement,
newer-local push, newer-peer pull, retry/reconnect/role-change state, passive
peer expiry, and terminal conflict/corruption handling. They remain caller-
owned and unregistered in production until the single writable profile owner
exists.

Durable origin is explicitly independent of USB role. On this board,
`MASTER_RIGHT` without a hand pin or `EE_HANDS` makes QMK's left/right fallback
depend on current master, so production must consume an explicitly provisioned
physical-half identity. Correcting the forced build mapping (left is
`FORCE_SLAVE`, right is `FORCE_MASTER`) is necessary but does not by itself
close generic physical-origin provisioning. Boot whole-profile validation,
owner installation, physical-origin evidence, and hardware convergence remain
required before peer capability or live mutation is advertised.

### D-018 — Physical Identity Is Provisioned In Each Firmware Artifact

Status: accepted on 2026-08-28; real-hardware role-swap evidence remains open

Profile Studio's left artifact builds with `NOAH_PHYSICAL_HALF=left`; its right
artifact builds with `NOAH_PHYSICAL_HALF=right`. The setting becomes one of two
mutually exclusive compile definitions and overrides QMK's weak
`is_keyboard_left_impl()` with flash-owned handedness. The same boundary
returns Profile Wire origin `0` for physical left and `1` for physical right.

Transport role remains a separate decision. Current dual-USB artifacts also
use `FORCE_SLAVE` for left and `FORCE_MASTER` for right because this board is
`MASTER_RIGHT`, but no profile generation or conflict decision may derive
origin from those flags or from `is_keyboard_master()`.

Generic firmware has neither physical-half definition and fails closed when
asked for a durable origin. EEPROM handedness is not the profile-origin source:
artifact-owned identity survives ordinary EEPROM reset/recovery flows and does
not share storage lifecycle with the profile slots. Hardware must still prove
that both USB orientations and an actual role swap preserve the reported
origin before mutation is advertised.

### D-019 — Durable Storage Work Is Mailbox-Deferred And Scan-Arbitrated

Status: accepted on 2026-08-28; writable-profile-owner and hardware evidence remain open

No QMK split RPC callback may access EEPROM, the wear-level cache, dynamic
keymap or macro storage, profile storage, or a validator backed by those
regions. A callback may validate fixed framing, admit one bounded request into
a publication-protected mailbox, and return an immediate structured BUSY,
error, or exact cached response.

Matrix scan is the durable execution context. One rotating scheduler owns the
current boot profile discovery, best-effort VIA write-through mirror, and VIA
durable reconciliation steps. It begins each scan at the next owner, skips idle
owners, and stops after the first owner reports work, preventing starvation
without allowing two of those subsystems to execute in one scan grant.

The future production profile owner must replace or extend the existing
profile-discovery scheduler entry; it may not introduce an independent EEPROM
tick. VIA macro default recovery remains ordered before this scheduler, and
must be included in a broader arbitration decision if later work makes it
concurrent or incremental alongside live-profile mutation.

### D-020 — One Writable Profile Backend Has Explicit Admission And Bounded Boot Adoption

Status: accepted on 2026-08-29; production-owner installation remains open

The production owner will hold one writable store/candidate backend shared by
host deployment and peer import. That backend exposes an explicit
`NONE`/`HOST`/`PEER` admission lease. A competing begin reports retryable busy
without poisoning the queued host transaction or retargeting the staged slot.
Admission remains held through validation, durable commit, and activation, and
is released only by a terminal abort, successful activation, or an explicit
owner-controlled cold-path release.

Boot selection and whole-profile adoption are separate bounded state machines.
The store scans at most one fixed read per step and checks both payload identity
and canonical domain shape. A selected committed record is not eligible for
activation until the reader-backed whole-profile validator has re-established
the exact compatibility derived from the actual compiled profile. The existing
one-shot boot wrapper is cold/test compatibility only; the future production
owner must consume the incremental entry inside D-019's durable scheduler.

A durable override record activates its validated snapshot. A durable record
with the override flag clear activates compiled defaults while retaining the
newer durable generation for authority and reset semantics. Split transport
registration is one-shot: reinstalling the same reconciler is idempotent and a
different owner is rejected. Mutation, activation, and peer capabilities stay
disabled until one production owner composes these rules.

### D-021 — Complete Owner Composition Lands Behind An Engineering Gate

Status: accepted on 2026-08-31; normal exposure and hardware acceptance remain open

One caller-owned `noah_profile_owner_t` composes the compiled reader and
validator, store, provider, shared candidate backend, host transaction, exact
peer backend, split reconciler, activation policy, and installed behavior/RGB
runtimes. The boot path validates compiled defaults before constructing this
graph, incrementally selects and semantically adopts a durable record, and
withholds its split descriptor until that validation succeeds.

Runtime scheduling is admission-first. `PEER` blocks host work until exact
peer commit and activation finish. `HOST` retains its lease through durable
commit and activation while split work is restricted to metadata, serving or
pushing the known durable local record; it cannot begin or advance a peer
import. A validated boot-selected record first runs full reconciliation, so a
newer peer can replace it before either half requests activation; only after
authority converges does boot activation begin. With no admission during normal
operation, host, split, and peer activation rotate. An inactive host transaction
may expire only before marker-last commit, never with a queued mailbox or after
durability becomes uncertain.

The complete graph is allocated only when a side-specific build supplies
`NOAH_LIVE_PROFILE_OWNER=yes` and a flash-owned physical half. Ordinary
firmware retains the read-only discovery shell. Even the engineering artifact
does not advertise or route candidate mutation through VIA. It publishes
coherent read-only owner status and cancels a precommit host candidate when an
already compatible peer generation is greater than or equal to the reserved
host generation. D-022 supersedes the original postcommit-race limitation with
a distributed prepare barrier. Hardware acceptance and resource-policy closure
remain open.

The linked engineering owner is 3,084 bytes per RP2040 half. Its SRAM0–3
linker/core-memory span passes policy, but `.bss` and `.data + .bss` fail their
separate regression policies. Those failures are not physical-SRAM exhaustion
and are not grounds to change policy without allocator/stack high-water data.
A dedicated engineering stack manifest covers owner boot, host write,
validation, marker-last commit, publication, split exchange, and profile split
callback paths.

### D-022 — Split Commit Uses A Distributed Prepare Barrier

Status: accepted on 2026-09-02; hardware acceptance and mutation exposure remain open

A peer-required host COMMIT does not begin local marker-last durability as soon
as semantic validation completes. The transaction enters `PREPARING_PEER`, and
the owner streams the exact validated candidate from the local inactive slot
into the peer inactive slot. `PREPARE_BEGIN` is provisional intent, not durable
peer authority. The peer holds `PEER` admission after all bytes arrive, while
the sender pauses in `PUSH_PREPARED`; neither half has advanced a marker yet.

Only after the peer is prepared does the owner authorize local marker-last
commit. A successful local marker moves the transaction to
`CONVERGING_PEER`, immediately republishes the new local durable descriptor,
and authorizes the already-prepared peer's marker-last commit. Provider
activation remains fenced until a fresh authority snapshot reports the exact
just-committed descriptor on both halves, `COMMITTED_CONVERGED`, with no
transfer pending. This order keeps the previously active slot pinned while
avoiding the impossible two-slot hot-import handoff that motivated the rule.

Simultaneous provisional host candidates are ordered before either marker.
Higher generation wins; equal generation uses the lower stable physical-origin
id as the deterministic winner. The loser first sends an idempotent peer abort,
then aborts its own inactive candidate and reports `PEER_PREPARE_YIELDED`.
Dynamic USB role never participates in this decision. A compatible durable
peer can still supersede any earlier precommit host phase.

Disconnect or timeout before local durability retains both leases until the
peer abort is confirmed, then discards only provisional data. A link reconnect
resumes the correlated prepared transfer. If the sender reboots after local
durability, boot discovery recovers the durable local record and normal split
reconciliation transfers that identity again; volatile prepare state is not
claimed to survive reset. An unexpected newer, concurrent, corrupt, or
incompatible peer observed after local durability never activates the local
candidate: the transaction enters `AUTHORITY_FAILED`, retains the HOST
lease/backing, and requires cold-path reconciliation or explicit Studio
resolution. Role changes restart or preserve the outbound prepared handshake
without abandoning its candidate correlation.

The reconciler callback remains mailbox-only. Each scan grant still performs
at most one transport exchange, bounded payload operation, validator step, or
marker-last commit step. Candidate-write routing and its write/commit/
activation/peer-operation capability bits remain coupled and disabled until a
separate engineering-mutation gate is accepted.

### D-023 — Live Mutation Is An Explicit Engineering-Only Capability Set

Status: accepted on 2026-09-03; production promotion and hardware acceptance remain open

`NOAH_LIVE_PROFILE_MUTATION=yes` is a separate engineering build gate and is
valid only with `NOAH_LIVE_PROFILE_OWNER=yes` and VIA enabled. It atomically
routes candidate-status, candidate-write, abort, validation, save/commit,
activation, and peer-reconciliation operations and advertises the matching
four capability bits plus the exact candidate chunk capacity. Owner-only and
ordinary builds remain read-only; the build and host compile gates reject
partial or contradictory configurations.

Profile Studio enables `Apply live` only after the connected device advertises
that complete set. It compiles the parsed RGB and key-behavior source model to
the canonical Profile Wire blob, stages and commits through the existing
coordinator, and reports success only when a fresh status read identifies that
exact digest as both durable and active. This is the first hardware-test path,
not production acceptance: the conservative BSS and combined-data regression
policies remain red and allocator/stack high-water plus the two-half recovery
matrix remain open.

### D-024 — Durable Split Work Runs From Both QMK Scan Hooks

Status: accepted on 2026-09-04; hardware re-test remains open

QMK routes the USB/master half through `matrix_scan_user()` and the passive
half through the distinct `matrix_slave_scan_user()` hook. Noah therefore owns
two matching runtime entry points. The master entry keeps the complete key,
macro, profile, and split-sender pipeline; the slave entry advances only the
shared durable-I/O scheduler.

The slave durable grant is a required production reachability contract, not an
optimization. It incrementally initializes the live-profile owner, registers
the local profile RPC endpoint, and drains the profile and VIA receiver
mailboxes. Core split key traffic is independent and cannot be used as evidence
that these higher-level endpoints are alive.

Weak defaults chain both QMK hooks to their `noah_*` helpers. Hook and runtime-
order tests enforce the two paths separately. Profile Studio additionally
requires fresh `peerKnown` and `peerConverged` status before enabling live
mutation, and exposes candidate state so an interrupted matching transaction
can be resumed without silently overwriting firmware state.
