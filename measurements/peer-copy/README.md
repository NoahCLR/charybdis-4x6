# Peer copy timing

Measures the copy of an unchanged committed custom profile to the other half
through the ordinary logical Apply, including receiver validation and
completion collection.

## Capture procedure

1. Record the exact firmware pair and userspace/QMK commits running on both
   halves. Do not flash as part of an agent capture.
2. Obtain Noah's authorization to re-Apply the current profile. Close other
   clients and take `/tmp/charybdis-keyboard.lock` with `mkdir`; wait while it
   exists, and release it with `rmdir` on every exit.
3. Use a scratch Node probe through the client's ordinary portable-profile
   restore path. Read the current complete profile, retain the recovery copy,
   and restore it unchanged. Do not toggle macros or edit any domain.
4. Record Apply step timestamps and candidate status page 1 transitions,
   polling about every 3 ms between completed HID requests. Include phase,
   status, offset, length, BUSY reason/state, transport failures, retry count
   and BUSY streak. Save stdout verbatim as `unchanged-N.txt`; the text format
   preserves the probe's original output. Profile bytes and recovery documents
   stay outside the repository.
5. Where supported, also collect candidate status page 2 at phase boundaries:
   reused bytes, reuse attempts, source-unavailable replies, reconciler scan
   invocations and RPC attempts. Read page 1 around each diagnostic sample to
   detect transitions. Older firmware returns `UNKNOWN_PAGE`.
6. Repeat unchanged under the same conditions. Report status-sampling
   uncertainty: these host timestamps are observations, not device-side scan
   timestamps. Page-1 retries alone do not give a scan count. Separate copy
   work, validation and time until the sender collects completion.

Comparisons require the same profile length/content, host, polling workload,
USB orientation and otherwise idle keyboard. A new firmware build is a new
set. Exact scan/RPC counts require page 2; serial transaction timing requires
the optional split recorder described under `pointing-cadence/`.

## Sets

| Set | Firmware | Purpose |
| --- | --- | --- |
| [2026-10-11-pair-51](2026-10-11-pair-51/README.md) | `00bc0f5c`, pair 51 | Establish whether steady-state committed-source reuse occurs and observe validation polling |
