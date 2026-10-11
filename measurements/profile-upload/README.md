# Profile upload timing

Measures the host candidate's copy and full validation separately. HID traces
give wall-clock progress; the firmware host harness gives deterministic scan
grants. A replay is not a hardware scan counter or input-cadence measurement.
The governing contract is
[upload work and elapsed time](../../docs/architecture/profile-wire-v1.md#upload-work-and-elapsed-time).

## Capture procedure

1. Name the clean firmware commit, BK pin, flashed pair and client revision.
   Keep the keyboard idle, with both halves connected and storage ready. Obtain
   explicit permission before re-Applying even an unchanged profile. Never
   flash as part of a capture.
2. Take the shared keyboard lock with
   `until mkdir /tmp/charybdis-keyboard.lock 2>/dev/null; do sleep 5; done`.
   Arrange `rmdir /tmp/charybdis-keyboard.lock` on every exit. Run local Node
   probes from `/tmp`, using explicitly selected client modules. Firmware host
   verification never loads those modules.
3. Read a coherent complete profile and retain its exact active custom blob
   locally. Keep this blob out of public captures: it can contain personal
   configuration. Note its length, CRC32 and FNV-1a digest so the replay can be
   matched to the upload declaration.
4. Wrap the client's existing `connection.request` during the upload step.
   After each request completes, record milliseconds from a common start.
   Record BEGIN, REUSE and VALIDATE requests; decode existing candidate-status
   page-0 replies and record `state`, `nextOffset`, `mailboxPending` and
   `operationSequence`. Observe requests the client already sends; an extra
   polling loop changes request contention. The initial set uses the fields
   `t`, `state`, `offset`, `pending`, `seq` and `command` for those records.
5. Re-Apply the exact document just read, save the trace directly as JSON, and
   confirm the operation succeeds. Save upload-step start/end times and request
   counts in the set record. Release the lock after closing the connection.
6. Replay the retained blob with the firmware harness:

   ```sh
   QMK_ROOT=/path/to/pinned-bk sh tests/host/run_profile_domain_owner_round_trip_tests.sh /tmp/capture/profile.bin
   ```

   Save stdout unedited. The optional argument exercises the real owner,
   candidate, store, validator and durable-I/O scheduler, with hardware
   eligibility and VIA callbacks injected. Both ordinary and sanitized runs
   must agree. The idle case grants the candidate every scan; the busy case
   models a continuously ready VIA worker and tests round-robin contention.
   Neither injects physical flash latency, keyboard tasks or split transport.
7. Split observed time at the first REUSE request, the VALIDATE request, and
   the first VALIDATED status. Status completion is sampled at HID/poll
   boundaries, so these intervals include host overhead. Count reuse pieces
   per request, not across the whole payload. Compare the observed intervals
   with the corresponding replay grants. Report implied milliseconds per
   grant as an inference, not a directly measured matrix-scan period.

## Validity and comparison

- The Apply must succeed with the same payload identity it began with.
- The replay blob must match the trace's length and both checksums. Its bytes
  remain local; the public trace includes no profile payload or VIA contents.
- Name extra pollers or other keyboard work; it affects timing. The lock
  prevents another agent driving the device during the capture.
- An upload step can include client identity reads and full firmware
  validation. Separate those from copy time before attributing a slow step.
- Compare builds with the same profile, client/polling policy, host and idle
  conditions. Increasing a budget also requires a pointing/typing cadence
  capture and interruption acceptance; this trace alone cannot approve it.

## Sets

| Set | Firmware | Purpose |
| --- | --- | --- |
| [2026-10-11 unchanged baseline](2026-10-11-unchanged-baseline/README.md) | `00bc0f5c`, dev pair 51 | Explain copy and validation time for the same 3,760-byte custom profile |
