# Authority state table

Which profile each half holds authority over and runs, as states: the
identities the keyboard compares, how the halves order their saved profiles,
the live-profile owner's states, the candidate's phases on both halves, the
prepares that end without a commit, and the safe boundary every activation
waits for. The steps of a save are
[the logical profile transaction](logical-profile-transaction-v1.md)'s; the
comparison frames, the authority table itself and the prepared push are
[the split protocol](profile-split-v1.md)'s; what a client reads of them is
[the HID channel](profile-wire-v1/hid-channel.md#profile-status--get-0x02)'s
and [candidate transfer](profile-wire-v1/candidate-transfer.md)'s.

## Identities

| What | Identity | Authority |
| --- | --- | --- |
| compiled defaults | compiled-default digest and action-ABI digest | what runs when nothing is saved, and while the halves reconcile at boot |
| saved profile | generation, origin half, override flag, schema, domain mask, payload length, CRC-32, FNV-1a digest, compiled-default digest, action-ABI digest | what the half boots and runs |
| working copy | session and revision, over its saved profile's generation and origin | runs until saved or reverted; never stored ([live edits](profile-wire-v1/live-edits.md#session-and-revision)) |
| the slave half's saved profile | its descriptor, the same tuple | must converge before a save activates |
| the slave half's provisional intent | the descriptor of its `PREPARE_BEGIN` | none: used only to order two candidates |

Only a commit makes a generation. Edits, a Revert, a working copy and a
candidate that never reached its commit marker make none. A saved profile
from other compiled defaults or another action ABI is never adopted
([device-resident profile](device-resident-profile.md#where-the-profile-is-stored)).

Held by: `tests/host/run_profile_store_tests.sh` (`test_commit_and_boot_selection`, `test_boot_rejects_old_compiled_default_identity`, `test_pd_header_all_identity_bits_and_boot_paths`), `tests/host/run_profile_split_foundation_tests.sh` (`test_authority_decision_table`), `tests/host/run_profile_split_reconciler_tests.sh` (`test_incompatible_firmware_stops_without_write`).

## Ordering between the halves

Each half compares its saved profile with the other half's by
[the split protocol's authority table](profile-split-v1.md#authority):

- The higher generation wins after a reconnect, when both are readable and
  compatible; the newer profile is copied to the other half. The transport
  role never decides.
- Equal generation, origin and contents: converged.
- Equal generation and origin, different contents: corrupt. Equal generation
  from different origins: a concurrent commit. Neither is resolved by the
  keyboard: the reconciler stops, neither half's record is overwritten, and
  profile status page 1 reports conflict count `1`.
- Incompatible firmware, an unreadable half or invalid metadata fail closed:
  nothing is copied.

Before any marker, two candidates are ordered by their provisional intent
([two candidates at once](logical-profile-transaction-v1.md#two-candidates-at-once)).

Held by: `tests/host/run_profile_split_foundation_tests.sh` (`test_authority_decision_table`, `test_authority_publication_and_activation_observer`), `tests/host/run_profile_split_reconciler_tests.sh` (`test_newer_master_pushes_exact_record`, `test_newer_slave_is_pulled_without_role_authority`, `test_concurrent_commit_stops_without_overwrite`, `test_same_tuple_corruption_stops_without_overwrite`, `test_max_generation_transfers_without_local_increment`), `tests/host/run_profile_owner_tests.sh` (`test_boot_reconciles_different_generations_before_activation`).

## Owner states

Each half's live-profile owner is in one of these states. Candidate frames,
live edits and Save are taken only in the two ready states.

| State | The half | Runs |
| --- | --- | --- |
| `UNINITIALIZED` | has no owner yet | the compiled defaults |
| `VALIDATING_COMPILED` | validates its compiled defaults in bounded steps, then installs its runtimes | the compiled defaults |
| `DISCOVERING` | selects a record from its profile slots ([boot selection](device-resident-profile.md#boot-selection)) | the compiled defaults |
| `ADOPTING_COMMITTED` | validates the selected record whole | the compiled defaults |
| `RECONCILING_COMMITTED` | waits until both halves hold the same saved profile, copying a newer one first | the compiled defaults |
| `ACTIVATING_COMMITTED` | activates its saved profile at a safe boundary | the compiled defaults until then |
| `READY_COMPILED` | holds no saved profile it runs (none saved, or a record without the override flag) | the compiled defaults, or a working copy over them |
| `READY_VALIDATED` | runs its saved generation | that generation, or a working copy over it |
| `GENERATION_CONFLICT`, `DURABILITY_UNKNOWN`, `COMPILED_ERROR`, `STORAGE_ERROR`, `INTEGRATION_ERROR`, `POSTCOMMIT_AUTHORITY_LOST`, `CONCURRENT_COMMIT` | stopped: only a restart leaves ([live edits](profile-wire-v1/live-edits.md#a-stopped-keyboard)) | whatever it ran when it stopped |

A keyboard without a slave half never enters `RECONCILING_COMMITTED`. Boot
selection finding nothing goes straight to `READY_COMPILED`.

Held by: `tests/host/run_profile_owner_tests.sh` (`profile_owner_stopped_test.inc`: `test_only_states_a_restart_leaves_are_stopped`; `test_empty_boot_live_commit_and_timeout`, `test_boot_reconciles_different_generations_before_activation`), `tests/host/run_profile_store_runtime_owner_tests.sh` (`test_an_empty_flash_boots_the_compiled_defaults`), `tests/host/run_profile_candidate_store_backend_tests.sh` (`test_reboot_adopts_committed_profile_through_bounded_validator`, `test_override_disabled_commit_activates_compiled_fallback`).

## Candidate phases on both halves

The candidate states are candidate status page 0's
([candidate transfer](profile-wire-v1/candidate-transfer.md#page-0-progress)).

| Candidate state | Master half | Slave half | Saved profile on both | Activation |
| --- | --- | --- | --- | --- |
| `4` validated | the complete candidate staged, validated, no marker | unchanged | the previous one | the previous generation runs |
| `8` preparing the other half | holds the candidate and its storage, no marker; once the slave half is prepared, waits for a safe boundary on itself | receives the exact copy, validates it, makes its prepared marker durable; the push then pauses | the previous one | forbidden |
| `6` committing | reads its record back and writes its markers, commit marker last | prepared, waiting | the previous one until the master half's commit marker reads back | forbidden |
| `9` converging the other half | the new record saved and published | its commit authorized; commits marker last | the master half newer, the copy pending | forbidden |
| `7` activating | both halves seen holding exactly the new record | committed; activates at its own safe boundary | the new one, converged | at the next safe boundary |
| `10` authority failed | its commit marker is durable but the slave half answered newer, conflicting, corrupt, incompatible or failed for good | not overwritten | as each half holds it | forbidden; the candidate keeps its storage and the owner stops |

A keyboard without a slave half goes from `4` to `6` to `7`.

Held by: `tests/host/run_profile_candidate_transaction_tests.sh` (`test_split_authorization_barriers`, `test_postcommit_authority_failure_is_terminal_and_retains_candidate`, `test_commit_activation_and_idempotent_retry`), `tests/host/run_profile_owner_tests.sh` (`test_two_consecutive_live_commits_use_distributed_prepare_barrier`, `test_commit_waits_for_a_safe_boundary_then_activates`, `test_unexpected_newer_postcommit_authority_never_activates`, `test_lost_link_after_decision_keeps_typing_and_resumes`), `tests/host/run_profile_split_reconciler_tests.sh` (`test_prepared_push_pauses_before_commit_then_authorizes`).

## Prepares that end without a commit

### Cancelled

A cancel before the master half's commit marker aborts the slave half's copy
first and releases the candidate after, bounded at 15 s
(`NOAH_PROFILE_SPLIT_PREPARED_ABORT_TIMEOUT_MS`) when the slave half stays
silent or answers `BUSY`
([cancellation](logical-profile-transaction-v1.md#cancellation),
[D-F41](../adr/0041-cancelled-save-ends-in-bounded-time.md)). Released
unacknowledged, it leaves durable authority unchanged on both halves, since
neither holds a marker for the copy, and sets *peer cleanup pending*.

### Interrupted

Only completion (both halves hold the exact copy), a cancel or a stop ends a
prepared push. A reply the master half cannot use (a reply to an earlier
request, a stale answer) returns it to its metadata poll with the push still
active; that poll, finding both halves on the previous profile, resumes the
copy with `PREPARE_BEGIN` at the slave half's offset and never reads the
halves as converged ([split protocol](profile-split-v1.md#sending-a-prepared-push)).

### Abandoned

A slave half's prepared copy waits for its master half's decision however
long that takes. A master half that restarts forgets its push; the slave half
then releases the copy, or commits it by repair, at the first poll the
restarted half sends ([split protocol](profile-split-v1.md#an-abandoned-prepared-copy)).
A copy committing or of unknown durability is never released.

Held by: `tests/host/run_profile_split_reconciler_tests.sh` (`test_prepared_abort_is_bounded_when_peer_goes_silent`, `test_prepared_abort_is_bounded_when_peer_stays_busy`, `test_prepared_push_resumes_from_the_metadata_poll`, `test_prepared_push_survives_one_replayed_reply_anywhere`, `test_a_restarted_sender_settles_the_prepared_copy`, `test_a_paused_sender_keeps_the_prepared_copy`, `test_a_committing_copy_is_never_released`), `tests/host/run_profile_owner_tests.sh` (`test_host_abort_ends_when_peer_never_acknowledges`).

## Profile status flags 5 and 8

| Flag | Set while |
| ---: | --- |
| 5 peer converged | this half's split authority holds both halves on the same saved profile, or both on the compiled defaults, with nothing in transfer |
| 8 peer cleanup pending | an `ABORT` released unacknowledged is still owed to the slave half; no prepared push or transfer starts, so a new save's commit waits in state `8` until its lease ends it, until the slave half acknowledges or the keyboard restarts |

Held by: `tests/host/run_profile_owner_tests.sh` (`test_host_abort_ends_when_peer_never_acknowledges`), `tests/host/run_profile_split_reconciler_tests.sh` (`test_prepared_abort_is_bounded_when_peer_goes_silent`). Flag 5's derivation from the split authority: no test.

## Safe boundary

A half activates a new generation or working copy, and the master half
takes its commit decision, only at a safe boundary: a moment when the
predicate below reports no reason. The predicate never forces a release to
get there. Until it clears, the previous generation stays active and the
pending one stays observable. Edit status page 2 reports the reason mask
([live edits](profile-wire-v1/live-edits.md#edit-status-get-0x20)).

| Bit | Reason | Evidence |
| ---: | --- | --- |
| 0 | physical press | the key runtime's press tokens |
| 1 | tap series | the key runtime's open tap series |
| 2 | runtime lease | the key runtime's leases, held and repeating actions and momentary ownership included |
| 3 | deferred release | the key runtime's pending releases |
| 4 | persistent intent | the key runtime's layer, pointing-mode and pointer locks, and a pointing mode active, locked or still releasing a consumed button |
| 5 | owned output | managed HID usages held, mouse buttons included |
| 6 | modifier or one-shot | real, weak, one-shot or locked one-shot modifiers |
| 7 | one-shot layer | QMK's one-shot layer |
| 8 | macro | the macro engine not idle, or a macro-owned hold |
| 9 | combo | a pending or active combo |
| 10 | slave half | the slave half not converged on this half's saved profile with nothing in transfer, or no answer |
| 30 | internal | the predicate itself not initialized |

- A missing or failing slave-half observation counts as unresolved: bit 10
  set.
- While an Apply or Save is activating a record the master half has already
  seen the slave half hold, bit 10 is clear without a new observation.
- The commit decision waits on every bit but 10; activation waits on all.
- Each evaluation is published as one snapshot, so a status read never mixes
  counts from two evaluations.

Held by: `tests/host/run_profile_activation_policy_tests.sh` (`profile_activation_policy_test.c`: `test_reason_bits_are_exact_and_composable`, `test_runtime_capture_and_status`, `test_peer_observation_fails_closed`, `test_status_fails_closed_during_publication`, `test_configured_pd_blocks_until_release_and_does_not_wrap`), `tests/host/run_profile_owner_tests.sh` (`test_commit_waits_for_a_safe_boundary_then_activates`, `test_activation_waits_for_a_safe_boundary_while_typing`, `test_link_lost_after_the_peer_commit_was_seen_still_activates`), `tests/host/run_profile_split_foundation_tests.sh` (`test_authority_publication_and_activation_observer`).
