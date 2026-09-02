# Stage 02 — Profile Schema, Store, And Atomic Commit

Status: implementation in progress behind an engineering gate

Implementation note: the storage package freezes the 32-byte header, checksum
helpers, read-only boot discovery, bounded inactive-slot writes, readback
validation, and final-marker commit. A slot-bounded QMK adapter and one-shot
ordinary matrix-scan boot owner make read-only discovery reachable on normal
firmware. A side-specific engineering artifact now replaces that shell with the
complete writable owner and exposes one coherent read-only snapshot of
compiled, active, pending, committed, peer, and candidate state through the
existing two-page status surface.
The generic schema package implements the canonical `NLP1` blob/domain
envelopes and four-byte semantic actions in firmware and Studio against one
shared exact-byte fixture. The key-behavior and RGB packages now implement
matching desktop and reader-backed firmware payload codecs against shared
exact-byte fixtures. A cold-path compiled-default materializer now translates
the real authored C tables into the same canonical profile and publishes exact
compiled-profile and action-ABI identities without retaining a profile-sized
buffer. The reader-backed whole-profile validator composes those layers,
streams both checksums, enforces schema/domain/action-ABI and compiled-reference
compatibility, and returns bounded borrowed views. Exact candidate codecs and
upload coordinators now exist on firmware and desktop, and the firmware
coordinator can stage and validate an inactive EEPROM slot through an injected
backend. These mutation paths remain intentionally unrouted and unadvertised. The
isolated effective-provider foundation is now hardened and tested after
lifecycle review, and every current behavior/RGB consumer family now has an
effective-profile seam. A dedicated split foundation freezes the exact 32-byte
internal protocol and D-014 authority decisions, including fail-closed peer
observation for activation. An isolated exact peer-store backend now imports a
sender-owned record through the shared validator/store owner and confirms
marker-last durability without activating it. An isolated scan reconciler and
QMK adapter now implement bidirectional push/pull, cached callback replies,
bounded scan work, retry/reconnect/role-change behavior, and passive-peer
expiry. Durable-commit ownership and runtime owner installation now exist only
in the engineering artifact; QMK mutation routing remains absent. Bounded boot
selection, exact compiled
compatibility, committed-record adoption, reset-to-compiled activation, and a
shared host/peer admission lease have landed in that gated owner. Side-specific
artifacts provide the D-018 flash-owned physical identity and the owner consumes
it for durable origin and split registration. Host and peer work share one
admission-first scheduler. D-022 now prepares the exact candidate on the peer
before local marker-last commit, commits local then peer, and permits activation
only after fresh exact durable convergence. Simultaneous provisional writers,
prepared timeout, role change, and postcommit authority loss have fail-closed
host coverage. Resource-policy closure, explicit mutation routing/capabilities,
and the real two-half hardware matrix still block stage completion.

## Objective

Implement the generic firmware and desktop foundation that can transfer,
validate, persist, activate, recover, and split-synchronize a versioned
Milestone A profile without yet migrating all RGB and behavior consumers.

## Entry Criteria

- Live Link transport and fake device are stable.
- Profile Wire v1, capacities, EEPROM map, split shape, authority, and preview
  semantics are accepted.
- Fresh baseline resource evidence exists.
- Golden fixtures cover representative and malformed RGB and behavior domains.

## Scope

### Schema

Implement deterministic desktop and firmware encoding/decoding for:

- metadata;
- RGB domain;
- key-behavior domain;
- domain presence and lengths;
- schema, compatibility, capacity, generation, and digest metadata.

Encoding must be independent of C struct layout and JavaScript object property
order.

### Validation

Separate:

- frame validation;
- wire structural validation;
- field range validation;
- domain semantic validation;
- cross-domain and layer/keycode reference validation;
- capacity validation.

One rejection result identifies the first stable error category and location
without exposing memory contents.

### Store

Implement the Stage 00-selected persistent layout:

- last-known-good discovery;
- candidate staging;
- checksum;
- monotonic or serial generation rules;
- recoverable commit marker;
- compiled-default fallback;
- factory reset;
- incompatible version handling.

Persistence operates in bounded scan-context work if a full write is unsafe in
the Raw HID callback.

### Protocol

Implement:

- begin candidate;
- write chunk;
- candidate status;
- validate;
- request activation or commit;
- abort;
- operation status;
- reset to compiled defaults;
- read committed profile or supported domains if required for pull.

Retries must be idempotent. Duplicate chunks with identical content are
accepted or explicitly reported; conflicting duplicates are rejected.

### Runtime Activation

Implement:

- active and pending generations;
- safe-boundary predicate;
- pending activation;
- atomic provider publication;
- domain invalidation callbacks;
- reset and diagnostics.

At this stage consumers may still use compiled defaults, but a test consumer
must prove generation publication and rollback.

### Split

Add committed live-profile state to the selected reconciliation design:

- local commit note;
- digest coverage;
- peer transfer;
- peer validation;
- peer commit;
- reconnect and role-swap authority;
- same-generation disagreement behavior;
- diagnostics.

Volatile preview must not outrank durable committed state after reconnect.

## Likely Files

- users/noah/lib/profile/schema/
- users/noah/lib/profile/store/
- users/noah/lib/profile/runtime/
- users/noah/lib/profile/protocol/
- users/noah/lib/compat/ storage, VIA, Raw HID, and split contracts
- selected split reconciliation package
- users/noah/source_manifest.mk
- users/noah/runtime_init.c and hooks only through narrow helpers
- tools/charybdis-profile-studio/ encoder, operation coordinator, and fake
  device
- new host runners and fixtures
- architecture and tooling docs

## Deliverables

- [x] Firmware and desktop Profile Wire v1 codecs, including generic blob,
  domain envelope, semantic actions, key behaviors, RGB, and compiled-default
  materialization
- [x] Shared golden fixtures for generic blob/action, representative behavior,
  representative RGB, and the complete real compiled profile
- [x] Layered validator (generic structure, canonical order, reserved fields,
  action capacity, behavior/RGB semantics, whole-profile checksums,
  action-ABI identity, domain masks, and compiled action references)
- [ ] Recoverable persistent store (read-only normal boot ownership and gated
  writable owner, provider-guarded staging, incremental selection/adoption,
  durable commit, reset activation, and exact peer import/reconciliation
  landed; host routing and hardware interruption evidence remain)
- [ ] Candidate protocol (exact firmware/desktop frame/status codecs, bounded
  scan coordinator, durable commit/activation coordinator, and desktop prepare
  plus commit coordinator landed; QMK routing and capabilities remain)
- [ ] Safe activation owner and predicate (predicate, coherent reason/count
  snapshot, and gated provider-owner installation landed; hardware evidence
  remains)
- [x] Effective profile generation publication
- [x] Domain invalidation contract
- [ ] Split convergence (bidirectional reconciler, transport adapter, flash
  physical identity, gated owner consumption, registration, precommit host
  supersession, and distributed prepare/commit/activation fencing landed;
  hardware convergence and recovery evidence remain)
- [ ] Reset and migration behavior (override-disabled durable records now
  activate compiled fallback; production routing and upgrade evidence remain)
- [x] Debug/status snapshots for the gated owner read surface
- [ ] Source manifest, docs, decisions, risks, and progress updates

## Verification

Required targeted tests:

- codec round trip and golden bytes in C and JavaScript
- truncated, overlong, duplicate, unknown, reserved-bit, overflow, and
  cross-reference-invalid candidates
- candidate chunk reordering, duplicate retry, conflict, abort, timeout, and
  restart
- power loss or reset at every storage transition
- last-known-good, compiled fallback, incompatible schema, corruption, and
  factory reset
- activation immediate-safe, waits-until-safe, rejection, rollback, and
  invalidation ordering
- split disconnect, reconnect, transient rejection, role swap, dual-USB,
  same-generation disagreement, and interrupted transfer
- QMK contracts, feature gates, runtime init order, tracing, and diagnostics
- full host suite
- fresh firmware compile
- fresh memory and stack gates
- hardware protocol, reboot, interruption, reconnect, and role-swap matrix
- git diff --check

Landed generic-codec evidence:

- `tests/fixtures/profile_blob_v1.fixture` is consumed directly by the C host
  test and the Profile Studio JavaScript test;
- normal and ASan/UBSan tests cover every truncated prefix, unknown and
  out-of-order domains, duplicates, trailing input, reserved flags, aggregate
  and action capacities, all unknown action tags and nonzero action flags, and
  a deterministic malformed corpus;
- the userspace source manifest and feature-gate matrix compile the firmware
  codec with VIA disabled and enabled, including split/RGB variants through the
  existing common-source matrix.

Landed behavior-codec evidence:

- `tests/fixtures/key_behavior_domain_v1.fixture` is the single payload,
  envelope, blob, and digest golden consumed by both languages;
- the firmware decoder operates through an injected bounded reader plus
  base/length and never requires a contiguous candidate buffer;
- the retained validated handle is at most 128 bytes and payload-independent,
  individual reads are at most 12 bytes, and row/step accessors resolve one
  record at a time;
- normal and ASan/UBSan coverage enforces ordering, uniqueness, fixed counts,
  sparse masks, stable hold ids, repeat rules, timing widths, non-none targets
  and branches, negotiated-lower ceilings, every truncated prefix, injected
  read failure, and deterministic malformed input.

Landed RGB-codec evidence:

- `tests/fixtures/rgb_domain_v1.fixture` is the single limits, payload,
  envelope/blob, and digest golden consumed by the firmware C and Profile
  Studio JavaScript tests;
- the firmware decoder retains a bounded reader-backed view rather than a
  payload, dictionary, or renderer-table copy; a 32-bit static assertion caps
  the view at 40 bytes;
- decode uses one 16-byte header read and fixed record reads of at most 11
  bytes, while record-at-a-time accessors expose every RGB table for a future
  provider;
- normal and ASan/UBSan coverage enforces exact lengths, capacities,
  dictionary identity/order/uniqueness, LED geometry, selectors, references,
  enums, brightness, stable PD ids, complete compiled surfaces, canonical
  absent-stage data, negotiated-lower limits, every truncated prefix, and
  injected reader failure.

Landed read-only store-integration evidence:

- QMK EEPROM access is restricted to `0x2000–0x3FFF` and the boot owner receives
  a null write callback;
- post-init only arms discovery; the first matrix scan selects the newest valid
  committed slot once, with compiled-default fallback or explicit
  equal-generation conflict state;
- status can report committed identity while active state remains truthfully
  compiled-only and every mutation/domain capability remains disabled;
- `tests/host/run_profile_store_runtime_tests.sh` covers adapter bounds,
  read-only ownership, one-shot scan behavior, committed discovery, and
  conflict discovery;
- the normal and sanitized store suite covers metadata-divergent equal
  generations plus power loss at every write and partial-byte boundary through
  the final commit marker;
- at this checkpoint, target gates passed at a 48,664-byte SRAM0–3
  `.data + .bss` regression metric against the 51,000-byte policy, with a
  213,472-byte linker/core-memory span at boot and a 480-byte reviewed boot
  path; the existing worst reviewed main/split paths remained 1,880 B and
  336 B.

Landed standalone candidate-coordinator evidence:

- exact 32-byte begin, chunk, validate, abort, custom-save `0x13` commit,
  immediate-admission, and operation-status layouts are frozen in
  `profile-wire-v1.md` and executable through
  `tests/fixtures/profile_candidate_v1.fixture`;
- the callback-side API only decodes and copies one bounded mailbox item, while
  scan context owns backend begin/write/read/abort, validation, marker-last
  persistence, and activation polling; validation and commit each advance
  through a common one-read-or-write / 20-byte maximum per scan-step contract;
- retries compare staged bytes: identical duplicates are accepted, conflicting
  duplicates poison the candidate, and wrong transaction ids preserve the
  active candidate identity;
- normal and ASan/UBSan tests cover every short frame length, overlong frames,
  every reserved/padding byte, gaps, partial overlaps, overflow, reset with a
  pending or staged candidate, idempotent abort, backend failures, structured
  validation locations, device-side precommit timeout, and the rule that a
  desktop transport timeout does not itself abort firmware state;
- the coordinator has a compile-time 256-byte ceiling; commit is idempotently
  correlated by transaction id and digest across COMMITTING, ACTIVATING, and
  final IDLE status, and an unconfirmed final-marker write is reported as
  durability-unknown rather than as a safe failure. Production QMK routing and
  candidate capability bits remain unchanged and disabled.

Landed whole-profile validation and staging-backend evidence:

- `tests/fixtures/profile_validator_v1.fixture` drives the whole-profile
  validator through an injected reader at a nonzero base offset;
- checksum work reads at most the supplied budget and 20 bytes, while domain
  composition retains the established maximum individual RGB and behavior
  reads of 16 and 12 bytes;
- schema, declared/required/allowed domain masks, action ABI, canonical domain
  ordering, duplicate/unknown/trailing data, domain semantic failures, and
  layer/PD/VIA/hardcoded action references produce stable structured errors;
- the candidate-store backend writes only the inactive slot, validates through
  a borrowed reader, and requires an explicit separate call before the store's
  marker-last commit can run;
- copied validated domain views retain an absolute committed-slot base, so
  beginning a later candidate cannot silently retarget an active view to the
  new inactive slot; and
- the ordinary boot shell still receives a null write callback, while the gated
  owner has the write-capable backend. No QMK callback can reach candidate
  staging yet.

Landed compiled-default materialization evidence:

- the real authored RGB and key-behavior tables materialize into one canonical
  1,089-byte profile with CRC32 `fd3c39ef`, FNV-1a `aee2da1b`, and action-ABI
  identity `dcb00959`;
- `tests/fixtures/compiled_profile_v1.fixture` is decoded and re-encoded
  byte-identically by both the firmware C tests and Profile Studio JavaScript;
- the materializer retains only a metadata handle capped at 20 bytes and the
  action-ABI walk is bounded by authored row counts (185 visits for the real
  profile, with a frozen 4,160-visit schema ceiling); and
- its virtual reader is explicitly cold-path only for identity, validation,
  export, and reset persistence. Hot key-event and RGB-frame consumers must use
  direct compiled semantic tables when the effective kind is compiled rather
  than replaying the virtual blob.

Landed isolated effective-provider evidence:

- copied active snapshots use the existing bounded `runtime_publication`
  protocol and fail closed for the entire invalidation interval;
- invalidators run in declared order with a callback-only view of the new
  generation, so ordinary readers cannot observe a new identity with stale
  derived state;
- safe-boundary evaluation is reentrancy-guarded, and behavior-bearing pending
  profiles cannot publish without an injected safe predicate;
- nested reader-backed domain views must remain inside the checksummed
  top-level blob; and
- destructive storage reuse requires an explicit complete-backing reservation.
  Active, compiled, and pending ranges remain pinned, while an overlapping
  rollback copy is discarded atomically. Discarding rollback alone is not
  storage authorization.

The persistent store and candidate backend now adopt this reservation protocol.
Every admitted prepare reserves the complete target-slot payload range before
the marker is invalidated and releases it on abort, commit, or terminal failure.
The backend can turn its exact durable record plus validated borrowed view into
a pending provider snapshot; provider polling still owns safe publication. A
runtime rollback that makes the store's nominal inactive slot active is
mechanically refused before EEPROM mutation, and moving active authority away
allows the next reservation to discard only the overlapping rollback copy.
This seam is still disconnected from QMK mutation routing. The safe predicate,
callback-only behavior and RGB invalidators, consumer lookup seam, and every
current RGB renderer-family adapter are installed by the side-specific gated
owner. Ordinary firmware still uses the compiled path. The engineering read
surface advertises only schema/digest knowledge; all mutation capability bits
remain disabled.

Landed desktop candidate evidence:

- exact JavaScript begin/chunk/validate/abort/commit/status bytes consume the
  shared firmware candidate fixture;
- upload tests cover sequential 20-byte chunks, wrapping nonzero request and
  transaction ids, queued-operation polling, byte-identical safe busy retries,
  unsafe overlap refusal, cooperative abort, bounded polling, and structured
  validation failures;
- preflight refuses all non-idle firmware candidates without sending a
  mutation, caller-owned blob bytes are snapshotted before asynchronous work,
  and deterministic failures clean up with an idempotent abort; and
- timeout or disconnect after a staging mutation is classified as ambiguous
  and is never retried on the same connection. The separate commit coordinator
  can resume an exact admitted commit from status because firmware retains its
  transaction/digest identity; durability-unknown remains explicitly unsafe to
  retry until status is reconciled. Neither coordinator is connected to the
  extension UI or device service.

The former whole-domain work blocker is resolved by incremental domain state
machines. `tests/host/run_profile_validator_work_budget_tests.sh` constructs
the schema-maximal behavior payload (64 rows, 128 fully populated steps;
3,216-byte complete profile) and mechanically limits every whole-profile step
to one reader call and 20 bytes. It completes in 1,061 steps: 897 behavior
domain steps with an actual maximum 12-byte read and zero reference rescans.
The maximal valid RGB payload completes in 58 domain steps with an actual
maximum 16-byte read. RGB, behavior, and whole-profile runners also compile the
state machines for Cortex-M0+. The whole validator is exactly 348 bytes on the
32-bit target under an explicit 352-byte regression policy; that policy is not
a hardware SRAM-capacity claim. Production routing and mutation capability
bits remain disabled pending installation of the landed safe predicate,
invalidators, and the QMK owner; real-device scan timing remains unconfirmed
until the hardware pass.

Landed safe-predicate evidence:

- `profile_activation_policy.c` maps authoritative key-runtime counts, managed
  HID outputs, live modifier/one-shot state, macro lifecycle, combo-origin
  state, and an injected peer observer into the frozen reason mask above;
- owned HID usage maintains an aggregate active-usage count, so a pending
  activation does not scan the full usage table on every matrix cycle;
- missing or failed peer observation and invalid policy state fail closed;
- reason/count status is copied through the established bounded publication
  protocol; and
- focused normal, ASan/UBSan, no-one-shot, and Cortex-M0+ tests cover every
  reason, peer failure, saturation, and in-flight status refusal. Runtime-debug
  and owned-keycode tests enforce the two new authoritative observation seams.

Landed split-foundation evidence:

- `profile-split-v1.md` and D-017 freeze a dedicated 32-byte profile protocol
  rather than extending VIA regions; descriptor frames carry the complete
  durable identity and firmware compatibility, while payload frames transfer
  at most 14 bytes with canonical padding and CRC8;
- the D-014 comparator distinguishes compiled/committed convergence, the newer
  side, disconnected concurrent commits, same-tuple corruption, incompatible
  firmware, unreadable state, and invalid metadata without using current USB
  role as authority;
- its coherent peer observer reports resolved only for an exact durable match
  with no transfer in progress, so stale, malformed, or in-flight observations
  cannot leave activation accidentally open; and
- normal, ASan/UBSan, Cortex-M0+, fixed-golden, truncation, padding, checksum,
  enum, bounds, publication, and fail-closed observer coverage is in
  `run_profile_split_foundation_tests.sh`. The files are in the userspace
  manifest but no QMK RPC or advertised capability uses them yet.

Landed exact peer-store evidence:

- `profile_peer_store_backend.c` serializes peer import through the same
  candidate backend, validator, store, provider reuse guard, and inactive slot
  as host deployment. It retains the sender's generation, stable physical
  origin, persistent flags, schema, CRC/FNV identity, compiled-default digest,
  action ABI, payload length, and domain mask unchanged;
- fully repeated chunks are read back and compared, while gaps, partial
  overlaps, conflicting retries, stale generations, equal-generation
  concurrent origins, same-tuple corruption, and incompatible firmware are
  rejected without starting or continuing unsafe writes;
- the store derives the domain mask during boot/commit shape validation,
  rejects records made against different compiled defaults, and latches
  durability-unknown until a conclusive boot selection. Peer success requires
  marker readback and exact field comparison excluding only the slot number;
- override-disabled records remain durable with zero persistent flags and do
  not request validated-profile activation. Boot-discovered exact records fail
  closed until the production owner advances the landed bounded adoption
  validator and retains their decoded view; and
- `run_profile_peer_store_backend_tests.sh` provides normal and ASan/UBSan
  coverage for exact commit/reboot identity, idempotence without writes,
  reset flags, chunk retry failures, ordering/compatibility decisions,
  domain-mask mismatch, provider reservation release, and ambiguous-marker
  reconciliation. The production source and runner are wired into the common
  manifest, explicit VIA/split compile matrix, and full host suite.

Landed production-owner safety-foundation evidence:

- the store boot selector has begin/step APIs whose tests enforce at most one
  read per step, a 32-byte maximum fixed read, budget-honoring payload reads,
  exact selection/conflict semantics, and idempotent terminal results;
- the real compiled-default materializer derives exact validator limits for
  domains, action ABI, layers, PD modes, macro spaces, RGB brightness/stages,
  and tap branches instead of exposing schema-wide maxima;
- the candidate backend can validate and adopt the exact selected committed
  record after reboot without writes, requests a validated snapshot only when
  the durable override flag is set, and otherwise requests compiled fallback;
- one explicit `NONE`/`HOST`/`PEER` admission lease prevents host and peer from
  interleaving against the shared store. Retryable host contention remains
  queued without poisoning its transaction, and terminal abort or successful
  activation releases the lease; and
- QMK profile split registration is idempotent for the same reconciler and
  rejects a second owner. At that safety-foundation checkpoint the pieces were
  still disconnected; the gated composition below supersedes that snapshot.

Landed gated owner-composition evidence:

- one caller-owned graph now composes compiled validation, incremental boot
  selection/adoption, the store/provider/candidate/peer/reconciler objects,
  the safe activation policy, and the behavior/RGB runtime invalidators;
- boot-selected records reconcile in full before activation begins. A newer
  peer may therefore replace an older validated local record without either
  half waiting behind the activation convergence predicate;
- host and peer retain one exact admission lease through activation. While the
  host owns it, split scheduling is convergence-only: metadata, serving, and
  pushing the known durable local record continue, but a peer import cannot
  begin or advance;
- ordinary abandoned host staging expires after 15 seconds. Distributed peer
  preparation uses a separate 60-second no-progress window refreshed only by
  acknowledged payload advance, and confirms peer abort before releasing the
  local candidate. Neither path expires marker-last commit, activation, or
  uncertain durability. Error id `19` is frozen in firmware and Profile Studio;
- the QMK EEPROM adapter enforces the 32-byte store I/O contract and uses a
  direct block write, preserving one wear-level write per scan grant without
  QMK's variable-stack update helper;
- a side-specific engineering build installs this graph and registers one
  profile split transport. Ordinary firmware retains its read-only shell, and
  both builds leave host mutation unadvertised and unrouted;
- the gated owner now exports one caller-owned snapshot of compiled, active,
  pending, committed, peer, and candidate state. VIA latches page zero and
  serves page one from the same observation, and its engineering capabilities
  advertise only the landed schemas/digests while all write-operation bits and
  chunk capacity remain zero;
- a compatible peer generation greater than or equal to the reserved host
  generation cancels the candidate before commit, including one queued commit,
  preserves transaction/digest correlation, releases the host lease, and
  reports stable error `20` without changing the prior committed record; and
- exact linked engineering evidence is recorded in the canonical memory doc.
  The owner is 3,084 bytes per half and its reviewed owner stack paths pass,
  but the artifact fails the BSS and combined-data regression policies. It is
  therefore not eligible for normal exposure without hardware high-water and
  an explicit resource-policy decision.

## Exit Criteria

- Desktop and firmware produce identical canonical bytes and digests.
- Any malformed candidate is rejected before mutation or activation.
- Reset during persistence always yields the old generation, new generation, or
  compiled defaults according to the written recovery contract.
- A valid candidate activates exactly once at the documented safe boundary.
- The prior generation remains active on failure.
- Both halves converge durably and role swap does not resurrect stale data.
- Resource budgets remain inside approved limits.
- A test provider proves runtime generation publication.

## Handoff

Stages 03 and 04 may now proceed independently if file ownership is separated.
They consume schema/provider APIs and must not invent new storage or transport
paths.
