# Pending-Release FIFO Architecture Review

This review follows the closed press-token identity review in
`review/2026-08-15-review-03/`. That folder is immutable. Finding 09 changes
the deferred-release queue's ordering representation and therefore uses this
next sortable review folder.

## Review Scope

- Remove uptime-dependent sequence ordering from pending releases.
- Preserve FIFO order, blocker policy, matching completion, and owner cleanup.
- Keep the queue bounded and avoid capacity-sized automatic storage.
- Prove structure, long-uptime behavior, RAM layout, and target stack reserve.

## Prior Finding Status

| Prior finding | Audit-time status | Current status | Evidence |
| --- | --- | --- | --- |
| Finding 09: pending-release sequence rollover | open | resolved | Explicit head/tail/next linkage, focused queue runner, 65,537-cycle stress, full host suite, firmware build, and fresh stack gate |
| Finding 01: target stack safety | resolved | resolved | Bounded four-record drain remains; linked drain frame is 120 B and worst reviewed path remains 1,808/1,920 B |
| Finding 02: press-token identity rollover | resolved | resolved | Queue owner IDs retain their existing liveness role; runtime debug and ownership integrations pass |
| Finding 08: synthetic-key ownership | open | open | Deliberately excluded; it is the next Phase 2 architecture finding |

## Intended Design

Pending-release order is a bounded linked list over the existing fixed slot
array. Each active slot stores the next slot index. Core state owns the head and
tail indices, with `UINT8_MAX` as an invalid sentinel. Capacity is statically
required to stay below the sentinel, so no allocation and no time-derived age
value are needed.

Tail append is O(1). Ordered inspection, first-eligible selection, and oldest
matching selection are O(N) and terminate after at most configured capacity.
The first eligible search may skip an entry whose owner press is active; that
entry stays linked in place while a later eligible entry is removed. When its
owner retires, its position relative to all remaining records is unchanged.

One unlink helper owns all structural mutation. It verifies the cursor,
reconnects the predecessor or moves the head, updates the tail, clears the
slot, decrements the count once, and then reevaluates the owner token. This
prevents the ordinary drain and delayed-dispatch observation paths from
diverging.

## Structural Contract

- Empty means count zero and both indices invalid.
- Nonempty means active, valid head and tail slots.
- Every active slot is reachable exactly once from head.
- Reachable count equals stored count.
- Tail is the only reachable slot whose next index is invalid.
- Traversal cannot exceed configured capacity.
- Owner pending state clears only after its final queued record is unlinked.

The complete structural validator is compiled only for host tests. Production
hot paths retain bounded local checks without an O(N²) audit scan.

## Resource Contract

The compact slot remains 12 bytes, the public drain snapshot shrinks from 14 to
12 bytes, and the host core-state shape remains 21,780 bytes. Target `.bss`
remains 65,204 bytes. No capacity-sized automatic array was added. The linked
bounded-drain frame is 120 bytes; the reviewed target maximum remains 1,808
bytes with a 640-byte configured reserve.

## Current Status

Verified and closed on 2026-08-15. Code, direct tests, integration tests,
developer documentation, Sol records, full host suite, target firmware, and
fresh target stack evidence agree on the explicit FIFO design.
