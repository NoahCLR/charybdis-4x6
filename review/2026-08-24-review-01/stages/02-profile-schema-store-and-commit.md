# Stage 02 — Profile Schema, Store, And Atomic Commit

Status: blocked on Stage 01

Implementation note: two isolated foundations landed early. The storage
package freezes the 32-byte header, checksum helpers, read-only boot discovery,
bounded inactive-slot writes, readback validation, and final-marker commit. A
slot-bounded QMK adapter and one-shot matrix-scan boot owner now make discovery
reachable on firmware and expose committed metadata through status. Boot
ownership still receives only the read-only adapter; the write-capable form is
available solely for the disconnected candidate backend.
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
backend. These paths remain intentionally unrouted and unadvertised. The
isolated effective-provider foundation is now hardened and tested after
lifecycle review. Durable commit ownership, runtime activation, reset, and
split reconciliation are still absent, so this stage remains
blocked/incomplete.

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
- [ ] Recoverable persistent store (storage foundation, read-only QMK boot
  ownership/status, provider-guarded inactive-slot staging, durable commit, and
  activation handoff landed; QMK mutation ownership, reset, and split
  integration remain)
- [ ] Candidate protocol (exact firmware/desktop frame/status codecs, bounded
  scan coordinator, durable commit/activation coordinator, and desktop prepare
  plus commit coordinator landed; QMK routing and capabilities remain)
- [ ] Safe activation owner and predicate
- [ ] Effective profile generation publication
- [ ] Domain invalidation contract
- [ ] Split convergence
- [ ] Reset and migration behavior
- [ ] Debug/status snapshots
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
- target gates pass at a 48,664-byte SRAM0–3 `.data + .bss` regression metric
  against the 51,000-byte policy, with a 213,472-byte linker/core-memory span
  at boot and a 480-byte reviewed boot path; the existing worst reviewed
  main/split paths remain 1,880 B and 336 B.

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
  validation locations, and the explicit v1 no-timeout behavior;
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
- the production boot owner still receives a null write callback, Profile Wire
  mutation/domain capability bits remain disabled, and no QMK callback can
  reach candidate staging yet.

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
This seam is still disconnected from QMK routing. No production safe predicate
or invalidator is installed, and no key/RGB consumer reads through it.

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
bits remain disabled pending the production safe predicate, invalidators, and
QMK owner; real-device scan timing remains unconfirmed until the hardware pass.

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
