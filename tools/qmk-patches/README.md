# QMK split activity hook

`split-activity-policy.patch` applies to QMK fork commit
`aac9f637ee4ad99fe8f962372b9dfd56be538703` (0.32.5 plus the auto-mouse helper).
It adds a default-preserving activity admission hook with successful-send state
and compile-gated PIO transaction instrumentation. It changes no wire format.

Use an isolated QMK branch/worktree and preserve unrelated fork changes:

```sh
git -C "$QMK_ROOT" apply --check /path/to/charybdis-4x6/tools/qmk-patches/split-activity-policy.patch
git -C "$QMK_ROOT" apply /path/to/charybdis-4x6/tools/qmk-patches/split-activity-policy.patch
```

The prepared checkout is `/Users/noah/dev/charybdis/bastardkb-qmk-split-optimize`
on `feat/split_sync_optimize`. It already contains the patch; do not apply it twice.
The corresponding fork commit is `6889960271bacd50dffac1d1d1f1bb5335ccddb5`.
Use both `QMK_HOME` and `QMK_ROOT` for CLI/test consistency when selecting this
checkout, and `QMK_USERSPACE` for the userspace checkout.

See `docs/architecture/split-activity-sync.md` for the admission and diagnostic
contracts. The host suite's actual QMK activity-handler test requires this patch.
