# Split Link, Activity And Runtime State

The split protocol's runtime part: the serial link between the master half
and the slave half, how often the master half sends its activity timestamps
and runtime state, and the diagnostic recorders that measure the link and
the main loop. The profile's own split RPCs are
[`profile-split-v1.md`](profile-split-v1.md)'s; Profile Wire's diagnostic
value ids are listed in
[`profile-wire-v1/hid-channel.md`](profile-wire-v1/hid-channel.md).

## The link

- Both halves run the split serial link at a fixed 460,800 baud
  (`SERIAL_USART_SPEED`, set in `users/noah/lib/compat/qmk_split_transport.mk`).
  There is no speed selector: `NOAH_SPLIT_BAUD` fails the build (D-F52,
  [0052](../adr/0052-split-activity-coalescing-and-fixed-baud.md)).
- `SERIAL_USART_TIMEOUT` is 5 ms, for each wait on the serial FIFO.
- Every split data frame carries the fork's frame CRC8 by default
  (`SPLIT_TRANSPORT_CRC`); `NOAH_SPLIT_CRC=no` builds without it. The build
  needs the fork's `QMK_SPLIT_TRANSPORT_CRC_VERSION` 1 and fails, naming the
  opt-out, without it. Both halves must match: a mixed pair fails at the
  handshake, before any data is sent.
- With the CRC, a write the slave half rejects is not committed; it reports
  the drop in the next handshake and the master half resends it on the next
  scan. A read the master half rejects is retried. The matrix checksum poll
  carries no CRC; the data read a changed checksum triggers does.
- A frame larger than the CRC staging buffer fails the build.

Held by: `tests/host/run_split_transport_build_tests.sh` (`split_transport_build_test.py`), `tests/host/run_split_frame_crc_tests.sh` (`split_frame_crc_test.py`, `split_frame_crc_harness.c`).

## Activity admission

QMK's split activity sync carries the slave half's matrix, encoder and
pointing activity timestamps so the lighting timeout ends at the same moment
on both halves. With coalescing, on by default
(`NOAH_SPLIT_ACTIVITY_COALESCE=no` builds without it), the master half sends
a changed snapshot only when it is due:

- The first delivery, QMK's forced resend, and a change after at least 32 ms
  without one are sent at once.
- Sustained activity sends the latest snapshot every 32 ms, and a final
  change is sent by that deadline even after input stops.
- The bound is the shortest nonzero lighting timeout: the compiled
  `RGB_MATRIX_TIMEOUT` or the profile's lighting timeout setting
  ([settings-domain.md](profile-wire-v1/settings-domain.md)). A timeout of
  64 ms or less sends every change at once, as does a snapshot within 32 ms
  of the last sent one's timeout. A changed timeout sends at once. A timeout
  of zero disables that consumer.
- Timeout age uses the same numerical maximum of the three timestamps as
  QMK's receiver, and scheduling uses unsigned elapsed time, so a timestamp
  wrap behaves as it does in QMK on both halves.
- A failed send leaves the snapshot due; only a successful one advances the
  sent snapshot and its clock. QMK's own retries stay in charge.
- Coalescing changes nothing else: keys are still read every scan, and the
  activity wire bytes and transaction ids are QMK's. A new consumer of the
  activity timestamps joins this timeout rule before it relies on them.

The hook is the fork's `QMK_SPLIT_ACTIVITY_POLICY_VERSION` 1:
`split_activity_sync_should_send` decides, `split_activity_sync_sent`
reports the result, and the fork's weak default sends every changed
snapshot. Coalescing against a fork without the hook fails the build, naming
the opt-out.

Held by: `tests/host/run_split_activity_tests.sh` (`split_activity_policy_test.c`, `split_activity_adapter_test.c`, `split_activity_qmk_test.py`).

## Runtime state packets

Four split RPCs carry the master half's runtime state the slave half's
lighting draws from. Both halves run one build, so each packet is that
build's packed C struct, sent whole.

| RPC | Packet |
| --- | --- |
| `PUT_SPLIT_RUNTIME_BASE_SYNC` | auto-mouse fade progress, active and locked pointing slot, whether the active mode shows its own light (a mode its linked layer started may not, D-F62), the pointing mode's owning half and keys (when `RGB_POINTING_MODE_ACTIVE_HALF_ENABLE`), key preview layer |
| `PUT_SPLIT_COMBO_FEEDBACK_SYNC` | combo feedback underlay and overlay key bitmaps |
| `PUT_SPLIT_KEY_FEEDBACK_SEMANTIC_SYNC` | key feedback flash visibility and semantic map |
| `PUT_SPLIT_KEY_FEEDBACK_BRANCH_SYNC` | key feedback owner map and tap branch map |

- A packet is sent when it changed, when forced, and as a heartbeat: every
  250 ms while it carries active state, every 1,000 ms otherwise. A locked
  pointing mode sends no fade progress.
- A send pass stops at its first failed RPC. Until the retry delay passes
  (50 ms, doubling to 1,000 ms) no packet is built; the next pass is a
  recovery probe that sends the latest state. A role change clears the
  backoff.
- The slave half drops a packet shorter than its struct; a longer one is
  logged and its leading bytes applied. It publishes each domain under its
  own generation; readers copy a whole domain or keep what they had.

Held by: `tests/host/run_split_runtime_sync_tests.sh` (`split_runtime_sync_test.c`).

## Split transaction recorder

`NOAH_SPLIT_DIAGNOSTICS=yes` builds a recorder of the master half's split
transactions (`SPLIT_TRANSACTION_DIAGNOSTICS`), independent of coalescing;
it needs the instrumented fork. Without the flag no recorder state or probe
exists.

- Each attempt records its transaction id, success, the attempted frame
  bytes (2 plus the request and reply sizes, including each frame's CRC byte;
  a failed attempt counts its whole frame), and its elapsed time on the RP2040
  realtime counter in µs. Retries are separate attempts. The span covers the
  transaction's execution only, and is wall time, not CPU use.
- The recorder is armed explicitly, runs for 10 s, then freezes. A
  transaction crossing the deadline is included and extends the duration.
  Nothing is logged or written during capture.

Profile Wire channel 0, value `0x0A`, 32-byte reports:

| Offset | Request |
| ---: | --- |
| 0 | `7` reads, `8` arms |
| 1 | `0` |
| 2 | `0x0A` |
| 3 | correlation, nonzero |
| 4 | page; arming takes page 0 only |
| 5–31 | zero |

- The reply echoes bytes 0–4; byte 5 is the status: `0` OK, `1` malformed,
  `2` unknown page (past the transaction count), `3` unavailable (on the
  slave half, or a transaction page before the recorder froze).
- Arming clears and starts capture and changes only volatile diagnostic
  state; its reply carries status only.
- A successful read has payload length 25 at byte 6 and format `2` at byte 7.

| Offset | Page 0 | Page n ≥ 1, transaction n − 1 |
| ---: | --- | --- |
| 8 | transaction count | transaction id |
| 9 | armed | attempts (4) |
| 10 | frozen | |
| 11 | duration in µs (4): frozen, or so far | |
| 13 | | failures (4) |
| 15 | `PUT_ACTIVITY` id | |
| 17 | | attempted bytes (4) |
| 21 | | total µs (4) |
| 25 | | longest µs (4) |
| 29 | | CRC failures (2): writes the slave half dropped, reads the master half rejected; zero without the frame CRC |

Every other byte is zero.

Held by: `tests/host/run_split_activity_tests.sh` (`split_diagnostics_test.c`).

## Cadence recorder

`NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS=yes` builds a recorder of the master
half's main loop. It keeps 30 one-second windows since boot, oldest first,
without arming, and times each loop stage exclusively: a nested stage's time
is not also counted in the stage it interrupts. Each loop counts one matrix
scan; a pointing poll is one pointing-task run, an upper bound on USB mouse
reports.

| Wire order | Stage | From → to |
| ---: | --- | --- |
| 1 | `matrixScan` | `keyboard_task` entry → `matrix_scan_user`: local matrix, debounce, QMK's split transactions |
| 2 | `durableIo` | the durable I/O scan: the profile store and its split RPCs |
| 3 | `keyRuntime` | combo origins, key runtime scan, macro engine |
| 4 | `splitSync` | runtime split sync RPCs |
| 5 | `qmkTasks` | `matrix_scan_user` return → pointing hook: key event dispatch, `quantum_task`, `rgb_matrix_task`, the keyboard's pointing code; also the pointing stages on a loop where the pointing task does not run |
| 6 | `processRecord` | `process_record_user` and its finalize, wherever they run |
| 7 | `rgbRender` | the render in `rgb_matrix_indicators_advanced_user` |
| 8 | `sensorRead` | the pointing driver's report read |
| 9 | `pointingTask` | `pointing_device_task_user` |
| 10 | `pointingReport` | pointing hook return → `keyboard_task` end: auto-mouse, USB mouse report, mousekey and LED tasks |
| 11 | `outsideKeyboardTask` | between `keyboard_task` calls: USB events, Raw HID, deferred executors, housekeeping |

The loop and sensor boundaries come from `lib/compat/qmk_loop_stages.c`,
which replaces QMK's weak `protocol_keyboard_task` and times the driver's
`get_report`; the userspace hooks mark the rest.

Profile Wire GET (`8`), channel 0, value `0x03`: byte 3 a nonzero
correlation, byte 4 the page, bytes 5–31 zero, else status `1` malformed; a
page past the last is `2` unknown page. A reply has status at byte 5, payload
length 25 at byte 6 and the payload from byte 7. Multi-byte values are
little-endian.

| Payload offset | Page 0, metadata |
| ---: | --- |
| 0 | format, `2` |
| 1 | page count, 121 |
| 2 | completed windows |
| 3 | sequence: windows stored since boot (4) |
| 7 | window length in µs, 1,000,000 (4) |
| 11 | gap histogram upper bounds in µs, 5 × 2: 1,000, 1,250, 1,500, 2,000, 5,000 |
| 21 | started |
| 22 | timed stages, 11 |
| 23 | stages per stage page, 5 |
| 24 | stage total unit as a power of two in µs, 4 (16 µs) |

| Payload offset | Pages 1–30, one per window |
| ---: | --- |
| 0 | sequence (4) |
| 4 | window index, `0xFF` when absent |
| 5 | longest pointing-poll gap in µs (4) |
| 9 | matrix scans (2) |
| 11 | pointing polls (2) |
| 13 | gap histogram counts, 6 × 2 |

Pages 31–120 hold three stage pages per window, in window order: sequence
(4), window index, then for each of up to five stages its total in stage
units (2) and its longest single loop in µs (2), both saturating. Slots past
the last stage are zero. A reader checks every page's sequence against page
0's: a window stored between them shifts the indices.

Held by: `tests/host/run_runtime_diag_tests.sh` (`runtime_diag_test.c`).

## Capturing

`node tools/capture-split-diagnostics.cjs` arms the transaction recorder,
waits without device requests, then reads the frozen pages as JSON. In a
cadence build it reads the cadence recorder's window count just before
arming and just after, and summarises the windows wholly between them, stage
times included. It also estimates the poll rate with measured split time
removed, a comparison between builds rather than a prediction. Install its
dependencies with `npm ci --prefix tools`. Capture with the procedure in
[`measurements/pointing-cadence/`](../../measurements/pointing-cadence/README.md),
which also keeps the recorded sets.

Held by: `tests/host/run_split_activity_tests.sh` (`split_diagnostics_tool_test.cjs`).
