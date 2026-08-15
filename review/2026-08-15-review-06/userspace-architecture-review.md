# Combo-Origin Candidate Lifecycle Architecture Review

Finding 08 closed in `review/2026-08-15-review-05/`, which is immutable.
Finding 06 changes the QMK compatibility lifecycle, so it uses this next
sortable review folder.

## Review Scope

- Bound pending combo-origin candidate lifetime without losing legitimate
  delayed QMK output attribution.
- Retire overlap-dropped or otherwise disabled candidates before they consume
  feedback/capacity state.
- Correlate pending and active origins by combo identity and physical
  completion generation rather than output keycode alone.
- Keep the solution inside `users/noah/lib/compat/`; do not reimplement QMK's
  combo engine or change authored combo precedence.

## Prior Finding Status

| Prior finding | Status | Evidence |
| --- | --- | --- |
| Finding 06: combo-origin pending lifecycle | resolved | `qmk_combo_origin.c` uses exact generations, disabled/deadline reconciliation, conservative capacity refusal, and exact promotion/release. `run_qmk_combo_origin_tests.sh`, runtime init order, QMK contract checks, full host, ordinary firmware, and the explicit 280 B target stack path pass. |
| Finding 08: aggregate synthetic ownership | resolved | Closed review 05; this pass must preserve its physical pre-hook and runtime ordering. |
| Finding 01: target stack safety | resolved | Any reconciliation path added to process or scan must remain inside the reviewed target budgets. |

## Pinned QMK Contract

The pinned fork runs the relevant stages in this order:

1. `pre_process_record_user()` observes the physical event.
2. QMK `process_combo()` updates combo state, disables overlap losers, and may
   emit release-triggered combo output.
3. `post_process_record_user()` runs only if QMK keeps processing the original
   record, so it is not a reliable reconciliation seam for consumed combo
   members.
4. On each keyboard cycle, `matrix_scan_user()` runs before matrix changes are
   processed and `combo_task()` runs afterward.

For this profile, `COMBO_TERM` is 50 ms and no per-combo must-tap, must-hold, or
term callbacks are enabled. A timer-driven output becomes legal when QMK sees
elapsed time strictly greater than the term. Because userspace scan runs before
`combo_task()`, a pending candidate must survive the first scan that observes
the deadline crossed. If QMK does not emit in that same cycle, the next
reconciliation may expire it.

`combo_t.disabled` and `combo_t.active` are direct fields in the normal layout;
under `EXTRA_SHORT_COMBOS` they are bits `0x40` and `0x80` in `state`. Those
fork-specific reads remain centralized in this compatibility module and need
compile coverage for both representations.

## Intended Design

Each physical completion gets a nonzero generation. A pending entry is one
exact `(combo_index, generation)` and retains its captured keycode, owner,
completion time, and bitmap. Re-observing the same still-pressed completion is
idempotent; a new completion after release gets a new generation even for the
same combo index.

Reconciliation runs at the start of later physical observations and at the
userspace scan boundary. It retires disabled candidates immediately, preserves
active candidates until their emitted press consumes them, and applies a
two-observation deadline rule so `combo_task()` receives its final legal
opportunity. Empty slots are the only insertion targets; a full cache refuses
the new candidate instead of overwriting a live origin.

On emitted press, compatibility selects the newest exact combo-index candidate
when QMK state identifies one, promotes that exact generation into the active
cache, and retires only that pending entry. Release resolves and clears the
matching active generation. Keycode-only fallback remains conservative and
must not union or clear unrelated same-keycode candidates.

## Closure Bar

Closure required suppressed overlap, deadline/wrap behavior, same-keycode
identity, capacity refusal/recovery, feedback bitmap cleanup, reset, compile
variants, full host, firmware build, fresh stack evidence, and documentation
to agree. The final evidence below satisfies that bar.

## Final Architecture State

- `noah_qmk_combo_origin_observe_physical_key_event()` captures physical
  completion before QMK processing and allocates a generation that cannot
  collide with a live pending or active entry.
- `noah_qmk_combo_origin_scan()` reconciles disabled and expired candidates
  before key-runtime scan. The two-observation deadline preserves one complete
  QMK `combo_task()` opportunity after the legal wait is first crossed.
- `noah_qmk_combo_origin_normalize_record()` promotes one exact pending origin
  and caches the matching active footprint. Same-output combos remain separate;
  release selection uses the triggering physical member.
- A full pending cache refuses the new origin and increments diagnostics. No
  live candidate is silently evicted and no refused bitmap becomes pending
  feedback state.
- Normal and `EXTRA_SHORT_COMBOS` layouts are compared with the pinned fork.
  The timerless branch compile proves it does not invent a deadline where QMK
  permits indefinite wait-until-release behavior.

## Enforcement and Measurements

- `run_qmk_combo_origin_tests.sh`: suppression, delayed release output,
  deadline edges/wrap, capacity, exact identity/release, diagnostics, reset,
  normal/compact layouts, and timerless compilation.
- `run_runtime_init_order_tests.sh`: compatibility reconciliation precedes
  key-runtime scan.
- `run_qmk_contract_checks.sh`: real/stub normal and compact `combo_t` parity;
  pinned pre-hook, matrix-scan, and `combo_task()` ordering.
- Full host suite and ordinary firmware build passed.
- Fresh target stack path for candidate retirement is 280 B. The reviewed main
  maximum is 1,816/1,920 B; split is 328/768 B.
- Final instrumented target: 144,172 B text, 245,592 B total BSS. Exact new
  lifecycle storage surfaces are 96 B pending, 96 B active, 12 B diagnostics,
  and 4 B generation state.

## Closure Verdict

Finding 06 is **resolved**. Every closure-bar item passed on the reconciled
tree. Review 06 is closed and immutable.
