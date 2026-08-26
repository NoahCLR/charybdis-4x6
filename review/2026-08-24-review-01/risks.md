# Risk Register

| Id | Risk | Severity | Primary stage | Required mitigation and closure evidence |
| --- | --- | --- | --- | --- |
| R-01 | A malformed host frame reads or writes outside its declared payload | critical | 01–02 | Resolve the active Review 19 mirror guard, validate length before every access, fuzz malformed frames, and pass sanitizer protocol tests |
| R-02 | A profile activates while keys or owned outputs are active and leaves stuck keys, modifiers, layers, modes, or mouse buttons | critical | 02 and 04 | Define a quiescence predicate, stage until safe, test every owned lifecycle, and run the Milestone A held-state hardware matrix |
| R-03 | Power loss or reset during persistence destroys the last known-good profile | critical | 02 | Use recoverable commit metadata or dual valid slots; interrupted-write fixtures must always select an intact generation or compiled defaults |
| R-04 | The halves run different behavior generations after disconnect or role swap | critical | 02 and 05 | Include profile state in durable digest reconciliation and pass disconnect, reconnect, forced-role, and dual-USB matrices |
| R-05 | Source and device silently diverge | high | 01 and 05 | Show independent digests and operation outcomes; implement explicit push, pull, retry, and reset flows |
| R-06 | Raw C struct persistence breaks across compiler, alignment, or schema changes | high | 00 and 02 | Canonical fixed-width schema, golden fixtures, migration rules, and hard rejection of unknown incompatible versions |
| R-07 | Logical EEPROM growth or live-profile buffers consume an RP2040 SRAM bank without bank-aware accounting, or policy slack is mistaken for hardware headroom | high | 00 | Keep physical banks, linked sections, regression policies, and runtime high-water evidence separate; prefer repartitioning existing EEPROM; pass fresh memory and stack gates and measure hardware high-water before closure |
| R-08 | Reserving profile storage removes too much VIA macro capacity | medium | 00 and 02 | Measure real and worst-case usage, publish capacity, add compile gates for non-overlap, and document the selected tradeoff |
| R-09 | Profile Studio and VIA issue interleaved requests on the same Raw HID endpoint | high | 01 | Serialize requests, use transaction ids where available, detect timeouts or unexpected replies, and clearly surface contention |
| R-10 | A native HID dependency fails under the VS Code extension runtime or on another architecture | high | 01 | Isolate the adapter, test the packaged extension, keep a helper-process fallback, and record supported platforms |
| R-11 | Direct const-array consumers bypass the effective profile | high | 03–04 | Add provider boundaries plus compile or source gates forbidding new direct profile-array reads outside default materialization |
| R-12 | RGB caches show mixed generations or stale colors | medium | 03 | Publish one RGB profile snapshot and invalidate derived colors/maps at a frame boundary; pass generation and render tests |
| R-13 | Key-behavior lookup becomes slower or consumes excessive RAM | medium | 04 | Use a compact indexed representation, measure lookup cost and target memory, and retain bounded behavior capacity |
| R-14 | Layer deletion changes numeric references inconsistently across tables | high | 07 | Treat structural layer changes as a whole-profile transaction and validate every cross-reference before commit |
| R-15 | Protocol changes strand already-persisted profiles | high | 02 and 08 | Define compatibility policy before v1 lands; test upgrade, incompatible version, corrupted header, and factory-reset paths |
| R-16 | A prototype becomes production without device-level validation | high | 05 and 08 | Keep hardware criteria open until named matrices pass on the real split keyboard |

## Risk Update Rule

Agents update this table when a risk changes, but do not delete closed risks.
Append closure evidence below the table with the date, stage, commands, and
hardware result. If mitigation changes the architecture, update decisions.md
and userspace-architecture-review.md in the same pass.

## Closure Evidence

No project risks are closed yet.

### 2026-08-25 Stage 00 and early implementation evidence update

- R-01's inherited Review 19 malformed mirror-frame defect is resolved with
  widened receiver guards, exact boundary cases, a real receiver harness, and
  ASan/UBSan coverage. The isolated candidate-write and custom-save commit
  frames now have exact-length, padding, bounds, correlation, malformed-corpus,
  and sanitizer coverage in C and JavaScript. R-01 remains open until that
  standalone codec is connected through and reverified at the production QMK
  callback boundary.
- R-07's original resource wording was corrected by D-016. On the current
  tree, the SRAM0–3 `.data + .bss` regression metric is 48,664 bytes against a
  51,000-byte policy, while the linker/core-memory span is 213,472 bytes at
  boot. Fixed linked occupancy across the RP2040's 270,336 bytes of physical
  SRAM is 56,128 bytes. These figures are not interchangeable,
  and runtime allocator high-water remains unmeasured.
- R-02 now has a production reason/count predicate backed by key-runtime
  press/tap/lease/release/persistent counts, aggregate managed HID outputs,
  QMK modifier and one-shot state, macro and combo lifecycles, and a fail-closed
  injected peer observer. Its status snapshot is generation-coherent. R-02
  remains open until the predicate is installed with provider invalidators,
  split convergence, migrated consumers, and the held-state hardware matrix.
- R-11 and R-13 now have a first production behavior seam: an installed live
  generation fully replaces compiled rows, the callback copies no payload, and
  old lookup epochs are refused after publication. The current ordered
  reader-backed lookup deliberately has no persistent max-sized index. Both
  risks remain open until the owner is installed, direct-bypass gates land,
  worst-case lookup timing is measured on the keyboard, and any index choice is
  rechecked against bank-aware linked accounting.
- The accepted design repartitions the existing 16 KiB logical EEPROM, so it
  does not enlarge the 16 KiB wear-level cache. Candidate payloads remain in
  inactive EEPROM for power-loss-safe staging and to avoid duplication. A
  nominal 4 KiB RAM buffer would violate current regression policies but is
  not physically impossible; runtime representation costs remain open.
- R-08 is bounded by retaining 7,551 VIA macro bytes; current authored defaults
  use approximately 258 bytes.
- R-10 selects a lazy `node-hid` N-API adapter behind an injected interface,
  with a helper-process escape hatch. Mock packaging/transport checks and
  read-only native enumeration pass; real VS Code-host packaging and board
  evidence remain open because enumeration found no matching attached device.
- R-15 now has a frozen v1 compatibility and action-ABI contract in
  `profile-wire-v1.md`; migration implementation and persisted-profile fixtures
  remain open.
