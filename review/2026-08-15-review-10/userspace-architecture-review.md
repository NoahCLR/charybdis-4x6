# Durable VIA Split Reconciliation Architecture Review

Review 09 is closed and immutable. Finding 05 replaces best-effort pre-apply
VIA command replay with committed, versioned reconciliation.

## Review Scope

- Classify every VIA-owned mutation from a validated full command shape.
- Publish only storage state that QMK and any deferred macro reseeding have
  committed locally.
- Persist local generation/dirty metadata without claiming transport delivery
  is durable application.
- Reconcile complete VIA-owned regions across loss, reboot, reconnect, and
  role change with bounded per-scan work.
- Keep this protocol's version/authority state separate from Finding 12's
  transient runtime-sync transport health.

## Prior Finding Status

| Prior finding | Status | Required preservation |
| --- | --- | --- |
| Finding 05: VIA split persistence | open | Replace raw replay with complete post-commit, acknowledged reconciliation. |
| Finding 03: VIA split packet validation | resolved | Validate every frame and region before storage effects. |
| Finding 12: runtime RPC backoff | resolved | Use bounded attempt timing without merging the two protocols' health state. |
| Finding 17: split timer sampling | resolved | Use one sampled, wrap-safe timestamp per reconciliation tick. |

## Baseline Ownership Audit

- `via_command_kb()` is a pre-QMK hook. It currently invalidates local caches
  and sends selected raw command bytes before QMK applies them.
- Macro set-buffer is unclassified. Macro reset only schedules local authored
  reseeding, so neither final macro result reaches the peer.
- `qmk_via_split_sync.c` ignores send failure and has no application ack,
  generation, snapshot, retry, reconnect, or role-change state.
- `runtime_init.c` is the only userspace owner of the legacy 32-bit user
  eeconfig word; it currently resets it to zero. QMK exposes atomic dword
  update semantics for this surface.
- Enabling a user data block would change the eeconfig/VIA layout and require
  an explicit migration/reset. The initial design therefore reserves the
  existing word for schema, dirty state, and a nonzero serial generation.
- Peer acknowledgement need not consume persistent metadata if every boot and
  role session starts with a mandatory metadata exchange; a reboot can never
  assume the peer is synchronized.

## Accepted Metadata Layout

- bits 31–28: schema version;
- bit 27: dirty/incomplete local mutation;
- bits 26–0: nonzero serial generation, skipping zero;
- serial comparison is valid only within the 26-bit half-range;
- digest is computed from canonical storage on boot/commit and exchanged over
  the wire rather than stored in the word.

The codec and persistent-state fixtures now enforce this layout. The existing
atomic 32-bit user-eeconfig update is the only write surface, so VIA addresses
do not move and no EEPROM migration is required. Unknown schema, zero
generation, or dirty boot state enters recovery and cannot publish a clean
local generation. Repeated pre-apply mutations coalesce under one dirty marker;
the clean generation advances only after validated canonical readback.

## Accepted Frame Envelope

- every request and response is exactly 32 bytes;
- byte 0 is protocol version and byte 31 is CRC-8;
- generation, region, offset, region length, snapshot digest, and payload
  length are encoded explicitly in endian-stable fields;
- a fragment carries at most 14 bytes;
- metadata, begin, chunk push/pull, commit, ack, and error kinds have distinct
  shape validation before any storage effect;
- generation zero/out-of-range, unknown kinds/regions/status, truncated frames,
  corrupt CRC, and overflowed fragment ranges are rejected.

## Landed Reconciliation Design

The envelope is now the registered live RPC protocol. The former raw VIA
command replay callback has been removed. Canonical region access is isolated
in `qmk_via_storage_regions.c` and covers VIA validity/layout options, dynamic
keymap, optional encoder map, and macro storage without moving QMK's EEPROM
layout.

The current USB master initiates metadata exchange on boot, periodically, and
after role change. Clean serial generations determine authority. Dirty state
cannot win; newer clean state supplies the snapshot; equal clean generation
with unequal digest is resolved by advancing the current master's generation.
If neither side is clean, the current master performs explicit VIA-default
recovery before advertising authority.

Snapshots transfer keymap, encoder, macro, then config-validity state. The
receiver persists dirty metadata and invalidates VIA before accepting bytes,
checks strict region order/ranges, accepts only an exact last-fragment
duplicate, and responds to commit with `BUSY` until an independent whole-state
digest finishes. Clean metadata and cache invalidation occur only after that
digest matches. The sender retains replication-pending state until the final
generation/digest acknowledgement. Failed exchanges use 50–1,000 ms bounded
backoff and a 1,000 ms metadata refresh catches rejoin without a separate
persistent peer-ack word.

QMK's split callback runs in `SlaveThread`, while digest and outbound work run
from matrix scan. Short atomic snapshots protect shared flags and counters;
EEPROM and digest work stays outside the scheduler lock. Receiver and local
digest epochs discard superseded verification. A finalization reservation
prevents a replacement snapshot from racing the persistent clean commit.

The diagnostic snapshot exposes local and peer generation/digest, the last
peer acknowledgement, phase, dirty/recovery/digest/replication state, receiver
activity, retries, rejected frames, conflicts, and last protocol error.

## Current Finding Status

Finding 05 is **implemented and software-verified**. Focused coverage, the
complete host suite, profile introspection, feature gates, ordinary target
build, and fresh instrumented reviewed-path stack gate are green. The post-LTO
maximum is 1,904/1,920 B in the main process and 336/768 B in the split worker;
the manifest explicitly covers the new live callback and main/slave VIA paths.
The physical disconnect, power-cycle, and USB-role-swap matrix remains the one
manual closure requirement.

## Closure Bar

Software closure requires command-completeness, post-apply ordering, metadata,
codec, fragment, ack, retry, reboot, reconnect, role-swap, conflict, cache
invalidation, full-host, firmware, and target evidence. Final Finding 05
closure also requires the physical disconnect/power-cycle/USB-role-swap matrix
listed in the Sol plan; software-only completion must remain explicitly
unverified until that hardware evidence exists.
