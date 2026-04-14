# Refactor Follow-Up Review

Date: 2026-04-14

Status: follow-up audit of the runtime-sealing remediation that landed in
review-11.

Scope:

- public runtime reset/debug seams after strict sealing
- host-test seam quality after the remediation pass
- review/doc integrity after the remediation notes were updated

## Findings

### No must-fix correctness regressions found in the reviewed areas

The runtime-sealing remediation materially improved the code. I did not find a
concrete firmware behavior regression in the reviewed runtime, pd, or host-test
paths.

### Should-fix: `runtime_debug.h` still exports a storage-shaped, matrix-sized contract

References:

- `users/noah/lib/state/runtime/runtime_debug.h:14-37`
- `users/noah/lib/state/runtime/runtime_debug.c:64-88`
- `tests/host/key_runtime_scenario_harness.c:232-253`

Why this still matters:

- The public snapshot is narrower than the old aggregate dump, but it still
  mirrors runtime storage shape closely: a full `slots_by_position[]` table plus
  ordering arrays sized by `MATRIX_ROWS * MATRIX_COLS`.
- `noah_runtime_debug_snapshot()` still copies every slot in the matrix on every
  call, even when callers only need one slot or one boolean.
- The scenario harness now has to materialize a full snapshot repeatedly just to
  answer single-slot questions such as owner keycode or hold completion.

Why this is a design-quality gap rather than just an implementation detail:

- The refactor goal was a semantic key-runtime observation seam. This API is
  better than the previous whole-runtime aggregate, but it is still partly a
  public mirror of runtime storage layout.
- Matrix size is now part of the public debug contract, which makes the API
  heavier than the actual observations most callers need.

Recommended direction:

- Keep the current helpers, but continue shrinking the public snapshot toward
  small semantic queries or narrower iterator-style reads.
- If the full snapshot remains useful for one integration test, keep that
  heavier shape local to the test harness instead of making it the primary
  public observation type.

### Should-fix: `host_runtime_fixture.h` is still a catch-all integration fixture

References:

- `tests/host/include/host_runtime_fixture.h:11-14`
- `tests/host/include/host_runtime_fixture.h:44-49`
- `tests/host/include/host_runtime_fixture.h:52-125`

Why this still matters:

- The new fixture does fix the reset contract, but it also centralizes runtime
  reset, runtime-debug capture, split-sync packet helpers, and pd snapshot
  synthesis in one default header.
- That makes lightweight host suites depend on more runtime/public APIs than
  they necessarily need.
- It also makes the intended boundary less obvious: the fixture looks like one
  blessed way to reach several unrelated subsystems at once.

Why this is a maintainability problem:

- The runtime storage leak is closed, but test coupling is still concentrated in
  a single broad helper surface.
- Future tests can easily pick up snapshot or pd helpers “because they are
  there,” which recreates some of the same cross-subsystem coupling on the test
  side.

Recommended direction:

- Split the fixture into smaller seams, at least `host_runtime_reset_fixture.h`
  and a separate pd/split helper header, or keep this header but stop treating
  it as the default include for unrelated suites.

### Optional cleanup: review-11 progress still records stale next steps

References:

- `review/2026-04-14-review-11/progress.md:63-66`
- `review/2026-04-14-review-11/userspace-architecture-review.md:117-143`

Why this is worth cleaning up:

- The review-11 architecture note says the remediation pass is resolved, but the
  paired progress log still ends with “run the full host suite” and “run the
  firmware compile after the full host suite is green.”
- That leaves the active review history internally inconsistent about whether
  the remediation was fully verified.

Why this is lower severity:

- It does not affect firmware behavior.
- It is still a review-integrity problem, because this repo uses review folders
  as the architecture record.

## Solid Areas

- The strict runtime reset seam looks sound now. `noah_runtime_reset_for_test()`
  is public through `runtime_reset.h`, and the production runtime no longer
  carries weak host-only fallbacks.
- Pd-mode raw storage sealing looks real. The public `pd_mode_runtime_shared_state.h`
  leak is gone, and only pd/runtime owner code now includes
  `pd_mode_runtime_shared_state_internal.h`.
- The compile gate meaningfully enforces the new boundary. It blocks the removed
  public runtime headers, internal runtime headers from host tests, and the
  removed public pd runtime header.
- The host tests now use module-owned debug seams for non-key-runtime state
  instead of rebuilding the old aggregate runtime snapshot.

## Conclusion

The landed refactor is in a good state overall. The important runtime-sealing
goals were achieved, and I did not find a new correctness regression in the
reviewed areas.

The main remaining quality gap is not a broken storage boundary anymore. It is
that the new debug/test seams are still somewhat overbuilt: cleaner than before,
but still wider and more layout-shaped than the review narrative suggests.

## Verification

Commands run for this review:

- `git status --short`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`

Current conclusion:

- no must-fix correctness issues found in the reviewed areas
- runtime sealing remains landed
- review-12 should focus any follow-up work on debug/test seam narrowing, not on
  reopening the storage seal itself
