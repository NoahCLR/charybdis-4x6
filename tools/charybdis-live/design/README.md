# Charybdis Live — UI direction

`charybdis-live-ui.html` is a clickable design prototype for the live app. Open
it in a browser; nothing else is needed. It renders the real keymap from
`keymaps/noah/keymap.c`, the real colours, timings and scalars from a profile
read off the keyboard, the app's own ANSI picker layout and its physical LED
numbering — so the screens show what the app actually has to display.

It is a design artefact, not a second implementation: it posts no messages and
talks to no device.

## The rule the design is built on

The interface is black and white. Every hue on screen is a colour the keyboard
emits — a layer colour, a pointing-mode colour, a feedback colour, the base
effect. Nothing decorative is coloured, so colour on screen always means light
on the keyboard. The single exception is one amber signal for unapplied work.

## Shape of the Keys view

The board is the constant: it stays on screen, full width, and it always shows
what the keyboard shows — the base effect and this layer's colour on the keys it
maps — with the legends drawn on top in whichever of black or white stays
readable. A pointing mode's colour is not on the board by default: it is an
overlay that paints its locality only while that mode is held, so the Lighting
stage has a *Preview active* switch that shows where it actually lands. Behaviour dots (tap, hold, long hold, in the
feedback colours, numbered when a tier repeats across branches) and combo
badges sit on the key face, as they did in the old board. There is no light
mode to switch into and no LED-index view here; LED numbering belongs to the
LED group selector in Lighting. Underneath the board, one workbench whose tabs
are the old layer-overview tabs — **Key · Behaviours · Combos · Macros ·
Pointing modes** — except each tab now owns its editing surface at full width.
Nothing is squeezed into a sidecar.

A key behaviour is tap-count × tier, so it is drawn as a grid: branches across,
tap / hold / long hold down, every filled cell visible at once. Clicking a cell
opens one editor under the grid — what it sends, how it runs, its repeat rate —
instead of unfolding five nested forms.

## Lighting has the same shape as Keys

Layer tabs on the board card choose what the board shows, as on Keys; the board
is the composed result; and the workbench tabs are the stages themselves, in the
order the firmware paints them — numbered 1 to 6, each in the colour it paints
now and drawn off when its stage is off: Base effect, Layer colours, Auto-mouse
fade, Pointing modes, Combo feedback, Key feedback — then, past a divider, LED
groups, which every stage can draw on. Each tab is a full-width surface with its list, its colour, where it
paints and what it does, and the stage's on/off switch sits in the tab bar. On
the LED groups tab the board becomes the LED selector, because that is what that
tab needs it to be.

## One lighting model

Layer colours, pointing-mode colours, the base effect and the seven key-feedback
semantics live in a single model. The Lighting editors write to it; the board,
the key-face dots, the branch numbers in the behaviour grid, the hover card, the
legend, the swatches in Settings and the before/after swatches in the review all
read from it. Editing a colour is a draft change like any other, and every
surface shows the draft — not the last value read from the keyboard.

A stage that is switched off looks switched off: key feedback off draws the dots
hollow, combo feedback off leaves the badges plain, base effect off turns the
unlit keys black.

## What changed against the current five tabs

- One draft, one gate. Every "keep in draft" button is gone; edits stage as they
  are made, undo, redo and the draft history sheet sit in the rail, and the floating commit bar is the
  only way changes leave the window. Forms that are only half-finished — the
  combo builder, a new layer, a macro step — still keep their own state.
- Five tabs of nested `<details>` become six places in a rail, each with one
  primary object.
- Pointing modes lead with name, movement, speed and actions; thresholds, axis
  ratios and button overrides stay under Advanced.
- The four health pills become four always-visible lines in the rail, with the
  last message from the keyboard in the free space above Draft history.

## Kept from the old app, deliberately

- **The key hover card.** Hovering a key shows everything it reaches: behaviour
  branches with each tier's lifecycle, macro payload steps, combos, pointing
  mode, and its light.
- **The keycode picker.** The real ANSI board (the same layout table the app
  ships), modifier wrappers, the layer section with hold / lock / tap-hold,
  pointing modes, both macro banks, every QMK section, search, the live
  expression, and OK / Cancel. List mode is used for combo inputs.
- **The combo builder beside the board**, with inputs picked on the physical
  keyboard, per-combo window, hold and order requirements, and the shared hold
  threshold shown as shared.
- **The LED group builder**, with the keyboard's own LED indices, the trackball
  LED, the rows already in each table, inline or reusable groups, and what each
  group is used by.
- **Tooltips that say what a control affects and where it writes.**
- Layer staging, the macro recorder, raw payload and parsed preview, read-only
  settings for firmware that cannot report them, the profile import review, the
  upgrade-export path, and the notice / stale-draft / mid-apply states.

## Deep links

The prototype routes on the hash, so any screen or state can be opened, shared
or rendered directly:

```
#keys?tab=key&key=51                         the selected key, across the stack
#keys?tab=behaviours&row=LEFT_THUMB&cell=2-hold   behaviour grid, one cell open
#keys?tab=combos&combo=picking               picking combo inputs on the board
#keys?tab=pointing&layer=4                   modes this layer reaches
#keys?hover=50   #keys?layer=3   #keys?theme=light
#lighting?stage=pd&pd=0   #lighting?lm=groups
#macros  #settings  #profile  #device
#review  #import  #picker  #notes
#keys?state=stale   #keys?state=applying   #keys?notice=error
```

## Regenerating the stills in `renders/`

```sh
python3 -m http.server 8971            # from this directory
"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" \
  --headless --disable-gpu --hide-scrollbars --force-device-scale-factor=2 \
  --window-size=1560,1200 --virtual-time-budget=4000 \
  --screenshot=renders/01-keys-key.png "http://localhost:8971/charybdis-live-ui.html#keys?tab=key"
```

## Where the stills are

The rendered stills are not checked in. Regenerate them into `renders/` with
the command above.
