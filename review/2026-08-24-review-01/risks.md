# Risk Register

| Id | Risk | Severity | Primary stage | Required mitigation and closure evidence |
| --- | --- | --- | --- | --- |
| R-01 | A malformed host frame reads or writes outside its declared payload | critical | 01–02 | Resolve the active Review 19 mirror guard, validate length before every access, fuzz malformed frames, and pass sanitizer protocol tests |
| R-02 | A profile activates while keys or owned outputs are active and leaves stuck keys, modifiers, layers, modes, or mouse buttons | critical | 02 and 04 | Define a quiescence predicate, stage until safe, test every owned lifecycle, and run the Milestone A held-state hardware matrix |
| R-03 | Power loss or reset during persistence destroys the last known-good profile | critical | 02 | Use recoverable commit metadata or dual valid slots; interrupted-write fixtures must always select an intact generation or compiled defaults |
| R-04 | The halves run different behavior generations after disconnect or role swap | critical | 02 and 05 | Include profile state in durable digest reconciliation and pass disconnect, reconnect, forced-role, and dual-USB matrices |
| R-05 | Source, Studio draft, and committed device profile silently diverge or overwrite one another | high | 01, 05, and 06 | Open and refresh complete device state, bind drafts to the device generation and digest, reject stale applies, and keep device apply, source import/export, retry, and reset as explicit operations |
| R-06 | Raw C struct persistence breaks across compiler, alignment, or schema changes | high | 00 and 02 | Canonical fixed-width schema, golden fixtures, migration rules, and hard rejection of unknown incompatible versions |
| R-07 | Logical EEPROM growth or live-profile buffers consume an RP2040 SRAM bank without bank-aware accounting, or policy slack is mistaken for hardware headroom | high | 00 | **Materialised as the inverse failure and mitigated on 2026-09-04.** The gates asserted a constraint that did not exist: a `.bss`-only limit measured an initialisation artifact rather than memory, and a 204,800 B arena floor reserved ~200 KB for a newlib arena whose only reachable consumer allocates tens of bytes once. Static RAM is now one regression tripwire, the arena has a derived 4 KiB safety floor, and every run prints true headroom separately from policy distance so slack can no longer read as a hardware limit. Both builds pass with ~206 KB genuinely free. Figures in `pointing-cadence-investigation.md`. Keep physical banks, linked sections, regression policies, and runtime high-water evidence separate; prefer repartitioning existing EEPROM; pass fresh memory and stack gates and measure hardware high-water before closure |
| R-08 | Reserving profile storage removes too much VIA macro capacity | medium | 00 and 02 | Measure real and worst-case usage, publish capacity, add compile gates for non-overlap, and document the selected tradeoff |
| R-09 | Profile Studio and VIA issue interleaved requests on the same Raw HID endpoint | high | 01 | Serialize requests, use transaction ids where available, detect timeouts or unexpected replies, and clearly surface contention |
| R-10 | A native HID dependency fails under the VS Code extension runtime or on another architecture | high | 01 | Isolate the adapter, test the packaged extension, keep a helper-process fallback, and record supported platforms |
| R-11 | Direct const-array consumers bypass the effective profile | high | 03–04 | Add provider boundaries plus compile or source gates forbidding new direct profile-array reads outside default materialization |
| R-12 | RGB caches show mixed generations or stale colors | medium | 03 | Publish one RGB profile snapshot and invalidate derived colors/maps at a frame boundary; pass generation and render tests |
| R-13 | Key-behavior lookup becomes slower or consumes excessive RAM | medium | 04 | Use a compact indexed representation, measure lookup cost and target memory, and retain bounded behavior capacity |
| R-14 | Layer deletion changes numeric references inconsistently across tables | high | 07 | Treat structural layer changes as a whole-profile transaction and validate every cross-reference before commit |
| R-15 | Protocol changes strand already-persisted profiles | high | 02 and 08 | Define compatibility policy before v1 lands; test upgrade, incompatible version, corrupted header, and factory-reset paths |
| R-16 | A prototype becomes production without device-level validation | high | 05 and 08 | Keep hardware criteria open until named matrices pass on the real split keyboard |
| R-17 | Durable commit origin is derived from dynamic USB role, or left/right forced-role artifacts are mapped backwards on this `MASTER_RIGHT` board | critical | 02 and 05 | Provision left/right identity independently in flash, keep role and origin independent in tests, build left as `FORCE_SLAVE` plus `NOAH_PHYSICAL_HALF=left` and right as `FORCE_MASTER` plus `NOAH_PHYSICAL_HALF=right`, and prove both USB orientations preserve origin before enabling mutation |
| R-18 | A split callback and matrix scan concurrently enter wear-level storage, or multiple durable subsystems perform unarbitrated work | critical | 02 | Keep callbacks mailbox-only, route profile discovery/VIA mirror/VIA reconciliation through one rotating scan scheduler, instrument callback tests against every storage effect, and include the future writable profile owner in the same scheduler |
| R-19 | Host deployment and peer import interleave against the same writable profile store, corrupt admission state, or activate the wrong durable record | critical | 02 and 05 | Use one owner-held candidate backend with explicit host/peer admission, make contention retryable without poisoning transactions, validate the exact committed record before activation, and pass production plus hardware contention/interruption matrices |
| R-20 | Capability/status metadata or VIA layout readback is mistaken for complete device-profile readback | high | 06 | Add bounded, correlated payload reads for every live-owned domain, decode them into one logical Studio snapshot, verify generation and digest consistency, and pass read-after-write, reconnect, malformed-frame, and real-device tests |
| R-21 | Live-profile persistence, reconciliation, status encoding, or rendering work reduces pointing report cadence | high | 06 | **Occurred and unexplained.** Measured ~450 Hz to ~300 Hz on the live-edit build, about 1.1 ms added per main-loop iteration. Parked by owner decision on 2026-09-04 with the candidate inventory, eliminated causes, and available measurement paths recorded in `pointing-cadence-investigation.md`. Closure still requires an explained cause, a removed regression, and an enforced threshold before the live surface expands |
| R-22 | VIA and custom stores are presented as one profile without one atomic logical generation | critical | 06 | Define a manifest covering every domain digest, coordinate cross-store prepare/commit/recovery, detect external VIA writes, expose partial outcomes, and pass interruption plus external-client matrices before claiming a complete profile commit |
| R-23 | The implementation remains coupled to an open firmware repository and never becomes normal keyboard control software | high | 06 and 08 | Separate transport/schema/device-session/profile-model services from source parsing, prove open/edit/apply/backup/restore without a repository, and retain source import/export as optional adapters |

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
- R-07's original resource wording was corrected by D-016. On the 2026-08-25
  tree, the SRAM0–3 `.data + .bss` regression metric was 48,664 bytes against a
  51,000-byte policy, while the linker/core-memory span was 213,472 bytes at
  boot. Fixed linked occupancy across the RP2040's 270,336 bytes of physical
  SRAM was 56,128 bytes. These figures are not interchangeable,
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
- R-12 now has a callback-only double-banked RGB view and a captured-frame
  epoch contract. Provider invalidation performs no payload reads, and any
  later publication makes an old frame token stale before another record is
  requested. R-12 remains open until every renderer family consumes that view,
  cache invalidation is ordered at the frame boundary, split generations
  converge, and the real keyboard passes the render matrix.
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

### 2026-08-27 all RGB consumer families evidence update

- R-11 is partially mitigated for the layer, pointing-mode, auto-mouse, and
  combo-feedback RGB families and for key-feedback rendering and tap policy.
  Their colors, render modes/locality, reusable group bitmaps, stage-group rows,
  fade destination, ordered tap-branch colors, and tap-commit policy now pass
  through `rgb_effective_config.c`; a source gate rejects direct reads of their
  compiled symbols from the render paths and orchestrator. The broader risk
  remains open until the effective owners are installed and the behavior
  fallback boundary and real-board timing are closed with evidence.
- R-12 is partially mitigated at the actual render boundary. One effective
  token is captured at frame start and shared by normal layers, auto-mouse
  destination layers, layer preview, the pointing-mode overlay, and the
  auto-mouse fade, combo-feedback, and key-feedback passes.
  Live-vs-compiled render tests cover colors, mapped-only mode, pointing
  locality, canonical group bitmaps, mode-specific/all-mode groups, inherited
  colors, live fade destinations, combo locality/group placement, and
  every visible key-feedback semantic, ordered tap branches, key locality and
  group placement, stage disablement, and stale-token fail-closed behavior
  before LED application. All current renderer families use the frame seam;
  the risk remains open for production owner installation, split convergence,
  real-board timing, and the hardware render matrix.

### 2026-08-27 split authority and protocol foundation update

- R-04 now has executable D-014 ordering and an exact dedicated split frame
  contract. Host tests cover both newer-side directions, compiled and exact
  committed convergence, equal-counter/different-origin conflict,
  same-tuple/different-record corruption, incompatible firmware, unreadable or
  malformed metadata, in-flight publication, transfer pending, saturation,
  fixed wire goldens, truncation, CRC, padding, enum, and range rejection.
- The activation observer is fail-closed and reports the peer resolved only for
  an exact durable match with no active transfer. This prevents the future
  production owner from interpreting missing or conflicting peer evidence as
  safe activation.
- R-04 remains open because no QMK RPC or scan reconciler transfers payloads,
  no exact peer record is yet staged and marker-last committed, and reconnect,
  role swap, dual USB, interruption, and real-keyboard convergence remain
  untested. Peer and mutation capability bits therefore remain disabled.

### 2026-08-27 exact peer-store durability update

- R-03 and R-04 gain an isolated exact peer import path. It uses the existing
  inactive-slot reuse guard, whole-profile validator, bounded marker-last
  commit, and field-by-field durable identity check. Tests cover sender-owned
  generation/origin/flags, reboot selection, duplicate chunks, stale/conflict/
  corruption decisions, reset flags, domain-mask mismatch, and ambiguous final
  marker readback.
- A durability-unknown marker now latches the store against every later
  prepare until boot selection conclusively rescans both slots. Boot selection
  and new candidates also reject a compiled-default digest mismatch, so an old
  compiled base cannot masquerade as a compatible peer record.
- R-03 remains open for the production writable owner and hardware interruption
  matrix. R-04 remains open for QMK transport, retry/reconnect/role-change
  orchestration, boot whole-profile validation, dual USB, and real-keyboard
  convergence. Capability bits remain disabled.

### 2026-08-28 split reconciler and physical-origin update

- R-04 now has an isolated scan-owned reconciler and appended QMK transaction
  adapter. Two real in-memory store/provider halves cover compiled convergence,
  exact newer-master push, exact newer-slave pull, bounded busy retries,
  disconnect/reconnect, malformed response recovery, role-change restart,
  conflict stop, passive-peer expiry, and per-scan transport/storage exclusion.
- R-17 records a newly verified blocker: this board defines `MASTER_RIGHT`, and
  upstream QMK's no-hand-pin/no-`EE_HANDS` fallback implements physical-left as
  the inverse of current master. It therefore changes with USB role and cannot
  source durable `origin_half`. Profile Studio's forced artifacts were also
  reversed; they now build left with `FORCE_SLAVE` and right with
  `FORCE_MASTER`, with a source check enforcing that mapping.
- R-04 remains open for production owner registration, boot whole-profile
  validation, transport arbitration, and the real-keyboard matrix. R-17 remains
  open until a physical identity is explicitly provisioned and hardware proves
  role changes never alter it. Mutation and peer capability bits remain off.

### 2026-08-28 flash-provisioned physical-origin update

- R-17 is partially resolved in code. D-018 adds mutually exclusive left/right
  artifact provisioning, a fail-closed generic-firmware query, and a strong
  QMK handedness override whose result never reads current USB role or EEPROM.
  Profile Studio now supplies both the physical-side and independent forced-
  role settings for each generated artifact.
- Host coverage proves left origin `0`, right origin `1`, correct handedness,
  generic refusal, null-output refusal, and conflicting-definition compile
  failure. The source/feature gates compile both provisioned forms.
- R-17 remains open for the production owner's use of this boundary and the
  real-keyboard both-orientations/role-swap matrix. Mutation and peer
  capability bits remain off.

### 2026-08-28 split callback and durable-I/O arbitration update

- R-18 is partially resolved for every production durable subsystem currently
  reachable from the split callbacks. VIA mirror and reconciliation callbacks
  now perform bounded mailbox/framing work only; host instrumentation fails if
  a callback reaches metadata, EEPROM, keymap, macro, recovery, or digest
  storage. A rotating scheduler grants at most one boot-discovery, mirror, or
  VIA-reconciliation step per scan and skips idle owners fairly.
- R-18 remains open until the writable profile owner replaces the read-only
  discovery step in that same scheduler and real hardware proves editing,
  reconciliation, reconnect, and role changes without storage concurrency or
  starvation. Mutation and peer-profile capability bits remain off.

### 2026-08-29 production-owner safety foundation update

- R-03 gains bounded boot selection and reader-backed adoption of the exact
  selected durable record. The backend activates validated data only for an
  override record; an override-disabled reset generation requests compiled
  fallback. Production boot still uses the compatibility wrapper until the
  writable owner replaces the read-only discovery shell, so R-03 remains open.
- R-04 and R-19 gain explicit `HOST`/`PEER` admission around the one shared
  candidate store backend. A competing host begin remains queued as retryable
  busy instead of poisoning the candidate, abort and successful activation
  release admission, and split transport registration rejects a second owner.
  Production owner assembly and real split contention evidence remain open.
- The compiled materializer now derives the validator's exact layer, PD-mode,
  VIA-macro, hardcoded-macro, RGB, domain, and action-ABI compatibility rather
  than allowing the broader schema maxima. This closes a compatibility gap in
  the isolated path but does not enable any capability.
- At this 2026-08-29 foundation checkpoint, R-18 remained open because the
  incremental store/adoption APIs were still disconnected. The 2026-08-31
  gated composition below supersedes that implementation-state snapshot.

### 2026-08-31 gated owner-composition update

- R-03, R-04, R-18, and R-19 now have one complete side-specific engineering
  composition. The owner replaces the read-only scheduler entry, validates and
  adopts boot state incrementally, installs the behavior/RGB consumers, and
  registers one profile reconciler. Host and peer share one lease through
  activation; host admission permits convergence-only split work and refuses a
  concurrent peer import. Abandoned host work expires only before commit.
- The QMK profile EEPROM adapter now enforces the store's 32-byte bound and
  uses one direct block write. It no longer enters QMK's variable-stack update
  helper, which could perform a read plus a write inside one scheduler grant.
  The dedicated owner stack manifest covers both main and split callback paths.
- Coherent external read-only status and precommit supersession are now landed.
  A compatible peer generation greater than or equal to the reserved host
  generation cancels host staging before commit, including a queued commit,
  without changing the prior durable record.
- These risks remain open because candidate routing and write-capability
  advertising are still disabled, postcommit concurrent-authority resolution
  is incomplete, and the two-half interruption/role-swap hardware matrix has
  not run.
- R-07 remains open with explicit evidence: the engineering owner is 3,084
  bytes per half and passes the core-memory-span and reviewed-path stack
  policies, but fails the `.bss` policy by 2,740 bytes and the combined-data
  policy by 740 bytes. This is not physical SRAM exhaustion. No policy will be
  revised without fresh allocator and stack high-water evidence on hardware.

### 2026-09-02 distributed prepare-barrier update

Reconciliation note: the 2026-08-31 statement that postcommit authority was
incomplete is an audit-time snapshot. D-022 and the current tree supersede that
specific implementation finding.

- R-04 and R-19 now have host proof for the missing postcommit race. A
  peer-required commit stages the exact candidate on the sibling before the
  local marker, pauses at `PUSH_PREPARED`, commits local then peer, and permits
  activation only after a fresh exact `COMMITTED_CONVERGED` observation. Two
  consecutive full-owner commits prove the second candidate does not attempt
  to overwrite the slot backing the previous active runtime.
- Simultaneous provisional host writers are deterministically arbitrated by
  generation and stable physical origin before either marker. The loser aborts
  both provisional preparations and reports a distinct status. Timeout waits
  for a peer abort before releasing local admission, and role changes restart
  the handshake without losing the prepared sender correlation.
- A newer, conflicting, corrupt, or incompatible peer detected after local
  durability enters terminal `AUTHORITY_FAILED` recovery. The new record is
  not activated and the HOST lease/backing remains retained; hot import is not
  attempted against the two pinned slots.
- Host and sanitizer tests cover prepare/pause/authorize/cancel, provisional
  versus durable metadata, immediate authority refresh, role swap, first and
  second complete split commits, simultaneous writers, prepared timeout, and
  postcommit fence loss. R-04 and R-19 remain project risks until the real
  two-half interruption, reboot, reconnect, dual-USB, and contention matrix
  supplies hardware evidence.
- R-07 remains open. No resource-policy value is described as physical SRAM
  capacity; fresh linked and reviewed-path stack evidence must be recorded for
  this larger owner before enabling mutation.

### 2026-09-03 engineering mutation and first-test reconciliation

The final 2026-09-02 sentence above meant “before enabling mutation in a
hardware-test artifact.” D-023 now supplies that explicit gate and fresh linked
evidence; it does not resolve R-07 or authorize production exposure.

- R-03, R-04, R-18, and R-19 are reachable end-to-end only in the labeled
  engineering mutation pair. Routing and the complete capability set are
  mechanically coupled, and Studio verifies the exact committed/active digest.
- Those risks remain open until the physical two-half persistence, reconnect,
  USB-orientation, role-swap, interruption, contention, and recovery matrix
  passes. Software/fake-device proof is not substituted for hardware evidence.
- R-07 remains open: optimized linked accounting fails the conservative BSS and
  combined-data policies even though each half has 270,336 B of physical SRAM
  and the SRAM0–3 linker/core-memory-span policy passes. Reviewed named stack
  paths pass; runtime allocator and stack high-water evidence is still absent.

### 2026-09-04 first-hardware-test reachability update

- The first real apply did not close R-04, R-18, or R-19. It exposed a
  production reachability gap: QMK's slave half uses
  `matrix_slave_scan_user()`, while Noah's durable scheduler was reachable only
  from the master's `matrix_scan_user()`. Core split keys still worked, but the
  slave never registered or advanced the profile endpoint, so the host remained
  in `PREPARING_PEER` with no known profile peer.
- D-024 now routes the slave scan through the same arbitrated durable scheduler
  while keeping master-only runtime work out of that path. Hook chaining,
  runtime ordering, the full host suite, ordinary and engineering compiles, and
  both reviewed-path stack gates pass. Studio additionally refuses mutation
  until fresh status reports a known and converged peer and exposes/resumes only
  a matching recoverable candidate.
- R-04, R-18, and R-19 remain open until the corrected pair proves real
  two-half commit, activation, persistence, interruption, reconnect, and role
  behavior. R-07 remains unchanged: the engineering `.bss` and combined-data
  policy gates are red, physical SRAM is 270,336 B per half, and runtime
  high-water has not been measured.

### 2026-09-04 device-resident authority replan

- D-026 makes the keyboard's committed logical profile the target live
  authority. The C files remain compiled defaults plus explicit import/export
  and version-control representations; they are no longer the intended
  connected-session authority. R-05 is therefore broadened to cover three-way
  device, draft, and source divergence with generation-bound conflict checks.
- R-20 records the current readback gap. Profile Wire v1 can report
  capabilities, status, generations, and digests, and VIA can return layout
  state, but Studio cannot yet retrieve and reconstruct every custom live
  profile domain. The existing read surface must not be described as complete
  profile readback.
- R-21 turns the observed pointing-rate concern into a release constraint.
  Device readback and later domain expansion must follow measurement and
  remediation of steady-state work; protocol correctness alone is not enough
  to close the milestone.
- These are planning and documentation updates only. No runtime risk is closed
  by this entry.

### 2026-09-04 first-grade control-software goal

- D-027 and `docs/tooling/PROFILE_STUDIO_PRODUCT_GOAL.md` define the finished
  product as repository-independent keyboard control software rather than a
  repo-backed live-upload feature.
- R-22 records that separate VIA and custom stores cannot truthfully appear as
  one committed profile without a logical manifest, coordinated transaction,
  and explicit handling of external VIA changes.
- R-23 records the delivery risk that the current VS Code/source parser remains
  the only usable shell. The reusable device and profile core plus normal
  backup/restore journeys now form part of production closure.
- This entry changes project scope and acceptance documentation only. R-22 and
  R-23 are open and no implementation evidence is claimed.
