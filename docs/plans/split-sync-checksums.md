# Parked: checksums for QMK's split syncs

Parked on 2026-09-28, a follow-up to the split work in `00b9723e`; its
measurement plan is [split transport optimization](split-transport-optimization.md)
and the contract is [split activity sync](../architecture/split-activity-sync.md).
When resolved, fold what still constrains the firmware into those and delete
this file (D-L07).

Goal: QMK's split syncs that carry no checksum get one, so a garbled message
is refused instead of shown.

The split link stays at QMK's default 230,400 baud. 460,800 was measured and
removed (D-L43); it is not part of this follow-up.

## Why

At 460,800 baud the link garbled messages. The profile copy survived because
its frames carry a CRC8 and a garbled request is now retried (`1089c397`), but
QMK's own syncs show whatever arrives: the other half's lighting flickered.
At 230,400 no garbling was seen, but a noisy cable, or the other half stalled
by a flash write, can still corrupt a message.

## To find out first

- Which split transactions carry a checksum in the fork today (the matrix
  transfer does) and which do not: RGB matrix sync, layer state, modifiers,
  activity timestamps, pointing, and userspace runtime syncs
  (`split_runtime_sync`).
- What each does with a bad frame now, and what it should do: keep the last
  good state and resend, never apply a garbled one.
- Where the check belongs: in the fork's transaction layer once for every
  sync, or per sync in userspace for the ones this repo owns. The fork route
  changes QMK and needs the same care as the activity hook.
- The cost: bytes per transaction and scan time, measured with the diagnostics
  build (`NOAH_SPLIT_DIAGNOSTICS=yes`).

Only after this is in place is a faster link worth measuring again.
