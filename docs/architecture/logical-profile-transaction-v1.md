# Profile Transaction V1: One Store

> Current firmware accepts only the formats it writes (D-F10, D-F14): profile
> schema 4.0; RGB v4, key behaviors v2, combos v3, settings v8, sparse PD v3, keymap v1 and macros v1;
> a slot-sized profile payload; and store format 6 (D-F28). Profile Wire is
> 2.0 and split framing version 4, with 32-bit profile lengths and offsets
> throughout. Older profile/store formats and the legacy GET 9 source page are
> rejected. A backup in an earlier format is refused, not translated.
>
> The file keeps its name, and the store its "logical" marker vocabulary, from
> the two-store design it replaces: until D-F31 every Apply also bound, staged
> and rolled forward the VIA keymap and macro store. VIA is not supported
> since D-F31, and the profile is the only store.

## Purpose

One Apply publishes one profile generation, the complete Profile Wire payload
with the keymap and macros in it (D-F30), on both halves. A reset or loss of
power may leave recovery work, but key output observes a complete profile:
the old generation, the new generation, or the compiled defaults during boot
reconciliation, never a mixture. At boot the USB half selects its new committed
record but types compiled defaults until the halves reconcile.

## Identity

A stored record carries its generation, origin half, payload length, CRC32
and FNV-1a digest, domain mask, override flag, compiled-default digest and
action ABI digest. The slot header has two marker states, `prepared` and
`committed`. The prepared marker is durable transaction intent but is never
active authority. The committed marker is the decision record. Format-1 `NP`,
format-2 `NQ`, format-3 `NR`, format-4 `NS` and format-5 `NT` records are
rejected at boot and admission; firmware does not migrate old storage, and
status reports an earlier format or retired format-6 test layout (Profile Wire
state flag 9), including slots carrying the removed VIA binding.

Format 6 (D-F28, extended by D-F30) binds schema 4.0. Its 36-byte header
stores the full seven-bit domain mask at byte 0 and a uint32 payload length
at bytes 1–4. Origin and override flags have their own byte 5. Generation,
payload CRC32, compiled-default digest and action ABI follow as uint32 fields
at 6, 10, 14 and 18. Bytes 22–32 are reserved zero and refused otherwise
(22–29 held the bound VIA generation and digest until D-F31); CRC16 at 33
covers the format version and bytes 0–32; byte 35 is the marker, published
last. Validation derives the payload digest again. See
[the storage contract](device-resident-profile.md#profile-domains-own-layout-and-macros)
and, for how the record lies on flash, D-F29.

The effective-profile owner publishes only a committed record whose payload
it has validated in full. A half running a profile always runs one complete
generation: activation swaps the whole profile at a safe behaviour boundary,
and nothing outside the profile is rewritten under it, so no input fence is
needed.

## Commit Sequence

1. The host captures the complete old profile and saves its recovery file.
2. Firmware acquires the candidate lease on the USB half. BEGIN declares the
   candidate's identity; its bytes 24–31 are zero.
3. The host streams the candidate, reusing ranges of the active profile where
   it can, and the USB half validates it.
4. On COMMIT the USB half copies the candidate to the other half, which
   validates it and writes a durable prepared record; the USB half writes its
   own. A copy that stops for good, or a peer that answers a terminal error,
   cancels the Apply before anything is decided.
5. The USB half writes its commit marker. This is the decision point. The
   owner first waits for a safe behaviour boundary, reporting the wait as
   peer-status flag bit 2, so a held key or locked layer can still end the
   Apply cleanly on the pre-decision timeout. The USB half keeps typing the
   complete old generation through the decision.
6. The USB half authorizes the peer's commit, and the peer commits the same
   record.
7. Once the USB half has seen the peer holding exactly that record, it
   authorizes activation, and both halves activate at the next safe behaviour
   boundary. The peer is not observed a second time, so a link lost after
   that confirmation cannot hold activation; nothing activates on one half
   before the other has committed. Ordinary reconciliation compares both
   halves again when the link returns, and a conflict there still ends in the
   existing conflict states.

## Power-Loss Outcomes

| Last durable point | Recovery authority | Visible profile |
| --- | --- | --- |
| Before both prepared markers | old committed record on each half | old |
| Both prepared, before USB commit marker | old record; prepared work may be discarded | old |
| USB commit marker durable | new record; the peer's prepared copy commits when it is reached | new; compiled defaults until the halves reconcile |
| Both commit markers durable | new record on both halves | new; compiled defaults until boot reconciliation completes |

An acknowledgement loss is resolved from markers and identities. A missing
peer before the decision is a safe abort. A missing peer after the decision is
a recovery state that resumes by itself when the link returns, with no
restart, while the USB half keeps typing the old generation if it has not
rebooted; after reboot it types compiled defaults until reconciliation. Unplugging the
link cable also powers the peer off. A peer that rebooted keeps its durable
prepared record but not its volatile leases, so the USB half sends the copy
again from its own committed record before the authorized commit. A USB half
that restarts alone ends the peer's prepared copy at the first of its metadata polls the peer takes
once it has read its own record: discarded before its commit marker,
committed after it ([split protocol](profile-split-v1.md#an-abandoned-prepared-copy-d-f34)).

## Cancellation And Lease Ownership

Only the profile owner ends a candidate. The host may abort before the
decision; after it, the keyboard refuses the abort and finishes the
generation, even when the host never saw the decision, because a host whose
status read failed after the marker became durable cannot tell a decided save
from an undecided one.

Every cancel before the decision takes the owner's one cancel path: the host's
candidate abort, the 15-second lease, peer supersession, a failed copy to the
peer, and a storage failure. It releases the candidate and aborts the peer's
prepared copy. Cancel is bounded without the peer: the candidate is released
at once, this half's old record is untouched, and the peer's ABORT completes
when the link returns. From COMMITTING on nothing expires or cancels.

A BEGIN retry with the current transaction ID and different metadata is
refused as `CONFLICTING_RETRY`; it cannot replace a durable-phase state with
`REJECTED` or change the decided identity. Identical retries remain
idempotent.

## Performance Contract

The host reads a complete snapshot once for review and recovery. Custom
uploads reuse exact active-source ranges and verify bounded batches of
streamed chunks; the copy to the other half reuses the same ranges from its
own active profile and sends the rest in chunks of up to 106 bytes. Both
halves still prepare the complete candidate and validate it; no durable
boundary is skipped. See
[differential candidate transfer](profile-wire-v1.md#differential-candidate-transfer).
Success uses exact readback of the committed identity on both halves; Refresh
and Export remain independent full reads.

## Implementation Status

The storage format, two-half durable prepare, USB-side decision marker, peer
commit, activation gate and app coordinator are implemented. Host tests
cover prepared-marker reboot selection, incompatible firmware identities,
predecision abort and every pre-decision cancel, decision ordering, role
recovery, split transfer, reboot recovery, a lost peer link after the
decision with and without the peer rebooting, the decision and activation
each waiting for a safe boundary while input still works, activation after
the link is lost once the peer was confirmed, and power cut at sampled points
of a maximum-profile save. The recovery file remains part of Apply while
physical interruption tests at every durable boundary are incomplete.

## Implementation Gates

- storage-format and boot-scan tests for prepared and committed markers;
- candidate protocol tests for duplicate chunks, abort, and acknowledgement
  loss;
- split tests for every row in the power-loss table;
- owner tests proving nothing activates before the peer is confirmed;
- feature-gate, stack, full host, and firmware builds;
- physical interruption tests at every durable boundary.
