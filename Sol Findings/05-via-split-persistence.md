# Finding 05: Make Split VIA Persistence Complete and Durable

## Plan metadata

- **Severity:** Must fix — persistent state divergence, role-swap data loss, and stale split rendering/playback
- **Status:** Planned; no remediation has landed
- **Affected surfaces:** VIA hook ordering, storage-command classification, macro default seeding, split RPC protocol, retry/rejoin state, persistent generation metadata, RGB/macro cache invalidation
- **Primary files:** [`qmk_via_contract.c`](../users/noah/lib/compat/qmk_via_contract.c), [`qmk_via_storage_contract.h`](../users/noah/lib/compat/qmk_via_storage_contract.h), [`qmk_via_split_sync.c`](../users/noah/lib/compat/qmk_via_split_sync.c), [`qmk_via_split_sync.h`](../users/noah/lib/compat/qmk_via_split_sync.h), [`via_macro_defaults.c`](../users/noah/lib/macro/via_macro_defaults.c), runtime scan/init wiring
- **Prerequisites:** Finding 03 packet validation; coordinate retry cadence with Finding 12; reserve and document persistent sync metadata before implementation
- **Recommended phase:** Phase 3, after immediate memory-safety/boundary fixes

## Problem statement

The current split-VIA integration best-effort replays selected incoming commands on the slave. It omits macro-buffer writes and macro-reset outcomes, sends before QMK applies the command on the master, ignores transport failure, and has no version, dirty state, retry, startup handshake, or reconnect reconciliation. The two EEPROM copies can therefore diverge permanently. The non-master can render stale RGB, macro playback after a role change can use stale contents, and a role swap can make older storage authoritative.

The replacement must synchronize committed VIA-owned state, not merely forward packets. It needs complete command classification for fast deltas plus a versioned, acknowledged snapshot protocol that can recover after loss, reset, reboot, disconnect, and role change.

## Current evidence and failure scenarios

- [`noah_qmk_via_command_effects()`](../users/noah/lib/compat/qmk_via_contract.c#L62) mirrors keycode, keymap-buffer, keymap-reset, and encoder writes, but omits `id_dynamic_keymap_macro_set_buffer`.
- `id_dynamic_keymap_macro_reset` requests only local reseeding at [`qmk_via_contract.c`](../users/noah/lib/compat/qmk_via_contract.c#L74); it is not mirrored.
- Slave application in [`qmk_via_split_sync.c`](../users/noah/lib/compat/qmk_via_split_sync.c#L23) has no macro set/reset cases.
- [`noah_qmk_via_split_sync_command()`](../users/noah/lib/compat/qmk_via_split_sync.c#L91) discards the Boolean result of `transaction_rpc_send()`, so failure creates no dirty/retry state.
- [`via_command_kb()`](../users/noah/lib/macro/via_macro_defaults.c#L192) runs before upstream QMK's command switch. It calls split sync at lines 205–206 before the master has applied the mutation.
- Macro reset seeding occurs later from matrix scan at [`via_macro_defaults.c`](../users/noah/lib/macro/via_macro_defaults.c#L173), so replaying the original reset still would not mirror the final authored-default bytes.
- There is no boot/rejoin version exchange, no snapshot, and no role-swap conflict rule.

Failure scenarios:

1. VIA writes a macro chunk. The master updates macro NVM, but the command is unclassified for split mirroring. After USB/role swap, the former slave plays the old macro.
2. A keymap-buffer RPC fails during a cable interruption. The return value is ignored; reconnect does not retry. Split-local RGB continues reading stale dynamic keymap data.
3. Macro reset reseeds defaults only on the master. The slave retains previous user macros indefinitely.
4. Mirroring happens before the master write. A reset/power interruption can leave the slave “ahead” of the master with no authoritative version to reconcile.

## Required invariants

1. The canonical unit is a committed VIA storage generation, never an uncommitted inbound command buffer.
2. Every VIA-owned mutation is classified from the full command shape, including keymap, encoder, macro set/reset, EEPROM reset, and layout-option changes when enabled.
3. The master marks a mutation pending in the pre-QMK hook, then reads back/commits sync state only after QMK and local macro reseeding have completed.
4. A generation is considered replicated only after the peer validates, durably applies, verifies, and acknowledges it.
5. Failed/disconnected sends leave persistent dirty state and retry with bounded backoff; they never disappear merely because one send was attempted.
6. Boot, reconnect, sequence gaps, digest mismatch, schema mismatch, and role change trigger reconciliation from a complete snapshot.
7. The half with the newer clean committed generation wins. A dirty/incomplete generation never becomes authoritative.
8. Equal-generation/different-digest divergence has a deterministic policy: current USB master wins, increments generation, records a conflict diagnostic, and sends a full snapshot.
9. Snapshot writes are range-checked, fragment-idempotent, and committed only after all regions and the final digest validate.
10. RGB and macro caches invalidate only after the corresponding local storage commit.

## Scope

### In scope

- Complete mutation classification and post-apply ordering.
- Persistent generation/dirty metadata with a migration/reset policy.
- Versioned RPC framing, response acknowledgment, bounded retry, and full two-way reconciliation.
- Keymap, encoder, macro, VIA validity/layout state, and authored-default outcomes.
- Host simulation of disconnect, loss, duplication, reordering, reboot, role swap, and divergent histories.

### Non-goals

- Editing upstream QMK.
- Mirroring unrelated eeconfig such as handedness, hardware calibration, or per-half sensor state.
- Merging two independently edited divergent keymaps. Conflict resolution is deterministic replacement.
- Blocking matrix scan while an entire EEPROM image transfers.
- Assuming transport success alone proves durable application.

## Proposed protocol and state model

### Persistent local metadata

Use the existing 32-bit user eeconfig word if it is confirmed exclusively owned by this userspace. Pack a metadata schema, `dirty` bit, and nonzero serial generation; reserve zero for uninitialized. If the required fields cannot fit with safe wrap rules, reserve a small versioned metadata block and explicitly migrate/invalidate the affected EEPROM layout rather than silently shifting VIA data.

Mutation transaction:

1. Pre-QMK hook validates/classifies the command and persists `dirty = true` before QMK can modify storage.
2. QMK applies the command after the hook returns false.
3. Deferred scan work completes authored macro reseeding when required.
4. Sync code reads back the affected canonical region, validates it, advances generation with serial-number arithmetic, and persists a clean commit marker last.
5. The new clean generation remains locally authoritative and “needs peer acknowledgment” until reconciliation succeeds.

On boot, a dirty record is never advertised as canonical. Reconcile from a clean peer if available; otherwise invalidate/reseed the affected VIA state under an explicit recovery path. This may discard an interrupted mutation, but it cannot promote partial storage.

### Wire framing

Define a packed protocol in a dedicated compatibility header with compile-time size assertions against both QMK RPC buffers. A concrete 32-byte request envelope can contain:

- protocol version and message kind;
- 32-bit generation;
- region ID;
- 16-bit region offset;
- payload length;
- up to 21 payload bytes;
- CRC-8 over header and payload.

Response/ack includes version, status, applied generation, region/next offset, and snapshot digest. Use `transaction_rpc_exec()` so success requires a structured slave response; the transport Boolean remains necessary but is not the application acknowledgment.

Message kinds: `HELLO/METADATA`, `SNAPSHOT_BEGIN`, `PUSH_CHUNK`, `PULL_CHUNK`, `SNAPSHOT_COMMIT`, and `ACK/ERROR`. Duplicate chunks for the same generation/offset are idempotent. Unexpected generation gaps return “snapshot required.”

### Canonical regions

Expose regions through `qmk_via_storage_contract`, not raw EEPROM accesses scattered in sync code:

- VIA validity/layout options that affect the profile;
- dynamic keymap bytes;
- dynamic encoder bytes when enabled;
- dynamic macro bytes;
- protocol metadata, managed separately from the snapshot digest.

Never copy handedness or unrelated QMK/user eeconfig. Region capacity, read, and write helpers must use overflow-safe checks from Finding 03.

## Implementation plan

### Step 1 — Complete command classification immediately

1. Replace command-ID-only effects with a validated full-command classifier so nested commands such as keyboard layout options can be identified.
2. Include `id_dynamic_keymap_macro_set_buffer`, `id_dynamic_keymap_macro_reset`, keymap/encoder mutations, dynamic reset, and EEPROM reset.
3. Update host `via.h` stubs and contract fixtures so omitted upstream command IDs fail tests instead of disappearing behind incomplete mocks.
4. Stop sending from `via_command_kb()`. It should mark pending effects/dirty state and return false so QMK remains the storage owner.
5. Preserve immediate local cache invalidation only where needed for correctness; generation completion waits for readback.

### Step 2 — Add post-apply commit coordination

1. Add a small pending-mutation state machine called from the existing scan hook.
2. For ordinary set operations, process on the next scan after QMK has returned.
3. For macro reset/EEPROM reset, have `apply_via_default_macros()` signal success/completion; do not commit or mirror the reset before final seeded bytes exist.
4. Coalesce multiple commands before a scan into one generation/snapshot-dirty set. Do not retain borrowed HID packet pointers.
5. Read back affected bytes and compute the canonical digest before publishing the new clean generation.

### Step 3 — Implement versioned metadata and migration

1. Prove ownership and atomic-update behavior of the 32-bit user eeconfig word. Define bit layout and modular generation comparison in one compatibility module.
2. Add clean, dirty, uninitialized, and unsupported-schema decode paths.
3. Define rollover before code lands: skip zero, use RFC-1982-style serial comparison within half-range, and force full resync near ambiguous distance.
4. Add an `eeconfig_init_user` reset path that initializes metadata after VIA defaults are seeded.
5. If a larger metadata block is required, document the one-time EEPROM migration and regenerate profile introspection because `users/noah/config.h` is an authored input.

### Step 4 — Build asynchronous snapshot push/pull

1. Register the versioned RPC and implement pure frame validation before storage effects.
2. Transfer at most one fragment per eligible scan tick. Share the failure/backoff gate designed in Finding 12 so a split outage cannot stall every scan.
3. Receiver stages generation/region progress, accepts exact duplicate fragments, rejects gaps/range errors, and writes only validated regions.
4. Commit the receiver's clean generation only after all expected region lengths and the complete snapshot digest match.
5. Acknowledge committed generation/digest. Sender clears peer-dirty state only on this application-level acknowledgment.
6. When peer metadata is newer, the current master pulls chunks through RPC responses, applies/validates locally, then becomes clean at that generation.

### Step 5 — Reconcile lifecycle and conflicts

1. Run `HELLO` after both split and VIA/default initialization; do not race default seeding.
2. Detect disconnect/reconnect and role changes. A fresh session always exchanges metadata even when no local command is pending.
3. Resolve states:
   - same clean generation and digest: synchronized;
   - different clean generations: higher serial generation supplies full snapshot;
   - same generation/different digest: current USB master wins, bumps generation, logs conflict;
   - either side dirty: clean side wins; if neither is clean, run explicit local VIA recovery/default seeding;
   - schema mismatch: compatible side requests reset/migration, never decodes unknown frames as commands.
4. Persist unresolved peer state so a reboot does not erase the need to reconcile.
5. After local apply, invalidate RGB layer maps and VIA macro caches once, then expose the generation as active.

### Step 6 — Remove legacy replay

1. Delete raw pre-apply command replay only after snapshot/delta parity tests cover all commands.
2. Keep all fork-specific VIA/storage assumptions under `users/noah/lib/compat/`.
3. If new source files are added, wire them into `users/noah/source_manifest.mk` and mirrored host compile gates in the same pass.
4. Preserve weak-hook chaining and runtime init order; extend enforcement tests for the new sync tick/init stage.

## Test and verification plan

### New protocol coverage

- Classifier table tests for every mutating VIA command and nested layout-option command.
- Post-QMK ordering: peer never observes a generation before master readback; macro reset waits for default seeding.
- Failed RPC retains dirty state; retries are backoff-bounded and eventual success clears it only after ack.
- Dropped, duplicated, corrupt, truncated, out-of-order, and stale fragments.
- Disconnect during every fragment boundary, then reconnect/resume or full resync.
- Master reboot, slave reboot, role swap, newer-slave pull, equal-generation digest conflict, dirty metadata recovery, generation wrap.
- Macro set/reset and EEPROM reset produce byte-identical regions and cache invalidation on both halves.
- Full snapshot is chunked without a long blocking scan.

### Targeted existing runners

```sh
sh tests/host/run_qmk_via_split_sync_tests.sh
sh tests/host/run_qmk_contract_checks.sh
sh tests/host/run_via_macro_defaults_tests.sh
sh tests/host/run_via_macro_action_lifecycle_tests.sh
sh tests/host/run_action_lifecycle_tests.sh
sh tests/host/run_macro_dispatch_tests.sh
sh tests/host/run_rgb_layer_render_tests.sh
sh tests/host/run_split_runtime_sync_tests.sh
sh tests/host/run_runtime_init_order_tests.sh
sh tests/host/run_hook_chaining_tests.sh
sh tests/host/run_feature_gate_compile_tests.sh
```

If `users/noah/config.h` changes for metadata storage:

```sh
python3 tools/profile_introspect.py --write
python3 tools/profile_introspect.py --check
```

### Closure gates

```sh
sh tests/host/run_all_host_tests.sh
qmk compile -kb bastardkb/charybdis/4x6 -km noah
```

Hardware closure also requires changing keymap and macro data while connected, disconnecting/reconnecting halves, power-cycling each half, and swapping USB master. Both halves must converge and render/play identical state.

## Measurements and observability

- Diagnostics: local generation/clean state, last acknowledged peer generation, peer digest, reconciliation state, retries, rejected frames, conflicts, and last error.
- Measure bytes/fragments and elapsed time for a full snapshot, per-scan work, EEPROM writes, and retry cadence.
- Count coalesced VIA commands and cache/RGB invalidations to prevent redundant work.
- Record flash, RAM, stack, and EEPROM metadata cost in the active review.

## Risks, tradeoffs, and fallbacks

- **EEPROM wear:** readback/coalescing avoids whole-image writes per command; use update semantics and only write changed chunks.
- **Metadata layout can shift VIA addresses:** prefer the existing user word; otherwise require an explicit migration/reset release note.
- **Two independently edited disconnected halves cannot be merged safely:** deterministic generation/digest conflict policy is required and must be documented.
- **Large snapshots can stall input:** one fragment per budgeted tick with backoff; never loop over the full image in one scan.
- **Transport success is weaker than durable apply:** require response generation/digest acknowledgment.
- **Protocol complexity:** keep codec/state machine pure and host-tested; centralize QMK storage access in compat helpers.
- **Power loss mid-write:** dirty metadata prevents partial state from winning. Losing an in-flight command is preferable to promoting corruption.

## Documentation and review-note updates

- Update the active open firmware architecture review with storage ownership, authority/conflict rules, metadata layout, retry state machine, and measured costs. If the relevant review is closed, create the next sortable review folder.
- Update `README.md` and relevant `docs/` pages for role-swap persistence, reconciliation behavior, diagnostics, and any EEPROM migration/reset requirement.
- Update keymap comments that currently describe seeding/reset workflow if the lifecycle changes.
- If config is an introspection input, regenerate and verify outputs in the same implementation pass.

## Acceptance checklist

- [ ] Every VIA-owned mutation is classified from a validated full packet.
- [ ] Macro set-buffer and the final result of macro reset/default seeding replicate.
- [ ] Sync publication occurs after QMK/local seeding, never from borrowed pre-apply command data.
- [ ] Metadata has tested clean/dirty/schema/wrap behavior and explicit migration.
- [ ] Every committed generation requires a peer generation/digest acknowledgment.
- [ ] Failed/disconnected delivery remains pending with bounded retry.
- [ ] Boot, reconnect, reboot, and role swap trigger version/digest reconciliation.
- [ ] Newer peer state can be pulled to the current master.
- [ ] Equal-generation divergence and dirty-state recovery follow documented deterministic rules.
- [ ] Snapshot fragments are bounds-checked, CRC-checked, idempotent, and commit atomically at generation level.
- [ ] RGB and macro caches invalidate after local commit on both halves.
- [ ] Targeted protocol, VIA, macro, RGB, init, and compile-gate runners pass.
- [ ] Profile introspection is regenerated/checked if authored config changes.
- [ ] `sh tests/host/run_all_host_tests.sh` passes.
- [ ] `qmk compile -kb bastardkb/charybdis/4x6 -km noah` passes.
- [ ] Physical disconnect/power-cycle/USB-role-swap verification passes.
- [ ] Architecture and user-facing documentation match the landed protocol.

## Next action

Write a command-completeness table and failing tests for macro set/reset plus failed RPC retention, then decide and document the persistent metadata bit layout before implementing wire frames.
