# Live Profile Editing Architecture Review

This review uses prompts/initial-architecture-review.md. It plans a new
architecture thread for live profile editing from Charybdis Profile Studio,
with Milestone A defined as complete live RGB and key-behavior editing.

The project opened against tree `87f356cd`. Stage 00 implementation and
evidence are tracked in `progress.md`; the opening findings below remain the
architectural rationale rather than a claim that no later work has landed.

## Must Fix Before The Milestone

### 1. The project must not extend the current VIA transport until Review 19's malformed-frame mirror defect is resolved

The active full-code audit records a reachable payload-length wrap in
users/noah/lib/compat/qmk_via_split_mirror.c:49 and :64. The path accepts
untrusted Raw HID input through users/noah/lib/macro/via_macro_defaults.c:217
and forwards it over the split transport.

Live profile editing will add more host-controlled frames, more storage, and
more split replication. Building on a known length-validation failure would
multiply the most sensitive attack and corruption surface.

Ownership: review/2026-08-16-review-02 retains finding lifecycle and resolution
evidence. Stage 01 may not declare its transport ready until the relevant
Review 19 finding is resolved and its targeted, sanitizer, full-host, and
firmware gates pass.

### 2. Runtime consumers currently read keymap-owned const data directly, so there is no single seam where a live profile can become authoritative

Examples:

- users/noah/lib/key/behavior/key_behavior_lookup.c:39 scans
  key_behaviors[] directly.
- users/noah/lib/rgb/stages/rgb_layer_stage.c:13 reads layer_colors[] and
  layer_led_groups directly.
- users/noah/lib/rgb/stages/rgb_pd_mode_stage.c:13 reads pd_mode_colors[] and
  pd_mode_led_groups directly.
- users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c:12 reads the compiled
  combo feedback tables directly.
- users/noah/lib/rgb/stages/rgb_key_feedback_stage.c:12 reads compiled
  feedback colors and LED groups directly.
- users/noah/lib/rgb/automouse/rgb_automouse_stage.c:18 reads the compiled fade
  config directly.

The compiled tables are appropriate defaults, but live state cannot safely
replace them through scattered special cases.

Required design: introduce narrow effective-profile providers or immutable
snapshots. Compiled defaults and persisted live data both implement the same
logical schema. Consumers stop knowing which source is active.

The compiled-default materializer now supplies the canonical identity/export/
validation/reset representation without a profile-sized buffer. Its virtual
reader may replay canonical records from byte zero and is therefore a cold-path
surface only. The effective behavior seam now branches compiled and RGB-only
generations to the existing direct authored semantic tables. The effective RGB
seam similarly branches compiled and behavior-only generations to the authored
RGB configuration and rejects frame tokens after a later publication. It is
not installed and renderer-family migration remains open; hot key events and
RGB frames must never replay the virtual compiled blob.

Enforcement must prevent new direct reads outside default materialization,
validation fixtures, and the provider implementation.

### 3. No canonical schema currently spans source, desktop transport, persistence, runtime lookup, and split synchronization

keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c:384 materializes native
key_behavior_t rows. users/noah/keymap_materialize.h:54 expands native macro and
combo symbols. keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c:327
materializes native RGB symbols.

Those C objects contain compiler-owned enum widths, bool representation,
padding, pointers, and variable-size arrays. They cannot be copied into EEPROM
or sent over Raw HID as a stable format.

Required design: Profile Wire v1 with fixed-width fields, explicit byte order,
stable ids, explicit lengths, documented maxima, schema and compatibility
versions, checksums, and golden desktop/firmware fixtures.

The wire schema should encode semantic values, not C syntax. Layer names and
source formatting remain desktop/source concerns.

### 4. Behavior-affecting profile activation has no defined safe boundary

The key runtime owns delayed taps, held keycodes, modifiers, layers, repeating
actions, pointer modes, feedback, and macro-triggered work. Replacing behavior
rows while any of those states refer to the old generation can orphan outputs
or make release planning interpret a press under a different contract.

Relevant ownership surfaces include:

- users/noah/lib/key/runtime/
- users/noah/lib/key/ownership/held_action.c
- users/noah/lib/action/owned_keycode.c
- users/noah/lib/state/ownership/layer_ownership.c
- users/noah/lib/pointing/runtime/pd_mode_state.c
- users/noah/lib/macro/macro_payload_run.c

Required design: the runtime exposes one tested quiescence predicate and one
activation owner. A complete validated candidate may wait in a pending state,
but behavior-affecting data becomes visible only after the safe predicate is
satisfied. Failure or timeout leaves the prior generation active.

RGB-only preview may use a weaker frame-boundary rule only when the candidate
is proven not to change behavior-affecting domains.

## Should Fix As Architecture, Not As UI Patches

### 5. Source and device authority must be explicit before Profile Studio performs live writes

Profile Studio currently patches source through
tools/charybdis-profile-studio/extension.js:1233-1323. VIA keymaps and macros
already permit EEPROM state that differs from source defaults. Adding custom
device writes without a state model would deepen silent drift.

Required model:

- draft is the unapplied Studio state;
- source is the parsed three-file state;
- compiled defaults are the firmware recovery state;
- active device is the deployed USB-half generation;
- peer device is the other half's reconciled generation.

Every operation must report partial success. Device-first versus source-first
apply remains an open Stage 00 decision, but neither outcome may be hidden.

### 6. The EEPROM and capacity budget must be measured before the storage format is fixed

users/noah/config.h:184 reserves a 32768-byte RP2040 wear-leveling backing
region, yielding 16384 bytes of logical EEPROM. The dynamic keymap consumes a
small fixed prefix and the current macro region consumes the remainder.

Increasing logical EEPROM also increases the wear-leveling cache and therefore
target RAM. Repartitioning the existing logical space may reduce macro capacity.

Required evidence:

- fresh target ELF memory report;
- per-half RP2040 physical-bank and linker-region accounting;
- explicit separation of hardware capacity, linked-section occupancy,
  regression policies, and runtime allocator/stack high-water evidence;
- exact current EEPROM address map;
- representative and maximum encoded RGB and behavior profiles;
- selected persistent strategy and power-loss overhead;
- fixed advertised capacities;
- compile-time non-overlap assertions;
- fresh memory and stack gates.

The schema must be designed from measured budgets rather than guessed C struct
sizes.

### 7. Profile Studio needs a transport boundary that is independent of UI rendering and testable without hardware

The extension currently has no HID dependency and routes webview messages
directly to source patch functions in
tools/charybdis-profile-studio/extension.js:1102.

Required design:

- a serial per-device transport interface;
- enumerate, connect, capabilities, request, cancel, timeout, disconnect, and
  contention states;
- a fake device implementation for automated checks and screenshots;
- no HID API calls from webview code;
- no protocol encoding inside visual component handlers;
- clear handling when VIA and Profile Studio contend for the same endpoint.

Whether the concrete adapter uses an N-API dependency or packaged helper is a
Stage 00/01 decision.

### 8. Split convergence must extend a mechanically aligned canonical region model

users/noah/lib/compat/qmk_via_storage_regions.c:111 currently enumerates VIA
config, keymap, encoder, and macro regions. The reconciliation protocol already
has generations, digests, chunking, commit, retries, and role-aware repair.

The live profile should reuse those proven concepts, but the existing command
effects, region switch, mirror switch, digest traversal, and reset behavior are
separate lists. Review 19 already found disagreement between two of those
lists.

Required design: select one integration shape and add mechanical coverage that
every advertised profile region is sized, readable, writable when appropriate,
digested, transferred, committed, invalidated, reset, and recovered.

### 9. Compile-time config must be divided into capability, safety ceiling, default, and live policy

keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h currently mixes:

- capacity such as KEY_BEHAVIOR_MAX_TAP_COUNT and LAYER_COUNT;
- compiled feature inclusion such as RGB feedback and auto-mouse defines;
- safety or resource policy such as RGB_MATRIX_MAXIMUM_BRIGHTNESS and LED
  flush cadence;
- practical user defaults such as timing, auto-mouse timeout, DPI, layer
  selection, and feedback enable state.

Only practical policy should become ordinary live data. Capabilities remain
compiled and discoverable. Safety ceilings remain compiled and may bound a
live value. Features required for live toggles are compiled in and gated by
runtime enable flags.

Some QMK core settings require an existing callback, a compat wrapper, or an
upstream-fork change. Each field must be classified before Stage 06 claims
coverage.

## Optional Cleanup And Later Scope

- Move source/device diff rendering into a reusable Studio state component once
  Milestone A proves the semantics.
- Live-edit hardcoded macro payloads by adding a custom persisted domain, or
  explicitly converge them with VIA macro slots.
- Add dynamic combo and logical-layer structure only after RGB and behavior
  activation is proven. Both have wider cross-reference and active-state
  hazards.
- Support multiple named profiles on one device only after one active profile
  has robust migration and recovery. Named device profiles are not required
  for this project.
- Consider a generated schema description for Studio controls after Profile
  Wire v1 stabilizes. Firmware-driven arbitrary UI generation is not required.

## Stage 00 Reconciliation Note — 2026-08-25

The opening findings above are the audit-time snapshot. Current status:

| Finding | Status | Reconciliation evidence |
| --- | --- | --- |
| 1 — malformed VIA mirror prerequisite | open, remediation in progress | owned by Review 19; targeted guard and sanitizer package opened |
| 2 — no effective-profile seam | partially resolved | a hardened generation-owned provider now enforces coherent publication, invalidation visibility, safe-predicate reentrancy, nested-view containment, and active-backing-aware reuse; behavior and RGB consumers can atomically capture validated reader-backed generations without replaying compiled virtual bytes, while production owner installation, renderer migration, a measured behavior-index decision, and split convergence remain open |
| 3 — no canonical schema | partially resolved | `profile-wire-v1.md`, D-010, and D-015 freeze the v1 contract; blob, RGB, behavior, validator, and real compiled-default materializer now share exact C/JavaScript fixtures, while production runtime and split integration remain open |
| 4 — no safe activation boundary | partially resolved | `authority-state-table.md` freezes the quiescence contract and `profile_activation_policy.c` now produces coherent production reason/count snapshots from authoritative runtime owners; callback-only behavior/RGB invalidators plus stale token refusal have landed, while provider-owner installation, split convergence, renderer migration, and hardware evidence remain open |
| 5 — source/device authority | resolved at contract level | D-013, D-014, and `authority-state-table.md` define operations, ordering, partial results, and conflicts |
| 6 — storage/capacity evidence | resolved at Stage 00 design level; runtime high-water remains open | D-016 and `stage-00-baseline.md` record the corrected per-half bank model, policy-versus-capacity distinction, exact EEPROM map, dual-slot partition, and ceilings |
| 7 — Studio transport boundary | partially resolved | D-012 accepts an injected serialized adapter and fake; concrete packaged board probe remains open |
| 8 — split integration shape | resolved at contract level | D-011 selects a sibling profile reconciler with mechanically aligned descriptors |
| 9 — config classification | resolved at inventory level | `field-classification.md` classifies every currently parsed Studio surface |

Contract-level resolution is not milestone closure. Runtime, protocol, storage,
split, and UI findings remain open until their stage gates and hardware evidence
pass.

## Resource-Truth Reconciliation — 2026-08-25

The earlier Stage 00 wording conflated an SRAM0–3 `.data + .bss` regression
threshold with the RP2040's physical RAM capacity. D-016 supersedes that
interpretation everywhere in this active review.

Each keyboard half has its own RP2040 and its own 270,336 bytes of physical
SRAM: 262,144 bytes in the `ram0` SRAM0–3 region plus separate 4,096-byte SRAM4
and SRAM5 banks. The current fresh ELF records 22,980 bytes of `.data`, 25,684
bytes of `.bss`, and therefore a 48,664-byte SRAM0–3 `.data + .bss` regression
metric. Its 51,000-byte ceiling has 2,336 bytes of policy slack; that number is
not total RAM headroom.

The current `__heap_base__` to `__heap_end__` span is 213,472 bytes. It is the
SRAM0–3 linker/core-memory span at boot and backs ChibiOS core allocation plus
the linked newlib allocation path. Actual runtime high-water is not yet
measured. Fixed linked occupancy across all banks is 56,128 bytes, including
alignment and reserved stacks but excluding runtime allocation.

SRAM4 remains the tight bank. Its 1,024-byte interrupt stack, 2,560-byte process
stack, and 288 bytes of RTOS state leave 224 bytes outside those reservations.
The 1,880-byte worst reviewed process path has 40 bytes to the stricter
1,920-byte reviewed-path policy and 680 bytes to its physical stack boundary.
The gate covers named paths only.

Consequently, reader-backed decoding and EEPROM candidate staging remain sound
choices for deterministic memory use and power-loss recovery, but they are not
required by a false claim that only 2–3 KiB of physical RAM remains. Future
resource decisions must cite the bank, linked metric, policy, and hardware
evidence they rely on.

## Verified Solid Foundations

- The three authored profile files are genuinely data-driven. keymap.c contains
  no function definitions and materializes tables through
  users/noah/keymap_materialize.h.
- Profile Studio already parses the relevant source surfaces into a structured
  model and has staged draft behavior.
- VIA is enabled in users/noah/rules.mk:17 and supplies the existing 32-byte Raw
  HID path.
- Layout and VIA macro storage already have live EEPROM representations.
- The VIA split reconciliation code already demonstrates chunked snapshots,
  generations, digests, retries, and durable peer recovery.
- Authored behavior and RGB validators already define much of the semantic
  rejection policy that runtime candidates need.
- The codebase has extensive host runners, target memory and stack gates, a
  firmware compile gate, and established hardware-matrix practice.

The project should extend these seams instead of creating a sidecar runtime
engine or a second unrelated transport.

## Intended Component Structure

### Default Materialization

Compiled keymap and RGB translation units remain keymap-owned. They expose
default-profile views to a serializer or provider but do not own mutable
storage.

### Profile Schema

The schema owns:

- schema and compatibility versions;
- domain and field ids;
- capacity descriptors;
- canonical encoding and decoding;
- whole-profile and per-domain validation;
- cross-reference validation;
- deterministic digest fixtures.

The initial domains are metadata, RGB, and key behaviors. Existing VIA keymap
and macro regions remain standard VIA domains until later integration stages.

### Profile Store

The store owns:

- valid-slot discovery;
- generation and checksum metadata;
- candidate writes;
- power-loss-safe commit;
- fallback to compiled defaults;
- schema incompatibility behavior;
- reset and recovery.

The store does not understand Raw HID commands or RGB rendering.

### Runtime Provider And Activation

The runtime package owns:

- effective profile selection;
- immutable generation snapshots;
- candidate validation handoff;
- safe-boundary activation;
- domain invalidation;
- reset of derived caches;
- diagnostics for active, pending, rejected, and peer generations.

Consumers never read the store directly.

### Host Protocol

The protocol owns:

- capability and schema negotiation;
- metadata and digest reads;
- chunked candidate begin, write, verify, commit, abort, and status;
- idempotent retry rules;
- error codes suitable for Studio;
- strict length and range validation before access.

The USB receive callback should do bounded framing work and schedule heavier
validation or persistence for scan context where necessary.

The v1 candidate coordinator realizes that boundary as one decoded-command
mailbox. The callback performs exact 32-byte validation and copies at most one
20-byte chunk; the scan owner alone calls the injected staging backend, reads
staged bytes for retry comparison, and advances one validation step. Every
whole-profile validation call now performs at most one reader operation and
observes at most 20 new bytes; checksum calls also honor a smaller supplied
budget. Incremental RGB and behavior state machines read each structural byte
once, and behavior action-reference events remove the former row/step rescans.
The schema-maximal 3,216-byte behavior profile completes in 1,061 whole-profile
steps, with 897 behavior-domain steps capped at one 12-byte read each. Maximal
RGB completes in 58 domain steps capped at one 16-byte read each. The owner
contains no profile-sized RAM buffer. Its custom-save commit path now advances
persistence through one EEPROM operation of at most 20 bytes per scan, then
requests and polls provider activation. It remains independently compiled and
unrouted until production safe-boundary and invalidation owners can support the
capabilities they would advertise; real-device scan timing remains part of the
hardware acceptance pass rather than an unbounded-code blocker.

The landed store backend now supplies that coordinator with a bounded inactive-
slot reader/writer and composes the whole-profile validator over the staged
bytes. Validation streams CRC32/FNV-1a, checks the canonical envelope and both
domain codecs, and resolves behavior references against exact compiled
layer/PD/macro identities. The store now has an injected destructive-reuse
guard: after candidate checks pass and before the target commit marker is
invalidated, the provider reserves that slot's complete payload range. Abort,
successful commit, and every terminal failure release the reservation; a
release failure stays fail-closed. The provider pins active and pending ranges,
discards an overlapping rollback only as part of a successful reservation, and
therefore prevents store bookkeeping from overwriting a runtime-rolled-back
generation. After durable commit, the backend can build the exact committed
snapshot and request safe-boundary publication. The candidate coordinator now
composes those operations behind custom-save `0x13`, retains idempotent
transaction/digest correlation through commit and activation, and distinguishes
an unconfirmed final marker from a safely failed commit. These APIs remain
disconnected from QMK routing. The production safe predicate and first
callback-only behavior invalidator have landed but are not yet installed by a
production provider owner; no capability or split owner is installed merely
because the isolated pieces exist.

### Split Reconciliation

The split owner publishes or reconciles committed persistent generations. A
volatile RGB preview may be mirrored separately, but it can never overwrite the
last committed profile or win durable authority after reconnect.

### Profile Studio

The extension host owns HID and filesystem side effects. The webview owns
draft presentation only.

Recommended layers:

- source model and patcher;
- canonical profile model;
- wire encoder and golden fixtures;
- device transport interface;
- connection and compatibility state;
- source/device diff and operation coordinator;
- existing UI views.

## State And Transaction Model

Minimum device states:

- no candidate
- receiving candidate
- candidate complete
- rejected
- validated
- waiting for safe boundary
- active volatile
- persistence pending
- committed locally
- peer convergence pending
- converged

Every status response includes enough identity to distinguish a retry from a
new operation: protocol version, schema version, transaction id, candidate
digest, active generation, and committed generation as the final format allows.

The old active generation remains readable until the new one is atomically
published. Derived RGB and behavior caches switch using the same publication
generation.

## Testing And Debuggability Requirements

The project needs diagnostics that can answer:

- which schema and capacities the firmware advertises;
- which source/default/device digests Studio sees;
- active, pending, committed, and peer generations;
- why a candidate was rejected;
- whether activation is waiting and which quiescence condition is open;
- whether persistence or peer convergence is pending;
- the last protocol and storage error.

Host tests must cover both byte-level fixtures and semantic scenarios. Hardware
tests must cover the real split link, both USB orientations, reconnect, reboot,
forced roles, held inputs, RGB rendering, and recovery from interruption.

## Cross-Thread Prerequisite Status

This is a new architecture topic, so it has no prior live-profile findings.
It depends on the following Review 19 items without taking ownership of their
finding lifecycle:

| Review 19 item | Dependency status at project opening |
| --- | --- |
| Finding 1: split mirror length guard | remediation in progress in Review 19; blocks Stage 01 until gates pass |
| Finding 4: advertised mirror effects without handlers | open; must be decided before profile-region effect wiring |
| Finding 3: base sync publication contract | open; relevant if profile previews reuse the base publication domain |
| Hardware persistence and role-swap matrix | open; baseline evidence needed before Milestone A closure |

## Current Architecture Assessment

The project remains realistic and does not require replacing the current runtime.
The repository already owns the difficult domain logic, structured source
editor, Raw HID endpoint, EEPROM layer, split reconciliation, validators, and
test infrastructure.

The central architecture change is to insert one canonical effective-profile
boundary between compiled defaults and runtime consumers. The largest
correctness challenges are transactional activation, source/device authority,
power-loss recovery, and split convergence—not RGB conversion or UI controls.

Milestone A is deliberately substantial. It represents a trustworthy daily-use
feature rather than a demo in which one color or behavior changes until reboot.

## Recommended Next Refactor Sequence

1. Finish Stage 00 by landing executable golden fixtures, storage contracts,
   and the real-board packaged HID probe; the resource and architecture
   decisions are accepted.
2. Resolve the Review 19 transport prerequisites in their owning review thread.
3. Land the Stage 01 transport interface, fake device, discovery, capability
   query, and read-only digest/status path.
4. Land Profile Wire v1, storage recovery, candidate protocol, validation,
   safe activation, and split convergence as Stage 02.
5. Migrate RGB through the effective provider one table family at a time and
   finish the live RGB vertical slice.
6. Migrate key-behavior lookup and validation through a compact indexed
   effective provider and finish safe behavior activation.
7. Integrate source/device operations, diagnostics, recovery, resource gates,
   and the full Milestone A hardware matrix.
8. Only then extend the same architecture to defaults, standard VIA surfaces,
   combos, and logical layer structure.
