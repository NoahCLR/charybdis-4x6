# Charybdis Live — Agent Instructions

This app edits the **connected keyboard**. It is the app; v1 beside it is frozen
and no longer maintained, and Profile Studio, frozen too, edits the `.c` files.
Do not blur those lines.

Read [`docs/LIVE_EDIT_APP_DIRECTION.md`](../../docs/LIVE_EDIT_APP_DIRECTION.md)
before changing anything here. It carries the goal, the decisions, and what is
deliberately still undesigned. The interface direction and its prototype are in
[`design/`](./design/).

## The one rule

**Nothing in this app may read the firmware repository.** No parsing `keymap.c`,
`config.h`, or `rgb_config.c`. No walking a QMK checkout at runtime. No
`vscode` import below `extension.js` and `panel-html.js`.

Two sanctioned exceptions, both by-hand build steps that write checked-in or
throwaway files and are never imported by the app:

- `scripts/generate-keycode-catalog.js` regenerates the vendored catalog.
- `scripts/preview.js` renders the interface against the test fixtures.

## Layout

```
extension.js        VS Code surface only: command, panel, message relay
panel-html.js       the panel's HTML shell; the only host file that knows webview URIs
core/               the app, with no host dependency
  transport/        device adapters and the request coordinator
  schema/           profile byte formats and domain decoders
  protocol/         wire formats spoken to the device
  model/            complete portable documents and layer-reference rewrites
  session/          stateful orchestration across a connection
  data/             vendored data, e.g. the keycode catalog
webview/            the interface: browser ES modules, no build step
  lib/              colour and DOM helpers (pure)
  view/             model → presentation (pure, tested)
  ui/               screens and components
  styles.css        the design system
design/             the interface direction and its clickable prototype
scripts/            developer entry points and build steps
tests/              mirrors core/, plus the view modules and the posted payloads
```

## Layer rules

Dependencies point one way. Adding an import that violates this is the moment to
stop and reconsider, not to work around. `tests/layering.test.js` enforces it.

| Layer | May import |
| --- | --- |
| `data/` | nothing — inert vendored content |
| `transport/` | `data/` |
| `schema/` | `data/` |
| `protocol/` | `transport/`, `schema/`, `data/` |
| `model/` | `schema/`, `data/` |
| `session/` | `transport/`, `protocol/`, `schema/`, `model/`, `data/` |
| `webview/` | nothing from `core/` — it receives the model as a message |

Every layer may also import from itself.

## Where new work goes

- A new device command or wire format → `protocol/`
- A new profile domain or byte layout → `schema/`
- Anything holding state across a connection → `session/`
- Portable documents, completeness and layer-reference rewrites → `model/`
- A new screen or component → `webview/ui/`
- A rule about what something *means* (a colour, a key face, a locality) →
  `webview/view/`, with a test. These modules are pure on purpose.

## Conventions

- Every `core/` module gets a test in the matching `tests/` directory.
- Every payload the webview posts gets a test in `tests/edits.test.mjs` that
  stages it against a real `ProfileDraftSession`, so a change of message shape
  fails there rather than on a keyboard.
- Build DOM nodes, never HTML strings from device values; everything from the
  keyboard goes through `esc()` or `textContent`.
- Decode defensively. Everything arriving from a device is untrusted input:
  validate length, reject noncanonical encodings, and fail with a stable code
  rather than a guess.
- Report device state verbatim, including zeros. A blanked value misreports the
  keyboard.
- Validate what the device rejects, not more. A profile the firmware would
  accept must not be refused here; where something is merely inert — a key bound
  to an empty pointing slot, for instance — the profile carries the fact and the
  interface explains the consequence.
- Colour comes from the model, never from a constant. If a surface shows a hue,
  it asks `view/lighting.mjs` for it, and a stage that is off must look off.
- Edits are posted, never applied locally. The draft lives in the host, so what
  is drawn is what would be applied.

## Verification

```sh
npm run check                 # syntax across the tree, then all tests
npm run keycodes -- --check   # fails if the vendored catalog has drifted
npm run preview               # then serve the folder and open dev/index.html
```

The repo's `sh tests/host/run_tooling_checks.sh` runs this app's checks too.
