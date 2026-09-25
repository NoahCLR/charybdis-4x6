# Logical Profile Transaction V1

> Schema-2 PD extension: side-specific PD-enabled builds keep the v1 HID/split
> envelope and add domain `0x50` (mask bit 4), profile schema 2.0, RGB/settings
> v2, eight PD slots, and a 5,088-byte custom payload ceiling. Logical storage
> is format 3 (`NR`) with the same VIA generation/digest binding; portable
> documents are version 2. Existing schema-1/format-2 bridge behavior below
> remains supported by the app. The exact version/geometry/ABI and legacy GET 9
> migration contract is in [PD-mode domain v1](pd-mode-domain-v1.md).


## Purpose

One Apply must publish one logical profile generation containing the custom
Profile Wire payload, VIA keymap, and VIA macro bank. A reset or loss of power
may leave recovery work, but normal key output must observe either the complete
old generation or the complete new generation. It must never observe a mixture.

This contract replaces the current recovery-file-only boundary between the
dual-slot custom store and the in-place VIA store. It retains standard VIA
storage and does not add a second 8 KiB EEPROM cache.

## Identity

The logical manifest contains the logical generation; custom profile generation,
origin, payload length, CRC32, and FNV-1a digest; VIA generation and canonical
VIA digest; action ABI digest; and storage schema.

The custom slot header advances to a format that carries the bound VIA identity
and two marker states: `prepared` and `committed`. The prepared marker is durable
transaction intent but is never active authority. The committed marker is the
logical decision record. Existing format-1 records remain readable as unbound
migration input; the first logical Apply replaces them.

Format 2 keeps the 32-byte header and 4,064-byte payload capacity. Its `NQ`
magic distinguishes it from the legacy `NP` header. It packs domain, origin and
profile flags into one byte; retains payload CRC32, compiled-default digest and
action-ABI digest; stores the bound VIA generation and digest; derives the
canonical payload digest again during validation; protects bytes 0–28 with
CRC16; and uses byte 31 as the one-byte prepared/committed marker. This preserves
firmware-update compatibility checks instead of treating the current firmware's
identities as if they had been stored with the profile.

The effective-profile owner may publish a committed custom record only while
the local VIA identity matches the record's bound VIA identity. A mismatch is a
recovery state and blocks normal output until reconciliation completes. The VIA
layer enforces this after a restart: output stays fenced while boot recovery
has not yet matched the local bank to the committed record's bound identity
(normally a fraction of a second, until the local digest completes), and while
a bank that booted dirty from an interrupted write has not been made clean
again. Both clear only from a complete generation, which may need the peer.
Uninitialized metadata on a freshly flashed half, an ordinary VIA edit in
progress, and owner error states do not fence output.

## Why The Peer Is The VIA Staging Copy

The 16 KiB logical EEPROM is fully partitioned. Adding an 8 KiB inactive VIA
bank by doubling the RP2040 wear-level backing would also add a 16 KiB RAM cache
on every half. The physical SRAM can hold that increase, but the current linked
`.data + .bss` regression policy cannot, and no runtime allocator high-water
evidence justifies changing that policy yet.

The split already has two durable VIA copies. During a transaction the non-USB
half becomes the inactive VIA staging copy while the USB half continues serving
the old generation. This uses existing physical storage and makes the ordering
explicit instead of allowing ordinary write-through commands to expose a
partially replaced bank.

## Commit Sequence

1. The host captures the complete old profile and saves its recovery file.
2. Firmware acquires the custom candidate lease on the USB half.
3. The host streams target VIA regions through the candidate channel. The USB
   half relays them to the peer only; ordinary VIA write-through and
   reconciliation are fenced for this transaction.
4. The peer computes the canonical VIA digest. A mismatch aborts before durable
   transaction intent exists, and the old USB-side VIA copy restores the peer.
5. The custom candidate is validated and prepared on both halves with the exact
   target VIA identity. Both prepared markers become durable.
6. The USB half writes its custom commit marker. This is the logical decision
   point. The owner first waits for a safe behavior boundary, reporting the
   wait as peer-status flag bit 2, so a held key or locked layer can still end
   the Apply cleanly on the pre-decision timeout. The USB half keeps typing
   the complete old generation through the decision: its VIA bank and active
   custom profile are both still old. Runtime activation remains blocked
   because its VIA store is still old.
7. The peer commits the same custom record, accepts its staged VIA generation,
   and becomes the complete new replica. The owner requests that VIA ACCEPT
   only at a safe behavior boundary (flag bit 2 again while it waits) and
   fences key output from the request until activation: from ACCEPT the USB
   half's VIA bytes, custom behaviors, or both may belong to different
   generations. Entering the fence at a safe boundary matters because
   activation needs the same boundary and fenced keys could not release a
   hold or unlock a layer. A press made during the fence also loses its
   release. Boot, discovery and owner error states keep typing on the loaded
   keymap, except while a restart left the local VIA bank suspect (see
   Identity).
8. After the decision is observable, the host waits for the peer VIA accept and
   writes only the changed VIA ranges to the USB half. A custom-only change uses
   one verified no-op keycode write to advance the bound VIA generation. The
   peer remains the complete recovery copy throughout this roll-forward.
   Ordinary split VIA mirroring is suppressed during staging and roll-forward,
   including mirror frames queued before staging began. A pause between local
   VIA write chunks cannot publish an intermediate digest or consume the target
   VIA generation. If no local VIA write arrives for
   `VIA_SPLIT_SYNC_LOGICAL_ROLL_FORWARD_IDLE_MS` (15 s) after ACCEPT or the
   last write, the USB half releases the hold itself, exactly as a reboot
   would. Its partly written bank stays dirty, or stays at the old generation
   if nothing was written, so ordinary reconciliation pulls the peer's complete
   accepted copy over it. The discarded partial mutation is covered by that
   pull. A late host write carries the same target bytes, so it only restarts
   the pull, which then converges.
9. The effective-profile owner activates at the existing safe behavior boundary
   once the peer has confirmed the new generation and this half holds it
   completely. The peer's confirmation is its durable custom commit, observed
   by the USB half, and its acknowledged VIA ACCEPT; ACCEPT is sent only while
   the peer is in sight. This half's part is its VIA bank clean at the target
   generation and digest. The peer is not observed a second time, so a link
   lost during roll-forward cannot hold activation; nothing activates on one
   half before the other has confirmed it. Ordinary reconciliation compares
   both halves again when the link returns, and a conflict there still ends
   in the existing conflict states.

## Power-Loss Outcomes

| Last durable point | Recovery authority | Visible profile |
| --- | --- | --- |
| Before both prepared markers | old USB-side committed manifest and VIA copy | old |
| Both prepared, before USB commit marker | old manifest; prepared work may be discarded | old |
| USB commit marker durable | new manifest; peer staging copy supplies target VIA bytes | output blocked until new is complete |
| Both custom markers durable, USB VIA incomplete | new peer replica | output blocked until new is complete |
| Both logical identities converged | new manifest | new |

An acknowledgement loss is resolved from markers and identities. A missing peer
before the decision is a safe abort. A missing peer after the decision is an
explicit recovery state because the only complete target VIA copy may be there.
It resumes by itself when the link returns, with no restart: before ACCEPT
the USB half keeps typing the old generation meanwhile. After the peer's
acknowledged ACCEPT, the host finishes rewriting the USB half and it activates
without the link (step 9). Output stays fenced across a lost link only when
ACCEPT is requested in the instant before the USB half notices the link is
gone (its acknowledgement then waits for the peer), or when the host also
disappears mid-rewrite: no complete generation exists on the USB half, and
the firmware roll-forward needs the peer's copy. Unplugging the link cable also powers the
peer off. A peer that rebooted keeps its durable prepared custom record and
staged VIA bank but not its volatile leases, so the USB half sends the custom
copy again from its own committed record (logical copies rebind first) before
the authorized commit, and the peer's ACCEPT matches the staged bank by its
persisted generation and recomputed digest.

## External VIA Writers

Ordinary VIA writes remain supported outside a logical transaction. Firmware
first converges VIA storage on both halves, then adopts its new VIA identity as
a new logical manifest generation bound to the unchanged custom digest. Until
adoption finishes, Studio reports an external-change transition and refuses a
stale Apply.

## Performance Contract

The host reads a complete snapshot once for review and recovery. After acquiring
the candidate lease, it checks custom identity, VIA identity, and settings digest
without rereading the 7,191-byte macro bank. It transfers only changed 28-byte
VIA blocks. Success uses exact changed-block readback plus stable custom and VIA
identities on both halves; Refresh and Export remain independent full reads.

The logical staging channel carries 12 data bytes per report because every chunk
also carries transaction, region, generation and digest correlation. Apply keeps
the differential range selection, so ordinary edits still avoid transferring the
unchanged macro capacity. Firmware holds ordinary full-store reconciliation after
the decision while the connected host performs this differential roll-forward.
If the host disappears, the firmware roll-forward (step 8) or a reboot clears
that volatile hold, and reconciliation copies the peer's complete target.

## Implementation Status

The storage format, candidate binding, peer VIA staging channel, two-half durable
prepare, USB-side decision marker, peer commit, VIA roll-forward, activation
gate, boot recovery fence, and app coordinator are implemented. Host tests cover
prepared-marker reboot selection, incompatible firmware identities, predecision
abort, decision ordering, role recovery, bound-record split transfer, staged VIA
acceptance, reboot recovery, and the firmware roll-forward after the host goes
quiet mid-write, never writes, or writes late during the pull; a lost peer link
after the decision, with and without the peer rebooting, resuming on reconnect;
ACCEPT waiting for a safe boundary while input still works; ACCEPT never sent
to a peer known to be unreachable; and activation with the link lost during
roll-forward. The firmware advertises atomic logical Apply as
a required write capability, so the app refuses complete Apply on older images.

Physical interruption tests at every durable boundary and external VIA-writer
adoption remain acceptance work. The recovery file remains part of Apply while
that hardware matrix is incomplete.

## Implementation Gates

- storage-format and boot-scan tests for prepared and committed markers;
- candidate protocol tests for VIA bind, relay, duplicate chunks, abort, and
  acknowledgement loss;
- split tests for every row in the power-loss table;
- effective-owner tests proving a bound-VIA mismatch cannot activate or emit
  normal output;
- external VIA adoption and conflict tests;
- feature-gate, stack, full host, and firmware builds;
- physical interruption tests at every durable boundary.
