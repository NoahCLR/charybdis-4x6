# `users/noah/lib/profile/`

The device-resident profile: its formats, its storage, how the two halves
compare and copy it, and the owner that runs it. How these modules cooperate
with the QMK adapters in `../compat/` is
[the profile flow](../../../../docs/architecture/profile-flow.md); every byte
and state is a spec's under [`docs/spec/`](../../../../docs/spec/README.md).

| Directory | Holds |
| --- | --- |
| `protocol/` | Profile Wire codecs: the read surface (`profile_wire_v1.c`) and candidate frames (`profile_candidate_v1.c`) |
| `schema/` | The blob and its envelope walker, the domain registry, each domain's codec and compiled writer, the whole-profile validator, the compiled defaults |
| `storage/` | The dual-slot store and its flash backend and layout, checksums, the candidate transaction and its store backend, the backend for a copy from the other half, and `profile_store_runtime.c`, the static home of the one owner |
| `split/` | The split authority, the profile split framing, the reconciler, the working copy's protocol, and the one exchange seam both send through |
| `runtime/` | The owner, the provider of the active profile, the activation policy, the working copy, and the `effective_*_runtime.c` views other packages read |

## Conventions

- **Bounded steps.** Every state machine here advances by one bounded step
  from `noah_profile_owner_scan`, never from a callback. Budgets live beside
  the machine: `NOAH_PROFILE_CANDIDATE_SCAN_BYTE_BUDGET`,
  `NOAH_PROFILE_WORKING_LOAD_BYTES`, `NOAH_PROFILE_WORKING_VALIDATION_STEPS`,
  `NOAH_PROFILE_SPLIT_PEER_SCAN_BYTES`.
- **Receive is a copy.** Each `*_receive` (candidate transaction, reconciler,
  working-copy protocol) decodes exactly one frame into a one-frame mailbox
  and answers from a published reply; the scan does the work.
- **The owner's scheduler.** `scan_running` rotates four subsystems (the
  candidate, the reconciler, activation of a received copy, the working
  copy), one step each. When the durable-I/O grant repeats a step,
  `noah_profile_owner_scan_again` sends it back to the subsystem whose step
  last worked, so the same code stays hot.
- **One admission table.** `owner_admit` in `profile_owner.c` decides every
  edit, Save, Revert, mode change, Apply frame and keyboard step, in answer
  order; Profile Wire's [admission](../../../../docs/spec/profile-wire-v1/live-edits.md#admission)
  mirrors it row for row.
- **One identity.** Every comparison between a store record, a
  candidate and a split descriptor lives in `storage/profile_record_identity.h`.
- **No mutable owner outside.** The channel reads copied snapshots
  (`noah_profile_owner_status`, `noah_profile_owner_working_status`) through
  `noah_profile_store_runtime_*`; only the reconciler and working-copy
  endpoints leave the owner, for split RPC registration.
- **Seams for two halves in one test.** The owner takes its flash
  (`noah_profile_store_io_t`), its split link (`noah_profile_split_exchange_fn`),
  its edit-mode storage, and its quiet and pause checks from its config. Host
  tests run two owners over `tests/host/split_fault_link.c`.
- **RAM.** Each long-lived state struct has a budget checked by a
  `_Static_assert` (`*_STATE_BUDGET_32BIT`; the validator's is
  `NOAH_PROFILE_VALIDATOR_V1_EMBEDDED_STATE_BUDGET`): a regression policy, not physical memory
  ([resource policy](../../../../docs/spec/resource-policy.md)). The compiled
  validator and the candidate backend share one union
  (`noah_profile_owner_staging_t`), since the first is done before the second
  exists.
- **Stack.** Every scan-path step shares one frame, so boot-only and rarely
  taken functions are `__attribute__((noinline))` and the flags every step
  reads sit first in `noah_profile_owner_t`. The reviewed paths are
  `tools/firmware_stack_budget_live_profile_owner.json`; a new step on them
  is measured there.
