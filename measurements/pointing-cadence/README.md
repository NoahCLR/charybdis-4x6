# Pointing cadence

How often the right (master) half runs its loop and polls the sensor, where
the loop's time goes, and what the split link costs. Captured with
`tools/capture-split-diagnostics.cjs` from two on-device recorders:

- the **split transaction recorder** (`NOAH_SPLIT_DIAGNOSTICS=yes`): attempts,
  failures and elapsed time per split transaction id over a ten-second capture;
- the **cadence recorder** (`NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS=yes`):
  pointing polls, matrix scans, poll-gap histogram and, from recorder format 2,
  exclusive time per loop stage, in one-second windows.

Both are specified in
[`docs/architecture/split-activity-sync.md`](../../docs/architecture/split-activity-sync.md),
including what each loop stage covers. Pointing polls are pointing-task runs,
an upper bound on the USB mouse reports; the host's own report rate is the
final word.

## Sets

| Set | Firmware | Recorder | Purpose |
| --- | --- | --- | --- |
| [2026-09-28-stage-timing](2026-09-28-stage-timing/README.md) | `ac9e9508`, coalescing on | format 2 | Attribute the loop to its stages |

## Capture procedure

Follow every step. Most of them remove a source of variation that the numbers
cannot reveal after the fact.

### 1. Build from committed source

1. Commit the change being measured. `git status --short` must show no changes
   under `users/`, `keyboards/` or `tools/`. Note the commit
   (`git rev-parse --short HEAD`).
2. Note the QMK commit and whether its tree is dirty:
   `git -C ../bastardkb-qmk log --oneline -1` and
   `git -C ../bastardkb-qmk status --short`.
3. Build the side-specific pair with both recorders:

   ```sh
   NOAH_SPLIT_DIAGNOSTICS=yes NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS=yes \
     sh tools/build-firmware-pair.sh
   ```

   Add only the variable under test, for example
   `NOAH_SPLIT_ACTIVITY_COALESCE=no`. The artifacts land in
   `../builds/<branch>/` as `<n>_charybdis_right_diagnostic_cadence.uf2` and
   `<n>_charybdis_left_diagnostic_cadence.uf2`; note `<n>`.

### 2. Prepare the keyboard and host

1. Flash both halves from the same build number. Plug the USB cable into the
   right half; it is the master and carries the sensor.
2. Use the profile the firmware was built with. After flashing, make no Apply,
   restore or VIA edit for the whole set. Record any other profile in the set
   record, and keep it for the whole set.
3. Plug directly into the computer: the same port and cable every time, no hub,
   dock or KVM. Record the host and its OS.
4. Quit Charybdis Ark, VIA and anything else that opens the keyboard's raw HID
   interface. Their traffic runs inside the loop being measured.
5. Leave the lighting as the profile sets it, and awake. The lighting sleeps
   after 15 minutes without input and its render is about a sixth of the
   loop; if the LEDs have gone dark, nudge the ball before capturing.
6. Wait at least 30 seconds after the halves boot, or after any replug. Boot
   runs profile reconciliation and VIA sync between the halves, and the
   recorder needs complete windows.
7. Do not unplug, reflash or reset between the captures of a set. If you
   have to, start a new set.

### 3. Capture

Run one capture per workload and run, writing straight into the set folder:

```sh
node tools/capture-split-diagnostics.cjs > measurements/pointing-cadence/<set>/idle-1.json
```

The capture lasts 10.5 seconds from the `Capture armed` line; the command then
reads the results back and exits. Start the workload before running the command
and hold it until the command returns.

| Workload | File | What to do |
| --- | --- | --- |
| Idle | `idle-<run>.json` | Hands off both halves and the ball. |
| Motion | `motion-<run>.json` | Roll the ball continuously in steady circles, about one per second, without pausing. |
| Motion and typing | `motion-typing-<run>.json` | As motion, and with the other hand type steadily on the left half: plain letter keys, a few per second. No modifiers, thumb keys, layer keys or combos. Type into a scratch text field. |

Capture each workload three times (`-1`, `-2`, `-3`), cycling through the
workloads rather than repeating one three times in a row. A single run per
workload is a spot check; say so in the set record.

### 4. Check each capture before keeping it

Delete a capture and repeat it if any of these fail:

- The command exited with status 0 and its JSON has no `cadence.unavailable`.
- `cadence.windows` is at least 9.
- Format 2 recorder: `cadence.coverage` is between 0.98 and 1.02.
- `metadata.transactionCount` matches the id table below. If it doesn't, the
  build's split features differ; regenerate the table (below) before reading
  the transactions.
- Failed transactions are a handful at most. Dozens mean an unhealthy link:
  reseat the TRRS cable, wait 30 seconds and capture again.
- For idle and motion, the poll rate's min–max across windows is within a
  few percent of its mean. A wider spread means the workload wasn't steady or
  something else ran on the host.
- Runs of the same workload agree within about 3% on polls per second. If not,
  capture more runs and find out why before recording the set.

A longest poll gap above about 50 ms is a failed scan's retries. It is valid
data; note it in the set record.

### 5. Record the set

1. Name the folder `YYYY-MM-DD-<label>`, the capture date and what the set is
   for.
2. Write its `README.md` from the template below.
3. Generate the results tables and paste them in:

   ```sh
   node tools/summarize-cadence-captures.cjs measurements/pointing-cadence/<set>/*.json
   ```

4. Add the set to the index above, and put what it decides in the plan or
   spec it serves, linking the set.

## Comparing sets

- Compare sets that differ in one thing: the firmware change under test, or a
  build flag. Name it in both set records.
- Compare the same recorder format. Stage timing (format 2) costs 8–10% of the
  poll rate, so format 2 rates read low against format 1 rates.
- The split frame CRC is on by default; a `_no_crc` pair is a different build.
  Its transaction table's attempted bytes include each frame's CRC byte, and
  CRC failures appear per transaction.
- Read stage shares as shares of the loop, not as absolute cost: the timing's
  own cost inflates every stage slightly.
- The `matrixScan` stage includes the split transactions in the transaction
  table; subtract their share for the local matrix scan.
- Compare distributions and gaps, not only mean polls per second: the gaps of
  5 ms or more are what a user feels.

## Split transaction ids

Captures number transactions; the numbering follows the build's split
features. For builds of this repository since `25dacb46` (22 transactions):

| Id | Transaction |
| ---: | --- |
| 0 | `GET_SLAVE_MATRIX_CHECKSUM` |
| 1 | `GET_SLAVE_MATRIX_DATA` |
| 2 | `PUT_MASTER_MATRIX` |
| 3 | `PUT_SYNC_TIMER` |
| 4 | `PUT_LAYER_STATE` |
| 5 | `PUT_DEFAULT_LAYER_STATE` |
| 6 | `PUT_RGB_MATRIX` |
| 7 | `GET_POINTING_CHECKSUM` |
| 8 | `GET_POINTING_DATA` |
| 9 | `PUT_POINTING_CPI` |
| 10 | `PUT_ACTIVITY` |
| 11 | `PUT_RPC_INFO` |
| 12 | `PUT_RPC_REQ_DATA` |
| 13 | `EXECUTE_RPC` |
| 14 | `GET_RPC_RESP_DATA` |
| 15–21 | userspace ids, carried by the RPC transactions above: runtime base, combo feedback, key feedback semantic and branch syncs, VIA keymap sync and mirror, profile split sync |

A runtime or durable RPC appears as one each of 11–14. To regenerate the
table for a build with different split features:

```sh
qmk compile --compiledb -kb bastardkb/charybdis/4x6 -km noah
python3 tools/split-transaction-ids.py
```

`--compiledb` rewrites `compile_commands.json` in this repository and in QMK.

## Set record template

```markdown
# <set name>

<One sentence: what this set is for.>

| | |
| --- | --- |
| Captured | YYYY-MM-DD, <who> |
| Userspace | `<commit>` <subject line> |
| QMK | `<commit>`; <clean, or what is dirty> |
| Build | `<exact build command>` → `<n>_charybdis_{right,left}_diagnostic_cadence.uf2` |
| Recorder | format <1 or 2> |
| Split link | 230,400 baud; activity coalescing <on/off> |
| Profile | <compiled profile, unchanged after flashing / other> |
| Host | <computer, OS>; <port and cable if noted> |
| Lighting | <profile default, awake / other> |
| Runs | <runs per workload> |

## Results

<summarize-cadence-captures.cjs output>

## Notes

<Anything unusual: deleted captures, failures, long gaps, deviations from the
procedure. Decisions go in the plan or spec, not here.>
```
