# What keys can do

Every key sends an action: a letter, a shortcut, a layer key, a macro, a
pointing mode. A **behaviour** gives a keycode more to do: something on tap,
on hold and on long hold, and again on a double, triple or later tap. A
behaviour belongs to the keycode, not to a position, so it runs wherever that
keycode is pressed, on any layer or as a combo's output.

Ark edits all of this on the keyboard
([Keys](https://github.com/NoahCLR/charybdis-ark/blob/dev/docs/guide/keys.md),
[Behaviours](https://github.com/NoahCLR/charybdis-ark/blob/dev/docs/guide/behaviours.md),
[Combos](https://github.com/NoahCLR/charybdis-ark/blob/dev/docs/guide/combos.md)).
This page is what the keyboard does with it. The C names are in
[Authoring in C](authoring.md).

## Tap, hold and long hold

How long you hold a key picks its **tier**:

- released before the **tap / hold threshold**: a tap;
- held past it: a hold;
- held past the **long hold threshold**: a long hold.

My profile uses 150 ms and 400 ms, and each behaviour can set its own. A hold
or long hold sends its action in one of four **hold styles**:

- **held until release**: the action goes down at the threshold and stays
  down until you let go. A macro here plays once at the threshold.
- **tapped at the threshold**: sent once, as soon as the threshold passes.
- **tapped on release**: nothing at the threshold; sent when you let go. A
  long hold reached first replaces it.
- **repeated while held**: sent at the threshold, then again and again at the
  rate you set, 1 to 100 times a second, until you let go.

A tier you leave empty does what the key does on its own. An empty tap sends
the key's own keycode. On the single tap, an empty hold holds the key: an
`LT()` key holds its layer, an `MT()` key its modifiers, a pointing key its
mode and a custom key its shortcut. A macro has no hold of its own, and from
the double tap on an empty hold is no hold at all.

### Hold and long hold that both send on release

When the hold and the long hold are both tapped on release, the hold is sent
only for a release after the tap / hold threshold and before the long hold
threshold. If the long hold threshold is at or before the tap / hold
threshold, that interval is empty and the hold can never be sent.

## Tap counts

Tap a key again within the **multi tap window** (150 ms in my profile) and
the next press is the next **tap count**: double, triple, quadruple and
quintuple. Each tap count is a **branch** with its own tap, hold and long
hold.

- A tap on a key with later branches waits one multi tap window before it is
  sent, to see whether another tap follows. That holds at every depth, the
  deepest included.
- A hold does not wait: crossing a hold threshold enters the branch at once.
- A branch with no tap of its own repeats the first tap once per press, so an
  empty double tap sends it twice and an empty triple tap three times.
- **Tapping past the deepest branch is typing.** If you keep tapping within
  the window after the deepest branch you set, no branch fires: every press
  in the run sends the first tap once. A Backspace whose double tap types V
  sends Backspace for one tap, V for two and three Backspaces for three. A
  press you hold during such a run takes the first branch's hold.

## Dual-role keys

`LT()`, `MT()`, `OSM()`, `TT()` and `OSL()` are dual-role keys, and the
firmware decides their tap or hold itself, with or without a behaviour. They
use the **dual-role threshold** (200 ms in my profile) unless their behaviour
sets its own.

- Held past it, `LT(layer, key)` holds its layer the way `MO(layer)` does,
  and `MT()` and `OSM()` hold their modifiers. A tap never turns on `LT()`'s
  layer or `MT()`'s modifiers; an `OSM()` tap arms its modifiers for the next
  key, as in QMK.
- Tap one and press it again at once, and the second press is a new hold, not
  a repeat of the tap as in stock QMK.
- QMK's own tap-hold options (permissive hold, hold on other key press, Flow
  Tap, Chordal Hold) don't apply to these keys.
- A behaviour on an `LT()` key that sets a tap but no hold keeps the layer
  hold.

## Layer keys

The firmware owns every layer key, so a layer turned on by one route and off
by another always ends up right.

- `MO(layer)` holds the layer while the key is down.
- `TG(layer)` and `LOCK_LAYER(layer)` lock the layer on, or off again.
- `TO(layer)` locks that layer alone and releases every other lock; `TO(0)`
  goes back to the base layer. Layers you are holding stay on until you let
  go.
- `TT(layer)` holds like `MO(layer)` and locks the layer on its fifth tap
  (QMK's `TAPPING_TOGGLE`), unless a behaviour on the key sets its own taps.
- `OSL(layer)` holds like `MO(layer)`, and a tap turns the layer on for the
  next key press only. A quick second tap cancels it. Modifiers don't use it
  up.
- `LM(layer, mods)` holds the layer and the modifiers together.
- `DF()` and `PDF()` are refused: layer 0 is always the base layer.

Pressing another key while an `MO()`, `TT()`, `OSL()` or `LM()` key is down
cancels that key's quick tap, since its layer was already on.

Where each one can go:

| Action | On a key | Behaviour tap | Behaviour hold | Combo output |
| --- | --- | --- | --- | --- |
| `TG`, `TO`, `LOCK_LAYER` | yes | yes | yes | yes |
| `MO`, `TT` | yes | no | held until release only | yes |
| `OSL` | yes | yes | no | yes |
| `LM` | yes | no | no | yes |
| `LT` | yes | no | no | no |

## Pressing another key before a hold decides

A key with a behaviour that is down and hasn't reached its hold meets the
next key you press in one of three ways, decided by what its hold would do at
the tap count it is on. Either way its own output comes first.

1. **Its hold changes other keys: later keys wait.** That is any dual-role
   key, or a key whose first hold is a layer, modifiers only, or a pointing
   mode. Keys pressed after it wait, as in QMK:
   - let go before the threshold, and it was a tap: its tap is typed, then the
     waiting keys, so rolling `/` into `,` types `/,`;
   - hold past the threshold, and its hold starts first, then the waiting keys
     run on it, on its layer and with its modifiers.

   A hold that goes on at the press itself (`LM()`, or a pointing key whose
   behaviour sets no hold) leaves nothing to decide: later keys go out at once
   under it, and its quick tap is cancelled.
2. **Its hold is its own modifier: the later press holds it.** A modifier key
   with only a tap set (Shift with Caps Lock on tap) holds itself as soon as
   another key is pressed, so Shift rolled into a letter types the capital.
3. **Anything else: the later press settles it as a tap.** Its hold is an
   output of its own (a shifted symbol, Shift+Enter, Alt+Left) or it has none.
   The later press types its tap first, then goes out itself, so rolling `,`
   into a space types `, `. A tap count still pending is typed at the count
   reached. Held past its threshold, it is a hold instead: a hold tapped at
   the threshold has already gone, and one tapped on release is sent at the
   later press.

Waiting keys go out in order, each at the time it was pressed, and the wait
lasts at most until the deciding key's first hold threshold. A modifier or
layer key you let go of while such a key is undecided waits too, as in QMK:
press Cmd, press F, let go of Cmd, then F, and it types Cmd+F. For a key of
the third kind, letting go of the modifier types its tap there and then, so
Cmd released just before `,` still types Cmd+,.

At most eight key events wait. If a ninth must wait, the oldest goes out at
once, on the layer and modifiers active then.

## Timing follows your fingers

Taps and holds are timed by when you pressed and let go, not by when the
keyboard got round to the key. A combo deciding or a key waiting behind an
undecided one can delay what is sent, but never shortens the multi tap window
or restarts a hold: a press that waited still continues its tap count, and a
key already released while it waited reaches no later hold. A double hold is
press, release, press and hold, and the first press must have been a tap.

## Combos

A **combo** is keys pressed together that send one action instead of their
own. Their presses must fall within the **combo window**: 50 ms in my
profile, or a combo's own window. A combo can send any action a key can,
except `LT()`.

When the combo's action has a behaviour, the combo uses it like a physical
key, including tap counts: a combo sending Left GUI joins Left GUI's tap
count. Its hold time starts when the combo sends. A switch on each combo, and
one for all combos, can make combos send their action's plain action instead.

QMK's combo keys, `CM_ON`, `CM_OFF` and `CM_TOGG`, turn combos on and off
until the keyboard is unplugged; they are never saved.

## Custom keys

A **custom key** is a key of your own: 128 of them, each with a name, an
optional **shortcut** and an optional behaviour. Put one on a layer or make
it a combo's output.

The shortcut is up to six keys held together, any modifiers, left and right
mixed (Left Ctrl + Right Alt + K), and any layers, each of them optional:
modifiers alone, a mouse button, or layers alone are shortcuts too. It is the
custom key's own action: held while you hold the key, tapped when you tap it.
Unlike a macro it stays down; unlike `C(A(KC_K))` its modifiers can come from
both sides; unlike `MO()` it can hold several layers at once (Left Cmd +
Navigation + Extra).

Its keys are keyboard keys, media and system keys (play/pause, volume,
sleep) and mouse keys (buttons, the wheel, cursor movement), in the order you
want them pressed: Shift + A + S + D holds Shift, then A, S and D, and lets go
of D, S, A, then Shift. At most six, whatever the build, and at most one media
key and one system key, since the computer reads one of each at a time. So a
media key works wherever only the shortcut runs: behind a behaviour, from a
bypassed key, or as a combo's output.

Its layers act together as one layer key, the shortcut's **layer kind**:

- **Hold**, as `MO()`: on while the shortcut is held, off when you let go,
  unless something else keeps them on (a lock, another key holding the same
  layer, or a pointing mode's key). Tapped, they are on only for the tap.
- **Lock**, as `TG()`: pressing the key locks them all on, or, when all are
  locked already, all off.
- **Move to**, as `TO()`: pressing the key locks them alone; every other lock
  goes. Layers you are holding stay on until you let go.
- **Tap-toggle**, as `TT()`: on while the key is held; its fifth quick tap
  locks them all, as Lock does.
- **One-shot**, as `OSL()`: on while the key is held; a tap turns them on for
  the next key press only, and a quick second tap cancels it.

The shortcut's keys and modifiers work the same with every kind. With
Tap-toggle and One-shot the key is a dual-role key, as `TT()` and `OSL()` are:
its layers go on at the press, and so do its keys and modifiers unless its
behaviour sets a hold, so a key you roll in while it is down goes out at once
on them. A layer that starts a pointing mode
starts it. The base layer and the default layers are always on, so a shortcut
cannot name them.

A behaviour on the custom key works on top: the tiers it leaves empty send the
shortcut. A behaviour on another key that sends a custom key sends only its
shortcut, layers included, never its behaviour; so does a combo whose output
it is. A custom key with neither does nothing.

## Where behaviours and combos run

Behaviours and combos can each be switched off without losing them, for the
whole keyboard, for a source layer, for one behaviour or combo, or for one
key on one layer (a **placement**). A key uses its behaviour, or joins a
combo, only where all four allow it. The layer that counts is the **source
layer**, the one the key came from after falling through transparent keys.
These switches are set from Ark; the compiled defaults allow everything
everywhere.
