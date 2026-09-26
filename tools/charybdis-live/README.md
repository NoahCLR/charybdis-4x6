# Charybdis Live

Live firmware editor for the Charybdis. It talks to the connected keyboard over
Raw HID and **never parses a firmware repository**.

This is the app. Profile Studio, which is frozen, authors the `.c` files. The
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
On a keyboard that owns its layer keys (Profile Wire feature bit 14), a
behaviour takes QMK's layer keys where the keyboard can run them: `TG()`,
`TO()` and `LOCK_LAYER()` anywhere, `OSL()` as a tap, `TT()` and `MO()` as a
"Press and hold until release" branch; `LT()` and `LM()` stay keys and combos,
and `DF()`/`PDF()` are refused.
On the board, drag one key onto another to swap them, ⌘C and ⌘V copy a key
onto the selected key, and Delete or Backspace makes it transparent. ⌘Z undoes
and ⇧⌘Z or ⌘Y redoes the draft everywhere except inside a text field, which
keeps its own undo. **Draft history** in the rail opens every step of the
draft, newest first, each with when it was made and what it changed from the
step before it (not from the keyboard, as the review compares); go straight
back or forward to any of them. Board keys work from the keyboard too: Space or Enter
selects one, and Enter on the selected key opens its picker.

The layer stack is not one of those tabs, because
it is not a property of the selected key. The layers are tabs along the foot
of the board card, next to the workbench, the picked one opening into the board
it shows, on Keys and on Lighting alike; **Rename & Reorder** stays at the end
of those tabs, outside what scrolls, and opens upward over the board, so a layer
is renamed and reordered where its keys are on screen. **Keys follow their layers** (on by
default) renumbers every layer key (MO, LT, TG, TO, TT, OSL, LOCK_LAYER…) on
layers, behaviours and combos so each still reaches the same layer; turned off,
the layers move but those keys keep their numbers. Names, colours and the
pointer and sniping settings move with their layer either way.

**⌘-click** (Ctrl-click off a Mac) more layer tabs to preview them on
together. The board then shows what the keyboard would answer with, by the
firmware's rule: the highest layer on wins, a transparent key is answered by the
highest layer below it that is also on, and Base is always on. Keys answered from
below are seen through the top layer's glass: frosted like a transparent key,
their legend sharp, in the light of the layer that answers, and
on Lighting the board paints every layer on, lowest first, as the keyboard does.
The highest layer keeps the tab joined to the board and every edit is stored on
it; a key answered from below says so on the Key tab, and setting it overrides
that answer. ⌘-click a layer again to take it out, or click any tab to go back to
one layer. Layers on under the top one are tinted and outlined in their own
light, so the set reads from the tabs. The set holds across Keys and Lighting and is dropped
when the keyboard or its layer order changes. It is a what-if: a set no key can
actually hold together still previews.

Before anything is applied, the review **checks** what the layers let you
reach. A layer that can lock with no way back to Base — nothing on it, or on
anything held over it, releases the lock or moves back — is a **trap**: the
review shows the steps into it and the way out, and Apply asks "Apply anyway"
before it writes. Every trap the draft keeps asks, including one already on the
keyboard. The review also notes, without asking, a layer with keys nothing
reaches, a layer key onto an empty layer, transparent keys on Base, a pointer
layer that cannot work, and layer keys the keyboard leaves to QMK. Each check
says whether the draft made it, the keyboard already has it, or the draft
fixes it.

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
ln -s "$PWD" ~/.vscode/extensions/noah.charybdis-live-0.1.0   # then reload the window
```

That gives a **Charybdis Live** button in the status bar. The
panel also opens from the command palette — **Charybdis: Open Charybdis Live** — and the repo's `.vscode/launch.json` has *Run Charybdis Live*, which
launches an Extension Development Host with a debugger attached instead.

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
| Lighting | Six stages, the stage mask, layer and pointing-mode colours with their localities, combo and key feedback, auto-mouse fade with its hold as a share of the timeout, LED group rows and reusable groups |
| Macros | Both banks: name, payload, insert-at-cursor step builder, reorder/remove controls, parsed preview, configurable recorder and placement; search by name, and the layers that set each macro off — by key, behaviour or combo — in their layer colour, each opening that layer in Keys with the macro picked |
| Mouse | Pointer speed, sniping and auto-mouse — the Settings sections the keyboard's model files under Mouse, drawn with the same cards and posted whole |
| Pointing modes | All eight slots: movement, speed, direction shortcuts, scroll tuning, buttons, bindings, placement, clear and duplicate |
| Settings | Every other section the keyboard reports, posted whole, read-only where the firmware cannot report |
| Profile & backups | Import (the file against the keyboard, counted by what it configures — keys, lighting, macros, mouse, pointing — before it becomes the draft), export, upgrade export, recovery state |
| Device | Read-only: connection, committed generation, what was read |

The keycode picker leads with the ANSI board, then task-shaped Symbols,
Navigation, Numpad, Layers, Pointing modes, Macros, Mouse, Media, Lighting,
Magic and Custom sections. The complete QMK catalogue remains available under
More keys, Other QMK and All keycodes, and search spans all of it — named
macros included, found by the name they were given. Layers offers each layer
as Hold (`MO`), Lock (`LOCK_LAYER`, the same lock as QMK's `TG`) and Tap-hold
(`LT`); a keyboard that owns its layer keys adds Tap-toggle (`TT`), One-shot
(`OSL`) and Move (`TO`).

Colour on any screen comes from the model, never from a constant, and a stage
that is switched off is drawn as off — hollow dots, plain badges, unlit keys.

Every edit is kept in a local draft and reaches the keyboard only through
review and apply. Review lists each changed thing once, under the area it is
edited in, with the fields that changed, can open it where it is edited, and
can discard part of the draft: things made by the same edit (a key swap, a
moved behaviour) go back together, as one undoable step. Layers are compared
by which layer they are, not where they sit, so a reorder is one **Layer
priority** item and a key edited or a layer renamed after it is its own item;
each can be discarded without the other. A keyboard the app cannot open a draft for — its profile
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
Switching keyboards closes an open layer editor or import review; reopen it on
the selected keyboard before keeping changes.
After a disconnect, a retained dirty draft requires **Review against the
keyboard** before editing or applying, even if the HID path and saved profile
look unchanged.

## Cleared pointing slots

A key bound to a **cleared pointing slot** is allowed. The keyboard keeps its
mode keycodes in a fixed registry and its runtime refuses to activate a slot
with an empty record, so such a key is inert, not invalid — it does nothing
until the slot is configured again. The clear goes through even while something
still reaches the slot, the validated profile counts what
still points at the empty slot, and the Pointing modes screen and the hover card
say the key does nothing for now.

The same reasoning runs the other way, so the keycode picker offers **every**
slot, configured or not, labelled `Slot 7 · hold (empty)` while the slot holds
nothing. A board can be laid out before its modes are, and the key says `empty`
on its second line until the slot is filled in.
