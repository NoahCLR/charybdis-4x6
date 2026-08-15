# Press-Token Identity Rollover Review

This review follows the closed Finding 04 review. That folder is immutable;
Finding 02 changes the reducer's ownership identity contract and therefore uses
the next sortable review folder.

## Review Scope

- Prevent zero press-token identities at `uint16_t` wrap.
- Prevent reuse while any reducer owner store still references an identity.
- Make finite-domain exhaustion atomic, visible, and fail closed.
- Preserve release, held-action, repeat, layer, modifier, PD, and deferred
  settlement behavior.
- Keep the added press-path work and target stack frame bounded.

## Prior Finding Status

| Prior finding | Audit-time status | Current status | Evidence |
| --- | --- | --- | --- |
| Finding 02: unchecked press-token rollover | open | resolved | `key_runtime_core_allocate_token_id()`, production/tiny-domain runtime-debug builds, focused ownership tests, full host suite, firmware build, and explicit target stack path |
| Finding 01: target stack safety | resolved | resolved | Fresh stack gate includes the new 1,104 B allocation path and retains the 1,808 B reviewed worst path under the 1,920 B budget |
| Finding 09: pending-release sequence rollover | open | open | Deliberately excluded; queue sequence age is the next Phase 1 finding |

## Intended Design

Zero remains the global “no owner” sentinel. `next_token_id` is only a next
candidate; allocation normalizes it into `1..UINT16_MAX`, tests liveness, and
advances explicitly from `UINT16_MAX` to `1`. A candidate is live if it appears
in an active press token, an inactive token retained for deferred settlement,
an active lease, or an active pending-release slot.

The allocator runs before any event-semantic mutation. Success commits only
the next candidate and then allows ordinary press construction. A complete
cycle without a free identity leaves press, series, interruption, lease, and
effect state untouched. It increments a saturating diagnostic and records the
failed physical position. The handled-key stage consumes that press with an
empty plan, so QMK cannot independently act on a behavior userspace failed to
own. Non-handled keys remain available to normal QMK processing without a core
owner.

Production capacity makes full exhaustion unreachable with coherent reducer
counts, but the algorithm is total for every configured finite domain. The
host harness recompiles the real runtime with a three-ID domain and reserves
all three IDs to execute the failure branch without a production test API.

## Owner-ID Liveness Contract

| Store | Reservation condition | Why |
| --- | --- | --- |
| `press_tokens[]` | `active`, `pending_release_emission`, or `RELEASE_PENDING` | Active release and retained settlement still identify the owner |
| `leases[]` | active | Layer, modifier, held, repeat, PD, and pointer cleanup is keyed by owner ID |
| `pending_releases[]` | active flag | Drain completion finds and clears the retained owner token by ID |
| `persistent_intents[]` | not included | Current persistent intents are lock-like state and carry no owner-token ID |

The owner-store comment in `key_runtime_core_state_t` is the maintenance hook:
adding another owner-bearing store requires extending both the reservation
predicate and the host test.

## Contracts

- No physical press is assigned token ID zero.
- Wrap cannot steal a live active, leased, or deferred owner identity.
- Allocation precedes every mutation that would require rollback.
- Exhaustion is diagnosable through
  `core_token_allocation_failure_count` and has no action effects.
- The common no-owner path does not scan inactive owner arrays.
- No sibling QMK source change is required.

## Verification Strategy

- Cross `0xFFFE`, `0xFFFF`, and `1` directly.
- Keep low ID `1` live while wrapping and require allocation of `2`.
- Reserve candidates through every current owner-bearing store and both held
  and repeat lease kinds.
- Release wrapped held/repeat owners and drain a wrapped deferred owner beside
  a post-wrap press.
- Force total exhaustion with a three-ID compile and verify an empty handled
  effect plan plus unchanged token/lease state.
- Retain focused release/ownership suites, feature compile gates, the full host
  suite, target firmware build, and reviewed-path target stack gate.

## Current Status

Verified and closed on 2026-08-15. Code, tests, developer documentation, Sol
records, full host suite, target firmware, and the fresh stack gate match this
design. The explicit allocation path is 1,104 B; the overall worst reviewed
main path remains 1,808 B with a 640 B reserve.
