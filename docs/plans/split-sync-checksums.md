# Plan: checksums on every split frame

A follow-up to the split work in `00b9723e`; its measurement plan is
[split transport optimization](split-transport-optimization.md) and the
contract is [split activity sync](../architecture/split-activity-sync.md).
When done, fold what still constrains the firmware into a split transport
spec under `docs/architecture/` and delete this file (D-L07).

Goal: a garbled split frame is refused instead of used, with no extra round
trip on the link.

The split link stays at QMK's default 230,400 baud. 460,800 was measured and
removed (D-L43); it is worth measuring again only once this is in place.

## How the link works

The right half is master and drives every exchange. A transaction is one
exchange in `platforms/chibios/drivers/serial_protocol.c`:

1. master sends the transaction id;
2. slave echoes it XORed with `NUM_TOTAL_TRANSACTIONS`, the only check today;
3. data flows one way: master → slave for a write, slave → master for a read.

Nothing follows a write's data, so the master counts a garbled write as
delivered. The slave receives straight into `split_shmem`, the shared memory
its main loop reads, and uses whatever arrived. The master's own copy of that
memory is what it last sent, so it resends only when its state changes again
or a forced resend is due.

| Sync | Direction | Checked today | Garbled, it is used until |
| --- | --- | --- | --- |
| Left-half matrix | slave → master | CRC, polled every scan before the data | never used: the master keeps the last good matrix |
| Right-half matrix mirror (reactive RGB) | master → slave | no | next change or forced resend, ≤ 100 ms |
| Layer state, RGB matrix config, activity timestamps | master → slave | no | forced resend, ≤ 100 ms; the lighting flicker at 460,800 |
| Sync timer | master → slave | no | next sync, ≤ 100 ms; the slave's clock jumps |
| Runtime syncs: base, combo, semantic, branch (`runtime_sync.c`) | master → slave, RPC | RPC header only | next change or heartbeat, ≤ 250 ms active, ≤ 1 s idle |
| VIA keymap mirror (`qmk_via_split_mirror.c`) | master → slave, RPC | no | writes the garbled bytes to the peer's EEPROM; durable reconciliation repairs it |
| Profile Split v1, VIA durable sync | RPC, both ways | CRC8 and version in our frames, retried | never used |

Pointing crosses no link: the sensor is on the master (`POINTING_DEVICE_RIGHT`).

## Design

One change in the fork's serial protocol covers every row, including the QMK
syncs userspace cannot reach.

### Frame CRC

- Every data frame, in either direction, carries one trailing CRC8 over the
  transaction id and the frame's bytes, using QMK's `crc8()`. Including the id
  means a frame cannot pass as another transaction's.
- The handshake stays one byte each way. Its XOR constant changes when the CRC
  is compiled in, so a half without the CRC fails at the handshake: a mixed
  pair loses the link at once rather than through timeouts.
- Compiled in with `SPLIT_TRANSPORT_CRC` in the fork, so a build without it
  behaves as QMK does. `QMK_SPLIT_TRANSPORT_CRC_VERSION 1` is the contract
  userspace asserts, as it does for the activity hook.

### Slave: drop a bad write

- Receive the frame and its CRC into a static staging buffer, check it, and
  only then copy it into `split_shmem`, all inside the lock
  `react_to_transaction` already holds. The main loop never sees a bad or
  half-copied frame.
- On a bad CRC: leave `split_shmem` unchanged, skip the slave callback, clear
  the receive queue as a failed transaction does today, and count the drop.
- The slave keeps showing the previous value until the master's next send.
  The staleness bounds in the table become "shows the previous value" instead
  of "shows garbage". The drop is reported to the master in the next
  handshake (below).

### Master: reject a bad read

- Receive a read into staging, check it, then copy it into `split_shmem`. On a
  bad CRC the transaction returns false, and QMK's existing retry
  (`transaction_handler_master`, up to 10 attempts) repeats it. No extra
  traffic while the link is clean.
- A frame that keeps failing counts toward the transport's disconnect, as a
  timeout does today. A link that noisy should read as disconnected.

### Drop report in the next handshake

The slave's echo also reports whether the data of the transaction before it
failed its CRC: the echo is `id ^ K` for a clean previous frame and
`id ^ K ^ F` for a dropped one, where `K` is the CRC-build constant and `F` a
bit no valid id reaches. Transactions run one at a time, so the master knows
which transaction the report is about. This adds no byte:

- The diagnostics recorder counts drops per transaction id on the master,
  where it already runs. No slave-side counter or readout is needed.
- The report is lost when the echo carrying it is garbled; that transaction
  fails anyway. Counts can therefore miss a drop, never invent one.
- Two echo values are valid instead of one, a negligible loss of handshake
  checking.
- Reads need no report: the master checks their CRC itself.

### Resend on a reported drop

Waiting up to 100 ms for QMK's forced resend is visible in the lighting, so a
reported drop is resent on the next scan:

- The fork keeps a resend-due flag per transaction id. A drop report sets it
  for the transaction it names; a successful send of that id clears it.
- `send_if_condition`, which `send_if_data_mismatch` goes through, treats the
  flag as a change. That covers layer state, RGB matrix config, the
  right-half matrix mirror and LED state.
- Handlers with their own send logic check the flag too: activity (a reported
  drop counts as a forced send, bypassing admission), the sync timer and mods.
- RPC needs no flag: its early stop hands the failure to the caller's retry.
- Not by spoiling the master's copy of the last-sent value: if the real state
  happened to equal the spoiled copy, the resend would never happen.
- A lost report falls back to the forced resend (≤ 100 ms). A noisy link adds
  resends only for frames that actually dropped.

The repair lands about one scan after the drop, a few milliseconds instead
of up to 100 ms.

### The lighting frame is the yardstick

Apart from the left matrix and the durable profile and VIA traffic, every
master → slave sync exists to feed the left half's lighting. The slave draws a
frame every `RGB_MATRIX_LED_FLUSH_LIMIT` (32 ms), computed in slices over a
few loop passes, and synced state is visible only when a frame is drawn. A
drop repaired on the next scan therefore usually never reaches a frame; at
worst one frame shows the previous value, never garbage. That is the
acceptance bar: a reported drop is repaired before the slave's next frame.
Nothing faster than one-scan repair is needed.

### RPC sequence

`transaction_rpc_exec` is four transactions: info, request data, execute,
response. Each is checked on its own; the sequence also needs:

- A drop of info or request data is reported in the next step's echo, so the
  master stops the sequence there and `transaction_rpc_exec` returns false.
  The runtime syncs' retry backoff then resends at once instead of waiting for
  a heartbeat. A drop of execute's own data is reported on the response step,
  with the same result.
- As a backstop, if info or request data was dropped, the slave skips the
  following execute instead of running the callback on the previous payload.
  A valid info starts a new sequence.
- A skipped execute zeroes the response buffer, so the master cannot read the
  previous reply as the answer. Profile Split v1 and VIA sync replies carry a
  version byte of 1, so an all-zero reply is refused and their retry runs.
  Runtime syncs and the VIA mirror have no reply; they recover by their
  resend.
- A dropped info leaves the slave's request length stale; the request frame
  then fails its CRC or times out, and the execute is skipped. Test this case
  explicitly.

### Userspace

- `qmk_split_transport.mk` enables `SPLIT_TRANSPORT_CRC` by default, with
  `NOAH_SPLIT_CRC=no` for the comparison pair, as coalescing does. The pair
  script and its build test follow the same pattern.
- A compat header asserts the contract version, so an older fork fails the
  build with the fork commit it needs.
- End-to-end CRCs in Profile Split v1 and VIA sync frames stay. They also
  catch buffer and logic faults, not only the wire.

## Cost

- One byte per data frame: 43.4 µs at 230,400 baud, nominal. No transaction
  and no turnaround is added.
- The largest steady cost is the left-matrix checksum poll every scan: 3 bytes
  becomes 4, about 43 µs per scan. Measure it with the diagnostics build. If
  it matters, exempt transactions whose contents are already checked end to
  end: the matrix checksum and data, and the RPC info block.
- Static staging buffers sized to the largest frame in the transaction table,
  on each half. Account them per half with fresh linked numbers, following
  [memory-budgets.md](../architecture/memory-budgets.md); check the slave
  thread's stack against its 1,024-byte working area.

## Open questions

- Whether the exemption above is needed at all, answered by measurement.

## Steps

1. Fork, on `sol`: frame CRC, staging, handshake constant and drop report in
   `serial_protocol.c`; per-id drop counts in the transaction diagnostic; the
   resend-due flag in `send_if_condition` and the activity, sync timer and mods
   handlers, and the RPC early stop and sequence guard, in `transactions.c`;
   the contract define.
   Gated by `SPLIT_TRANSPORT_CRC`.
2. Host tests driving the fork's real `serial_protocol.c` over a fake
   half-duplex transport that can corrupt any byte:
   - a corrupted write is not committed and its callback does not run;
   - a corrupted read fails and the retry succeeds;
   - a corrupted write is reported in the next echo and counted against its
     id, and resent on the next scan for every write sync; a corrupted echo
     loses the report, fails that transaction, and the forced resend repairs
     it;
   - a corruption at each RPC step stops the sequence on the master, and the
     slave still skips the execute and zeroes the reply;
   - a stale request length after a dropped info;
   - a mixed pair fails at the handshake;
   - nothing changes on a clean link.
3. Userspace: make fragment default, opt-out, compat assert, pair script,
   build test; docs.
4. Measure with the diagnostics build, CRC against no CRC: per-scan split
   time, matrix poll time, report rate, drops and retries.
5. Hardware acceptance at 230,400: Apply, lighting, typing on the left,
   pointing modes, peer reboot.
6. Only then consider measuring 460,800 again.
7. Update the optimization plan: stage 2's candidate exchange no longer needs
   its own CRC field.

## Reviewed and kept

Before this plan, the rest of the split design was reviewed, to see whether
the checksum should come with a wider change. It should not:

- Master-driven synchronous transactions are QMK's model. Moving to an
  asynchronous transport is stage 3 of the optimization plan and only earns
  its complexity if measurements show report stalls.
- QMK's change-driven sends with a forced resend every 100 ms heal
  themselves. With the frame CRC they bound how long the slave shows a
  previous value.
- The left-matrix checksum poll every scan is the latency-critical path and is
  already safe.
- Activity coalescing reduces the heaviest steady traffic and is the default.
- The runtime syncs are idempotent snapshots with heartbeats and publication
  generations, the right shape for drop-on-bad. Their four-transaction RPC
  cost is efficiency, not correctness, and is stage 2 of the optimization
  plan.
- Profile Split v1 and VIA durable sync have their own CRC, generations,
  retries and reconciliation. They stay as they are.
- The VIA mirror is best-effort by design and repaired by reconciliation; the
  frame CRC removes its one real flaw, writing garbled bytes.
- The right-half matrix mirror feeds the reactive RGB effects on the left and
  stays.
