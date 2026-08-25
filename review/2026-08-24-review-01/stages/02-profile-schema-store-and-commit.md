# Stage 02 — Profile Schema, Store, And Atomic Commit

Status: blocked on Stage 01

Implementation note: two isolated foundations landed early. The storage
package freezes the 32-byte header, checksum helpers, read-only boot discovery,
bounded inactive-slot writes, readback validation, and final-marker commit. A
read-only, slot-bounded QMK adapter and one-shot matrix-scan boot owner now make
discovery reachable on firmware and expose committed metadata through status;
they do not activate the discovered payload or expose a write callback.
The generic schema package implements the canonical `NLP1` blob/domain
envelopes and four-byte semantic actions in firmware and Studio against one
shared exact-byte fixture. The key-behavior and RGB packages now implement
matching desktop and reader-backed firmware payload codecs against shared
exact-byte fixtures. A standalone exact candidate-frame codec and scan-owned transaction
coordinator now cover begin/chunk/validate/abort and operation status, but are
intentionally not routed through QMK or advertised. These packages still do
not provide the full cross-reference/action-ABI validator, commit, runtime
activation, reset, or split reconciliation, so this
stage remains blocked/incomplete.

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

- [ ] Firmware and desktop Profile Wire v1 codecs (generic blob, domain
  envelope, semantic-action, key-behavior, and RGB domain layers landed;
  compiled-default materialization and whole-profile composition remain)
- [ ] Shared golden fixtures (generic blob/action vector is executable in C and
  JavaScript; representative behavior and RGB vectors are also shared;
  compiled-profile fixtures remain)
- [ ] Layered validator (generic structural, canonical-order, reserved-field,
  action-capacity, behavior, and RGB wire-semantic validation landed;
  action-ABI and cross-domain reference layers remain)
- [ ] Recoverable persistent store (storage foundation plus read-only QMK boot
  ownership/status landed; candidate mutation ownership, reset, activation,
  and split integration remain)
- [ ] Candidate protocol (exact standalone frame/status codecs and bounded
  scan coordinator landed; QMK routing and commit remain)
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
- target gates pass at a 48,640-byte SRAM0–3 `.data + .bss` regression metric
  against the 51,000-byte policy, with a 213,496-byte linker/core-memory span
  at boot and a 480-byte reviewed boot path; the existing worst reviewed
  main/split paths remain 1,880 B and 336 B.

Landed standalone candidate-coordinator evidence:

- exact 32-byte begin, chunk, validate, abort, immediate-admission, and
  operation-status layouts are frozen in `profile-wire-v1.md` and executable
  through `tests/fixtures/profile_candidate_v1.fixture`;
- the callback-side API only decodes and copies one bounded mailbox item, while
  scan context owns backend begin/write/read/abort and 20-byte-budgeted
  validation steps;
- retries compare staged bytes: identical duplicates are accepted, conflicting
  duplicates poison the candidate, and wrong transaction ids preserve the
  active candidate identity;
- normal and ASan/UBSan tests cover every short frame length, overlong frames,
  every reserved/padding byte, gaps, partial overlaps, overflow, reset with a
  pending or staged candidate, idempotent abort, backend failures, structured
  validation locations, and the explicit v1 no-timeout behavior;
- the coordinator has a compile-time 256-byte ceiling and exposes no commit or
  activation operation. Production QMK routing and candidate capability bits
  remain unchanged and disabled.

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
