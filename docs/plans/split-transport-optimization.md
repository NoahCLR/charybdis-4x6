# Split transport optimization handoff

## Objective and status

Reduce split work on the right master's mouse-report path at the existing
230,400 baud, preserving the connected keyboard experience: left-key response,
combos and holds, mouse buttons and pointing modes, lighting wake/sleep and
feedback, and Charybdis Live's durable apply/recovery contract.

Activity coalescing is on in the default build (stage 1 below); bounded
transaction diagnostics stay opt-in. Coalescing is accepted on hardware; its
measurement remains pending. The runtime
RPC replacement and asynchronous stages remain unimplemented behind their
measurement gates. See `docs/architecture/split-activity-sync.md` for the current
contract. This document is not a hardware performance result. After completion, fold durable contracts into
`docs/architecture/runtime-flow.md` and the relevant protocol spec, then delete
this plan under D-L07. Do not treat proposals here as accepted wire contracts.

User evidence: the OS reports approximately 400 mouse reports/s with both halves
connected, and the right half's mouse felt faster with the left disconnected;
that disconnected state is not a dependable measurement (stage 0).
The disconnected rate and interval distribution have not been supplied.

Source basis:

- Userspace: `4280eeffff92170288d0e61e9277e5032e8c3984`.
- QMK: `aac9f637ee4ad99fe8f962372b9dfd56be538703`,
  `0.32.5-1-gaac9f637ee`.
- Actual QMK checkout: `/Users/noah/dev/charybdis/bastardkb-qmk`; the documented
  sibling path is not valid relative to this T3 worktree.
- Right is the USB master and sensor half. Mouse motion is read locally; it does
  not need to cross the split link.
- The split link stays at 230,400 baud. A paired 460,800 option was built,
  measured and removed (D-L43): it garbled split messages. Setting
  `NOAH_SPLIT_BAUD` now fails the build.
- QMK has pre-existing dirty/untracked submodule/module state. Preserve it.
  Implement required fork work on an isolated branch/worktree, with ownership
  explicit, rather than modifying unrelated work.

Read `AGENTS.md`, `docs/LIVE_EDIT_APP_DIRECTION.md`,
`docs/architecture/device-resident-profile.md`,
`docs/architecture/profile-split-v1.md`, and
`docs/architecture/memory-budgets.md` before implementation.

## What the code establishes

| Path | Current behavior | Consequence |
| --- | --- | --- |
| QMK `quantum/keyboard.c:keyboard_task` | Matrix scan, userspace scan work, quantum/RGB work, then pointing task | Synchronous split work delays the next sensor poll and report |
| QMK `quantum/matrix_common.c:matrix_post_scan` | Calls split transport before userspace scan returns | Mouse is coupled to completion of the split scan |
| QMK `transactions.c:slave_matrix_handlers_master` | Reads a checksum every scan; fetches matrix on change or forced refresh | Keep frequent key acquisition; reducing this blindly trades mouse speed for left-key latency |
| QMK `transactions.c:activity_handlers_master` | Sends three 32-bit timestamps when any changes | Sustained motion ordinarily triggers a 12-byte activity payload every scan |
| QMK `keyboard.c:last_pointing_device_activity_trigger` | Changes pointing/input activity timestamp on pointing activity | Creates next scan's activity-sync traffic |
| QMK `transactions.c:transaction_rpc_exec` | Four transactions: metadata, request, execute, response | Even a zero-response runtime send pays four handshakes |
| `users/noah/lib/split/runtime_sync.c` | Four independent snapshot domains, changed data plus heartbeats | Domain transitions can produce several RPCs in one scan |
| `users/noah/lib/rgb/automouse/rgb_automouse.h` | Fade progress already quantized to lighting interval, currently 32 ms | Do not claim fade is sent at the mouse rate |
| VIA sync and profile reconciler | Settled refresh/poll intervals are 1,000 ms | Distinguish occasional long gaps from the steady motion-rate limit |
| QMK `transaction_handler_master` | Up to 10 attempts while considered connected, with retry waits | Failures can produce long gaps; normal-path savings do not solve retry stalls |

Custom feedback builders also have CPU cost, but no measured attribution exists.
For example, combo bitmaps are still rebuilt every tick; existing tests explicitly
preserve immediate combo observation. Do not replace that with dirty-only
building until every mutation has a proven invalidation path.

## Savings: quantities we can calculate

These are protocol byte counts and nominal serialization times, not measured
CPU utilization or end-to-end latency. The RP2040 PIO UART uses one start, eight
data, and one stop bit. At B = 230,400 baud:

```
byte_time = 10 / B = 43.4027778 microseconds
transaction_bytes = 1 transaction ID + 1 handshake + request + response
```

Exclude retries, IRQ/thread scheduling, mutex wait, CRC/copies, and turnaround
from these numbers. The driver's `enter_rx_state` waits for its TX FIFO and then
calls `wait_us(floor(11,000,000 / B))`, or 47 us at this baud. That wait includes
completion of the last transmitted byte: adding 47 us to every nominal byte
count would double-count part of transmission. Measure complete transactions to
learn the additional elapsed cost. Clock-divider rounding is also unmeasured.

### A. Activity traffic: strongest first optimization

Current successful activity send: 2 handshake bytes + 12 payload bytes = 14
bytes = **607.639 us** nominal serialization. At an assumed one send per report
and 400 reports/s: **5,600 bytes/s**, **243.056 ms/s** of serialization.

Proposed sustained-motion refresh interval: 32 ms, at most approximately 31.25
refreshes/s in steady state. Immediate wake, final timestamp, retries and
reconnection can add messages outside that steady-state assumption.

| Quantity during sustained movement | Current at 400/s | Proposed at 31.25/s | Reduction |
| --- | ---: | ---: | ---: |
| Activity transactions/s | 400 | 31.25 | 368.75 (92.1875%) |
| Bytes/s | 5,600 | 437.5 | 5,162.5 |
| Nominal serialization ms/s | 243.056 | 18.989 | **224.067** |
| Average serialization per original 2.5 ms report interval | 607.639 us | 47.472 us | **560.167 us** |

Count actual activity transactions first: 400 OS reports/s does not prove exactly
400 activity sends/s. For measured rate A, the steady savings are
`(A - 31.25) * 607.639 us/s`, provided A exceeds the refresh rate and there are no
extra transitions/retries.

A self-consistent, serialization-only throughput illustration is:

```
T0 = 1 / 400 = 2.5 ms
c = 0.607638889 ms per activity transaction
other_work = T0 - c = 1.892361111 ms
F = (1000 ms/s - 31.25/s * c) / other_work
  = 518.406 reports/s, approximately +29.6%
```

This assumes all other per-loop work is unchanged, one report per loop, adequate
motion, no new bottleneck, and no remaining USB/scheduler quantization. It is a
scenario, not a prediction guarantee or lower bound. Eliminated turnarounds
could increase gains; changed scheduling or another bottleneck could reduce them.
Do not present 224 ms/s as 22.4% CPU utilization recovered.

### A combined with 460,800 baud — rejected

Built and measured on 2026-09-28 with coalescing: profile copies logged roughly
20–30 split transport failures per Apply at 460,800 against none at 230,400, and
the other half's lighting flickered because QMK's lighting sync has no checksum.
The option is removed (D-L43). Revisit only after the unprotected syncs carry
checksums, and measure again.

### B. Runtime RPC overhead

For a P-byte send-only snapshot, current RPC wire bytes are:

```
PUT_RPC_INFO:      2 + 4
PUT_RPC_REQ_DATA:  2 + P
EXECUTE_RPC:       2 + 1
GET_RPC_RESP_DATA: 2 + 0
TOTAL: P + 13 bytes, 4 transactions
```

The final zero-data transaction still handshakes. Do not merely remove it: it
also orders completion relative to the preceding callback/worker activity.

Candidate dedicated fixed-size runtime exchange: one transaction per domain,
request = 2-byte sequence + P-byte snapshot + 1-byte CRC8; response = 2-byte
sequence + 1-byte status + 1-byte CRC8. CRC covers domain identity and contents.
That is **P + 9 bytes**, with a response produced after validation/publication.
This candidate saves **4 bytes = 173.611 us and 3 handshakes per update** while
adding explicit publication confirmation. Final framing and capability/version
negotiation must be designed and tested before implementation. A weaker response
must not be substituted simply to claim larger savings.

Current production geometry is 10 rows x 6 columns = 60 slots, bitmap size 8,
three-bit semantic/tap maps of 23 bytes, and 7 broad-owner bytes. Verify these
against a production-config size probe; the usual host stub defaults to 8x8.

| Runtime domain | P | Current bytes / us | Candidate bytes / us | Byte reduction |
| --- | ---: | ---: | ---: | ---: |
| Base, including owner bitmap | 14 | 27 / 1,171.875 | 23 / 998.264 | 14.81% |
| Combo bitmaps | 16 | 29 / 1,258.681 | 25 / 1,085.069 | 13.79% |
| Semantic feedback | 31 | 44 / 1,909.722 | 40 / 1,736.111 | 9.09% |
| Branch feedback | 30 | 43 / 1,866.319 | 39 / 1,692.708 | 9.30% |
| All four once | 91 total | 143 / 6,206.597 | 127 / 5,512.153 | 11.19% |

All four together: **694.444 us less byte time and 12 fewer handshakes**.
At a base fade update rate of 31.25/s: **5.425 ms/s** less byte time.
At four unchanged idle-domain heartbeats once per second: **0.694 ms/s** less.
These are different workloads; do not add them indiscriminately to A's sustained
motion savings. Direct transport primarily improves feedback bursts and callback
handoff overhead; it alone is unlikely to explain a large steady-rate gain.

An idealized zero-overhead P+2 exchange would save 11 bytes (477.431 us), but
omits the proposed integrity/confirmation fields. It is a theoretical comparison,
not the proposed implementation or a savings claim.

### C. Costs that remain

An unchanged matrix checksum poll is 3 bytes, nominally 130.208 us, before
turnaround/worker cost. Keep it initially. Matrix changes add a separate data
transaction; do not combine matrix data and checksum without preserving a
coherent snapshot and corruption detection.

A 32-byte request + 32-byte response profile/VIA RPC costs 77 bytes = 3.342 ms
serialization, before other overhead. Even one bounded RPC per scan can therefore
create a multi-millisecond mouse gap. It is infrequent when settled but matters
during Apply and recovery. Phase A/B do not eliminate this problem.

Moving the pointing task earlier in a sequential loop saves zero loop work.
Core-1 sensor sampling alone also leaves report submission behind the split wait.
Neither earns a calculated report-rate improvement in this plan.

## Implementation sequence and acceptance

### 0. Establish attribution with bounded measurement

1. Capture with the procedure in `measurements/pointing-cadence/README.md`,
   which holds baud, profile, firmware feature set, lighting and app state
   fixed, and record each set there. Measure connected idle, connected motion, and motion while typing on the
   left. Not with the left half disconnected: the master then keeps probing
   and waiting on transport timeouts, so the reading includes stalls a
   connected keyboard never has. Instead the capture tool estimates the
   ceiling as pointing polls with measured split transaction time removed.
2. Count transactions by ID, payload bytes, failures/retries, and elapsed time
   for the complete split scan. Add sensor/USB spans only if needed to explain
   remaining time. Instrument in the actual PIO/serial path; do not time only
   the user matrix hook, which runs after QMK's built-in split work.
3. Use compile-gated fixed counters and bounded RAM summaries, no per-scan
   printf, allocation, flash write, or active USB diagnostic streaming. Read
   results after the capture; exclude the readback window. Compare stock and
   instrumented OS cadence to quantify observer cost. For stage cost use one
   stage per pass or a short bounded capture. Sampled data cannot prove maximum
   latency; label it accordingly.
4. Existing `runtime_diag.c` measures loop/poll gaps, not per-ID transaction
   costs. Reuse its readback where appropriate, but keep production builds free
   of the new recorder. Measure p50/p95/p99 and longest report gap over equal
   windows, alongside mean rate, errors, and input workload.

Deliverable: transaction distribution and elapsed costs on the actual keyboard.
Proceed with A if activity traffic follows the source-backed expectation.

Baseline, 2026-09-28, `diagnostic_cadence` pair from `feat/split_sync_rework`
(coalescing on, 230,400 baud), ten-second captures with both halves connected:

| | idle | motion | motion + typing |
| --- | ---: | ---: | ---: |
| Pointing polls/s (one per loop; bounds reports) | 538 | 497 | 436 (417–480) |
| Share of the loop in split transactions | 17.9% | 18.2% | 17.6% |
| Estimated polls/s with no split time | ~655 | ~608 | ~529 |
| Poll gaps 1.5–2 ms / 2–5 ms | 78% / 21% | 72% / 28% | 57% / 41% |
| Poll gaps ≥ 5 ms | ~3/s | ~3/s | ~8.5/s |
| Longest poll gap | 8.8 ms | 8.9 ms | 9.8 ms |
| Failed transactions | 1 | 3 | 6 |

The matrix checksum poll is most of the split time (12–13% of the loop, about
245 µs each, half of it direction turnarounds). Activity runs at about 30/s in
motion. Everything else is QMK's 100 ms forced resend, about 10/s each. RPCs
run 6–9/s at 2.1–2.4 ms each, about 2% of the loop.

What follows from it:

- No loop is shorter than 1.5 ms and four fifths of it is not split work, so
  the report rate is bounded by the master's own per-loop work.
- Gaps of 5 ms or more are the hitches; RPCs landing in one loop are the
  likely source.

Stage timing, same day and workloads, set
`measurements/pointing-cadence/2026-09-28-stage-timing/` (stages in
`docs/architecture/split-activity-sync.md`). Share of the idle loop, which is
about 2.0 ms:

| Stage | Share | Per loop | Longest loop |
| --- | ---: | ---: | ---: |
| Local matrix scan (matrix stage less split transactions) | ~18% | ~355 µs | |
| Split transactions | 19% | ~390 µs | |
| Durable I/O scan | 12% | 249 µs | 4.5 ms |
| Runtime split sync | 15% | 295 µs | 6.5 ms |
| Sensor read | 11% | 223 µs | 0.3 ms |
| Lighting (our render and QMK's task) | ~14% | ~290 µs | 1.0 ms |
| Report, LED, VIA, housekeeping, rest | ~13% | ~260 µs | |

Motion and typing have the same shape. The timing itself costs 8–10% of the
poll rate (497/448/406 against 538/497/436), so read shares, not rates.

The baselines are complete; no further baseline is needed. What they decide:

- The largest cost we own is CPU, not link: the durable I/O scan and the runtime
  split sync spend about 27% of every loop, idle included, while their RPCs are
  about 2%. Both do per-loop work whether or not anything changed (split sync
  rebuilds its base and combo packets every tick). Making them change-driven
  is the next optimization, verified with one capture after the change.
- Their RPCs are also where the hitches are (longest loops 6.5 and 4.5 ms).
- The local matrix scan (~355 µs) is the next largest; check its I/O delay.
- The frame-aware coalescing candidate below is not indicated: no bursts.
- Stage 2 is worth about 1% of the loop at these rates.

### 1. Coalesce activity timestamps, preserve behavior

Ownership: keep policy in a small tested compat helper under
`users/noah/lib/compat/`. QMK's activity handler is static and currently has no
policy hook. Add a narrow, default-preserving hook in the fork rather than using
linker wrapping or replacing the whole transactions implementation. The hook must
see current snapshot, last successfully transmitted snapshot, and send outcome.
A skip must not advance the last-success clock or overwrite the sent snapshot.
Test QMK's actual handler integration as well as the pure policy helper.

Proposed policy and constraints:

- First contact and first new activity after a quiet period send immediately.
- During continuous activity, keep one latest pending snapshot and refresh at
  most once per 32 ms. Do not queue obsolete timestamps.
- Once the source snapshot stabilizes, send the final exact snapshot by the
  refresh deadline even if no further input arrives. Never invent the receive
  time as the activity time: that would extend the sleep timeout.
- Preserve all three timestamp values and local master activity/noise behavior.
  Only change transport admission, not `last_*_activity_trigger` semantics.
- Retain forced recovery/heartbeat behavior and send immediately on a known
  reconnect. Handle a peer reboot that the link never classified as disconnected.
- Use unsigned elapsed arithmetic for scheduling and test clock wrap. Do not
  introduce a second wrap bug via numerical timestamp ordering.
- To preserve RGB output, the pending latest timestamp must reach the slave
  before it could render an erroneous sleep frame. Make the interval aware of
  the shortest enabled consumer timeout and current configuration; immediate
  wake and timeout-boundary flush override the sustained throttle. For very
  short timeouts use immediate traffic. Turning timeout off/on or shortening it
  requires a flush/re-evaluation. Inventory both QMK's compiled RGB timeout and
  our portable `rgb_runtime.c` timeout, plus any newly enabled consumers.
- If strict rendered-frame equivalence cannot be established with this policy,
  revise it around explicit active/idle transitions or synchronized sleep
  deadlines before shipping. Do not silently accept a 32 ms premature sleep.
- Failure leaves the latest snapshot pending; never call failed delivery success.
  Avoid stacking a new retry loop over QMK's existing retry mechanism.

Tests: long continuous movement, brief bursts, quiet-to-active wake, final stop,
left-key activity while pointing, source timestamp wrap, scheduling wrap, failed
send, peer reboot/rejoin, timeout 0/short/default, live timeout changes, and both
compiled and portable RGB paths. Compare rendered sleep/wake decisions against
the original at each simulated frame. Preserve per-scan left matrix acquisition.

Exit: substantial measured drop in PUT_ACTIVITY count, improved reporting or
split elapsed time, no regression in the above behavior. Keep baud unchanged.

Status: the hook is fork commit `6889960271` on `sol`, and coalescing is the
default build; `NOAH_SPLIT_ACTIVITY_COALESCE=no` builds the comparison pair.
It was made the default after daily use on hardware, with Applies saving on
both halves and no split transport failure across three Applies at 230,400.
Hardware acceptance is settled: daily use shows no regression in lighting
wake/sleep, left keys, chords, holds, pointing modes or reconnects. Still to
measure: the mouse report rate with coalescing at 230,400 (about 600 Hz was
seen with 460,800, against about 400 Hz originally).

### 2. Reduce custom snapshot handshake overhead

Only after A is measured, introduce a dedicated fixed-size runtime exchange in
the fork and a userspace compat adapter. QMK's current user transaction IDs have
callbacks registered for generic RPC; simply writing them directly does not
provide the needed buffers, sizes, or response contract.

- Explicitly provision transaction entries/buffers and a capability/version
  contract. Do not reinterpret generic RPC shared memory behind QMK's back.
- Keep the four domains initially. Preserve publication generations and all
  remote mode/application callbacks; acknowledge after coherent publication.
- Correlate responses, validate CRC/status, make snapshot retries idempotent,
  and handle sequence wrap and reboot. Handle lost ACK without applying harmful
  side effects twice. Do not use a pre-publication handshake as success.
- Either negotiate legacy fallback or document a required matching firmware
  pair with clean mixed-version failure. Do not let a layout mismatch publish
  arbitrary mode state. Keep durable Profile Split v1 and VIA transactions on
  their current transport during this step.
- Check fixed buffer cost and stack paths per half using fresh linked accounting;
  follow memory-budgets.md. Do not present policy margins as physical RAM limits.
- Preserve changed-domain immediacy, heartbeat recovery, dirty state, send-success
  snapshots, retry backoff, packet limits, and main/split-worker publication rules.

Tests: production payload size probes, malformed requests/responses, loss at
request/callback/ACK boundaries, duplicates, sequence wrap/reset, mixed firmware,
receiver restart, and interleaved snapshot readers. Count actual handshakes using
real protocol code or a faithful instrumented transport fixture.

Exit: one confirmed exchange per runtime domain; measured gap reduction during
feedback transitions; no mode/lighting/recovery regression. Do not optimize away
heartbeats just because a static snapshot appears unchanged.

Not indicated by the baseline (6–9 RPCs/s, no bursts); kept for the record.
Candidate, to decide with the recorder's per-id counts: the runtime syncs
send every change at once, so several changes within one 32 ms lighting frame
spend four-transaction RPCs on states the slave never draws. The activity
pattern fits: the first change at once, then at most one send per frame with
the latest value, and the final state always delivered. Feedback would appear
no later, since the first change is immediate. Worth building only if typing
or pointing shows such bursts. The activity interval (32 ms) is its own
constant, equal to the frame period but governing sleep timing; revisit it if
`RGB_MATRIX_LED_FLUSH_LIMIT` changes.

### Failed transactions: what they cost

A failed attempt waits the full `SERIAL_USART_TIMEOUT` (5 ms). While the
link counts as connected, `transaction_handler_master` tries each handler up
to 10 times with growing waits, so one failed scan stalls the loop for about
54 ms. QMK declares the peer disconnected after `SPLIT_MAX_CONNECTION_ERRORS`
(10) failed scans in a row, about 0.5 s of near-frozen reporting, and then
retries once every `SPLIT_CONNECTION_CHECK_TIMEOUT` (500 ms), each retry a
5 ms gap. It reconnects on the first retry that succeeds.

Deciding later that the peer is gone would lengthen the expensive phase, not
help. The candidates are making failure cheap: a short timeout for the
reconnection retry only, since a live peer answers well within a millisecond,
and a bound on total retry time per scan. Both are fork changes, and the same
retry path carries a busy peer, for example one writing flash during Apply,
without dropping the link, so neither is safe to guess. Decide from the
baseline capture: failures and the longest span per transaction id show how
often, and how long, the connected keyboard stalls on retries.

### 3. Bound remaining report stalls if necessary

Evaluate after A/B. Remaining problems may be generic durable RPCs, retries, or
several changed domains in one scan. A simple global rate limit is unsuitable:
it can delay key feedback or starve profile recovery.

Investigate a single transport owner with asynchronous completion and a bounded
work queue. Matrix snapshots retain priority and freshness; obsolete display
snapshots may be coalesced; durable ordered requests may not be discarded or
reordered. Serve pointer sampling/report work between bounded transport steps.
Do not call QMK pointing/key/layer/USB functions concurrently from an IRQ or core
1 without an explicit synchronization and ownership design. Preserve ordering
between left mouse-button/PD keys and motion; define an acceptable freshness
bound before decoupling key acquisition. Ensure a stuck peer cannot monopolize
mouse reporting and cannot leave left keys held indefinitely.

This phase needs its own design review and measurements. No numerical saving is
claimed: it changes when waiting occurs, not necessarily total bytes or work.
Keep explicit profile leases, peer expiry, committed-generation confirmation,
marker-last storage, cancellation ownership, and recovery fencing intact.

## Verification and handoff completion

Run targeted tests throughout, using the actual fork through QMK_ROOT:

```
sh tests/host/run_qmk_contract_checks.sh
sh tests/host/run_split_runtime_sync_tests.sh
sh tests/host/run_pd_runtime_tests.sh
sh tests/host/run_pointer_layer_policy_tests.sh
sh tests/host/run_rgb_layer_render_tests.sh
sh tests/host/run_qmk_durable_io_tests.sh
sh tests/host/run_profile_split_reconciler_tests.sh
sh tests/host/run_qmk_via_split_sync_tests.sh
sh tests/host/run_runtime_diag_tests.sh
sh tests/host/run_feature_gate_compile_tests.sh
sh tests/host/run_all_host_tests.sh
```

Add dedicated activity-admission and actual-QMK-handler tests to the full suite.
Run firmware compile only after required host gates pass:

```
qmk compile -kb bastardkb/charybdis/4x6 -km noah
QMK_ROOT=/Users/noah/dev/charybdis/bastardkb-qmk sh tools/build-firmware-pair.sh
```

Set `QMK_USERSPACE` to this worktree and use the correct QMK checkout. Build the
side-specific owner-enabled pair for hardware acceptance; the generic build
alone is factory-only.
If authored config changes, regenerate/check introspection as AGENTS.md requires.
If memory/stack paths change, run the corresponding firmware budget gates and
collect proportional hardware high-water evidence.

Hardware acceptance: repeat baseline workloads; exercise left keys, chords,
combos, held mouse buttons, every pointing slot, RGB wake/sleep/fade and key
feedback, half restart/reconnect, Apply/backup/restore, and the existing physical
interruption matrix. Compare latency distributions and error/retry counts, not
only mean mouse Hz. Keep an uninstrumented final comparison at the original baud.

Update README and governing specs with actual supported behavior. Record test
runs/build numbers in the handoff or commit message, not durable architecture
contracts. Report measured savings separately from this document's byte model;
do not promise 1,000 Hz from phase A or claim hardware acceptance without it.
