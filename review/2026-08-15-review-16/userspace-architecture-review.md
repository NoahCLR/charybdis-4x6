# Runtime Lookup Hot-Path Architecture Review

Review 15 is closed and remains immutable. Finding 16 is a distinct key-event
resolution, active-state traversal, and feedback invalidation topic, so this
work uses Review 16. The repository's
`prompts/initial-architecture-review.md` template guides this review.

## Findings

### Must-fix — One physical event resolves authored behavior repeatedly

`key_behavior_lookup.c` linearly searches the authored table, while
`handled_key_lookup_tap_count_into()` performs another one or two searches for
the selected tap step and future-tap state. The process pipeline then resolves
again in preflight and the handled-key stage even though reducer observation
has already materialized the interaction into the position-owned press token.

The press token should be the event-local authority after observation. The
reducer must perform one primary authored-row search, derive the complete
resolution from that row, and cache the materialized interaction. Preflight
and handled press/release routing should consume that cached state. Only an
unmatched event or token-allocation failure needs a fallback resolution.

### Should-fix — Tap-series acceptance needs distinct authored branch state

`tap_series_t.has_more_taps` describes the materialized tap source, which can
differ from the authored key for transparent branches. Acceptance previously
searched authored behavior again. The current tree now caches a distinct
`authored_has_more_taps` bit from the original resolution, removing that query
without changing transparent-source semantics.

### Should-fix — Matrix-capacity scans scale with slots, not active work

Interrupt, time refresh, scan planning, and feedback projection traverse
matrix-sized press-token and tap-series arrays. Counts provide an early idle
exit but do not bound visits once one entry is active. Operation counters must
establish the real cost before adding an active bitmap or compact index, which
would become a second lifecycle invariant.

The measured baseline was 180 matrix-slot visits for one active press. The
current tree therefore uses two compact bitmaps to iterate only active press
tokens and tap series. Array storage remains authoritative, and host-only
verification recomputes both bitmaps and counts from the arrays.

### Should-fix — Scan dirtiness is broader than visible feedback change

`scan.c` marks split key feedback dirty on every scan with active core work.
The landed invalidation is driven by transition effects or by a change in the
reducer feedback sequence when a scan has no effect plan. Unchanged active
scans do not mark feedback dirty. A tested release-hold deadline crossing marks
exactly once. Once a packet is active, split sync's existing
`last_sent_active` policy rebuilds it until inactive, so flashing and expiry
remain time-correct without broad scan invalidation.

### Solid — Position-owned interaction caching is already the right model

The reducer stores a fully materialized interaction in the press token and
keeps tap-series branch data by matrix position. Matched releases can settle
from that state without consulting mutable authored data. The optimization
should reinforce this ownership model rather than add a global resolution
cache.

## Prior Finding Status

| Prior finding | Status | Preservation requirement |
| --- | --- | --- |
| Finding 02: press identity | resolved | Position-owned tokens and release identity remain authoritative. |
| Finding 08: synthetic-key ownership | resolved | Synthetic events continue to bypass physical observation. |
| Finding 09: runtime stack budget | resolved | Event-routing changes must remain inside the reviewed stack budget. |
| Finding 15: RGB render work | resolved | One snapshot per frame remains intact. |
| Finding 16: runtime lookup hot path | resolved | Position-owned lookup propagation, active bitmaps, and semantic/deadline invalidation are enforced by focused counters, the full host suite, the target build, memory gate, and fresh linked stack gate. |

## Selected First-Pass Architecture

- Add host-only deterministic counters at the authored lookup boundary.
- Build a complete handled-key resolution from one located config pointer.
- Cache authored continuation separately from transparent materialized-source
  continuation for series acceptance.
- Treat the observed press token as the source of handled status and
  interaction data for normal press and matched-release routing.
- Keep one explicit resolution fallback for unmatched events and allocation
  failure.
- Use two compact active bitmaps after the measured 180-slot active-scan
  baseline justified their lifecycle cost; keep the arrays authoritative.
- Use transition-plan invalidation for projected effects and feedback-sequence
  invalidation for visible no-plan deadline changes.
- Do not add a lookup index: after duplicate searches are removed, the measured
  normal event budget is one authored search and the index lifecycle would add
  complexity without evidence of a remaining problem.

## Current Architecture Assessment

Finding 16 is **resolved**. Authored resolution propagates through the event
pipeline with enforced one-search press and zero-search matched release
budgets. Active scans are proportional to active entries under a verified
bitmap invariant. Unchanged active scans issue zero dirty marks, while a
visible no-plan deadline transition issues exactly one. The current evidence
does not justify a lookup index.

## Finding 16 Closure Evidence

- Code references: `key_behavior_lookup.c` and `handled_key_lookup.c` own
  one-row resolution; `process.c`, `press.c`, `release.c`, and `transition.c`
  propagate token-owned state; `reducer/runtime.c` owns active bitmap lifecycle;
  `scan.c` owns no-plan feedback-sequence invalidation.
- Enforcement references: `key_behavior_lookup_test.c` checks direct lookup
  budgets; `real_profile_thumb_layer_lock_integration_test.c` checks one-search
  press, zero-search matched release, exact active-slot visits, bitmap
  consistency, zero unchanged dirty marks, and one deadline-crossing mark.
- Boundary enforcement: `run_feature_gate_compile_tests.sh` passed, and the
  fresh linked stack manifest accepts only real direct/declared-indirect edges.
- Verification passed: `sh tests/host/run_all_host_tests.sh`,
  `qmk compile -kb bastardkb/charybdis/4x6 -km noah`,
  `PYTHON=/usr/bin/python3 sh tests/host/run_firmware_memory_budget_checks.sh`,
  `PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_checks.sh`,
  and `git diff --check`.
- Target evidence: 152,232 B text, 25,524 B static BSS, 212,352 B linker
  heap, 1,912/1,920 B worst reviewed main path, and 336/768 B worst reviewed
  split path.

## Residual Tradeoffs

- The active bitmaps are a derived invariant and therefore retain host-only
  recomputation checks; position-indexed arrays remain authoritative.
- The stack result is a reviewed-path regression gate, not a global maximum
  proof.
- No sorted/hash lookup index was added because the enforced event budget no
  longer shows repeated authored searches.
