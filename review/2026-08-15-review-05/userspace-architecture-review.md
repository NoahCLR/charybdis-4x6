# Synthetic-Key Ownership Architecture Review

Finding 09 closed in `review/2026-08-15-review-04/`, which is immutable. This
next sortable folder records Finding 08's physical/synthetic report-ownership
contract and its closure state.

## Review Scope

- Define aggregate ownership for every literal report domain currently emitted
  by userspace.
- Introduce owner-scoped, idempotently releasable synthetic leases.
- Make physical event suppression authoritative at the pre-process/preflight
  seam.
- Make macro-local hold balance strict before migrating macro holds to leases.

## Prior Finding Status

| Prior finding | Status | Code and enforcement evidence |
| --- | --- | --- |
| Finding 08: aggregate synthetic ownership | resolved | `action/owned_keycode.c` owns physical/managed literal usage counts and leases; `key/runtime/process.c` observes physical events before QMK default handling; `key/runtime/preflight.c` suppresses aggregate-owned defaults; held actions, macro holds/chords, and PD arrow selection retain exact leases. Ownership, scenario, macro, PD, boundary, full-host, target-build, and stack gates pass. |
| Finding 09: pending-release FIFO rollover | resolved | Closed review 04, direct queue runner, full host suite, target build, and stack gate. |
| Finding 01: target stack safety | resolved | The final Finding 08 target gate retains the 1,920 B reviewed main-process budget and 768 B split-thread budget. |

## Landed Design

`owned_keycode.c` classifies a literal action once into a modifier mask and an
optional native 8-bit report usage. It supports the basic keyboard, system,
consumer, and mouse domains accepted by the pinned QMK `register_code()` and
`unregister_code()` implementation. QK_MODS actions share the same basic usage
counts and delegate modifier ownership to `keyboard_mod_ownership.c`.

Physical and managed counts jointly determine report visibility. Managed
acquisition emits a QMK press only when no physical or managed owner already
keeps the usage live. Managed release emits a QMK release only after the last
managed and physical owner is gone. The real pre-process hook records physical
ownership before QMK default handling; preflight then suppresses a physical
default transition while managed ownership is authoritative. A handled-key
release bypasses that suppression only when its actual press token marks it as
handled.

Persistent producers retain `owned_keycode_lease_t` values. Acquisition
prevalidates modifier capacity before changing the basic count. Release
prevalidates every recorded component, is idempotent after successful release,
and cannot derive a different action at teardown. Held-action slots use a
packed lease-managed marker, macro holds keep leases aligned with their local
balance entries, macro chords unwind acquired leases in reverse order, and the
PD arrow handler retains one Shift lease.

Unsupported 16-bit actions remain on the established action lifecycle. The
legacy unscoped register/unregister entry points exist only for the literal
fallback in `action_kind_dispatch.c`; a source guard rejects new callers. Raw
QMK register/unregister calls are limited to that compatibility fallback and
the aggregate owner itself.

## Failure and Reset Policy

- Saturated physical, managed, or modifier counts fail closed and never wrap.
- Release underflow is diagnosed before any component is mutated; no recovery
  path blindly unregisters a possibly physical key.
- A failed held-action literal acquisition remains a tracked, inactive lease so
  teardown cannot fall back to an unscoped release.
- Macro orphan key-up is rejected during authored parsing, QMK/VIA decoding,
  and whole-IR preflight. Playback abort cleanup releases only leases that the
  active macro acquired.
- `noah_runtime_reset_for_test()` clears the aggregate ownership state through
  a weak reset seam overridden by `owned_keycode.c`. Production teardown stays
  owner-driven; it does not erase physical ownership behind QMK's report.

## Resource Tradeoff

The compact usage range ends at `QK_MOUSE_ACCELERATION_2`, so physical and
managed arrays cover 224 entries rather than all 256 byte values. The linked
ownership state is 456 B. Per-held-action leases and their packed marker grow
`noah_runtime_singleton` from 19,752 B to 20,000 B (+248 B), and the PD arrow
lease grows from 1 B to 4 B (+3 B). Explicit ownership storage therefore costs
707 B.

For comparison, the exact baseline commit linked at 142,364 B text and 245,840
B BSS. The final instrumented image linked at 143,524 B text and 245,592 B BSS.
The lower total linked BSS is an LTO/link result and is not used to hide the
707 B symbol-level ownership cost.

## Verification and Closure Verdict

The final tree passed:

- focused ownership, held-action, macro, VIA, PD-mode, hook, scenario, QMK
  contract, and feature-gate runners;
- `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh`;
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`;
- `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_checks.sh`;
- `git diff --check`.

The fresh stack report measured a 1,808 B worst reviewed main-process path
against a 1,920 B budget and a 328 B worst split path against a 768 B budget.

**Closure verdict: resolved.** The current code, mechanical boundaries, tests,
target measurements, and maintainer documentation agree. This folder is a
closed architecture snapshot; later ownership work belongs in a new sortable
review folder.
