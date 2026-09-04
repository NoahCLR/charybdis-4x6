# Charybdis Live

Live firmware editor for the Charybdis. It talks to the connected keyboard over
Raw HID and **never parses a firmware repository**.

This is the separate live-editing app described in
[`docs/LIVE_EDIT_APP_DIRECTION.md`](../../docs/LIVE_EDIT_APP_DIRECTION.md).
Authoring `keymap.c`, `config.h`, and `rgb_config.c` belongs to Profile Studio,
which is frozen and lives beside this one.

## Layout

- `extension.js` — the VS Code surface only: command, panel, message relay.
- `core/` — the app, with no host dependency, layered so imports point one way:
  - `transport/` — device adapters and the request coordinator
  - `schema/` — profile byte formats and domain decoders
  - `protocol/` — wire formats spoken to the device
  - `session/` — stateful orchestration across a connection
  - `data/` — vendored data such as the keycode catalog
- `webview/` — Profile Studio's editing UI, ported verbatim. It renders the
  `model` the host posts and sends edits back as typed messages; it never
  imports the core and cannot touch the filesystem.
- `tests/` — mirrors `core/`, one directory per layer.

The layer rules and where new work belongs are in
[`AGENTS.md`](./AGENTS.md). The short version: nothing here may read the
firmware repository.

## Commands

```sh
npm install
npm run check           # syntax + the full live-link test suite
npm run probe:live-link # read-only enumeration of matching HID interfaces
npm run keycodes        # regenerate core/data/keycode-catalog.json from a QMK checkout
```

The keycode catalog is vendored on purpose. The app renders keycodes read back
from a device, where they arrive as bare `uint16` values, and it must do that
without a firmware workspace. `npm run keycodes -- --check` fails if the
checked-in catalog has drifted from the QMK tree beside this repo.

Open the panel from the command palette: **Charybdis: Open Charybdis Live**.

## Current state

Slice 1. The app connects to a keyboard and shows what the keyboard says about
itself: VIA and Profile Wire versions, schema, capacities, storage geometry,
and committed profile status including whether both halves agree on a
generation.

Reading the committed payload back is the next slice. The Profile Wire
`READ_SURFACE` bit is capability and status reporting only and is not profile
readback.
