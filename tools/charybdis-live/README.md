# Charybdis Live

Live firmware editor for the Charybdis. It talks to the connected keyboard over
Raw HID and **never parses a firmware repository**.

This is the separate live-editing app described in
[`docs/LIVE_EDIT_APP_DIRECTION.md`](../../docs/LIVE_EDIT_APP_DIRECTION.md).
Authoring `keymap.c`, `config.h`, and `rgb_config.c` belongs to Profile Studio,
which is frozen and lives beside this one.

## Layout

- `extension.js` — the VS Code surface only: command, panel, message relay.
- `media/` — the webview, as real ES modules loaded through
  `webview.asWebviewUri()`. No template literal, no bundler.
- `live-link/` — the device core: transport, Profile Wire protocol, profile
  schema domains, and the device session. It has no `vscode` import, so this
  extension shell can be replaced by a standalone app without touching it.
- `tests/live-link/` — host tests for the core, run with `node --test`.

## Commands

```sh
npm install
npm run check           # syntax + the full live-link test suite
npm run probe:live-link # read-only enumeration of matching HID interfaces
npm run keycodes        # regenerate live-link/keycode-catalog.json from a QMK checkout
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
