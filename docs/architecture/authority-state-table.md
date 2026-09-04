# Source, Device, And Split Authority

## Identities

| State | Identity | Authority |
| --- | --- | --- |
| Studio draft | device base generation/digest plus optional imported source revision | none until an explicit device apply or source export |
| Source files | canonical digest plus file revision | compiled-default and explicit import/export representation |
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
  stop and require explicit Studio resolution.
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

## User Operations

| Operation | Source | USB half | Peer half | Required visible result |
| --- | --- | --- | --- | --- |
| Open from keyboard | unchanged | read complete supported profile | prove matching peer or expose drift | generation-bound editable draft |
| Refresh from keyboard | unchanged | reread complete supported profile | prove matching peer or expose drift | draft refreshed or local-draft conflict shown |
| Save desktop backup | unchanged | read exact logical profile or use a verified draft | prove matching peer or label backup as unresolved | named portable backup with schema and compatibility identity |
| Restore desktop backup | unchanged | validate and apply as a new generation | prepare then converge | backup restored and exact result read back |
| Import source | read | unchanged | unchanged | source-derived draft; device unchanged |
| Export draft to source | write | unchanged | unchanged | source updated; device unchanged |
| Preview live RGB | unchanged | volatile preview | mirror preview or show peer pending | preview active; never persisted |
| Roll back preview | unchanged | committed profile | committed profile | preview cleared |
| Apply draft to keyboard | unchanged | compare base generation, validate, then commit | prepare then converge | device persisted and exact payload read back |
| Explicit source + device sync | validate proposed source export, then write | prepare before source write, commit after it | prepare/commit with USB half | synchronized or explicit partial outcome |
| Reset device | unchanged | commit override-disabled generation | converge reset generation | compiled defaults active; source unchanged |
| Reconcile external VIA change | unchanged | detect changed VIA digest and refuse stale logical generation | prove peer VIA state or expose drift | adopted new logical generation or explicit conflict; never silent divergence |

## Compound Apply Ordering

An explicit compound source-and-device synchronization uses:

1. parse proposed source and build a canonical candidate;
2. validate locally against connected capabilities;
3. send and prepare the complete candidate on the device without activation;
4. write source files;
5. commit the prepared device candidate;
6. reread USB and peer status before reporting synchronization.

If source writing fails, Studio aborts the prepared device candidate. If source
succeeds and device commit fails, Studio reports `source updated but device
failed` and offers Retry Deploy. If the acknowledgement is lost, Studio queries
generation and digest before deciding whether the commit failed.

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
is intentionally unsafe. The predicate is installed by the gated D-021 owner
together with the behavior and RGB invalidators and exact split peer observer.
Ordinary firmware still allocates only the read-only shell. The D-022
distributed barrier enforces postcommit authority in the gated owner. The
separate engineering-mutation gate now couples routing with the complete,
truthful write/commit/activation/peer capability set for hardware testing;
resource policy and the hardware matrix still block ordinary exposure.

## Connection Status Shown By Studio

At minimum:

- disconnected, discovering, compatible, incompatible, busy, or contended;
- source, compiled-default, active, committed, preview, and peer digests;
- active, pending, committed, and peer version tuples;
- candidate transaction and validation result;
- safe-boundary wait reasons;
- persistence and peer-convergence state;
- last protocol, storage, validation, and split error;
- explicit partial result for the last source/device operation.
