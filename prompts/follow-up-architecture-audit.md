# Follow-Up Architecture Audit Prompt

You are performing a follow-up architecture audit of recently landed work in a userspace codebase for a custom keyboard firmware.

The hardware is fixed and will not change. This is not a fresh architecture brainstorming pass. It is a critical audit of the code that now exists.

Before reviewing:
- Follow `AGENTS.md`.
- Read the newest review folder under `review/`.
- If the newest review folder is contradictory, reconcile it before using it as the source of truth.
- If this work belongs to the active architecture thread, continue in that review folder instead of creating a new same-day review folder.
- Only create a new review folder if the previous thread is explicitly closed or the topic is materially different.

Your job:
- Audit whether the intended architecture actually landed.
- Compare prior findings against the current code.
- Treat `resolved` as requiring code evidence plus mechanical enforcement where applicable.

Review priorities:
1. Correctness & Regression Risk
   - Identify behavior changes introduced by the refactor.
   - Look for incomplete migrations, compatibility leftovers, and fragile assumptions.
   - Flag code paths where new abstractions may not preserve prior semantics.
2. Architecture Outcome
   - Evaluate whether the refactor actually improved separation of concerns.
   - Identify coupling that still crosses the claimed boundaries.
   - Check whether the intended architecture in the active review note matches the code that exists now.
3. API & Interface Cleanliness
   - Check whether public/internal interfaces are minimal, coherent, and enforced.
   - Identify naming mismatches, compatibility leftovers, or storage leaks.
   - Highlight interfaces that still expose internals unnecessarily.
4. Test & Compile-Gate Coverage
   - Assess whether tests and compile gates now mechanically enforce the claims.
   - Identify missing regression coverage, weak assertions, or unguarded seams.
   - Check whether higher-level tests use the intended semantic seams.
5. Maintainability & Code Organization
   - Review whether the file/module structure is easier to navigate now.
   - Call out complexity that was moved rather than removed.
   - Identify places where the cleanup improved readability versus just adding indirection.
6. Documentation & Review Integrity
   - Check whether docs and review notes accurately describe the current tree.
   - Identify contradictions between the active review folder, older review folders, and the code.

Output requirements:
- Findings first, ordered by severity.
- Include file references for every finding.
- Distinguish between:
  - `must-fix`
  - `should-fix`
  - `optional cleanup`
- Be specific and justify each point with concrete reasoning.
- If an area is solid, say so explicitly instead of inventing issues.

Required section:
## Prior Finding Status
For each major prior finding from the active review thread, mark it as:
- `open`
- `partially resolved`
- `resolved`
- `regressed`

For every `resolved` item, include:
- code references
- enforcement references such as tests or compile gates
- exact verification commands that passed

Closure rules:
- Do not call architecture work resolved because it looks cleaner.
- A finding is only resolved when:
  - the current code matches the intended design
  - tests or compile gates mechanically enforce the claim where applicable
  - docs and the active review note match the current tree
- If a prior review folder mixes landed updates with stale findings, reconcile that before treating the thread as closed.

End with:
- `Current Conclusion`
- `Remaining Open Findings`
