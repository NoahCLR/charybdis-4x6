# Risk Register

| Id | Risk | Severity | Primary stage | Required mitigation and closure evidence |
| --- | --- | --- | --- | --- |
| R-01 | A malformed host frame reads or writes outside its declared payload | critical | 01–02 | Resolve the active Review 19 mirror guard, validate length before every access, fuzz malformed frames, and pass sanitizer protocol tests |
| R-02 | A profile activates while keys or owned outputs are active and leaves stuck keys, modifiers, layers, modes, or mouse buttons | critical | 02 and 04 | Define a quiescence predicate, stage until safe, test every owned lifecycle, and run the Milestone A held-state hardware matrix |
| R-03 | Power loss or reset during persistence destroys the last known-good profile | critical | 02 | Use recoverable commit metadata or dual valid slots; interrupted-write fixtures must always select an intact generation or compiled defaults |
| R-04 | The halves run different behavior generations after disconnect or role swap | critical | 02 and 05 | Include profile state in durable digest reconciliation and pass disconnect, reconnect, forced-role, and dual-USB matrices |
| R-05 | Source and device silently diverge | high | 01 and 05 | Show independent digests and operation outcomes; implement explicit push, pull, retry, and reset flows |
| R-06 | Raw C struct persistence breaks across compiler, alignment, or schema changes | high | 00 and 02 | Canonical fixed-width schema, golden fixtures, migration rules, and hard rejection of unknown incompatible versions |
| R-07 | Enlarging logical EEPROM consumes too much RP2040 RAM through the wear-leveling cache | high | 00 | Measure fresh ELF layout; prefer repartitioning the existing logical region; pass fresh memory and stack budget gates |
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
