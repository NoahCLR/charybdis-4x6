# Split activity admission and measurement

Activity coalescing is on in the default build. `NOAH_SPLIT_ACTIVITY_COALESCE=no`
builds the uncoalesced comparison firmware. It is accepted on hardware in daily
use; its measured effect on the report rate is still open. Measure it with the
recorders below.
The split link runs at QMK's default 230,400 baud, with no speed selector; see
D-L43 for why 460,800 was removed.

## Ownership and compatibility

`users/noah/lib/compat/qmk_split_activity_policy.h` decides whether a changed
activity snapshot is due. `qmk_split_activity.c` adapts QMK's timer and RGB timeout
consumers. Neither module changes key state, local activity timestamps, mouse
processing, or the RGB renderer. The fork's activity sender owns the last
successfully written snapshot and calls the version-1 admission/result hooks.
Its weak default admits every changed snapshot, preserving ordinary QMK use.

The hook is fork commit `6889960271` on `noah-userspace-contracts`, so the default build needs that
commit or later. Building coalescing against a fork without
`QMK_SPLIT_ACTIVITY_POLICY_VERSION == 1` fails compilation and names the
opt-out. Activity wire bytes
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
before enabling coalescing with them. Host tests exercise policy equivalence under
their simulated delivery assumptions; hardware use has shown no sleep/wake,
reconnect or lighting regression. Latency and error rates are measured with the
recorders below.

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

- 32-byte request: byte 0 = `7` to read or `8` to arm, VIA's `id_custom_set_value`
  and `id_custom_get_value` used the other way round (QMK routes both to the
  same handler); the cadence recorder reads with `8`. Byte 1 = 0; byte 2 = `0x0A`;
  byte 3 = nonzero correlation; byte 4 = page; bytes 5–31 zero.
- Arm (`8`) page 0 clears/arms capture. It changes only volatile diagnostic state.
- Read (`7`) page 0 returns metadata. Pages 1..N return transaction ID page-1; reads
  before freezing return unavailable. Malformed/unknown/unavailable status codes
  match Profile Wire (1/2/3). Replies echo bytes 0–4; status is byte 5.
- A successful read has payload length 25 at byte 6, format version 2 at byte 7.
  Metadata: count at 8, armed at 9, frozen at 10, LE32 duration us at 11,
  PUT_ACTIVITY ID at 15, remaining bytes zero.
- Transaction page: ID at 8; LE32 attempts/failures/attempted bytes/total us/max us
  at 9/13/17/21/25; format 2 adds LE16 CRC failures at 29: writes the slave
  dropped and reported, and reads the master rejected (zero without the frame
  CRC). Attempted bytes include each frame's CRC byte. Remaining bytes zero.

`node tools/capture-split-diagnostics.cjs` arms, waits without device requests,
then reads frozen pages as JSON. It uses the firmware tools' own node-hid
installation (`npm ci --prefix tools`) and is a separate engineering tool. Close competing app/VIA
connections. Select `--path` if more than one matching keyboard is attached.
Capture with the procedure in
[`measurements/pointing-cadence/`](../../measurements/pointing-cadence/README.md),
which also keeps the recorded sets.

The same run reads the pointing-cadence recorder (custom value `0x03`) when
the firmware is built with `NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS=yes`. It keeps
one-second windows since boot, unarmed. The tool reads the recorder's window
count just before arming and just after the capture, and summarises the windows
wholly between them: pointing polls per second, matrix scans per second, the
longest gap between pointing polls, and a gap histogram. Pointing
polls are pointing-task runs, an upper bound on USB mouse reports; the host's
own report cadence is still the final word.

The recorder also times each stage of the master's loop, exclusively: a
nested stage's time is not also counted in the stage it interrupts. Per
stage the tool reports its share of wall time, its mean per matrix scan (one
per loop) and its longest single loop, which attributes long poll gaps. The
stages, in wire order:

| Stage | From → to |
| --- | --- |
| `matrixScan` | `keyboard_task` entry → `matrix_scan_user`: local matrix, debounce, QMK's split transactions |
| `durableIo` | VIA macro defaults and the durable I/O scan (profile and VIA sync RPCs) |
| `keyRuntime` | combo origins, key runtime scan, macro engine |
| `splitSync` | runtime split sync RPCs |
| `qmkTasks` | `matrix_scan_user` return → pointing hook: key event dispatch, `quantum_task`, `rgb_matrix_task` (effect and LED flush), the keyboard's pointing code |
| `processRecord` | `process_record_user` and its finalize, wherever they run |
| `rgbRender` | our render in `rgb_matrix_indicators_advanced_user` |
| `sensorRead` | the pointing driver's report read |
| `pointingTask` | `pointing_device_task_user` |
| `pointingReport` | pointing hook return → `keyboard_task` end: auto-mouse, USB mouse report, mousekey and LED tasks |
| `outsideKeyboardTask` | between `keyboard_task` calls: USB events, raw HID and VIA, deferred executors, housekeeping |

`matrixScan` includes the split recorder's transactions; subtract its share
for the local scan. The shares sum to about 1 (`coverage`). On a loop where
the pointing task does not run, its stages are counted in `qmkTasks`. The
loop and sensor boundaries come from `lib/compat/qmk_loop_stages.c`, which
replaces QMK's weak `protocol_keyboard_task` and times the pointing driver's
`get_report` through a copy of its driver table; the noah hooks mark the rest.
Stage timing adds about a dozen counter reads per loop: compare poll rates with
a format 1 recorder build to see its cost. The wire layout is specified in
`runtime_diag.h`.

The tool also estimates a ceiling:
the poll rate with measured split transaction time removed. It excludes
userspace packet building and ignores the 1000/s USB cap, so it compares
builds rather than predicting a rate. A capture with the left half
disconnected is not a substitute: the master keeps probing and waiting on
transport timeouts.

## Remaining work

A dedicated runtime exchange replacing the four-transaction RPC, and an
asynchronous transport, are not built. Measure with the recorders below before
building either. No 1 kHz claim or acceptance is implied by the activity
implementation or calculated byte savings.

## Shared main-context optional bus service

When the complete Lighting Live consumer is enabled, userspace's optional split
RPCs share one main-owned admission record: base state, retained snapshots,
preview, profile reconciliation, durable VIA, the VIA mirror and the legacy
facts fallback. QMK's mandatory matrix and pointing transport stays outside
this budget. Main opens one admission pass per matrix scan, before durable I/O
and key processing, and at most one userspace RPC completes per pass. A
round-robin wanted mask survives denied attempts and drops inactive demand
within two passes. A client asks before any state side effect, so deferral
never advances a sequence, clears a pending cut or counts as transport failure.

Every actual completion reserves quiet time after the measured transaction:
`4 * (elapsed_ms + 1)` ms for CURRENT-only, preview and durable work, capped at
1,000 ms for a transaction of 250 ms or more. Qualified history cuts and cold
coherent synchronization get a 1 ms opportunity instead, so short retained
feedback stays useful. That priority never bypasses one RPC per pass, fairness
or the shared outage cooldown. Repeated PENDING is not progress: a second
consecutive PENDING for the same retained packet gets ordinary quiet time, and
stale ACKs, REJECT and NEED_HELLO never earn prompt service. A failure from any
client reserves at least 50 ms, doubling to 1,000 ms; success ends the outage
backoff but still reserves its quiet time. No idle heartbeat or busy wait is
added. Receivers, local stores, lease expiry and local END release keep running
while sends are denied.

CURRENT producers keep advancing while an immutable cut is in flight; the
collector takes the latest CURRENT state only after that cut's actual main
ACK, and an old ACK cannot clear a newer qualified history. The legacy facts
fallback likewise holds its existing built bank as the cut and sends one
admitted changed chunk per pass, committing with the final chunk; it adds
control bits and cursors, not another facts bank. Builds without the complete
live consumer keep their previous send paths and resource selectors.

VIA mirroring stays best effort. The master owns one exact outgoing command
(32-byte report, length, pending), separate from the peer's incoming mailbox.
Accepted storage changes made while it waits restart the canonical digest and
converge through authoritative full reconciliation. Queueing does not imply
remote storage or convergence. A role change, the inactive logical-bank guard
or a VIA generation change drops the queued command, so it cannot undo newer
authoritative state. Logical transaction operations and qualified lighting
histories never use this coalescing path.

`tests/host/run_split_service_tests.sh` charges 230,400 baud 8N1 plus the pinned
PIO waits on one clock and gates ordinary CURRENT scenarios at a 12 ms maximum
service gap, one RPC per pass, and at most 25% blocked main time and 100 RPC/s.
Its unpaced negative control must still reproduce the original 266 RPC/s and
734/1000 blocked time; admission brings the same continuous producer to
66 RPC/s and 180/1000. With all six consumers under the production durable-I/O
scheduler, including a preview BEGIN, publish, two renewals and END, the model
gives 149 calls in 3 s, 170/1000 blocked, a 6,122 us maximum gap and bursts of
one, then exact storage, digest and CURRENT convergence within 2 s of
quiescence. An all-client outage gives seven attempts in 3 s. Qualified input
every 50 ms is a separate stress case at 152 RPC/s and 427/1000 blocked: prompt
history service keeps the existing 200 ms positive and expiry assertions, and
there is no 25% claim under arbitrary qualified input. These are executable
schedules, not physical guarantees or evidence of real packet loss; pointing
cadence under load, drift, high-water and sustained hardware load remain
acceptance work.
