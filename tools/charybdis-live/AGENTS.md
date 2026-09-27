# Charybdis Live — Agent Instructions

This app edits the **connected keyboard**. It is the only live app; Profile
Studio, which is frozen, edits the `.c` files. Do not blur that line.

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
- The words for a value the keyboard stores (a hold helper, a locality, a
  pointing axis, a stage) → `core/model/vocabulary.js`. It reaches the
  interface as `model.vocabulary` (read it through `webview/view/vocabulary.mjs`);
  never write an enum-to-words table in a screen. A new enum value without a
  word fails `tests/model/vocabulary.test.js`.
- Naming an action, its native keycode, decode limits, a "Layer n" reference →
  `core/schema/actions.js`. The pointing-mode keycode registry and the key
  layout are inert data in `core/data/`, readable from every layer; the
  interface gets a slot's bindings as `pdModes[].binding`.
- What a panel control does in order (read, apply, discard, rebase, import,
  layers) → `core/session/panel-controls.js`. `extension.js` passes in only
  what a host has: progress, the recovery file, a chosen profile file.

## Conventions

- Decode a document once. A snapshot may carry its read-only `decoded` form;
  take it with `decodedOf(snapshot)`, which decodes afresh when the snapshot's
  document is not the one it was decoded from. Draft history entries are
  frozen and decoded once each, so a model publish decodes nothing new.
- A webview number that is also a wire number (pointing kinds, axes, buttons,
  modifier bits) is pinned to its schema by `tests/enum-drift.test.mjs`.
- Whether an area can be edited is `canEdit(area)` in `webview/store.mjs`,
  nowhere else.

- Every `core/` module gets a test in the matching `tests/` directory.
- Every edit the webview posts is built by a pure function in
  `webview/view/edits.mjs` (pointing records by `view/pointing-config.mjs`),
  and screens post what those return rather than assembling objects inline.
  `tests/edits.test.mjs` stages each builder's output against a real
  `ProfileDraftSession`, so a change of message shape fails there rather than
  on a keyboard.
- Markup is built with `el()` from template strings, so every value from the
  keyboard or the host goes through `esc()` (or `textContent`) — including in
  attributes. Nothing unescaped reaches `el()` or `innerHTML`.
- Decode defensively. Everything arriving from a device is untrusted input:
  validate length, reject noncanonical encodings, and fail with a stable code
  rather than a guess.
- Report device state verbatim, including zeros. A blanked value misreports the
  keyboard.
- Validate what the device rejects, not more. A profile the firmware would
  accept must not be refused here; where something is merely inert — a key bound
  to an empty pointing slot, for instance — the profile carries the fact and the
  interface explains the consequence.
- Look a key up by what its value means, never by what the keyboard calls it.
  A position carries both: `keycode` is the stored name (`QK_USER_16`, or bare
  hex where the vocabulary names nothing) and `semantic` is the name every other
  domain uses (`PD_SLOT_0`, `VIA_MACRO_0`, `LEFT_THUMB`). Behaviour rows, macro
  slots and pointing slots are keyed by the second, so a lookup goes through
  `keyMeaning(position)`; matching on `position.keycode` silently finds nothing.
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

### Checking it the way the panel renders it

`dev/index.html` is this interface alone. The panel is this interface *inside*
the host's own stylesheet, which arrives in a cascade layer — so it loses to
every property this sheet declares, and wins every property it does not. That
has already cost us twice: `body { padding: 0 20px }` squeezed the whole app,
and the host's `code { background; color; padding; border-radius }` turned every
inline keycode into a coloured chip in a black-and-white interface.

`npm run preview -- --vscode` writes `dev/vscode-dark.html` and
`dev/vscode-light.html`: the same page with the host's real stylesheet and theme
colours read out of the installed app, under the theme class it puts on `<body>`.
Look at a new surface there, not only in `dev/index.html`.

### Proving a control is wired

A control that posts nothing looks exactly like one that works, and no unit test
sees it: the tests stage payloads, they do not press buttons. So when a screen
gains an editable control, drive it in the preview, which logs every post to
`window.__posted`:

```js
// change one control, then press the surface's own save button if it has one
window.__posted.length = 0;
el.value = "777"; el.dispatchEvent(new Event("change", {bubbles: true}));
// the sentinel has to appear in what was posted
JSON.stringify(window.__posted).includes("777");
```

Two failures this catches, both of which once shipped unnoticed: a field with
no listener at all, and a field rendered twice, where the second registration
silently wins and edits to the visible one are dropped. Then add the payload to
`tests/edits.test.mjs` so its shape is pinned for good.
