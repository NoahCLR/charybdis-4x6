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
tier, so it is drawn as a grid. **Change key…** moves a behaviour to another
key through the keycode picker; when that key already has one, you choose to
overwrite it, swap the two, or cancel, and either is one undoable draft step.
On the board, drag one key onto another to swap them, ⌘C and ⌘V copy a key
onto the selected key, and Delete or Backspace makes it transparent. ⌘Z undoes
and ⇧⌘Z or ⌘Y redoes the draft everywhere except inside a text field, which
keeps its own undo. Board keys work from the keyboard too: Space or Enter
selects one, and Enter on the selected key opens its picker.

The layer stack is not one of those tabs, because
it is not a property of the selected key: **Edit layers** sits after the last
layer on the row above the board and drops open over it, so a layer is renamed
and reordered where its keys are on screen. **Keys follow their layers** (on by
default) renumbers every layer key (MO, LT, TG, TO, TT, OSL, LOCK_LAYER…) on
layers, behaviours and combos so each still reaches the same layer; turned off,
the layers move but those keys keep their numbers. Names, colours and the
pointer and sniping settings move with their layer either way.

Every colour on screen is a colour the keyboard emits; one amber signal marks
work that has not reached the keyboard yet. Complete edits stage into one draft
as they are made; incomplete builders such as a new combo keep their local form
until it is valid. The floating bar is the only way changes leave the window.

## Commands

```sh
npm install
npm run check           # syntax across the tree, then all tests
npm run preview         # build dev/model.json from the test fixtures
npm run preview -- --device  # …or from the keyboard that is plugged in, read-only
npm run preview -- --vscode  # also write dev/vscode-{dark,light}.html, as the panel renders
npm run probe:live-link # read-only enumeration of matching HID interfaces
npm run keycodes        # regenerate core/data/keycode-catalog.json from a QMK checkout
```

## Installing it

The extensions in this repo are installed by symlinking the folder into VS
Code's extension directory, so the checkout *is* the installed extension and a
window reload picks up every edit:

```sh
ln -s "$PWD" ~/.vscode/extensions/noah.charybdis-live-v2-0.1.0   # then reload the window
```

That gives a **Charybdis Live v2** button in the status bar, beside v1's. The
panel also opens from the command palette — **Charybdis: Open Charybdis Live
v2** — and the repo's `.vscode/launch.json` has *Run Charybdis Live v2*, which
launches an Extension Development Host with a debugger attached instead.

Both apps can be installed at once; they own separate commands and panels. Only
one may hold the keyboard's Raw HID interface at a time, so close one panel
before reading from the other.

To work on the interface without a keyboard, run `npm run preview`, serve this
folder (`python3 -m http.server 8972`) and open `dev/index.html`. The preview
stands in for the extension host: it answers the webview's `ready` with one
fixture model and logs every edit the interface posts back.
Use `npm run preview -- --multiple --vscode` to inspect the selector with two
fixture keyboards in the VS Code themed preview.

## State of the build

Every screen is drawn and wired to the host:

| Surface | What it edits |
| --- | --- |
| Keys | Layout keys, key behaviours, combos, layer names and priority; reachable behaviours, macros and pointing modes are shown in place, whether a key, a behaviour branch or a combo reaches them |
| Lighting | Six stages, the stage mask, layer and pointing-mode colours with their localities, combo and key feedback, auto-mouse fade, LED group rows and reusable groups |
| Macros | Both banks: name, payload, insert-at-cursor step builder, reorder/remove controls, parsed preview, configurable recorder and placement; search by name, and the layers that set each macro off — by key, behaviour or combo — in their layer colour, each opening that layer in Keys with the macro picked |
| Pointing modes | All eight slots: movement, speed, direction shortcuts, scroll tuning, buttons, bindings, placement, clear and duplicate |
| Settings | Every section the keyboard reports, posted whole, read-only where the firmware cannot report |
| Profile & backups | Export, import with review, upgrade export, recovery state |
| Device | Read-only: connection, committed generation, what was read |

The keycode picker leads with the ANSI board, then task-shaped Symbols,
Navigation, Numpad, Layers, Pointing modes, Macros, Mouse, Media, Lighting,
Magic and Custom sections. The complete QMK catalogue remains available under
More keys, Other QMK and All keycodes, and search spans all of it — named
macros included, found by the name they were given.

Colour on any screen comes from the model, never from a constant, and a stage
that is switched off is drawn as off — hollow dots, plain badges, unlit keys.

Every edit is kept in a local draft and reaches the keyboard only through
review and apply. Review lists each changed thing once with the fields that
changed, can open it where it is edited, and can discard part of the draft:
things made by the same edit (a key swap, a moved behaviour) go back together,
as one undoable step. A keyboard the app cannot open a draft for — its profile
could not be read, or its firmware predates profile editing — is read-only:
the host refuses any edit rather than writing it directly.

**Read from keyboard** runs one complete read at a time. The health strip says
both halves agree only after the firmware reports a known, converged peer;
matching generation numbers alone are insufficient. A failed committed-profile
read remains an error instead of being labelled as compiled defaults. Those
defaults are shown only when fresh device status reports no committed profile.
When more than one compatible keyboard is connected, the selector at the top
left chooses which one to read and edit. Switching keeps a dirty draft attached
to its original keyboard; the other keyboard stays read-only until you switch
back or discard that draft. Device choices keep their identity across rescans
within the panel, so replacing one keyboard cannot reuse its draft by list order.
After a disconnect, a retained dirty draft requires **Review against the
keyboard** before editing or applying, even if the HID path and saved profile
look unchanged.

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
