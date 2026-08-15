# Durable VIA Split Reconciliation Progress

## Why This Review Exists

Reviews 08 and 09 closed the shared clock and transient runtime outage work.
Finding 05 now addresses durable VIA storage authority and convergence.

## 2026-08-15 — Baseline

Passed before implementation:

- `sh tests/host/run_qmk_via_split_sync_tests.sh`
- `sh tests/host/run_qmk_contract_checks.sh`
- `sh tests/host/run_via_macro_defaults_tests.sh`
- `sh tests/host/run_via_macro_action_lifecycle_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_macro_dispatch_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_runtime_init_order_tests.sh`
- `sh tests/host/run_hook_chaining_tests.sh`

## Audit Findings

- Current sync is pre-apply raw command replay, not committed-state sync.
- Macro set/reset completeness, send failure retention, application ack,
  versioning, snapshots, boot handshake, and role reconciliation are absent.
- The 32-bit user eeconfig word is locally unclaimed beyond initialization and
  can hold schema/dirty/generation without shifting VIA addresses.
- A mandatory metadata exchange every session can preserve unresolved-peer
  semantics without storing peer acknowledgement in the same word.
- Physical closure cannot be inferred from host simulation; it remains a
  separate required gate.

## Checkpoint Status

Finding 05 is **open**. Command completeness, metadata persistence, reset/seed
ordering, and the frame codec are green; canonical storage readback plus the
scan-time acknowledged snapshot consumer are next.

## 2026-08-15 — Metadata and Command-Completeness Foundation

### Completed

- Added a pure 32-bit metadata codec with schema, dirty bit, 27-bit nonzero
  generation, zero-skipping increment, half-range serial comparison, and
  explicit ambiguity handling.
- Added round-trip, invalid-schema, reserved-zero, rollover, and half-range
  tests and wired the new source through the canonical firmware manifest and
  complete host runner.
- Added a validated full-packet mutation classifier covering keycode, keymap
  buffer/reset, macro buffer/reset, EEPROM reset, and nested layout options.
- Added destination and transport-length checks for keymap and macro writes.
- Expanded the host VIA command IDs so missing upstream mutations cannot hide
  behind incomplete fixtures.
- Replaced `via_command_kb()`'s raw pre-QMK send with an owned pending-effects
  notification. Macro reset is now correctly classified as reseed plus split
  replication; no borrowed HID pointer is retained.

### Red/Green Evidence

- `sh tests/host/run_qmk_via_command_classifier_tests.sh` first failed because
  the full-packet classifier did not exist, then passed after implementation.
- Correctly classifying macro reset initially made
  `run_via_macro_defaults_tests.sh` fail because the old hook attempted the
  pre-apply split send. Replacing it with pending notification made the suite
  pass without publishing unseeded macro state.

### Verification Passed

- `sh tests/host/run_qmk_via_command_classifier_tests.sh`
- `sh tests/host/run_qmk_via_sync_metadata_tests.sh`
- `sh tests/host/run_via_macro_defaults_tests.sh`
- `sh tests/host/run_qmk_via_split_sync_tests.sh`
- `sh tests/host/run_qmk_contract_checks.sh`
- `sh tests/host/run_runtime_init_order_tests.sh`
- `sh tests/host/run_hook_chaining_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

### Important Incomplete Boundary

Pending mutation effects deliberately have no publication consumer yet. This
checkpoint must not be committed or described as restored durable sync until
scan-time readback advances/persists a clean generation and the peer provides
an application-level generation/digest acknowledgment.

## Planned Continuation

1. Add canonical VIA-config, keymap, encoder, and macro region read/write plus
   whole-snapshot digest helpers under the compatibility boundary.
2. Connect post-apply readback to clean-generation completion; keep reset
   completion blocked until explicit macro reseed success.
3. Replace the legacy raw-command RPC callback with the validated frame codec
   and an acknowledged one-fragment-per-tick snapshot state machine.

## 2026-08-15 — Durable State and Frame Codec

### Completed

- Added the persistent state controller over the existing atomic 32-bit user
  eeconfig word. Clean boot, pre-apply dirty marking, coalescing, validated
  completion, rollover, invalid schema, dirty boot, and explicit recovery are
  covered by focused tests.
- Wired split-sync initialization to load metadata and mutation notification to
  persist dirty state before returning control to upstream QMK.
- Replaced the old reset-time zero write with a clean generation-1 publication
  only after authored VIA macro seeding reports success. Failed seeding writes
  dirty generation 1 and remains non-authoritative.
- Added an endian-stable 32-byte frame codec with CRC-8, 14-byte fragments,
  version, generation, region/range, digest, status, and strict per-kind shape
  validation.
- Wired the new state/protocol sources through the canonical firmware manifest
  and their runners through the full host suite.

### Verification Passed

- `sh tests/host/run_qmk_via_sync_state_tests.sh`
- `sh tests/host/run_qmk_via_sync_metadata_tests.sh`
- `sh tests/host/run_qmk_via_sync_protocol_tests.sh`
- `sh tests/host/run_qmk_via_split_sync_tests.sh`
- `sh tests/host/run_via_macro_defaults_tests.sh`
- `sh tests/host/run_runtime_init_order_tests.sh`
- `sh tests/host/run_qmk_via_command_classifier_tests.sh`
- `sh tests/host/run_qmk_contract_checks.sh`
- `sh tests/host/run_hook_chaining_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `git diff --check`

### Still Open

The clean-after-readback function is deliberately not called yet, and the new
frame codec is not yet the live RPC handler. The legacy raw replay callback is
therefore still present only as transitional code while outbound mutation
publication remains disabled. This tree is not a Finding 05 completion or a
commit point.

## Next Steps

1. Add canonical VIA-config, keymap, encoder, and macro region read/write plus
   whole-snapshot digest helpers under the compatibility boundary.
2. Connect post-apply readback to clean-generation completion; keep reset
   completion blocked until explicit macro reseed success.
3. Replace the legacy raw-command RPC callback with the validated frame codec
   and an acknowledged one-fragment-per-tick snapshot state machine.

## Reconciliation Note

The earlier “Still Open” and “Next Steps” sections are chronological
checkpoint records. They no longer describe the current tree: the canonical
storage layer, live framed protocol, and acknowledged state machine have now
landed as described below.

## 2026-08-15 — Live Durable Reconciliation

### Completed

- Added bounded canonical region access and incremental whole-state digest for
  VIA validity/layout options, dynamic keymap, optional encoders, and macros.
- Moved cache invalidation and clean-generation completion to the post-QMK
  scan boundary; reset publication waits for authored macro seeding success.
- Replaced raw pre-apply replay with the live 32-byte framed metadata and
  snapshot RPC using application acknowledgements.
- Added deterministic clean/newer, dirty, schema, equal-digest, divergent
  equal-generation, reboot, rejoin, and role-change reconciliation.
- Added one-chunk/one-RPC scan work, 50–1,000 ms retry backoff, periodic session
  refresh, and explicit two-dirty-half VIA-default recovery.
- Added receiver ordering/range/digest enforcement, exact duplicate handling,
  commit `BUSY` state, and cache invalidation only after verified local commit.
- Separated main-scan verifier cursors from the split callback and added atomic
  snapshots, digest epochs, and finalization reservation for `SlaveThread`
  concurrency.
- Added diagnostics for local/peer generation and digest, last peer ack,
  reconciliation state, retries, rejected frames, conflicts, and last error.
- Added focused storage, classifier, metadata, state, protocol, lifecycle,
  disconnect/loss, conflict, recovery, role, corruption, duplicate, digest,
  and replacement-snapshot tests, including sanitizer and encoder variants.

### Verification Passed So Far

- `sh tests/host/run_qmk_via_split_sync_tests.sh`
- `sh tests/host/run_qmk_via_sync_state_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `git diff --check`

### Remaining Gates

1. Run the complete host suite on the final tree.
2. Run the ordinary Charybdis firmware build.
3. Run a fresh instrumented build and target stack budget check; reconcile
   reviewed LTO edges and record size/stack measurements.
4. Perform the manual two-half disconnect, power-cycle, reconnect, and USB-role
   swap matrix before changing Finding 05 from verification-incomplete to
   verified.

## 2026-08-15 — Final Software Verification Checkpoint

### Follow-up Defect Closed

The target call-path audit showed that production `eeconfig_init_via()` resets
QMK VIA regions but does not invoke the userspace macro-default hook or publish
clean reconciliation metadata. The original host fake did both and masked a
two-dirty-half convergence loop. Recovery now explicitly resets VIA, reseeds
authored macro defaults through a side-effect-free recovery API, publishes
generation 1 only on seed success, recomputes the canonical digest, and
invalidates RGB/macro caches only after that digest commits. The fake no longer
performs userspace work on QMK's behalf, and the regression asserts authored
macro bytes, clean metadata, and post-digest invalidation.

### Final Verification Passed

- all focused Finding 05 runners, including normal, ASan/UBSan, and encoder
  split-sync variants;
- `sh tests/host/run_feature_gate_compile_tests.sh`;
- `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_tool_tests.sh` (15 tests);
- `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh`;
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`;
- `PYTHONPYCACHEPREFIX=/tmp/noah-profile-pycache python3 tools/profile_introspect.py --write`;
- `PYTHONPYCACHEPREFIX=/tmp/noah-profile-pycache python3 tools/profile_introspect.py --check`;
- `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_checks.sh`;
- `git diff --check`.

### Measurements

- Ordinary linked target: 150,748 B text, 0 B data, 245,584 B BSS.
- Delta from the Finding 12 recorded baseline: +5,544 B text, -8 B BSS.
- Canonical target snapshot: 16,345 B total (600 B keymap, 0 B encoder,
  15,743 B macro, 2 B config), or 1,169 data fragments at 14 B each, plus
  metadata/begin/commit exchanges.
- Fresh reviewed-path main-process maximum: 1,904/1,920 B (`matrix scan
  bounded deferred fallback settlement`).
- Fresh reviewed-path split-worker maximum: 336/768 B (`VIA pushed macro
  fragment write`).
- Finding 05-specific reviewed paths pass for canonical digest reads,
  metadata/snapshot RPC, pulled and pushed macro fragments, verified metadata
  commit, two-dirty recovery, snapshot-begin persistence, and structured
  acknowledgement encoding.

### Fresh Stack Gate Passed

After the earlier permission block cleared,
`PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh
tests/host/run_firmware_stack_budget_checks.sh` completed a clean instrumented
build and passed. The post-LTO report covers the reconciled live callback and
main digest/RPC/pull/recovery/commit paths plus split begin/read/write/ack
paths. Its 1,904 B main maximum retains 16 B inside the 1,920 B reviewed-path
budget; the 336 B split maximum retains 432 B inside the 768 B budget. As the
tool states, this is an explicit reviewed-path regression proof, not a global
maximum-stack proof.

### Current Status and Next Steps

Finding 05 is implemented and all software verification is green. It remains
**hardware-verification incomplete**: perform the physical disconnect,
per-half power-cycle, reconnect, and USB-role-swap matrix before moving the
finding to fully verified.
