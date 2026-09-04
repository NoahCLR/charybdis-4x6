# Charybdis Live Core

This is the app itself: everything except the VS Code shell. It is layered
`transport` -> `schema` -> `protocol` -> `session`, with imports pointing one
way; see [`../AGENTS.md`](../AGENTS.md).

It has no VS Code or webview dependency. The coordinator and protocol layers have no VS Code or webview
dependency. The concrete desktop adapter keeps `node-hid` lazy-loaded and
injectable so the same boundary remains usable in tests and harnesses.

`session/profile-device-service.js` owns the Stage 01 read path and the explicitly gated
engineering live-apply path. It enumerates devices, keeps native paths and
connected handles outside the webview, reads the capability and status pages,
and publishes a sanitized snapshot for the app UI. Its compatibility result
checks protocol and schema majors, 32-byte framing, Milestone A domain support,
the current source profile against advertised capacities, and all four
candidate/persistence/activation/peer capability bits before enabling writes.
It also requires fresh firmware status proving that the peer is known and both
halves are converged. Ordinary firmware remains read-only.

Each refresh first reads the standard VIA protocol and firmware versions, then
cross-checks the firmware version reported by Profile Wire. Custom Profile Wire
pages use one monotonically increasing nonzero request-id sequence for the
whole connected session (wrapping `255` to `1`), so a delayed response from an
older refresh cannot satisfy a newer request.

## Canonical profile blob foundation

`profile-blob-v1.js` is the desktop-side Stage 02 codec foundation. It encodes
and strictly decodes the `NLP1` header and ordered domain envelopes. Unknown
domains remain rejected by default; dedicated domain codecs compose through
the explicit domain-version registry.

The same module owns four-byte semantic action values, including bounded
logical-layer, PD-mode, VIA-macro, and hardcoded-macro operands, plus canonical
FNV-1a 32-bit and CRC32 helpers. Its stream readers return exact next offsets
so later domain decoders can compose them without accepting implicit padding or
trailing bytes. `compiled-profile-v1.js` converts Profile Studio's parsed model
into that exact canonical blob for the RGB and key-behavior Milestone A
domains.

`profile-candidate-v1.js` is the exact desktop codec for the Stage 02 candidate
mailbox: begin, sequential chunks, validate, custom-save commit, abort,
immediate admission, and operation status. `candidate-upload-coordinator.js`
derives metadata from a canonical blob, prepares it without activation, and
offers a separate commit call for D-013's source-then-device ordering. Staging
never retries an ambiguous transport outcome and performs an idempotent abort
after deterministic failure. Commit is transaction/digest-correlated,
idempotent across a lost acknowledgement, waits through bounded committing and
safe-activation states, and reports final-marker uncertainty as an ambiguous
outcome requiring status reconciliation. The device service now uses this
coordinator for the engineering `Apply live` operation and accepts success only
after a fresh general-status read reports the exact candidate digest as both
committed and active. Refresh also reads candidate status. A matching
recoverable transaction is resumed instead of uploaded over, while a mismatched
or unsafe active candidate is refused with an explicit two-half cold-recovery
instruction. Production firmware routing and capability advertising remain
disabled; only a firmware built with the separate owner and mutation gates
exposes this path.

`rgb-domain-v1.js` is the desktop half of the domain `0x10` v1 codec. It covers the
complete Milestone A RGB surface, canonicalizes Profile Studio's parsed model,
assigns name-independent ids to 58-bit LED-group bitmaps, preserves ordered
renderer rows, and enforces compiled stages, complete identity tables,
brightness, references, and fixed capacities. The exact byte contract and
intentional runtime/device deferrals are frozen in
[`schema/rgb-domain-v1.md`](./schema/rgb-domain-v1.md). The matching firmware decoder is the
reader-backed `users/noah/lib/profile/schema/profile_rgb_v1.c` implementation.

## Adapter contract

An injected device adapter implements:

```js
listDevices() -> Promise<Array<{id: string, ...metadata}>>
connect(deviceId) -> Promise<ConnectedDevice>
```

Each connected device implements:

```js
write(report32) -> Promise<void>
onReport(listener) -> disposer
onDisconnect(listener) -> disposer
close() -> Promise<void>
```

Reports are always `Buffer` or `Uint8Array` values of exactly 32 bytes. Adapters
may return a disposer function or an object with `dispose()` from either event
subscription.

`FakeDeviceAdapter` implements the same contract for unit tests.

## Native desktop adapter

`NodeHidDeviceAdapter` is the concrete `node-hid` 3.x implementation. Requiring
its module does not load `node-hid`; the native dependency is first requested
when `listDevices()` or `connect()` runs. Tests may inject either `hidModule` or
`loadHid` into its constructor.

Enumeration is restricted to all four QMK Raw HID identifiers:

- vendor id `0xA8F8`
- product id `0x1833`
- usage page `0xFF60`
- usage `0x61`

Only paths returned by a matching enumeration may be opened. On macOS the
adapter requests non-exclusive access so a desktop connection does not claim
the full HID device. The adapter boundary always uses 32-byte protocol frames;
native writes are prefixed with the zero report-id byte required by `node-hid`,
and reads accept either 32 protocol bytes or 33 bytes with that zero prefix.
Any other native report invalidates the device session.

The read-only CLI lists matching interfaces without opening or writing to one:

```sh
npm run probe:live-link
npm run probe:live-link -- --json
```

## Coordinator contract

Create one `DeviceRequestCoordinator` for an adapter, then call `connect(id)` to
obtain a `CoordinatedDeviceConnection`. Connections for different device ids
have independent queues. A single device sends exactly one request at a time.

```js
const connection = await coordinator.connect(deviceId);
const response = await connection.request(report, {
    timeoutMs: 1000,
    signal,
    matchResponse(responseReport, requestReport) {
        return responseReport[0] === requestReport[0];
    },
});
```

The default matcher compares byte 0. Versioned custom live-profile frames must
supply a matcher that also checks their transaction id.

Cancellation of a queued request removes only that request. Timeout,
cancellation, write failure, malformed input, or matcher failure after a
request has reached the device invalidates the whole connection. This is the
stale-reply isolation rule: a request with an ambiguous outcome is never
followed by another request on the same HID session. Reconnect before retrying.

Stable error codes are exported as `LIVE_LINK_ERROR_CODES`:

- `ALREADY_CONNECTED`
- `CANCELLED`
- `CONNECT_FAILED`
- `DEVICE_NOT_FOUND`
- `DISCONNECTED`
- `ENUMERATION_FAILED`
- `INVALID_ADAPTER`
- `INVALID_REPORT`
- `NATIVE_MODULE_UNAVAILABLE`
- `NOT_CONNECTED`
- `RESPONSE_MATCH_FAILED`
- `TIMEOUT`
- `WRITE_FAILED`

The UI should branch on `error.code`, not error message text.

## Tests

Run:

```sh
npm run test:live-link
```

The tests use only Node's built-in test runner and the fake adapter.
