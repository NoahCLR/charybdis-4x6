# Logical Profile Transaction V1

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

The effective-profile owner may publish a committed custom record only while
the local VIA identity matches the record's bound VIA identity. A mismatch is a
recovery state and blocks normal output until reconciliation completes.

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
   point. Runtime activation remains blocked because its VIA store is still old.
7. The peer commits the same custom record, accepts its staged VIA generation,
   and becomes the complete new replica.
8. VIA snapshot reconciliation copies the peer's target VIA regions to the USB
   half. The USB half verifies and accepts the target VIA generation.
9. Once both halves report the same logical manifest and no transfer is pending,
   the effective-profile owner activates at the existing safe behavior boundary.

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

The optimized host path implements this transfer rule before the new firmware
transaction lands. It does not by itself make the two stores atomic.

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

