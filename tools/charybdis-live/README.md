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
- `webview/` — the ported editing UI and a separate device-profile renderer. They render the
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

The app connects to a keyboard and shows what the keyboard says about
itself: VIA and Profile Wire versions, schema, capacities, storage geometry,
and committed profile status including whether both halves agree on a
generation.

The app reads the VIA layout and the committed profile payload. If nothing is
committed, it reads the firmware's compiled defaults from the keyboard. These
are labelled separately from committed generations.

The **Behaviours** tab displays all returned rows, their timing values, sparse
tap branches, hold modes, and auto-mouse anchor flags. RGB colours, LED
membership, group references, locality, and enabled stages appear in **RGB**.
Group and layer labels are generated from device IDs; source names are not
available. A timing value of zero selects an unreported firmware default.

The layer preview honours the device's mapped-keys-only policy: transparent
and disabled keys do not receive the layer colour. All-keys mode and the layer
stage enable flag are respected too. Standard VIA RGB Matrix GETs read the
current base effect, relative brightness, hue, saturation and speed separately
from the custom profile. Two matching consecutive samples are required, with
a bounded retry if settings change during readback. Failure clears stale base
colour without discarding the verified profile.

The selected-layer preview paints the base effect, base and selected layer
colours, then applicable LED-group rows in device order. Pass-through leaves
the colour underneath visible; transparent-key symbols still identify unmapped
positions. For example, a red base effect and Layer 4's nine mapped grey keys
produce nine grey keys over 47 red keys. It uses the last read, not the current
active layer stack or live feedback. **Read from keyboard** refreshes it.
Solid colour and off are supported; other effects have an explicit unavailable
preview. VIA brightness is relative to an unreported firmware ceiling, so the
display brightness is approximate. Idle/suspend state, LED effect flags,
animations and temporary feedback are not observed by these settings reads.

Standard modifier shortcuts, tap-hold keys,
and one-shot modifiers are decoded from their numeric QMK values. Device
semantic targets have readable labels when the advertised action ABI is known;
other custom keys retain complete numeric IDs because source names are not sent.

The model adapter translates validated wire domains into presentation fields;
it does not parse or consult firmware source. Tests cover chunked readback
through the real service and extension publication, the display conversion,
and the extracted renderer. The recorded device payload under `tests/fixtures/`
is test input only, never an app default or runtime fallback.

The combo editor beside the layout reads the keyboard's native combo table through optional
Profile Wire value `0x06`: inputs, output, enabled state, timing, hold/tap/order
requirements and per-layer input references. Combo IDs become stable `C1`,
`C2`, etc. labels; source names are not sent. Layout badges use the device's
reference mapping and transparent inputs inherited from Layer 0. This remains
a selected-layer preview; any additional firmware trigger/release conditions
are flagged, not reconstructed. All rows remain visible in the collapsed all-combos list below the layer overview even
when globally disabled or unavailable on the selected layer.

The firmware and app share a byte fixture for this bounded readout. Metadata
is checked before and after the rows and a digest verifies the entire read.
Unsupported firmware leaves combos empty with an update message, without
discarding RGB or behaviours. **Build and flash the updated firmware pair to
enable combo readback**, then choose **Read from keyboard**.

Layout writes work today. RGB colours, stage policies, auto-mouse fade,
localities, LED membership and assignments save through the complete profile
transaction. The auto-mouse end colour is unused in follow-real-destination
mode; its colour control is disabled in that mode. The static preview does not
animate timeout fades.

Combo input selection and editing remain beside the physical layout. Save,
add and delete use canonical combo domain `0x30`, preserving RGB and behaviours.
Each combo has a window, hold/tap requirement and ordering flag. The hold
threshold is shared by all combos, matching QMK. Firmware callback outputs and
custom trigger/release hooks remain explicitly unsupported for editing.
Picker shortcuts such as Cmd+N (`G(KC_N)`) encode identically to their long
QMK names (`LGUI(KC_N)`) for both combo and layout writes. Standard shortcut
names come from the shipped keycode vocabulary and need no custom action ABI.

A save binds the profile generation/digest/origin, rechecks it after obtaining
the candidate lease, waits for both halves, and requires exact payload readback.
Combo saves also verify the effective native table. An absent combo domain
retains compiled combos; an empty domain disables them. Flash the current
side-specific pair before using combo writes, then reload the extension and
read the keyboard.

Behaviour edits use the same verified profile save. Select a key in Layout or
choose **Edit behaviour** in the Behaviours list. Save changes to timing,
tap-count branches, tap/hold/long-hold actions, repeat rates and the additional
auto-mouse anchor flag; add a row for a key without one or delete an existing
row. Blank timing values become zero (firmware default), and disabled branches
are removed. An empty row can still carry timing and anchor policy. The app
preserves untouched rows and RGB/combo bytes, validates advertised capacities
and action references, and requires exact committed readback before reporting
success. The existing behaviour-capable firmware needs no reflash for this
editor connection; reload VS Code and read the keyboard again.

Behaviour drafts stay attached to their device and row while switching keys
or views. Failed saves keep the fields editable. A draft based on an older
profile remains visible but cannot be saved over the new profile; use
**Discard row changes** to load that row again, or **Read from keyboard** to
discard local edits and refresh. Successful saves clear only the saved row's
draft. These drafts are held in the open editor, not stored as backups.
Timing fields use millisecond labels, key shortcuts use readable names, and
tap branches wrap to fit the window.

Complete readback includes both macro banks and global policy defaults. All
editors contribute to one local draft. Apply saves a recovery snapshot, stages
only changed VIA blocks on the other half, prepares the custom profile on both
halves, and then publishes one logical generation. Activation waits for stable
custom and VIA identities on both halves. A reboot after the decision marker
recovers the bound VIA generation before enabling the saved profile.

After updating the installed extension, reload VS Code and choose **Read from
keyboard**. Firmware that already returns the profile needs no reflash for UI
changes.
