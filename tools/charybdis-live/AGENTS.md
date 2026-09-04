# Charybdis Live — Agent Instructions

This app edits the **connected keyboard**. Profile Studio, beside it, edits the
`.c` files and is frozen. Do not blur that line.

Read [`docs/LIVE_EDIT_APP_DIRECTION.md`](../../docs/LIVE_EDIT_APP_DIRECTION.md)
before changing anything here. It carries the goal, the decisions, and what is
deliberately still undesigned.

## The One Rule

**Nothing in this app may read the firmware repository.** No parsing `keymap.c`,
`config.h`, or `rgb_config.c`. No walking a QMK checkout at runtime. No
`vscode` import below `extension.js`.

The app must work for someone who has a keyboard and this extension and nothing
else. Every shortcut through the repo is a shortcut that has to be removed
later, and it will be removed later, because the product goal requires it.

The one sanctioned use of a QMK checkout is `scripts/generate-keycode-catalog.js`,
which runs by hand and writes a checked-in file. That is a build step, not a
runtime dependency.

## Layout

```
extension.js        VS Code surface only: command, panel, message relay
core/               the app, with no host dependency
  transport/        device adapters and the request coordinator
  schema/           profile byte formats and domain decoders
  protocol/         wire formats spoken to the device
  session/          stateful orchestration across a connection
  data/             vendored data, e.g. the keycode catalog
webview/            Profile Studio's editing UI, ported verbatim
scripts/            developer entry points and build steps
tests/              mirrors core/, one directory per layer
```

### About `webview/`

`studio-ui.js` is Profile Studio's UI, unmodified. It is a pure renderer: it
reads the `model` the host posts and sends edits back as typed messages. That
seam is why the port works, and it is why the UI did not have to be rebuilt.

It is one large template literal, the shape this app otherwise avoids. Keeping
it verbatim was deliberate: it is roughly 8,700 lines of working, visually
tuned UI, and rewriting it to prove a structural point would have traded a real
editor for a tidier empty one.

It is exempt from the layer rules by path. Two things still hold and are
tested: it may not import `transport/`, `protocol/` or `session/`, and it may
not touch the filesystem.

It still carries stale copy — tooltips and help text mentioning the C source
files, inherited from the editor it came from. Those are display strings, not
behaviour. Rewrite them as you rewire each tab to the device, rather than in
one sweep.

## Layer Rules

Dependencies point one way. Adding an import that violates this is the moment
to stop and reconsider, not to work around.

| Layer | May import |
| --- | --- |
| `data/` | nothing — inert vendored content |
| `transport/` | `data/` |
| `schema/` | `data/` |
| `protocol/` | `transport/`, `schema/`, `data/` |
| `session/` | `transport/`, `protocol/`, `schema/`, `data/` |
| `media/` | nothing from `core/` — it receives snapshots as messages |

Every layer may also import from itself.

`extension.js` imports `core/session/` and the `vscode` module. Nothing else
imports `vscode`.

The webview never imports the core directly. It renders whatever snapshot the
host posts, so the core stays runnable in a plain Node process and the UI stays
replaceable.

## Where New Work Goes

- A new device command or wire format → `protocol/`
- A new profile domain or byte layout → `schema/`
- Anything holding state across a connection → `session/`
- A new panel or a rewritten tab → `webview/`, decomposed out of the
  monolith as you touch it
- The canonical profile format, drafts, and generation binding → a new
  `core/model/` layer between `schema/` and `session/`. It does not exist yet;
  create it when the format is designed, not before.

## Conventions

- Every `core/` module gets a test in the matching `tests/` directory.
- `node --check` every new file: add it to the `check` script in
  `package.json`. Tests are discovered by directory, so new test files are
  picked up automatically.
- Build DOM nodes, never HTML strings. Device-supplied values must not be able
  to become markup.
- Decode defensively. Everything arriving from a device is untrusted input:
  validate length, reject noncanonical encodings, and fail with a stable code
  rather than a guess.
- Report device state verbatim, including zeros. A blanked value misreports the
  keyboard.

## Verification

```sh
npm run check                 # syntax across the tree, then all tests
npm run keycodes -- --check   # fails if the vendored catalog has drifted
```

The repo's `sh tests/host/run_tooling_checks.sh` runs this app's checks as well
as Profile Studio's.
