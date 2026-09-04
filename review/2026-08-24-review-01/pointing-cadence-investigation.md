# Pointing Cadence Regression — Investigation State

Status: **parked by owner decision on 2026-09-04**, unresolved. R-21 remains
open and blocking for Stage 06 closure.

This file exists so the next pass does not re-derive what has already been
ruled out. It records the measured symptom, the complete inventory of
steady-state work the current branch added, and which candidates were
eliminated with evidence rather than by inspection.

## Measured Symptom

Reported mouse report rate fell from **~450 Hz to ~300 Hz** on the
**live-edit engineering build** (`NOAH_LIVE_PROFILE_OWNER=yes`,
`NOAH_LIVE_PROFILE_MUTATION=yes`) relative to `refactor/aug` (`87f356cd`).

That is a main-loop period of 2.22 ms rising to 3.33 ms, so roughly
**1.1 ms of additional work per main-loop iteration**.

`POINTING_DEVICE_TASK_THROTTLE_MS` is `0` on both branches
(`users/noah/config.h`), so the pointing poll rate is the main-loop rate and
every microsecond added per iteration costs polls directly.

The magnitude is the most useful constraint on any future hypothesis: 1.1 ms
per iteration is three orders of magnitude above the per-iteration cost of
any individual candidate measured so far. A correct explanation has to
account for a millisecond, not for microseconds.

## Steady-State Work Added Since 87f356cd

Complete inventory of what newly runs on every cycle. None of these has been
shown to cost a millisecond; they are listed so the next pass can cost them
directly rather than rediscover them.

### Present in every build

1. `noah_qmk_durable_io_matrix_scan()` replaced the single
   `noah_qmk_via_split_sync_matrix_scan()` call in `noah_matrix_scan_user()`.
   It round-robins three steps — profile store, split mirror, split sync —
   and stops at the first that reports work, so an idle scan probes all
   three. Each idle probe is a flag or mailbox check.
2. `noah_matrix_slave_scan_user()` is new. The slave half now runs that same
   rotation every scan; `87f356cd` ran nothing in the slave scan hook.
3. RGB stage re-materialization. `RGB_MATRIX_LED_PROCESS_LIMIT` is
   `(58+4)/5 = 12`, so a frame is five batches and the stage pipeline runs
   five times per frame. `rgb_effective_config_capture_frame()` is correctly
   guarded to once per frame, but the consumers are not:
   `rgb_runtime_layer_stage_profile()` rebuilds all `LAYER_COUNT` colors,
   including `hsv_to_rgb()`, on every batch, where `87f356cd` read a
   `layer_rgb[]` table built once in `post_init`. The pd-mode, combo (twice),
   key-feedback, and automouse stages follow the same per-batch pattern.
   Roughly 30 accessor calls per batch, ~160 per frame.
4. Every `rgb_effective_config_*` accessor calls `frame_status()` first.
   Nothing caches the result across a frame or a batch.

### Present only in the live-owner build

5. `noah_profile_owner_scan()` runs on every matrix scan. In the steady
   `READY_*` state it enters `scan_running()`, whose three-slot round-robin
   steps the candidate transaction state machine even with no host activity.
6. `frame_status()` resolves through `copy_active()` once an owner is
   installed: a publication seqlock read plus two frame copies, per accessor
   call. With no owner installed it returns `COMPILED_FALLBACK` and the
   accessors read the compiled tables directly, so ordinary firmware does not
   pay items 4 and 6.
7. A second split RPC channel, `PUT_PROFILE_SPLIT_SYNC`.

## Eliminated With Evidence

Do not re-investigate these without new evidence.

| Candidate | Basis for elimination |
| --- | --- |
| Pointing task itself | `pd_runtime.c` is identical to `87f356cd` apart from the compile-gated cadence counter |
| Poll throttle change | `POINTING_DEVICE_TASK_THROTTLE_MS` is `0` on both branches |
| Split pointing round trips | Not enabled; the trackball is master-side only |
| Split reconciler steady state | Measured. `test_converged_steady_state_cost_is_bounded_by_poll_deadline` scans a converged pair across 5 s of elapsed time at 450 Hz: **5 exchanges, 0 EEPROM reads, 0 writes over 2250 scans**. Now enforced |
| EEPROM access cost in the render path | **The premise was wrong.** QMK's wear-levelling driver mirrors the whole 16 KiB logical EEPROM in RAM (`wear_leveling`, 16,392 B linked). `wear_leveling_read()` is a `memcpy` from that cache — "Only need to copy from the cache". `eeprom_read_block` does not touch flash on a read, so the reader-backed RGB view costs a few-byte RAM copy per accessor |
| VIA macro default reseeding | `via_macro_seed_scan_pending` is a one-shot flag cleared after one attempt regardless of success; there is no per-scan retry |
| Split mirror step | A mailbox drain that returns false when empty. It moved blocking EEPROM writes *out* of the split RPC callback into the scan, which is a correctness improvement |
| Layer key-LED map rebuild | Lost its early-out, but 4 of 5 authored layers already used `KEYS_MAPPED_ON_THIS_LAYER_ONLY`, it is dirty-flag guarded, and `keycode_at_keymap_location()` reads the compiled keymap from flash rather than dynamic EEPROM |

Items 3, 4 and 6 above remain genuine waste worth removing on their own
merits. They were **not** shown to explain the symptom: at ~31 frames per
second the arithmetic lands in the microseconds-per-second range, not
milliseconds per iteration.

## Not Yet Costed

- `noah_profile_candidate_transaction_scan()` on an idle host transaction.
- `noah_qmk_via_receiver_verify_tick()` and `noah_qmk_via_local_digest_tick()`
  under live-owner conditions. Both also exist on `87f356cd`, but the digest
  is restarted from more call sites now; a digest that never settles would
  read VIA storage every scan.
- Anything that blocks rather than computes. The magnitude points at a
  blocking or waiting operation, and no such operation has been found in the
  added code.

## Available Measurement Paths

- **Hardware bisect.** 25 commits from `87f356cd` to HEAD, about five flashes
  using the owner's existing report-rate measurement as the test. Declined by
  the owner on 2026-09-04.
- **On-device cadence recorder.** Landed compile-gated behind
  `NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS=yes` in `f2d68521`. It does not print
  or log: it keeps a 30 × 20 B RAM ring read back over the existing Profile
  Wire HID channel after the capture window. Its own hot-loop cost is two
  counter increments plus a 1 µs timer read per scan, and 640 B of `.bss`.
  Declined by the owner on 2026-09-04; the host-side decoder is tested and
  registered, the capture CLI is not committed.
- **Host timing harness.** The host tests already build the real RGB stage
  pipeline. Timing N iterations on both branches would give a hardware-free
  ratio for items 3, 4 and 6 specifically. Not built.

## Measured RAM State (R-07)

Recorded here because it was measured during this investigation and bears on
any fix that wants to spend RAM. Fresh builds at `8bf794cb`:

| | ordinary | live-owner |
| --- | ---: | ---: |
| `.bss` | 25,868 B | 28,820 B |
| `.data` | 22,996 B | 23,000 B |
| `.data` + `.bss` | 48,864 B | 51,820 B |
| Linked occupancy, unique banks | 56,328 B | 59,280 B |
| Free/core span at boot | 213,272 B | 210,320 B |

Against `tools/check_firmware_memory_budget.py`:

- `.bss` ≤ 26,000 B: ordinary has 132 B of slack. **The live-owner build is
  2,820 B over and fails.**
- `.data` + `.bss` ≤ 51,000 B: ordinary has 2,136 B of slack. **The
  live-owner build is 820 B over and fails.**
- Free/core span ≥ 204,800 B: both pass.

Physical SRAM is 270,336 B, so roughly 205 KB is unused. Exactly the
distinction R-07 warns about: there is ample hardware headroom and almost no
policy headroom. The largest single consumer is the wear-levelling RAM mirror
at 16,392 B in both builds; `runtime_owner` at 3,172 B is the entire
live-build delta.

The live-owner build not passing its own memory policy is an open Stage 06
item independent of the cadence regression.
