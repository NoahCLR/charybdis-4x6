# QMK compatibility

Every assumption about the BK commit `qmk-pin.json` names lives here, pinned by
`run_qmk_contract_checks.sh`. Nothing here owns key-runtime state: it adapts
QMK's records and queues so the runtime can decide. Where these sit on a key
press is [key-press.md](../../../../docs/architecture/key-press.md).

`run_compat_boundary_tests.sh` keeps it the only place: it fails, naming file
and symbol, when userspace or keymap C outside this folder names a fork-only
or internal QMK symbol compat adapts. A hook QMK calls by name that lives
with its owner (`process_record_admit_user()` in `hooks.c`, the tapping and
report hooks) is allowlisted there with its reason. Adapting a new fork hook
or QMK internal here adds its name to the check's `SYMBOLS`.

## Record admission

`qmk_record_admission.c` holds records back behind an undecided tap/hold key
(D-F03), at the fork's `process_record_admit_user()`: the top of
`process_record()`, after combos and native tapping, before every QMK feature,
so a held record is processed once, on replay.

- **The one decision it reads** is `key_runtime_core_undecided_dual_role_key_pos()`:
  a handled key, down in its `TAP_WINDOW`, whose hold changes other keys
  (`key_runtime_core_press_token_hold_changes_other_keys()`: every `LT`, `MT`
  and `OSM`, a layer held until release, or a first hold tier that is a layer
  action, modifiers alone or a pointing mode). A hold that went on at the
  press (`PRESS_HELD_WINDOW`) is not undecided.
- **What waits**: every press while a key is undecided or a record is held; a
  release whose press is held; a modifier's or layer key's release while a key
  is undecided, except the undecided key's own
  (`key_runtime_core_release_is_mods_or_layer()`: for a handled key, whether
  it holds a layer or real modifiers now; otherwise QMK's rule for its
  keycode). A combo output's release is judged at its combo's owner key
  (`noah_qmk_combo_origin_release_owner()`). Every combo output of one keycode
  is one key here: a held release closes the earliest held press still open.
- **Replay** (`noah_record_admission_task()`) runs after the key runtime scan,
  so a hold reached that scan applies first. It stops before a press, or a
  release that would wait, while a replayed key is undecided. Each replayed
  press first settles other keys' pending layer taps.
- **Overload**: at `NOAH_RECORD_ADMISSION_CAPACITY` (8) records, capture
  replays exactly the oldest, then appends. Nothing is dropped, no release
  overtakes its press, and a replayed record keeps its timestamp, keycode,
  event type and tap data. A replay guard prevents recapture.
- **Participation** is deferred on capture (`noah_participation_record_defer`)
  and settled when the record is replayed, on the layer it then resolves on.
- **Timing queries.** `noah_record_admission_press_pending()` keeps a key's
  tap series alive for a held press; `noah_record_admission_release_pending()`
  counts a held release as queued for the hold gate, except the undecided
  key's, whose press overload may have replayed ahead of its release.

`qmk_gesture_timing.h` is the runtime's only view of queued records: QMK's
combo and tapping queues (`combo_key_event_pending`, `tapping_key_event_pending`)
and admission's, matched by position, direction and unsigned 16-bit interval.
No queue is mirrored. Narrow host tests deliver synchronously at
`timer_read()`; only `run_qmk_gesture_pipeline_tests.sh` runs real QMK queues
and timestamps, and neither replaces hardware acceptance.

## The profile's QMK side

How these adapters sit on the profile's path is
[the profile flow](../../../../docs/architecture/profile-flow.md); the bytes
and states are the specs'.

- **`qmk_profile_split_transport.c`** is the one QMK adapter for both of the
  profile's split RPCs: the durable reconciler's and the working copy's. A
  request outside its protocol's length bounds is refused unsent, and every
  send is first counted for the durable-I/O grant, which repeats no storage
  step after one that used the split link (`noah_qmk_durable_io_repeat()`). On the slave half a
  callback hands the frame to that protocol's receive, which only copies it
  into its mailbox; a refused request is answered with zeros, never with the
  reply buffer's previous contents. Both RPCs are registered once, after a
  publication barrier, since the slave thread may run a callback as soon as
  it is registered. The mailboxes stay in their protocols: reconciler frames
  carry no sequence and a retry is matched on its whole request, while
  working-copy frames are matched by kind and sequence.
- **`qmk_profile_working.c`** keeps the boot session (from the RP2040's
  random bit and the timer) and the edit-mode record: two 16-byte CRC records
  in QMK's user datablock, the higher counter winning; a write goes to the
  other record, is read back, and waits for a quiet keyboard.
- **`qmk_keyboard_restart.c`** carries the restart (D-F34): it registers its
  RPC lazily from housekeeping, and of several requests the first one's time
  stands.
- **`qmk_usb_product_name.c`** builds the product string in two buffers and
  swaps a pointer to serve it, caching the name per settings publication
  (D-F25; [the keyboard name](../../../../docs/spec/profile-wire-v1/settings-domain.md)).

## Combo origin

`qmk_combo_origin.c` exists because QMK delivers a combo output as a
`COMBO_EVENT` at `(0,0)`, while feedback, pointing owners, lighting and split
sync need the combo's owner key and footprint. Its only handoff into the
runtime is the normalized event key and origin bitmap in
`key/runtime/slot/origin_registry.c`; it never creates or changes press
tokens, tap series, release decisions, leases, locks, modifier or pointing
ownership.

It mirrors only what repairing that origin needs: physical member state
observed in pre-process; the effective combo table (`noah_qmk_combo_get()`);
QMK's active and disabled bits, in both the normal and `EXTRA_SHORT_COMBOS`
layouts; combo keycode lookup through `COMBO_ONLY_FROM_LAYER` or
`combo_ref_from_layer`; and one completion candidate per combo index, keyed by
its physical completion generation. A combo completes again only after a
member release, which makes QMK fire or drop the earlier completion, so a
newer generation supersedes the older and a burst of combos cannot crowd one
out.

**Lifecycle**, reconciled at the scan boundary (`noah_qmk_combo_origin_scan`):
QMK marks a fired combo both active and disabled, so only a completion
disabled before it was ever seen active was suppressed, and it retires at
once. A completion not fired yet counts its deadline from the latest physical
press (QMK restarts its combo timer at each combo key press) and expires only
on the second scan past it, leaving `combo_task()` one last legal emission. A
fired completion whose press QMK has not delivered can only wait behind a tap
key QMK decides itself, so it retires once the tap / hold threshold (the
dual-role setting, D-F47) has passed since it was seen firing. A delivered
press has no deadline: admission can hold it for as long as the key ahead is
undecided, and always dispatches it. Owner recovery from
the latest or last physical member, with a full-keyboard bitmap, remains only
for a completion never seen firing.

### Combo origin pairing

The adapter relies on two facts of the pinned QMK: it activates a combo before
emitting its press, and it emits fired outputs in firing order, which is
completion order. Between QMK handing over an output record (delivery) and
the runtime processing it (dispatch), the record can wait in admission, even
after its combo was let go, and two combos with one output keycode can both
wait. So pairing follows the records, never only the combos still held:

- a delivered press pairs with the oldest completion QMK fired (active, or
  seen active before it was let go) whose press it has not delivered, and
  takes that completion's participation
  ([participation](../../../../docs/spec/participation-policy.md));
- a dispatched press pairs with the oldest completion whose press was
  delivered: both queues keep delivery order;
- a delivered release pairs with the completion holding the member whose
  physical release QMK is processing, dispatched or not, else the oldest, and
  is numbered in delivery order; a dispatched release pairs with the lowest
  number.

A completion seen firing keeps its exact footprint, owner and participation until
its press is dispatched, and its owner until its release is.

**Lighting.** `feedback.c` passes the keys holding a momentary layer
(`layer_ownership_momentary_owner_bitmap()`) to
`noah_qmk_combo_origin_active_bitmaps_partitioned()`; combos those keys own
light neither combo stage. This is a visual filter: their origins still match
releases ([the render](../../../../docs/architecture/rgb-render.md#stages-and-their-truth)).

Origin normalization is its own compiled call boundary, so its reconstruction
workspace retires before the process stages can project held actions; the
reviewed stack paths cover it.

Held by `run_qmk_combo_origin_tests.sh` (both combo layouts, the timerless
branch compile-checked), `run_qmk_contract_checks.sh` (combo layouts, the
pre-hook, matrix-scan and `combo_task()` order), `run_qmk_gesture_pipeline_tests.sh`
(a combo tapped behind an undecided `LT`), `run_participation_tests.sh` and
`run_pd_mode_key_runtime_integration_tests.sh` (same-output combos that wait
together).
