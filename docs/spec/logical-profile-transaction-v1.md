# Logical profile transaction V1

How an Apply or a Save makes a candidate the next generation on both halves:
the order of the durable steps across the halves, the commit, activation, what a
power loss or restart leaves, and how a candidate is cancelled. The frames a
client sends and the status it reads are
[candidate transfer](profile-wire-v1/candidate-transfer.md)'s; a Save's own
path to a candidate is [live edits](profile-wire-v1/live-edits.md#save)'s;
the copy between the halves is [the split protocol](profile-split-v1.md)'s;
the owner states and the safe-boundary predicate are
[the authority state table](authority-state-table.md)'s; the flash region,
profile slots, record layout and marker values are
[the device-resident profile](device-resident-profile.md#where-the-profile-is-stored)'s.

## One store, one generation

A save writes one thing: the complete profile, every domain the keymap and
macros included, as one generation in one profile slot on each half
([D-F40](../adr/0040-apply-publishes-one-atomic-generation.md),
[D-F31](../adr/0031-via-is-not-supported.md)). Nothing outside the profile is
part of a save: the edit mode, stored apart from every profile, and the
runtime toggles are neither written nor rolled back by one, and the settings
QMK holds are pushed into QMK from the profile, never saved from it
([live edits](profile-wire-v1/live-edits.md#toggles-and-the-settings-qmk-holds)).

Key output always runs one whole profile: the previous generation, the new
one, or the compiled defaults while the halves reconcile at boot, never a
mixture. Activation swaps the whole profile at a safe boundary and nothing is
rewritten under the profile that runs, so no input fence is needed.

Held by: `tests/host/run_profile_owner_tests.sh` (`profile_owner_stopped_test.inc`: `test_a_restart_anywhere_in_a_save_keeps_a_whole_profile`; `profile_owner_auto_save_test.inc`: `test_auto_save_power_cuts`), `tests/host/run_profile_flash_store_tests.sh` (`test_power_loss_at_every_flash_operation`). That the edit mode and the toggles stay out of a save: no test.

## Generations

Generation numbering is [the device-resident profile](device-resident-profile.md#generations)'s:
the next is the saved one plus one, and a half with none to offer begins no
candidate. On top of it:

- Every Apply, and every Save that writes, makes a new generation, also when
  its bytes equal the saved profile's. Only a Save compares the bytes first,
  and then writes nothing ([live edits](profile-wire-v1/live-edits.md#save)).
- A saved profile's origin is the physical half that made it, never the
  transport role ([split protocol](profile-split-v1.md#physical-origin)).
- Every candidate the master half begins carries the record's override flag.
  A saved record without it activates as the compiled defaults.

Held by: `tests/host/run_profile_store_tests.sh` (`profile_store_test.c`: `test_generation_conflict_and_rollover`), `tests/host/run_profile_candidate_store_backend_tests.sh` (`profile_candidate_store_backend_test.c`: `test_override_disabled_commit_activates_compiled_fallback`, `test_stage_validate_and_commit`), `tests/host/run_profile_owner_tests.sh` (`test_two_consecutive_live_commits_use_distributed_prepare_barrier`, `profile_owner_auto_save_test.inc`: `test_saving_the_saved_bytes_writes_nothing`).

## The record on one half

Each half writes its record in the order
[writing a slot](device-resident-profile.md#writing-a-slot) gives: the free
slot leaves boot selection first, the payload follows, then the header, the
read-back checks, the prepared marker and last the commit marker, each made
durable and read back. For the transaction:

- The free slot is the one not holding the saved profile, and a prepare is
  refused while the active profile still reads from it, so the profile that
  runs is never written under.
- A prepared record is durable intent, never authority: boot selection
  ignores it, and it may still be aborted.
- A half's commit is its own commit marker read back. Only then does the
  candidate leave committing, profile status show the new saved generation,
  or the slave half answer its `PREPARE_COMMIT`.
- A failed write, sync or read-back of the commit marker is
  `DURABILITY_UNKNOWN`: the owner stops, and only the boot scan after a
  restart says which profile the half holds
  ([live edits](profile-wire-v1/live-edits.md#a-stopped-keyboard)).

Held by: `tests/host/run_profile_store_tests.sh` (`test_durable_prepare_is_not_authority_until_decided`, `test_durable_prepare_can_be_aborted_before_decision`, `test_destructive_reuse_guard_brackets_every_prepare`, `test_final_marker_io_failure_is_durability_unknown`), `tests/host/run_profile_candidate_store_backend_tests.sh` (`test_runtime_rollback_pins_store_target_until_active_backing_moves`), `tests/host/run_profile_candidate_transaction_tests.sh` (`test_unknown_marker_durability_is_not_reported_as_safe_failure`), `tests/host/run_profile_owner_tests.sh` (`profile_owner_stopped_test.inc`: `test_an_unknown_commit_marker_stops_until_a_restart_reads_it`).

## Boot

Each half selects its record by
[boot selection](device-resident-profile.md#boot-selection) and validates it
whole. On a split keyboard each half then runs the compiled defaults until
the halves reconcile: it activates its saved profile only once both halves
hold exactly that profile, or both hold none, with nothing in transfer. A
newer saved profile on either half is copied to the other first. A keyboard
without a slave half activates its record without waiting for another half.

Held by: `tests/host/run_profile_owner_tests.sh` (`test_boot_reconciles_different_generations_before_activation`, `test_committed_reuse_saves_both_halves_and_reboots`), `tests/host/run_profile_store_runtime_owner_tests.sh` (`test_an_empty_flash_boots_the_compiled_defaults`), `tests/host/run_profile_split_foundation_tests.sh` (`test_authority_publication_and_activation_observer`).

## Commit sequence

On a split keyboard an Apply or a Save runs these steps; the candidate states
are [candidate transfer](profile-wire-v1/candidate-transfer.md#page-0-progress)'s.

1. The master half stages the candidate in its free profile slot without a
   marker ([writing a slot](device-resident-profile.md#writing-a-slot),
   steps 1–3) and validates it whole.
2. Commit enters preparing the other half (`8`). The master half copies the
   candidate to the slave half as a prepared push, reusing ranges the slave
   half holds in its own active profile. The slave half validates the
   complete copy and makes its prepared marker durable; the push then pauses
   ([split protocol](profile-split-v1.md#sending-a-prepared-push)).
3. The master half waits for a safe boundary on itself, every reason but the
   slave half's ([safe boundary](authority-state-table.md#safe-boundary)).
   Candidate status page 1 flag bit 2 reports the wait. Typing goes on, so
   whatever holds it can be released; if it does not clear, the candidate's
   no-progress lease ends it before anything is decided.
4. The decision: the master half writes and reads back its header, reads
   back its payload, then writes its prepared and commit markers (steps 4–7,
   committing `6`). Its durable commit marker is the commit. The previous
   generation keeps running on both halves through it.
5. The master half publishes its new saved profile to the reconciler, then
   authorizes the slave half's commit: the push sends `PREPARE_COMMIT`, and
   the slave half commits the same record, marker last (converging `9`).
6. Once the master half has seen the slave half hold exactly that record with
   nothing in transfer, it authorizes activation (activating `7`). Both halves
   activate at their next safe boundary; the slave half activates the record
   it committed at its own.

Nothing activates on a half before the other half has committed: the slave
half commits only after the master half's marker, and the master half
activates only after seeing the slave half's commit. That sighting is not
needed again: a link lost after it does not hold the master half's
activation. Ordinary reconciliation compares the halves again when the link
returns.

Both halves always prepare and validate the complete candidate; reusing
ranges skips sending bytes, never a durable step
([D-F18](../adr/0018-differential-upload-builds-complete-candidate.md),
[D-F19](../adr/0019-peer-copy-reuses-what-the-other-half-has.md)).

A keyboard without a slave half goes from validated straight to committing,
writes steps 4–7 and activates.

Held by: `tests/host/run_profile_owner_tests.sh` (`test_two_consecutive_live_commits_use_distributed_prepare_barrier`, `test_commit_waits_for_a_safe_boundary_then_activates`, `test_activation_waits_for_a_safe_boundary_while_typing`, `test_link_lost_after_the_peer_commit_was_seen_still_activates`, `test_committed_reuse_saves_both_halves_and_reboots`, `test_empty_boot_live_commit_and_timeout`), `tests/host/run_profile_candidate_transaction_tests.sh` (`test_split_authorization_barriers`, `test_commit_activation_and_idempotent_retry`), `tests/host/run_profile_split_reconciler_tests.sh` (`test_prepared_push_pauses_before_commit_then_authorizes`, `test_maximum_profile_prepares_and_commits_on_the_peer`), `tests/host/run_profile_candidate_store_backend_tests.sh` (`test_scan_owner_composes_bounded_commit_and_safe_activation`).

## Two candidates at once

A slave half's `PREPARE_BEGIN` is recorded as its provisional intent, never
as authority ([split protocol](profile-split-v1.md#authority)). While a
candidate prepares on this half, before any marker:

- When the other half's provisional candidate has the higher generation, or
  the same generation from the lower origin (the left half, `0`), this half
  yields: its candidate ends `PEER_PREPARE_YIELDED`.
- When the two cannot be ordered (the same generation from the same origin,
  or incompatible firmware), this half yields and its owner stops with
  `CONCURRENT_COMMIT`.
- When the other half's saved profile is compatible and its generation is at
  least the one the candidate would take, the candidate ends
  `PEER_SUPERSEDED`.

Once either marker is durable there is no tiebreak: two saved profiles of one
generation from different origins are a concurrent commit
([authority state table](authority-state-table.md#ordering-between-the-halves)).

Held by: `tests/host/run_profile_owner_tests.sh` (`test_simultaneous_hosts_choose_stable_physical_origin_before_durability`, `test_peer_authority_supersedes_only_precommit_host_generation`), `tests/host/run_profile_split_reconciler_tests.sh` (`test_crossed_prepare_begin_loser_abort_does_not_deadlock`).

## After the commit

From the decision on, nothing cancels the candidate and no lease expires it.
While the master half converges the slave half (`9`):

| The master half sees | Outcome |
| --- | --- |
| the slave half holding exactly the record, nothing in transfer | activation is authorized |
| a newer saved profile on the slave half | `POSTCOMMIT_AUTHORITY_LOST`, state `10`; the owner stops |
| a concurrent commit, the same tuple with different contents, or incompatible firmware | `PEER_COMMIT_CONFLICT`, state `10`; the owner stops with `CONCURRENT_COMMIT` |
| the copy stopped with `CONFLICT`, `CORRUPT` or `INCOMPATIBLE` | `PEER_COMMIT_CONFLICT` |
| the copy stopped with `STORAGE_ERROR`, `VALIDATION_ERROR` or `DIGEST_MISMATCH` | `POSTCOMMIT_AUTHORITY_LOST` |
| no slave half (link lost, slave half restarting) | it waits, without limit, typing the previous generation |

A candidate in state `10` is not activated and keeps its storage; only a
restart leaves it.

Held by: `tests/host/run_profile_owner_tests.sh` (`test_unexpected_newer_postcommit_authority_never_activates`, `test_lost_link_after_decision_keeps_typing_and_resumes`, `test_decided_commit_is_never_cancelled`, `profile_owner_stopped_test.inc`: `test_authority_changing_after_the_decision_stops_the_owner`), `tests/host/run_profile_candidate_transaction_tests.sh` (`test_postcommit_authority_failure_is_terminal_and_retains_candidate`, `test_explicit_precommit_expiry_never_crosses_durable_boundary`).

## Activation

A half activates a new generation by swapping its whole active profile at a
safe boundary ([authority state table](authority-state-table.md#safe-boundary)).
It never forces a release to get there: until the boundary comes, the
previous generation stays active and the pending one is reported (profile
status flag 6 while a pending generation waits, the reason mask on edit
status page 2, [live edits](profile-wire-v1/live-edits.md#edit-status-get-0x20)).
A generation that is durable but fails to activate reports
`ACTIVATION_FAILED` and the owner stops.

Held by: `tests/host/run_profile_owner_tests.sh` (`test_commit_waits_for_a_safe_boundary_then_activates`, `test_activation_waits_for_a_safe_boundary_while_typing`), `tests/host/run_profile_candidate_transaction_tests.sh` (`test_activation_failure_is_durable_and_requires_status_clear`), `tests/host/run_profile_candidate_store_backend_tests.sh` (`test_scan_owner_composes_bounded_commit_and_safe_activation`, `test_activation_request_retries_after_transient_provider_reuse`).

## Power loss and restarts

A restart is a reset between two main-loop passes and is exactly as safe as
a power loss ([live edits](profile-wire-v1/live-edits.md#why-a-restart-is-safe)).

| Last durable step | What each half boots | Outcome |
| --- | --- | --- |
| before the slave half's prepared marker | its previous saved profile | the previous generation |
| the slave half prepared, before the master half's commit marker | its previous saved profile | the previous generation; prepared work is discarded |
| the master half's commit marker | the master half the new record, the slave half its previous one | the new generation once the slave half commits it; the compiled defaults until the halves reconcile |
| both commit markers | the new record | the new generation; the compiled defaults until boot reconciliation |

- A slave half that loses the link or restarts after the decision boots its
  previous saved profile; its prepared copy is never booted. The master half
  sends the copy again from its own saved record when the link returns, then
  commits it, with no restart of its own. Unplugging the cable between the
  halves also powers the slave half off.
- A master half that restarts alone forgets its push. The slave half's
  prepared copy ends at the first poll after the master half has read its
  saved profile: released when that profile is not the copy, committed by
  repair when it is ([split protocol](profile-split-v1.md#an-abandoned-prepared-copy)).
- A lost acknowledgement is resolved from the markers and the records'
  identities, never by repeating a decision: a client reads candidate status
  and profile status ([candidate transfer](profile-wire-v1/candidate-transfer.md#retries-and-lost-replies)).

Held by: `tests/host/run_profile_owner_tests.sh` (`test_lost_link_after_decision_keeps_typing_and_resumes`, `test_peer_rebooted_while_unlinked_after_decision_resumes`, `test_committed_reuse_saves_both_halves_and_reboots`, `profile_owner_stopped_test.inc`: `test_a_restart_anywhere_in_a_save_keeps_a_whole_profile`, `test_stopped_owners_and_restarts`; `profile_owner_auto_save_test.inc`: `test_auto_save_power_cuts`), `tests/host/run_profile_flash_store_tests.sh` (`test_power_loss_at_every_flash_operation`), `tests/host/run_profile_store_tests.sh` (`test_durable_prepare_is_not_authority_until_decided`, `test_pd_header_power_loss_keeps_complete_generation`), `tests/host/run_profile_split_reconciler_tests.sh` (`test_a_restarted_sender_settles_the_prepared_copy`).

## Cancellation

Only the keyboard ends a candidate
([D-F48](../adr/0048-only-the-keyboard-ends-a-candidate.md)). Before the
decision every cause takes one path; after it nothing cancels, whatever the
client saw, and an abort answers `INVALID_STATE`.

| Cause | Candidate error |
| --- | --- |
| the client's abort | none |
| the lease runs out ([candidate transfer](profile-wire-v1/candidate-transfer.md#lease-and-cancellation)) | `TIMEOUT` |
| the slave half's saved profile supersedes the candidate | `PEER_SUPERSEDED` |
| the slave half's own candidate wins | `PEER_PREPARE_YIELDED` |
| the copy to the slave half stopped for good | `PEER_TRANSFER_FAILED` |
| the master half's own prepare failed in storage | `STORAGE_FAILURE`; the owner then stops |

The path:

1. A push not yet handed to the transport ends at once. Otherwise the master
   half sends the slave half `ABORT` for the copy.
2. The candidate is released, its prepare aborted and the master half's
   saved profile left as it was, once the slave half acknowledges the
   `ABORT`, or answers it with a storage error (it rejected the copy itself),
   or after 15 s (`NOAH_PROFILE_SPLIT_PREPARED_ABORT_TIMEOUT_MS`) without
   either ([D-F41](../adr/0041-cancelled-save-ends-in-bounded-time.md)).
3. Released unacknowledged, the `ABORT` becomes peer cleanup: neither half
   holds a marker for the copy, so durable authority is unchanged on both;
   profile status flag 8 stays set, the `ABORT` is retried about once a
   second, and no prepared push or transfer starts until the slave half
   acknowledges it or the keyboard restarts.

An `ABORT` the slave half refuses because it holds that copy committed is
not released: the reconciler stops with `CONFLICT`.

Held by: `tests/host/run_profile_owner_tests.sh` (`test_every_precommit_cancel_releases_the_candidate`, `test_decided_commit_is_never_cancelled`, `test_host_abort_is_coordinated_at_each_prepare_boundary`, `test_prepared_peer_is_aborted_before_host_timeout_releases_candidate`, `test_host_abort_ends_when_peer_never_acknowledges`, `test_local_known_commit_failure_aborts_prepared_peer_first`, `test_peer_store_write_failure_retries_then_fails_fast`), `tests/host/run_profile_candidate_transaction_tests.sh` (`test_abort_reset_and_no_timeout`, `test_explicit_precommit_expiry_never_crosses_durable_boundary`, `test_split_commit_failure_reports_and_cleans_known_precommit_failure`), `tests/host/run_profile_split_reconciler_tests.sh` (`test_prepared_push_cancel_aborts_peer_lease`, `test_prepared_abort_is_bounded_when_peer_goes_silent`, `test_prepared_abort_is_bounded_when_peer_stays_busy`, `test_cancel_after_dropped_prepare_begin_releases_or_noops`, `test_cancel_refuses_matching_durable_peer`, `test_receiver_recovers_when_its_abort_write_fails`).
