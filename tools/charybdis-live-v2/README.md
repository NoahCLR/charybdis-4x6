# Charybdis Live

Live firmware editor for the Charybdis. It talks to the connected keyboard over
Raw HID and **never parses a firmware repository**.

This is the app. `charybdis-live/` beside it is the previous interface, frozen
and unmaintained; Profile Studio, also frozen, authors the `.c` files. The
product direction lives in
[`docs/LIVE_EDIT_APP_DIRECTION.md`](../../docs/LIVE_EDIT_APP_DIRECTION.md), and
the interface direction with its clickable prototype lives in
[`design/`](./design/).

## Shape

- `extension.js` — the VS Code surface: command, panel, message relay.
- `panel-html.js` — the panel's HTML shell, the only host file that knows
  webview URIs.
- `core/` — the device, with no host dependency, layered so imports point one
  way: `transport/`, `schema/`, `protocol/`, `model/`, `session/`, `data/`.
- `webview/` — the interface as browser ES modules, no build step. `lib/` and
  `view/` are pure and carry tests; `ui/` draws; `styles.css` is the design
  system.
- `tests/` — mirrors `core/`, plus the view modules and the payloads the
  interface posts.

The layer rules and where new work belongs are in [`AGENTS.md`](./AGENTS.md).
The short version: nothing here may read the firmware repository, and the
webview receives the model as a message rather than importing the core.

## The interface

The board is the constant: it stays on screen, full width, painted with the
light the keyboard would actually show for that layer — the base effect, then
the layer's colour on the keys it owns — with the legends drawn on top in
whichever of black or white stays readable, and the behaviour dots and combo
badges on the key face in the feedback colours the keyboard flashes.

Underneath it, one workbench whose tabs are the key, its behaviour, its combos,
and the macros and pointing modes the layer reaches. A behaviour is tap count ×
tier, so it is drawn as a grid.

Every colour on screen is a colour the keyboard emits; one amber signal marks
work that has not reached the keyboard yet. Edits stage into one draft, and the
floating bar is the only way they leave the window.

## Commands

```sh
npm install
npm run check           # syntax across the tree, then all tests
npm run preview         # build dev/model.json from the test fixtures
npm run probe:live-link # read-only enumeration of matching HID interfaces
npm run keycodes        # regenerate core/data/keycode-catalog.json from a QMK checkout
```

Open the panel from the command palette: **Charybdis: Open Charybdis Live v2**.

To work on the interface without a keyboard, run `npm run preview`, serve this
folder (`python3 -m http.server 8972`) and open `dev/index.html`. The preview
stands in for the extension host: it answers the webview's `ready` with one
fixture model and logs every edit the interface posts back.

## State of the build

Every screen is drawn and wired to the host:

| Surface | What it edits |
| --- | --- |
| Keys | Layout keys, key behaviours, combos; reachable macros and pointing modes are shown in place |
| Pointing modes | All eight slots: movement, speed, direction shortcuts, scroll tuning, buttons, bindings, clear and duplicate |
| Lighting | Six stages, the stage mask, layer and pointing-mode colours with their localities, combo and key feedback, auto-mouse fade, LED group rows and reusable groups |
| Macros | Both banks: payload, step builder, parsed preview, recorder |
| Settings | Every section the keyboard reports, posted whole, read-only where the firmware cannot report |
| Profile & backups | Export, import with review, layer priority, upgrade export, recovery state |
| Device | Read-only: connection, committed generation, what was read |

The one gap against the previous interface is the **keycode picker's catalogue
sections**: it offers the keyboard's own groups, the ANSI board, layers,
pointing modes and both macro banks, and searches across all of them, but does
not yet reproduce v1's hand-curated symbol, numpad and mouse panels.

Colour on any screen comes from the model, never from a constant, and a stage
that is switched off is drawn as off — hollow dots, plain badges, unlit keys.

## One rule that differs from v1

A key bound to a **cleared pointing slot** is allowed. The keyboard keeps its
mode keycodes in a fixed registry and its runtime refuses to activate a slot
with an empty record, so such a key is inert, not invalid — it does nothing
until the slot is configured again. v1 refused to clear a slot while anything
still reached it; here the clear goes through, the validated profile counts what
still points at the empty slot, and the Pointing modes screen and the hover card
say the key does nothing for now.

The same reasoning runs the other way, so the keycode picker offers **every**
slot, configured or not, labelled `Slot 7 · hold (empty)` while the slot holds
nothing. A board can be laid out before its modes are, and the key says `empty`
on its second line until the slot is filled in.
