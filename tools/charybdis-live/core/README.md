# Charybdis Live Core

This is the app itself: everything except the VS Code shell and the webview.
It has no VS Code or webview dependency. Its layers — `data`, `transport`,
`schema`, `protocol`, `model`, `session` — import in one direction only; the
table is in [`../AGENTS.md`](../AGENTS.md) and `tests/layering.test.js`
enforces it.

## Reading the keyboard

`session/profile-device-service.js` enumerates matching interfaces, keeps
native paths and handles out of the webview, and reads what the keyboard says
about itself: VIA and firmware versions, the Profile Wire capability and status
pages, the layout, the committed profile, combos, macro banks, settings,
keyboard options and the base RGB effect. It publishes a sanitized snapshot;
`session/device-model.js` turns that into the `model` the webview renders, and
`session/panel-session.js` adds the draft and the panel's own state to it,
routes each message the webview posts, and runs the Rename & Reorder panel — so
`extension.js` keeps only VS Code's dialogs, files and progress.

Custom Profile Wire pages use one monotonically increasing nonzero request-id
sequence per connected session (wrapping `255` to `1`), so a delayed response
from an older request cannot satisfy a newer one.

## Editing and applying

Every edit is staged in `session/profile-draft-session.js`: a complete
portable profile (`model/portable-profile.js`) with undo and redo history, a
review built by `model/profile-review.js`, and staleness checks on every
message (draft id, revision, and the base the form was built from). The domain
edits themselves live in `session/device-profile-edits.js` and
`session/key-behavior-edits.js`.

Apply goes through `session/portable-profile-session.js`: a recovery copy is
written before the first device write, the profile is staged on both halves
through the candidate mailbox (`protocol/profile-candidate-v1.js`,
`session/candidate-upload-coordinator.js`) and the peer VIA stage
(`session/logical-via-stage-coordinator.js`), and one generation is published.
Staging never retries an ambiguous transport outcome, commit is
transaction/digest-correlated and idempotent across a lost acknowledgement, and
success is accepted only after a fresh status read reports the new generation
on both halves. A keyboard the app cannot open a draft for is read-only; the
host refuses edits rather than writing them directly.

## Byte formats

`schema/profile-blob-v1.js` encodes and strictly decodes the `NLP1` header and
its ordered domain envelopes, the four-byte semantic action values, and the
FNV-1a 32-bit and CRC32 helpers. Unknown domains are rejected. The domain
codecs — `rgb-domain-v1.js`, `key-behavior-domain-v1.js`,
`combo-domain-v1.js`, `pd-mode-domain-v1.js`, `settings-domain-v1.js` — are
exact inverses of their decoders and enforce the firmware's limits. The RGB
byte contract is frozen in [`schema/rgb-domain-v1.md`](./schema/rgb-domain-v1.md);
its firmware counterpart is `users/noah/lib/profile/schema/profile_rgb_v1.c`.
`schema/compiled-profile-v1.js` resolves expressions in the keyboard's
vocabulary (`KC_A`, `LT(1, KC_A)`, `DRAGSCROLL`, `VIA_MACRO_3`) to actions and
native keycodes, refusing any whose bits would land on another keycode.

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

When nothing on the keyboard handles a report, QMK answers with the request
echoed back and byte 0 set to `0xFF`. Every protocol module accepts exactly that
echo of its own request (`protocol/via-unhandled-v1.js`) and treats it as a
definite refusal: a stable error such as `UNHANDLED` or `KEYBOARD_NOT_READY`,
with the connection left open. A `0xFF` report that is not an echo of the
request — another client's reply — is never taken for an answer.

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

```sh
npm run check   # syntax across the tree, then every test
npm test        # the tests alone
```

The tests use Node's built-in runner and the fake adapter. Golden vectors are
read from the firmware repository's `tests/fixtures/` and the upstream
`keyboard.json` in `../bastardkb-qmk`, so the suite expects this checkout's
sibling layout; the app itself never reads either at runtime.
