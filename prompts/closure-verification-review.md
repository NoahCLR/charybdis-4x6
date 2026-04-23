# Closure Verification Review Prompt

You are performing a closure-verification review for an active architecture/refactor thread in a userspace codebase for a custom keyboard firmware.

The hardware is fixed and will not change. This is not a fresh review and not a brainstorming pass. Your job is to determine whether the active review thread is actually ready to close.

Before reviewing:
- Follow `AGENTS.md`.
- Read the newest review folder under `review/`.
- If the newest review folder is contradictory, reconcile it before using it as the source of truth.
- Do not create a new same-day review folder for routine closure verification on the same thread unless `AGENTS.md` explicitly requires it.
- Treat a review folder with a closure verdict as immutable history after the
  closure pass is complete. Do not add follow-up findings, cleanup notes, or new
  verification to a closed review folder. If new work happens after closure,
  open the next sortable review folder and state why the closed folder was not
  continued.

Your job:
- Re-check every major finding from the active review thread.
- Verify that claimed resolutions are backed by code, tests, compile gates, and current documentation.
- Identify anything still open, partially resolved, or regressed.
- Decide whether the thread should close now or remain open.

Review priorities:
1. Prior Finding Closure
   - Re-evaluate each major finding from the active thread.
   - Check whether each item is truly `resolved`, still `open`, only `partially resolved`, or `regressed`.
2. Mechanical Enforcement
   - Verify that boundary, API, and architecture claims are enforced by tests or compile gates where applicable.
   - Flag any claim that is documented as landed but not mechanically enforced.
   - Do not add tests or compile gates whose only purpose is to assert that a
     deleted historical symbol, file, or function name remains absent. For a
     removed API, successful compilation of the current source lists and test
     callers is usually the mechanical enforcement.
   - Negative grep-style gates are appropriate only for active architecture
     boundaries that should remain true for future code, such as forbidden
     include directions or ownership-boundary imports. They should not encode
     one-off historical cleanup decisions.
   - If a deleted API has no current callers and the current build surfaces
     compile without it, document that compile/link coverage as the enforcement
     instead of inventing a brittle absence check.
3. Documentation and Review Consistency
   - Check whether the active review folder, relevant docs, and the current code all describe the same state.
   - Identify stale claims, contradictory notes, or closure statements that are too strong.
   - Verify that post-closure work was not appended to a closed review folder.
4. Final Verification Integrity
   - Confirm the exact verification commands that were run.
   - Flag any closure claim that is missing required host-suite or firmware-build evidence.

Output requirements:
- Findings first, ordered by severity.
- Include file references for every finding.
- Distinguish between:
  - `must-fix`
  - `should-fix`
  - `optional cleanup`
- Be specific and justify each point with concrete reasoning.
- If an area is solid, say so explicitly.

Required section:
## Prior Finding Status
For each major finding from the active review thread, mark it as:
- `open`
- `partially resolved`
- `resolved`
- `regressed`

For every `resolved` item, include:
- code references
- enforcement references such as tests or compile gates
- exact verification commands that passed

Closure rules:
- Do not call the thread closed because the code looks cleaner.
- A thread is only ready to close when:
  - the current code matches the intended design
  - major findings are either resolved or consciously deferred as optional cleanup
  - tests or compile gates mechanically enforce the important claims where applicable
  - mechanical enforcement is focused on active contracts and behavior, not on
    preserving the absence of old symbol names
  - docs and the active review folder match the current tree
  - `sh tests/host/run_all_host_tests.sh` passed
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah` passed

End with:
- `Closure Verdict`: `close thread` or `keep thread open`
- `Remaining Open Findings`
