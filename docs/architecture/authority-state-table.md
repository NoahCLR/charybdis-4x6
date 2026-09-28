# Source, Device, And Split Authority

## Identities

| State | Identity | Authority |
| --- | --- | --- |
| Ark draft | device base generation/digest | none until an explicit device apply |
| Source files | canonical digest plus file revision | compiled-default representation |
| Compiled defaults | canonical digest and action-ABI digest | recovery fallback |
| Device commit | `{counter, origin_half}` plus canonical digest | deployed runtime authority |
| RGB preview | transaction id, base generation, digest | volatile overlay only |
| Peer half | committed version tuple and digest | must converge before milestone success |

Only a successful durable device commit advances the device counter. Source and
draft changes never manufacture device generations.

## Version Ordering

- Higher generation counter wins after reconnect when both records are valid.
- Equal counter, equal origin, equal digest means converged.
- Equal counter and origin but different digest is corruption; stop and expose
  conflict.
- Equal counter with different origins is a disconnected concurrent commit;
  stop and require explicit user resolution.
- Reset commits an override-disabled record with a new generation.
- Applying an unchanged canonical payload is a no-op.

The origin half is a stable physical-half id, not current USB role.

## Distributed Commit Phases

| Phase | USB/local half | Peer half | Durable authority | Activation |
| --- | --- | --- | --- | --- |
| validated | inactive candidate validated | unchanged | prior committed descriptor | prior generation stays active |
| `PREPARING_PEER` | HOST lease retained; no marker | exact bytes staged under PEER lease; no marker | unchanged | forbidden |
| peer prepared | pauses in `PUSH_PREPARED` | complete inactive candidate | unchanged | forbidden |
| local committing | marker-last advances locally | prepared and fenced | prior descriptor until local marker succeeds | forbidden |
| `CONVERGING_PEER` | new descriptor durable and published | marker-last commit authorized | local newer, transfer pending | forbidden |
| exact converged | exact new descriptor | exact new descriptor | `COMMITTED_CONVERGED`, no transfer | eligible for normal safe-boundary activation |
| authority failed | local marker is durable but fresh peer evidence is newer, conflicting, corrupt, incompatible, or terminally failed | do not overwrite | explicit recovery state | forbidden; HOST lease/backing retained |

`PREPARE_BEGIN` intent is provisional and never replaces durable peer metadata.
If both halves hold provisional host candidates, higher generation wins before
either marker. Equal generation is ordered by the lower stable physical-origin
id; the loser aborts peer staging first and then reports
`PEER_PREPARE_YIELDED`. Once either marker is durable, D-014 ordering applies
without a tiebreaker: equal-generation/different-origin records are an explicit
conflict.

### Cancelled prepare, peer

A host abort, host timeout, supersession or yield before the local marker
cancels the peer staging with a split `ABORT`, and the host transaction cannot
end while that prepared push is active. The `ABORT` is bounded by
`NOAH_PROFILE_SPLIT_PREPARED_ABORT_TIMEOUT_MS` (15 s). If the peer stays
silent or answers `BUSY` past it, the USB half releases its own side: the host
transaction ends, and durable authority on both halves is unchanged because no
marker exists. The peer may still hold its provisional lease, so the USB half
reports peer cleanup pending, retries the same `ABORT` about once a second in
idle slots, keeps polling metadata, and starts no prepared push or ordinary
transfer until the peer acknowledges it. A refused cancel (the peer reports
matching durable state) is not released: it stops fail closed as before.

The bound exists because of a field wedge on 2026-09-23. The peer answered
every `ABORT` with `BUSY`; a peer store in `RECONCILE_REQUIRED` (durability
unknown) does that until it restarts. The unbounded retry held the host's own
`ABORT` in the candidate mailbox, and the host timeout, which requires an empty
mailbox, never ran, so only a power cycle released the keyboard. A peer that
never acknowledges still needs a restart, but the keyboard now says so instead
of hanging.

### Interrupted prepare, peer

Only completion (both halves hold the exact copy), a cancel, or a terminal
stop ends a prepared push. A reply the sender cannot use, such as a reply to
an earlier request or a `STALE` error, drops it back to its metadata poll with
the push still active. That poll then finds both halves on the old profile.
It must resume the copy with `PREPARE_BEGIN`, which picks up at the receiver's
offset, instead of reading the two halves as converged.

This rule comes from a field wedge on 2026-09-26. QMK's split RPC returns the
peer's previous response when the peer skips its callback. The sender took
that stale, well-formed reply mid-copy as converged, polled metadata once a
second, and never sent the next chunk. A converged push cannot be cancelled,
so the owner's no-progress timeout could not end it, and the host's `ABORT`
sat unprocessed in the candidate mailbox until a power cycle.

## User Operations

| Operation | Source | USB half | Peer half | Required visible result |
| --- | --- | --- | --- | --- |
| Open from keyboard | unchanged | read complete supported profile | prove matching peer or expose drift | generation-bound editable draft |
| Refresh from keyboard | unchanged | reread complete supported profile | prove matching peer or expose drift | draft refreshed or local-draft conflict shown |
| Save desktop backup | unchanged | read exact logical profile or use a verified draft | prove matching peer or label backup as unresolved | named portable backup with schema and compatibility identity |
| Restore desktop backup | unchanged | validate and apply as a new generation | prepare then converge | backup restored and exact result read back |
| Preview live RGB | unchanged | volatile preview | mirror preview or show peer pending | preview active; never persisted |
| Roll back preview | unchanged | committed profile | committed profile | preview cleared |
| Apply draft to keyboard | unchanged | compare base generation, validate, then commit | prepare then converge | device persisted and exact payload read back |
| Reset device | unchanged | commit override-disabled generation | converge reset generation | compiled defaults active; source unchanged |
| Reconcile external VIA change | unchanged | detect changed VIA digest and refuse stale logical generation | prove peer VIA state or expose drift | adopted new logical generation or explicit conflict; never silent divergence |

## Source and device separation

Authored C files are edited and compiled independently. Ark neither
writes those files nor combines source writes with a device transaction. Its
Apply ordering follows the [logical transaction contract](logical-profile-transaction-v1.md).

## RGB Preview Rules

- Preview is device-only, volatile, debounced, and replaceable atomically.
- It is tied to the committed base generation that produced it.
- Only RGB fields that do not affect key planning use the frame boundary.
- Tap-commit policy uses strict behavior activation.
- Cancel, Reload Source, profile switch, panel disposal, timeout, disconnect,
  reboot, or explicit rollback removes the preview.
- A preview never writes either durable slot and never wins reconciliation.

## Safe Behavior Activation

A prepared behavior-affecting candidate waits until the runtime reports no:

- physical press tokens or pending tap series;
- held or repeating actions;
- owned keycodes, modifiers, mouse buttons, oneshots, or deferred releases;
- layer or PD-mode ownership leases;
- active macro holds or macro engine work;
- active or pending combo-origin state;
- unresolved peer prepare/convergence state.

The activation owner reports a reason mask and counts. It never forces releases
just to make a commit progress. Until the predicate is satisfied, the prior
generation remains active and the prepared candidate remains observable.

The landed predicate freezes the following firmware reason bits:

| Bit | Reason | Authoritative evidence |
| --- | --- | --- |
| `0` | physical press | key-runtime press-token count |
| `1` | pending tap series | key-runtime tap-series count |
| `2` | runtime-owned lease | key-runtime lease count, including held/repeat and momentary ownership |
| `3` | deferred release | key-runtime pending-release count |
| `4` | persistent intent | key-runtime persistent layer/PD/pointer intent count |
| `5` | owned output | aggregate managed HID usage count, including mouse buttons |
| `6` | modifier or one-shot modifier | live real, weak, one-shot, or locked one-shot modifier state |
| `7` | one-shot layer | QMK one-shot-layer state |
| `8` | macro | non-idle macro engine or macro-owned hold count |
| `9` | combo | pending or active combo-origin count |
| `10` | peer | unresolved or unreadable peer-convergence state |
| `30` | internal | missing or invalid local policy state |

The predicate snapshot uses bounded generation publication so an extension-host
status read cannot observe counts from two evaluations. A missing peer observer
is intentionally unsafe. One exception is bounded: while a host Apply is
`ACTIVATING` the record this owner already saw the peer durably commit (and,
for a logical record, whose VIA ACCEPT the peer acknowledged), the peer reason
is satisfied by that confirmation instead of a second live observation. The predicate is installed by the gated D-021 owner
together with the behavior and RGB invalidators and exact split peer observer.
Ordinary firmware still allocates only the read-only shell. The D-022
distributed barrier enforces postcommit authority in the gated owner. The
separate engineering-mutation gate now couples routing with the complete,
truthful write/commit/activation/peer capability set for hardware testing;
resource policy and the hardware matrix still block ordinary exposure.

## Connection Status Shown By the Ark Client

At minimum:

- disconnected, discovering, compatible, incompatible, busy, or contended;
- source, compiled-default, active, committed, preview, and peer digests;
- active, pending, committed, and peer version tuples;
- candidate transaction and validation result;
- safe-boundary wait reasons;
- persistence and peer-convergence state;
- last protocol, storage, validation, and split error;
- explicit outcome for the last device operation.
