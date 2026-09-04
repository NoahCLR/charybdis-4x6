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
media/              webview: real ES modules, no template literal
  views/            one module per panel
scripts/            developer entry points and build steps
tests/              mirrors core/, one directory per layer
```

## Layer Rules

Dependencies point one way. Adding an import that violates this is the moment
to stop and reconsider, not to work around.

| Layer | May import |
| --- | --- |
| `transport/` | nothing in `core/` except `transport/device-adapter` |
| `schema/` | `schema/` only |
| `protocol/` | `transport/`, `schema/` |
| `session/` | `transport/`, `protocol/`, `schema/` |
| `media/` | nothing from `core/` — it receives snapshots as messages |

`extension.js` imports `core/session/` and the `vscode` module. Nothing else
imports `vscode`.

The webview never imports the core directly. It renders whatever snapshot the
host posts, so the core stays runnable in a plain Node process and the UI stays
replaceable.

## Where New Work Goes

- A new device command or wire format → `protocol/`
- A new profile domain or byte layout → `schema/`
- Anything holding state across a connection → `session/`
- A new panel → `media/views/`, one module per panel
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
