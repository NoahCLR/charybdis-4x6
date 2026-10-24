# How the profile moves through the firmware

Which modules a client's Apply, Save or live edit passes through, how the two
halves end converged, and what boot and a restart do. The bytes, states and
timings are the specs'; this file names the module path and points at them.
Module internals under `users/noah/lib/profile/` are in
[its README](../../users/noah/lib/profile/README.md).

## The build gate

`users/noah/rules.mk` builds the live-profile owner into every image with a
provisioned half (`NOAH_PHYSICAL_HALF`, set by `tools/build-firmware-pair.sh`),
since a saved profile's origin is side-specific (D-F35). The write path
(`NOAH_LIVE_PROFILE_MUTATION`: candidate routing, saves, the USB product
string) follows the owner. A half-less `qmk compile` omits both, with a message only for the owner.
`NOAH_LIVE_PROFILE_OWNER=no` with `NOAH_LIVE_PROFILE_MUTATION=no`
(`tools/build-firmware-pair.sh --no-owner`) builds an owner-free image from
the same source, for comparing behaviour: `profile_store_runtime.c` then only
runs boot selection over a read-only view of the slots and the compiled
defaults run ([device-resident profile](../spec/device-resident-profile.md#authority)).

## Who owns which fact

| Fact | Owner |
| --- | --- |
| The one owner, its working-copy buffer and the durable-I/O grant | statics of `profile/storage/profile_store_runtime.c` |
| Owner state, admission of every client operation, Save, keyboard adjustments, the candidate's lease | `profile/runtime/profile_owner.c` |
| A half's saved generations in its two profile slots, boot selection | `profile/storage/profile_store.c` over `profile_flash_store.c` and `compat/qmk_profile_flash.c` |
| A client's candidate and its phases | `profile/storage/profile_candidate_transaction.c` |
| Writing a candidate into the slot not in use, asking for its activation | `profile/storage/profile_candidate_store_backend.c` |
| A copy received from the other half: the master half's push, or a newer saved profile the master half pulls from the slave half | `profile/storage/profile_other_half_store_backend.c` |
| Which half's saved profile is newer, and whether the halves are converged | `profile/split/profile_split_authority.c`, fed by `profile_split_reconciler.c` |
| The active profile and its publication | `profile/runtime/active_profile_provider.c` |
| Whether now is a safe boundary | `profile/runtime/profile_activation_policy.c` |
| The working copy, its session and revision | `profile/runtime/profile_working_copy.c` |
| The slave half's working copy and the edit mode following the master half's | `profile/split/profile_working_sync.c` |
| The edit-mode record and the boot session | `compat/qmk_profile_working.c` |
| The name USB enumerated with | `compat/qmk_usb_product_name.c` |
| A requested restart | `compat/qmk_keyboard_restart.c` |
| This half's physical origin | `compat/qmk_physical_half.c` |

Runtimes read the active profile only through the views the provider's
invalidators rebuild: `active_profile_keymap_macros_runtime.c` (read by
`compat/qmk_profile_keymap.c` and `macro/profile_macro_provider.c`),
`active_profile_key_behavior_runtime.c` (`key/behavior/`),
`active_profile_rgb_runtime.c` (`rgb/core/rgb_active_profile_config.c`),
`active_profile_combo_runtime.c` (`compat/qmk_active_profile_combos.c`),
`active_profile_settings_runtime.c` (`noah_setting()` everywhere; QMK's own
settings through `compat/qmk_portable_profile.c`'s apply hook) and
`active_profile_pd_runtime.c` (`pointing/`). `initialize_runtime_graph` in
`profile_owner.c` registers them in that order.

## Which context runs what

| Context | Runs |
| --- | --- |
| Matrix scan, both halves (`noah_matrix_scan_user`, `noah_matrix_slave_scan_user` in `runtime_init.c`) | the durable-I/O grant (`compat/qmk_durable_io.c`) and through it `noah_profile_owner_scan`: every validation step, flash erase and program, split send, publication and activation |
| Raw HID, master half, main loop after the scan (`raw_hid_receive` in `compat/qmk_profile_channel.c`) | bounded admission: decode, `owner_admit`, one copy into the candidate mailbox or the working copy's pending edit, a status snapshot; the restart request's wait for the slave half |
| Key processing, master half | `noah_qmk_profile_adjustment_process` queues a keyboard adjustment (`noah_profile_owner_adjust`) |
| Housekeeping, both halves (`noah_housekeeping_task_user`) | the USB rename, flash-pause figures, restart registration and the restart itself, last |
| Split slave thread, slave half (QMK RPC callbacks) | copy one request into its protocol's one-frame mailbox and answer from the last published reply; read the grant's volatile pause flag |

This is the mailbox pattern: a callback, Raw HID or split, never touches
flash or validates; the scan takes the mailbox, does one bounded step and
publishes the next reply. A step that sends a split RPC ends its grant, so a
scan makes at most one exchange (`noah_qmk_durable_io_repeat` in
`compat/qmk_durable_io.c`; [split RPCs](../spec/profile-split-v1.md#split-rpcs)). The owner's clock is
the scan clock less every grant its step was not allowed to run, so a save
waiting for a quiet keyboard expires no lease.

## Boot

1. `keyboard_pre_init`: `noah_qmk_profile_flash_pre_init` forgets the saved
   profiles after an EEPROM reset, then `noah_qmk_usb_product_name_pre_init`
   reads the boot-selected record's keyboard name by borrowing the owner's
   not yet started store, before USB starts.
2. `keyboard_post_init`: `noah_profile_store_runtime_init` binds the grant to
   the slot flash and starts the owner validating its compiled defaults.
3. Each scan steps the owner through
   [its boot states](../spec/authority-state-table.md#owner-states). Once the
   compiled defaults validate, `initialize_runtime_graph` installs the
   runtimes on them and builds store, transaction, backends, working copy and
   reconciler. Right after that step `register_transports` registers
   `PUT_PROFILE_SPLIT_SYNC` and `PUT_PROFILE_WORKING_SYNC` together, once; a
   refused registration is an integration error, since no later scan heals
   it.
4. The halves reconcile before either activates its saved profile
   ([boot](../spec/logical-profile-transaction-v1.md#boot),
   [authority](../spec/profile-split-v1.md#authority)). The first
   housekeeping pass registers `PUT_KEYBOARD_RESTART`, apart from the owner.

## Apply

Candidate frames reach `noah_profile_owner_receive`, which refuses what
admission's APPLY rows refuse and hands the rest to the transaction's
mailbox. The owner's scheduler (`scan_running`) rotates four subsystems, one
step each: the candidate, the reconciler, activation of a copy received
from the other half, and the working copy. The candidate validates in the
slot not in use; at `PREPARING_PEER`, `begin_or_advance_host_barrier` has the
reconciler push it, reusing what the slave half already holds
(`host_reuse_plan`, D-F19); the slave half's
`profile_other_half_store_backend.c` stores and validates it as a prepared copy;
the master half's commit marker commits the Apply, then
`advance_host_postcommit_barrier` waits for the slave half to commit its copy
(D-F40). Contracts: [candidate
transfer](../spec/profile-wire-v1/candidate-transfer.md), [commit
sequence](../spec/logical-profile-transaction-v1.md#commit-sequence), [sending a
prepared push](../spec/profile-split-v1.md#sending-a-prepared-push), [receiving
a copy](../spec/profile-split-v1.md#receiving-a-copy).

## Activation

`profile_candidate_store_backend.c` asks the provider for the new generation
and polls it. The provider evaluates `noah_profile_activation_policy_safe_boundary`
once per poll; the slave-half reason comes from the owner's observer
(`owner_other_half_observer`), which reads the split authority. With no reason it
switches its active bank and runs the invalidators in order, each passed the
changed-domain mask. A copy from the other half (pushed to the slave half,
or pulled by the master half, [repair and pull](../spec/profile-split-v1.md#repair-and-pull))
lands in `profile_other_half_store_backend.c`: once a reconciler step leaves it
awaiting activation (`took_other_half_copy`), the owner publishes its descriptor
(`publish_validated_descriptor`) and `scan_other_half_activation` activates it
the same way. Contracts:
[activation](../spec/logical-profile-transaction-v1.md#activation),
[safe boundary](../spec/authority-state-table.md#safe-boundary).

## Live edits and Save (D-F32)

SET `0x20`–`0x27` reach `compat/qmk_profile_edit_channel.c`, then
`noah_profile_store_runtime_edit_*` and the owner, which admits each report
synchronously. The owner's working-copy subsystem (`scan_working`) runs, in
order: a Save step, an edit's lease, `noah_profile_working_copy_scan` (load,
check, publish), the edit-mode write, the keyboard's automatic step, then the
working-copy protocol. Publication is `noah_active_profile_provider_publish_in_place`:
the bytes are rewritten inside it at the safe boundary and every invalidator
runs; the combo, pointing and settings views skip a working copy that left
their domain unchanged, while the keymap and macro, key-behaviour and
lighting views rebuild on every publication. The master half's `noah_profile_working_sync_master_scan`
then sends the same edit; the slave half's callback fills the mailbox and its
`noah_profile_working_sync_slave_scan` runs the same steps. Save queues its
commands into the candidate transaction's mailbox as a client would, with a
candidate made of REUSE from the working copy, so it takes the Apply path
above; a working copy whose bytes equal the saved profile writes nothing
(`NOAH_PROFILE_OWNER_SAVE_SAME`). Contracts: [live edits](../spec/profile-wire-v1/live-edits.md),
[working copy sync](../spec/profile-split-v1.md#working-copy-sync).

## The keyboard's name on USB (D-F25)

`noah_qmk_usb_product_name_task` (housekeeping, master half) watches the
settings publication count. When the active profile's name differs from the
one enumerated and `noah_profile_owner_usb_restart_safe` holds, it switches to
the new descriptor and restarts the USB driver. Rules:
[keyboard name](../spec/profile-wire-v1/settings-domain.md#keyboard-name).

## Restart (D-F34)

A stopped owner (`noah_profile_owner_stopped`) refuses everything but reads;
on the slave half `answer_stopped` keeps answering status polls. SET `0x28`
goes from the edit channel to `noah_qmk_keyboard_restart_request`, which asks
the slave half on `PUT_KEYBOARD_RESTART` from the Raw HID handler; each
half's `noah_qmk_keyboard_restart_task` then calls `soft_reset_keyboard`
between main-loop passes. Nothing on the path reads the owner. Contracts:
[a stopped keyboard](../spec/profile-wire-v1/live-edits.md#a-stopped-keyboard),
[restart](../spec/profile-wire-v1/live-edits.md#restart),
[keyboard restart](../spec/profile-split-v1.md#keyboard-restart).

## Changing the path

- A new profile request/reply split RPC: its framing, mailbox and replay in
  its protocol module under `profile/split/`, its id after the existing ones
  in `SPLIT_TRANSACTION_IDS_USER` (`users/noah/config.h`), and its send and
  registration in `compat/qmk_profile_split_transport.c`, the one QMK
  adapter for the profile's two split RPCs.
- A new Raw HID request: a handler in `compat/qmk_profile_channel.c`'s
  dispatch (or `qmk_profile_edit_channel.c` for live edits), bounded, its
  work queued for the scan; capabilities in `compat/qmk_profile_capabilities.h`.
- A new runtime reading the profile: an `active_profile_*_runtime.c` view with
  an invalidator added in `initialize_runtime_graph`, under
  `NOAH_ACTIVE_PROFILE_MAX_INVALIDATORS`.
- Every new step on the scan path runs on the reviewed stack paths
  (`tools/firmware_stack_budget_live_profile_owner.json`,
  [resource policy](../spec/resource-policy.md)).

## Tests that hold it

- Owner, boot and Apply on two halves: `tests/host/run_profile_owner_tests.sh`,
  `run_profile_store_runtime_owner_tests.sh`, `run_profile_store_runtime_tests.sh`
- Grant and flash pacing: `run_qmk_durable_io_tests.sh`, `run_qmk_profile_flash_tests.sh`
- Candidate and store: `run_profile_candidate_transaction_tests.sh`,
  `run_profile_candidate_store_backend_tests.sh`, `run_profile_other_half_store_backend_tests.sh`,
  `run_profile_store_tests.sh`
- Split protocol over `tests/host/split_fault_link.c`: `run_profile_split_foundation_tests.sh`,
  `run_profile_split_reconciler_tests.sh`, `run_profile_working_sync_tests.sh`;
  the QMK adapter: `run_qmk_profile_split_transport_tests.sh`
- Activation and views: `run_active_profile_provider_tests.sh`,
  `run_profile_activation_policy_tests.sh`, `run_profile_keymap_macros_runtime_tests.sh`,
  `run_active_profile_rgb_runtime_tests.sh`,
  `run_active_profile_combo_runtime_tests.sh`
- Live edits: `run_profile_working_copy_tests.sh`, `run_qmk_profile_edit_channel_tests.sh`,
  `run_qmk_profile_adjustments_tests.sh`, `run_qmk_portable_profile_tests.sh`
- Raw HID channel: `run_profile_wire_v1_tests.sh`
- USB name: `run_qmk_usb_product_name_tests.sh`, `run_qmk_usb_product_name_owner_tests.sh`
- Restart: `run_qmk_keyboard_restart_tests.sh`
- Build gate and wiring: `run_feature_gate_compile_tests.sh`; stack paths:
  `run_live_profile_owner_stack_budget_checks.sh` (target build)
