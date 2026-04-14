# Initial Architecture Review Prompt

You are performing an initial architecture review of a userspace codebase for a custom keyboard firmware.

The hardware is fixed and will not change. Focus on software architecture, structure, maintainability, correctness risk from design choices, and long-term extensibility.

Before reviewing:
- Follow `AGENTS.md`.
- Start by reading the newest review folder under `review/`.
- If the newest review folder is contradictory, reconcile it before using it as the source of truth.
- If this work belongs to the active architecture thread, continue in that review folder instead of creating a same-day duplicate.
- Only create a new review folder if this is a materially different architecture topic or the previous thread is explicitly closed.

Review priorities:
1. Architecture & Separation of Concerns
2. Modularity & Extensibility
3. Abstractions & Interfaces
4. Code Organization & Structure
5. State Management & Flow
6. Scalability of the Design
7. Testing & Debuggability
8. Concrete Recommendations

Output requirements:
- Findings first, ordered by severity.
- Include file references for every finding.
- Distinguish between:
  - `must-fix`
  - `should-fix`
  - `optional cleanup`
- Be specific and justify each point with concrete reasoning.
- Avoid generic pattern advice unless the current code clearly needs it.
- If an area is solid, say so explicitly.
- End with:
  - `Current Architecture Assessment`
  - `Recommended Next Refactor Sequence`

Review integrity requirements:
- Do not mark a finding resolved in the same pass unless the current tree and verification already prove it.
- If this review updates an active thread, include a `Prior Finding Status` section for any previously open major findings.
