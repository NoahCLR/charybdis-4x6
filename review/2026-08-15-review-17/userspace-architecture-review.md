# Combined Firmware Closure Verification Review

Review 16 is closed and remains immutable. This post-implementation review
checks the complete `dev` to `sol` firmware result as one integrated system,
so it uses the next sortable review folder. The repository's
`prompts/closure-verification-review.md` template guided the pass.

The review covered key-event observation and ownership, handled-key
press/hold/release behavior, macro output, split worker publication, RGB frame
snapshots, pointing-mode residual work, VIA persistence, resource gates, and
the open hardware-verification record. No firmware source was changed during
this pass.

## Findings

### Must fix — Consumed handled keys are recorded as physical report owners

`noah_pre_process_record_user()` records every physical key in the basic-key
and modifier ownership ledgers before the runtime knows whether QMK's default
handler will be allowed to process that event
(`users/noah/lib/key/runtime/process.c:219-229`). When the handled-key stage
later consumes the event, `noah_process_record_user_finalize()` does not
rollback that provisional physical ownership
(`users/noah/lib/key/runtime/process.c:284-292`).

That is incompatible with the aggregate ownership rule. A managed basic-key
acquire deliberately skips `register_code()` while its physical count is
nonzero (`users/noah/lib/action/owned_keycode.c:89-102`). A handled hold that
emits the same basic usage as its source therefore sees a physical owner that
never reached the HID report and suppresses the only real key-down. The
authored profile contains this exact pattern for all shifted symbol holds and
Shift+Enter (`keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c:373-396`).

For example, holding `KC_ENT` records a physical Enter owner, consumes the QMK
Enter press, then acquires `S(KC_ENT)`. Shift is registered, but Enter is not.
The host receives Shift without Enter. A related modifier interleaving can
leave a report modifier asserted: a quick handled Shift press is counted as a
physical modifier even though it is consumed, another managed Shift owner may
then register and release it, and the consumed physical release does not
perform QMK's default modifier teardown
(`users/noah/lib/state/ownership/keyboard_mod_ownership.c:86-114,141-199`).

Existing tests cover direct ledger behavior and an unhandled physical-key
overlap, but not a handled same-basic hold or a consumed modifier interleaving.
Finding 08 is therefore **regressed**, and the branch is not ready for closure.

Required remediation:

1. Treat pre-process physical ownership as provisional.
2. Commit it only when `keep_processing` allows QMK default handling, or
   rollback it in the finalize hook when userspace consumes the event.
3. Preserve the pre-process visibility needed to suppress a default release
   while a managed owner remains active.
4. Add integration scenarios for a shifted-symbol hold, Shift+Enter hold, a
   handled pure modifier overlapping a managed modifier, and a handled
   `QK_MODS`/basic-key overlap.

### Must fix — Split worker publishes multi-field display state non-atomically

The slave RPC callbacks run in QMK's split worker context, but publish packet
fields directly into shared state one field or byte range at a time
(`users/noah/lib/split/runtime_sync.c:317-408`). The main RGB path copies those
same maps into its per-frame snapshot without a lock, generation check, or
double buffer (`users/noah/lib/rgb/core/rgb_runtime.c:66-115`). The frame cache
prevents changes between later LED chunks, but its first copy can still be
preempted halfway through an RPC publication and capture a mixture of two
packets.

The base RPC has the same issue when it forwards mode IDs and an owner bitmap
into the PD-mode shared state (`users/noah/lib/pointing/runtime/pd_mode_state.c:363-393`).
The main context reads those fields independently when building a display
snapshot (`users/noah/lib/pointing/runtime/pd_mode_snapshot.c:45-67`).

Current split and RGB tests invoke callback updates synchronously. They prove
final packet copies and mid-frame cache stability, but not worker/main
interleaving during initial snapshot capture. A rare one-frame mixture can
therefore produce incorrect feedback colors or ownership highlighting on the
non-master half. Finding 15's frame-local caching remains valuable, but its
exact-coherence claim is only **partially resolved** until the source
publication seam is coherent.

Required remediation:

1. Publish each logical remote snapshot as one coherent generation, using a
   small critical section, double buffer, or bounded seqlock appropriate to
   the QMK/ChibiOS contexts.
2. Make RGB and PD-mode readers retry or select only a complete generation.
3. Add a deterministic interleaving harness that pauses publication between
   fields and proves readers return either the old or new snapshot, never a
   mixture.
4. Re-run the split/RGB tests, feature compile gates, full host suite, target
   build, memory budget, and fresh linked stack budget.

### Should fix — Arrow mode retains dormant whole-tap debt across axis changes

Arrow mode maintains independent bounded axis states and selects the dominant
axis per report (`users/noah/lib/pointing/modes/pd_mode_arrow.c:50-87`). When a
pure vertical report switches away from X, `dx == 0`, so the inactive X state
is not reset. The symmetric case applies to Y. A large report can leave up to
the bounded backlog behind; after unrelated movement on the other axis, later
motion can reactivate and emit that old debt.

The per-tick and stored-step bounds from Finding 10 work as designed, but
inactive-axis lifecycle is not specified or tested. Finding 10 is therefore
**partially resolved**: the safety bound is present, while the user-visible
axis-switch behavior and the flashed timing/feel check remain open.

Required remediation:

1. Decide and document whether changing dominant axes cancels inactive debt.
2. Prefer clearing the inactive state on an actual axis transition, even when
   the new report contains zero input on the old axis.
3. Add both X-to-Y and Y-to-X backlog transition tests before hardware feel
   validation.

### Should fix — Dragscroll and pinch accumulation have signed-overflow paths

The shared dragscroll state stores X/Y residuals in signed 32-bit fields
(`users/noah/lib/pointing/modes/pd_mode_dragscroll.c:64-76`) and adds or
subtracts every legal mouse report without saturation
(`users/noah/lib/pointing/modes/pd_mode_dragscroll.c:253-276`). Sustained input
can eventually overflow an `int32_t`; reverse-axis subtraction can do the same.
`dragscroll_abs32()` also negates its input and is undefined for `INT32_MIN`.
Pinch mode reuses this handler, so the risk applies to both modes.

Finding 11's expiry-before-accumulation correction remains resolved for its
documented stall boundary, but this is a new bounded-state defect. It is less
likely than the two must-fix findings, yet signed overflow is not an acceptable
long-running firmware state.

Required remediation:

1. Use explicit saturating accumulation at a justified residual limit.
2. Make absolute-value handling defined for the complete representable input
   domain.
3. Add near-limit, reverse-axis, repeated-extreme, expiry, and pinch-reuse
   tests.

### Optional cleanup — Trace-enabled split callbacks need an explicit context rule

The production configuration does not enable `NOAH_RUNTIME_TRACE_ENABLE`, so
this is not a current release blocker. If tracing is enabled later, split
worker callbacks emit into the same mutable trace ring used by main-context
code without synchronization. Either document tracing as main-context-only or
make the ring's writer contract safe before relying on trace-enabled hardware
diagnostics.

## Prior Finding Status

All resolved rows below were rechecked by the named enforcement surface and by
the combined closure commands in the Verification section.

| Prior finding | Status | Current code and enforcement evidence |
| --- | --- | --- |
| 01 — Target stack safety | resolved | Bounded deferred draining and compile-time plan-size limits remain in `users/noah/lib/key/runtime/`; stack-tool fixtures and the fresh ELF/map/disassembly gate pass. |
| 02 — Press-token rollover | resolved | Position-owned token allocation and collision/exhaustion behavior remain covered by key-runtime release/scenario tests and the full host suite. |
| 03 — VIA split buffer validation | resolved | VIA framing/bounds remain centralized under `users/noah/lib/compat/`; QMK contract and VIA sync protocol/state tests pass. |
| 04 — VIA macro byte validation | resolved | Whole-payload compilation/preflight remains in `users/noah/lib/macro/`; macro payload/default/action-lifecycle tests pass. |
| 05 — VIA split persistence | partially resolved | Durable generation/digest/ack software and target paths pass; the physical disconnect, power-cycle, reconnect, and USB-role-swap matrix is still required. |
| 06 — Combo-origin lifecycle | resolved | Compatibility-owned origin generations and retirement remain enforced by combo-origin, scenario, and real-profile tests. |
| 07 — Nonblocking macro playback | resolved | Scan-driven single-engine playback, cancellation, pinned provider ownership, and busy behavior remain enforced by macro dispatch/payload/lifecycle tests. |
| 08 — Synthetic-key ownership | regressed | Aggregate ledgers exist, but pre-process observation commits false physical owners for later-consumed handled events; same-basic holds and consumed-modifier overlaps lack integration coverage. |
| 09 — Pending-release ordering | resolved | Explicit linked FIFO, bounded batch drain, and re-entry suppression remain enforced by pending-release and release-matrix tests. |
| 10 — Pointing backlog bounds | partially resolved | Per-tick/per-axis bounds and extreme input tests pass; dormant-axis debt behavior is open and flashed timing/feel remains pending. |
| 11 — Dragscroll stall recovery | resolved | The original expiry-before-current-motion and one-timer-read contract remains enforced by pointing handler tests; the accumulator overflow issue is a separate new finding. |
| 12 — Split RPC failure backoff | resolved | Stop-on-first-failure and wrap-safe bounded retry remain enforced by split runtime tests and linked target paths. |
| 13 — RGB preview parity | resolved | Preview and normal layer rendering share the selected-layer renderer; validation and layer-render parity suites pass. |
| 14 — Macro-cache RAM | resolved | One pinned active IR plus compact slot metadata remains enforced by macro tests and the named-symbol memory budget. |
| 15 — RGB render work | partially resolved | Per-frame lazy caching and work-count reductions pass; the worker-to-main remote snapshot publication feeding that cache is not coherent. |
| 16 — Runtime lookup hot path | resolved | One-search press, zero-search matched release, active bitmap consistency, and deadline-safe invalidation remain enforced by lookup/real-profile tests and resource gates. |
| 17 — Split timer sampling | resolved | One sampled outbound timestamp and wrap-safe elapsed behavior remain enforced by split runtime tests and the explicit linked stack paths. |

## Verification

Focused review suites passed:

- `sh tests/host/run_owned_keycode_tests.sh`
- `sh tests/host/run_held_action_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_keyboard_mod_ownership_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`

Combined closure gates passed:

- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `PYTHON=/usr/bin/python3 sh tests/host/run_firmware_memory_budget_checks.sh`
- `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_checks.sh`

Fresh target evidence:

- static BSS span: 25,524 B of 26,000 B;
- linker heap: 212,352 B, above the 204,800 B minimum;
- named macro storage: 599 B of 8,192 B;
- worst reviewed main path: 1,912 B of the 1,920 B reviewed-path budget;
- worst reviewed split-worker path: 336 B of 768 B.

These green gates establish that the current tree builds, fits its enforced
resource budgets, and preserves tested behavior. They do not cover the
specific ordering and interleaving gaps described above.

## Closure Verdict

**Keep the remediation program open.** The integrated branch is not ready to
call done because Finding 08 regressed at the physical/default-processing seam
and split worker publication can expose torn remote snapshots. Both are
software correctness issues and must be fixed before hardware-only closure.

### Remaining Open Findings

1. **Must fix:** make physical ownership conditional on QMK default processing
   and add same-basic/handled-modifier integration tests.
2. **Must fix:** make split remote snapshot publication coherent across the
   worker/main boundary and test forced interleaving.
3. **Should fix:** define and enforce inactive arrow-axis backlog lifecycle.
4. **Should fix:** saturate dragscroll/pinch residual accumulation and remove
   complete-domain signed undefined behavior.
5. **Hardware verification:** complete Finding 05's two-half persistence and
   role-swap matrix after its software blockers remain green.
6. **Hardware verification:** complete Finding 10's flashed timing/feel matrix
   after arrow backlog semantics are settled.
