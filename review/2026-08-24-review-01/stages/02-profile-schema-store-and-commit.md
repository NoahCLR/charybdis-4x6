# Stage 02 — Profile Schema, Store, And Atomic Commit

Status: blocked on Stage 01

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

- [ ] Firmware and desktop Profile Wire v1 codecs
- [ ] Shared golden fixtures
- [ ] Layered validator
- [ ] Recoverable persistent store
- [ ] Candidate protocol
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
