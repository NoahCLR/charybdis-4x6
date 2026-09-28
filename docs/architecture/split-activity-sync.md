# Split activity admission and measurement

Activity coalescing is an opt-in firmware build feature pending physical
acceptance. Enable `NOAH_SPLIT_ACTIVITY_COALESCE=yes` on the flashable pair.
Default baud remains QMK's 230,400. `NOAH_SPLIT_BAUD=230400` or `460800` is a
separate paired experiment; never use different speeds on the two halves.

## Ownership and compatibility

`users/noah/lib/compat/qmk_split_activity_policy.h` decides whether a changed
activity snapshot is due. `qmk_split_activity.c` adapts QMK's timer and RGB timeout
consumers. Neither module changes key state, local activity timestamps, mouse
processing, or the RGB renderer. The fork's activity sender owns the last
successfully written snapshot and calls the version-1 admission/result hooks.
Its weak default admits every changed snapshot, preserving ordinary QMK use.

The required QMK changes live on `feat/split_sync_optimize` in the isolated
`/Users/noah/dev/charybdis/bastardkb-qmk-split-optimize` checkout. The original
fork checkout is unchanged. The reproducible patch is
`tools/qmk-patches/split-activity-policy.patch`. Enabling coalescing against a fork without
`QMK_SPLIT_ACTIVITY_POLICY_VERSION == 1` fails compilation. Activity wire bytes
and transaction IDs are unchanged, so this optimization alone does not require
a new runtime protocol or a storage migration.

## Admission contract

- Initial delivery, QMK's forced repair, and a new event after at least 32 ms
  without a source timestamp change bypass coalescing.
- Sustained activity refreshes the latest snapshot every 32 ms. A final changed
  snapshot is delivered by that deadline even after input stops.
- The shortest nonzero compiled/portable RGB timeout bounds admission. Timeouts
  of 64 ms or less preserve immediate changed-snapshot writes; approaching the
  prior snapshot's sleep boundary also bypasses coalescing. Timeout changes
  invalidate admission. Timeout zero means that consumer is disabled.
- Timeout age uses the same numerical maximum as QMK's receiver; scheduling uses
  unsigned elapsed time. This preserves QMK's existing source-wrap semantics,
  rather than silently changing them on one half.
- A failed send leaves admission pending and does not advance the successful
  snapshot or its clock. The transport's mutable staging buffer is never used
  as evidence of successful delivery. Existing QMK retries remain in charge.
- Left matrix acquisition stays at every scan. Layer/mode/combo/key-feedback
  packets and durable profile reconciliation retain their existing rules.

Future activity consumers must be included in the timeout/admission contract
before enabling coalescing with them. Physical sleep/wake, short live timeout
changes, reconnects, error rates, and latency still require acceptance; host tests
exercise policy equivalence under their simulated delivery assumptions.

## Ten-second transaction recorder

Enable `NOAH_SPLIT_DIAGNOSTICS=yes` independently of coalescing. It adds counters
only to the master-side PIO serial transaction path. Each attempt records its
transaction ID, success/failure, nominal attempted frame bytes, elapsed counter
time, and maximum elapsed time. Failed attempts count the nominal entire frame;
these are not measured electrical bytes. The RP2040 realtime counter is in us.
Per-ID totals include retries as separate attempts. The measured span covers
transaction execution; queue clearing just before it and recorder bookkeeping
after it are excluded. Counters report wall time, not CPU utilization.

The recorder is explicitly armed after boot, runs for ten seconds, then freezes.
A transaction crossing the deadline is included and extends the reported duration.
There is no logging, EEPROM write, or continuous USB readback during capture.
All recorder state and transport probes are absent when the flag is unset.
Compare instrumented and ordinary firmware to establish measurement overhead.

VIA custom channel 0, value `0x0A`, diagnostic builds only:

- 32-byte request: byte 0 = GET `7` or SET `8`; byte 1 = 0; byte 2 = `0x0A`;
  byte 3 = nonzero correlation; byte 4 = page; bytes 5–31 zero.
- SET page 0 clears/arms capture. It changes only volatile diagnostic state.
- GET page 0 returns metadata. Pages 1..N return transaction ID page-1; reads
  before freezing return unavailable. Malformed/unknown/unavailable status codes
  match Profile Wire (1/2/3). Replies echo bytes 0–4; status is byte 5.
- Successful GET has payload length 25 at byte 6, format version 1 at byte 7.
  Metadata: count at 8, armed at 9, frozen at 10, LE32 duration us at 11,
  PUT_ACTIVITY ID at 15, remaining bytes zero.
- Transaction page: ID at 8; LE32 attempts/failures/attempted bytes/total us/max us
  at 9/13/17/21/25; remaining bytes zero.

`node tools/capture-split-diagnostics.cjs` arms, waits without device requests,
then reads frozen pages as JSON. It uses the existing Charybdis Live node-hid
installation but is a separate engineering tool. Close competing app/VIA
connections. Select `--path` if more than one matching keyboard is attached.
Capture baseline and optimized firmware with the same profile, cable, baud,
lighting and motion workload. Physical USB report cadence must be measured
separately; this recorder cannot establish p99 report gaps by itself.

## Remaining work

The runtime RPC replacement and asynchronous transport remain behind the
measurement gates in `docs/plans/split-transport-optimization.md`. No 1 kHz claim
or acceptance is implied by the activity implementation or calculated byte savings.
