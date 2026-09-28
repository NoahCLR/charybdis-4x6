# Parked: default build with activity coalescing, then checksummed syncs

Parked on 2026-09-28. Two follow-ups to the split work in `00b9723e`; its
measurement plan is [split transport optimization](split-transport-optimization.md)
and the contract is [split activity sync](../architecture/split-activity-sync.md).
When both are resolved, fold what still constrains the build into those and
delete this file (D-L07).

1. A plain `qmk compile` and `tools/build-firmware-pair.sh` build activity
   coalescing, with no extra checkout or environment.
2. QMK's split syncs that carry no checksum get one, so a garbled message is
   refused instead of shown.

The split link stays at QMK's default 230,400 baud. 460,800 was measured and
removed (D-L43); it is not part of either follow-up.

## 1. Coalescing in the default build

### Where it stands

Coalescing is opt-in: `NOAH_SPLIT_ACTIVITY_COALESCE=yes`, selected in
`users/noah/lib/compat/qmk_split_transport.mk`. It needs the QMK activity
admission hook, which only one checkout of the fork has:

| Checkout | Branch | Hook |
| --- | --- | --- |
| `../bastardkb-qmk` (`qmk config user.qmk_home`) | `sol` at `aac9f637` | no |
| `../bastardkb-qmk-split-optimize` (worktree) | `feat/split_sync_optimize` at `68899602` | yes |

`68899602` is one commit on top of `aac9f637`, so `sol` can fast-forward to it.
The same change is kept as `tools/qmk-patches/split-activity-policy.patch`.

Coalescing fails closed: without the hook, `qmk_split_activity.c` stops with
"Activity coalescing requires the QMK split activity policy hook". It cannot
ship silently inert.

### Known breakage while parked

`sh tests/host/run_all_host_tests.sh` fails in `split_activity_qmk_test.py`
(`send_if_data_mismatch`, `split_shmem` undeclared). The test compiles QMK's
own `transactions.c` from `QMK_ROOT`, which defaults to the unpatched
`../bastardkb-qmk`. Run it with the patched checkout as `QMK_ROOT`, or run the
runners after it by hand, until this is resolved.

### Building the coalescing pair today

```sh
Q=/Users/noah/dev/charybdis/bastardkb-qmk-split-optimize
QMK_HOME=$Q QMK_ROOT=$Q QMK_USERSPACE=$PWD \
  NOAH_SPLIT_ACTIVITY_COALESCE=yes sh tools/build-firmware-pair.sh
```

Artifacts are suffixed `_activity`.

### Results so far

- Coalescing plus 460,800 baud gave about 600 Hz of mouse reports against about
  400 Hz originally; 460,800 has since been removed for link errors. The rate
  with coalescing at 230,400 is not measured yet.
- Coalescing at 230,400 (`2_…_activity`, from `1089c397`): Applies save on both
  halves, and no split transport failure across three Applies.
- Hardware acceptance from the optimization plan is still pending: RGB
  wake/sleep and timeout boundaries, left keys, chords, holds, pointing modes,
  peer reboot, and error behaviour.

### Options

1. **Fast-forward the fork's `sol` to `68899602`** and push it. The ordinary
   checkout then has the hook, the host test passes, and the default in
   `qmk_split_transport.mk` can switch coalescing on, keeping the opt-out.
   Anyone building against an older fork gets the fail-closed error, so the
   fork commit becomes a stated build requirement (README setup).
2. **Apply the patch at build time** from `tools/qmk-patches/`. Rejected unless
   option 1 fails: the build would modify a sibling repository and could
   double-apply or drift from the fork.

Leaning: option 1 once hardware acceptance passes.

### To pick up

1. Measure the mouse report rate with coalescing at 230,400.
2. Finish the acceptance list in the optimization plan.
3. With the user's go-ahead, fast-forward `../bastardkb-qmk` `sol` to
   `68899602` (sibling repository) and push the fork.
4. Default coalescing to on in `qmk_split_transport.mk`, keeping
   `NOAH_SPLIT_ACTIVITY_COALESCE=no`; update
   `tests/host/split_transport_build_test.py`, the pair script's artifact names,
   README setup and `split-activity-sync.md`.
5. Run the full host suite with the ordinary checkout, `qmk compile`, the pair
   build, and the memory and stack gates.

## 2. Checksums for the split syncs

### Why

At 460,800 baud the link garbled messages. The profile copy survived because
its frames carry a CRC8 and a garbled request is now retried (`1089c397`), but
QMK's own syncs show whatever arrives: the other half's lighting flickered.
At 230,400 no garbling was seen, but a noisy cable, or the other half stalled
by a flash write, can still corrupt a message.

### To find out first

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
