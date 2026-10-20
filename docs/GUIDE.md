# Firmware guide

The long version of the [README](../README.md): what the firmware does in
depth, how to author its compiled defaults in C, how the two halves stay in
sync, and how to build it yourself. On a connected keyboard the profile it has
stored is the source of truth; edit that with
[Charybdis Ark](https://github.com/NoahCLR/charybdis-ark).

## Trackball modes

The trackball can change roles instead of only moving the cursor. The current
profile assigns these behaviors to slot keycodes. The slot number stays fixed
when a mode is renamed or reconfigured in Charybdis Ark:

- `PD_SLOT_0` (Dragscroll): ball motion becomes scrolling, as either a momentary hold or a
  lock
- `PD_SLOT_5` (Pinch): command-modified scrolling for pinch-style zoom on macOS; it
  needs an app that turns that gesture into zoom (see
  [Third-party apps in my setup](MY_SETUP.md))
- `PD_SLOT_3` (Zoom): explicit keyboard zoom using `Cmd+=` and `Cmd+-`
- `PD_SLOT_4` (Arrow): dominant ball motion sends arrow-key taps instead of cursor
  movement
- `PD_SLOT_1` (Volume) and `PD_SLOT_2` (Brightness): vertical ball motion changes system
  volume or display brightness
- `CUSTOM_KEY_2` (Click Spam): not a pointing mode, but a mouse-button combo
  output that uses the behavior table to repeat left-click while held
- window drags: also not a pointing mode, but hold branches that hold an extra
  mouse button so the ball resizes the window under the pointer (`MS_BTN3`
  double-tap hold, button 7) or moves it (`CUSTOM_KEY_3` (Drag Window) hold,
  button 6); an app binds those buttons to window management (see
  [Third-party apps in my setup](MY_SETUP.md))
- auto-mouse and auto-sniping layers that keep pointer work available
  automatically while you move between typing and trackball use

Because mode keycodes and their generated `*_LOCK` keycodes are normal actions,
`key_behaviors[]` can make one key hold a mode momentarily and toggle its lock on
a double-tap or double-tap hold. Locks are also easy to leave: pressing or
holding the same runtime-handled mode key while that mode is locked clears the
lock, then behaves as a normal momentary mode until release. Activating or
locking a different pointing mode clears the previous mode lock too.

Those are capabilities, not a fixed layout prescription. `keymap.c` decides
where these ideas live. Pointing-mode keys can be simple momentary holds, locks,
or richer tap/hold keys using the same behavior table as the rest of the board.

Charybdis Ark has **Pointing modes** with 32 slots and 32 matching
RGB configurations. Dragscroll, Volume, Brightness, Zoom, Arrow and Pinch occupy
the first six slots; slot 6 provides Undo / Redo and slots 7–31 start empty
(`PD_SLOT_0`…`PD_SLOT_31` hold a slot, `PD_SLOT_n_LOCK` locks it). Only the
slots in use take space in the saved profile. Create directional key,
shortcut, macro or mouse-button click actions with the shared keycode picker, duplicate a mode, or configure
scrolling with optional held modifiers. The everyday flow shows name, movement,
DPI and actions; pointer policy, thresholds, timing, modifier rules and mouse
buttons are under **Advanced**. Keep changes in the shared draft, bind its
hold/toggle action, set RGB, then review and Apply. See
[Pointer modes](POINTER_MODES.md).

A direction, diagonal or button tap override can start a macro. Triggers
while another macro is playing are dropped, never queued. **Every step** is
allowed; **Once per movement** usually fits a longer macro better. An empty
macro sends nothing and leaves the movement unspent. Macro outputs inherit
held modifiers (Ignore and Exact cannot cover playback across scans). A mouse
click inside the macro bypasses the mode's button overrides and reaches the
host through the existing owned-output ledger.

## Combo outputs and shared behaviours

A combo can use its output keycode's shared behaviour or take that keycode's
normal action. **Use output behaviour** controls one combo; **Combo outputs
use behaviours** controls all combo outputs while leaving physical-key
behaviours eligible. Both start on, and the existing behaviour master remains
a veto. Disabling the combo itself still stops the chord entirely.

An ordinary keycode keeps its normal action when bypassed. A custom key's
normal action is its shortcut, so bypassing it sends the shortcut, or nothing
if it has none. Output permission uses the
output behaviour's allowed-layer mask on the last declared input's captured
source layer; the layer behaviour switch remains physical-only. An output
already completed keeps its decision through queued delivery and release.

These firmware controls require an Ark version that adopts capability bit 30.
The app editor follow-up remains pending; the current app rejects the new
capability bit. Existing profile flags are zero and retain previous behaviour.

## The keyboard's name

Name the keyboard in Ark's Settings and your computer lists it under that
name: Chrome's HID chooser, the system's device list and Ark's keyboard menu.
It is stored in the profile, so it travels with backups. Left empty, the
keyboard keeps its built-in name, `keyboard_name` in the QMK fork's
`keyboard.json`. A keyboard with no saved profile uses the name its firmware
was built with, `NOAH_KEYBOARD_NAME` in the keymap's `config.h`.

Computers read a USB device's name only when it connects. After an Apply that
renames the keyboard, the half the cable is plugged into disconnects from USB
for a moment and reconnects under the new name. It waits until nothing is
held, no macro is running, both halves have the new profile and Ark has
stopped talking to it for a second; the other half never switches its USB on.
Installing firmware with this feature changes the profile format: the keyboard
starts from its compiled defaults, and a backup made with earlier firmware
cannot be restored onto it, only onto the firmware that made it. Keep the
earlier firmware and its backup if you may want to go back.

## Custom keys and their shortcuts

A custom key is a key of your own: you name it, place it on a layer or send
it from a combo, and decide what it does. It can carry a **shortcut**: one
key plus any modifiers, left and right mixed, such as Left Ctrl + Right Alt +
K, or modifiers alone (a held Hyper), or a key or mouse button alone.

The shortcut is the key's own action. Hold the custom key and the shortcut
stays held until you let go; tap it and the shortcut is tapped. That is the
difference from a macro, which plays and finishes. Modifiers you hold on other
keys stay down when you let go of it.

A behaviour on the custom key works on top of the shortcut, as it does on any
key: the taps and holds the behaviour leaves out send the shortcut, and so
does the key when its behaviour is bypassed. A behaviour on another key can
send a custom key too; it sends that key's shortcut only, never its
behaviour. A custom key with no shortcut and no behaviour does nothing.

Shortcuts are stored in the profile and need an Ark version that edits them.
Installing firmware with this feature changes the profile format: the keyboard
starts from its compiled defaults, and a backup made with earlier firmware
cannot be restored onto it, only onto the firmware that made it. Keep the
earlier firmware and its backup if you may want to go back.

## Authoring the profile in C

Edit compiled defaults directly in `keymap.c`, `config.h`, and `rgb_config.c`
under `keyboards/bastardkb/charybdis/4x6/keymaps/<name>/`. Validate authored
changes with the firmware host tests and regenerate their read-only overview
with `python3 tools/profile_introspect.py --keymap <name> --write`.
For the default profile, see [KEYMAP-OVERVIEW.md](KEYMAP-OVERVIEW.md) and
[KEYMAP.md](KEYMAP.md). On a connected keyboard, the committed device
profile is the source of truth; edit it with Charybdis Ark.

### The keymap model

The authored defaults live in the four keymap files. `keymap.c` holds the
layouts and layer names, each custom key's name and shortcut, each macro's name beside its
payload, the combos with any window of their own (`COMBO_WINDOW`), and the
behaviours; `config.h` the timing and policy values; `pd_config.c` the
pointing modes; `rgb_config.c` the lighting, including LED groups kept for
later. A keyboard with nothing stored reports exactly these; a stored profile
keeps its own.

[`keymap.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c) is the
main profile file. It is where you make the board yours.

Use it for:

- the physical layer layout
- combos
- macro defaults (`VIA_MACRO_n`, a historical name)
- custom keys, their names and shortcuts
- `key_behaviors[]`
- pointing-mode key placement and richer mode gestures

The important table is `key_behaviors[]`. Stock QMK already has useful pieces
of this idea: `LT()` and `MT()` cover common tap/hold keys, and Tap Dance can
make tap counts choose different outputs. This userspace is a different model:
one authored behavior row can combine tap counts, hold tiers, timing, RGB
feedback, combo outputs, pointer-mode ownership, layer ownership, and split
state in one place.

That means a key can branch more deliberately:

- tap can send one action
- hold can keep a modifier, layer, mouse button, or pointing mode active
- longer hold can do a stronger or different action
- double-tap and higher tap counts can expose locks, media, macros, layer
  changes, or alternate actions
- timing can be left at profile defaults or tuned per key

Combos can enter that same table too. If a combo emits a keycode that has a
`key_behaviors[]` row, the chord can reuse the same tap, hold, longer-hold, and
multi-tap behavior as a physical key.

Here is the shape of one authored row, based on the `CUSTOM_KEY_0` (Right Thumb) row in
[`keymap.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c#L478).
The lines between the keycode and `.tap_counts` are optional row-local
settings, shown here at their default values: three timing overrides and one
policy flag. The snippet shows one useful helper mix, not the full helper
vocabulary; the list below shows the other helpers you can use.

```c
{
    .keycode = CUSTOM_KEY_0,
    .tap_hold_term = 150,
    .longer_hold_term = 400,
    .multi_tap_term = 150,
    .keeps_auto_mouse_anchored = false,
    .tap_counts = {
        [0] = {
            .tap = TAP_SENDS(LOCK_LAYER(LAYER_NAV)),
            .hold = PRESS_AND_HOLD_UNTIL_RELEASE(MO(LAYER_NAV)),
        },
        [1] = {
            .tap = TAP_SENDS(KC_MPLY),
            .hold = TAP_ON_RELEASE_AFTER_HOLD(KC_ESCAPE),
            .long_hold = TAP_AT_HOLD_THRESHOLD(LOCK_LAYER(LAYER_NUM)),
        },
        [2] = {
            .tap = TAP_SENDS(KC_MNXT),
            .long_hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_MNXT),
        },
        [3] = {
            .tap = TAP_SENDS(KC_MPRV),
            .long_hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_MPRV),
        },
    },
},
```

In that example, `CUSTOM_KEY_0` can be placed directly on a layer or emitted by a
combo. Either way, the behavior row is the same.

A tap count that leaves `.tap` out repeats the first tap: the row's `[0].tap`,
or the key's own tap when that is left out too, once per press, so an empty
`[1].tap` sends it twice. A quick run of taps that goes past the deepest
tap count the row sets is **typing**: every press in it sends the first tap
once. A Backspace whose `[1].tap` types V sends Backspace for one tap, V for
two, and three Backspaces for three; a Delete whose rows set only holds on
its double tap deletes five times for five quick taps. A press held past the
deepest tap count takes the first tap count's hold, after the taps before it.

`CUSTOM_KEY_0` through `CUSTOM_KEY_127` are custom keys: named keys of your
own. Each is named in the `CUSTOM_KEYS(KEY, SHORTCUT)` table in `keymap.c` (at
most 32 bytes of UTF-8, `""` for an unused slot).

A custom key can also carry a **shortcut**: one key plus any modifiers, left
and right mixed, such as Left Ctrl + Right Alt + K. Write that slot's row as
`SHORTCUT(CUSTOM_KEY_4, "name", SHORTCUT_MODS(KC_LCTL, KC_RALT), KC_K)`.
Name the modifier keys in `SHORTCUT_MODS(...)`, or write `SHORTCUT_NO_MODS`
for none; a QMK `MOD_*` constant does not compile, since `MOD_RALT` would mean
other keys here. Use `KC_NO` for modifiers alone; the key is a basic
key or a mouse button (`MS_BTN1`–`MS_BTN8`). The shortcut is the key's own
action, as `KC_K` is `KC_K`'s: holding the custom key holds the shortcut until
you let go, and tapping it taps it. A behavior row on the key works on top of
that, exactly as on an ordinary key: the taps and holds the row leaves out
send the shortcut. Unlike a macro, a shortcut stays held; unlike `C(A(KC_K))`,
its modifiers can come from both sides.

Without a shortcut or a row a custom key does nothing. A behavior step on
another key may send a custom key: it sends that key's shortcut only, never
its behavior, and nothing if it has no shortcut.

The double-tap `.hold` uses the release-based helper because that branch also
has a later `.long_hold`. The single-tap layer hold and the media long-holds use
the press-and-hold helper because those branches can stay active until key
release.

The vocabulary is:

- `tap_counts[0]`, `[1]`, `[2]`, and onward are tap-count branches: single
  press, double press, triple press, and so on
- `.tap` is the quick-release tier for that branch
- `.hold` is the first hold tier for that branch
- `.long_hold` is the later hold tier for that branch
- `.tap_hold_term`, `.longer_hold_term`, and `.multi_tap_term` override timing
  for one row
- `.keeps_auto_mouse_anchored = true` marks the row as a mouse gesture, so
  pressing it keeps the pointer layer up instead of letting auto mouse reset on
  it. Needed for keys that drive the mouse without being mouse keycodes or
  pointer-mode keys, such as `CUSTOM_KEY_3` (Drag Window) and `CUSTOM_KEY_2`
  (Click Spam)

The helper vocabulary is:

- `TAP_SENDS(action)`: quick release sends `action`
- `PRESS_AND_HOLD_UNTIL_RELEASE(action)`: cross the hold threshold, press or
  register `action`, release it when the key is released
- `REPEAT_WHILE_HELD(action, hz)`: cross the hold threshold, tap `action`
  repeatedly at `hz` until release
- `TAP_AT_HOLD_THRESHOLD(action)`: send `action` once as soon as the hold tier
  commits
- `TAP_ON_RELEASE_AFTER_HOLD(action)`: qualify the hold at the threshold, then
  send `action` on release unless a longer hold replaces it

What a hold does decides what happens when you press another key before it
starts. A hold that is a layer, modifiers or a pointing mode makes that key
wait, so it lands on the layer or gets the modifier if you keep holding; a
hold that is already on from the press, such as `LM()` or a pointing key with
no hold of its own, makes nothing wait. Any
other hold, such as `S(KC_1)` on `KC_1`, is the key's own output, so the
other key's press types this key's tap first: rolls keep their order, and
for the hold you hold the key on its own. A hold that waits for release is
sent at that press if its hold time has passed. While a key that makes
others wait is undecided, letting go of a modifier or layer key you pressed
before it waits as well, as in QMK: Cmd released just before F still types
Cmd+F. A chord or custom key that holds modifiers counts as a modifier key
here. For any other key, letting go of the modifier types its tap at once,
so Cmd released just before `,` types Cmd+,. The
[interaction model](INTERACTION_MODEL.md#keys-pressed-while-a-taphold-key-is-undecided)
has the details.

`action` can be a normal keycode, a modified keycode such as `S(KC_1)`, a
macro, a supported QMK behavior keycode such as `OSM()` or
`MT()`, a generated pointing-mode lock such as `PD_SLOT_0_LOCK`, or a layer
lock through `LOCK_LAYER(layer)`. QMK's `TG(layer)` is the same lock as
`LOCK_LAYER(layer)`, and `TO(layer)` locks that layer alone and releases every
other lock (`TO(0)` returns to the base layer; layers held with `MO()` stay on
until released). Both work the same on a plain key, in a behaviour and as a
combo output. For custom momentary layer holds, use
`PRESS_AND_HOLD_UNTIL_RELEASE(MO(layer))` so the userspace owns the layer
state. `TT(layer)` holds its layer like `MO(layer)`, and its
`TAPPING_TOGGLE`-th tap (QMK's default, 5) sends `LOCK_LAYER(layer)`; an
authored row with steps on a `TT()` key replaces those taps. `OSL(layer)` holds
its layer while down, and a tap turns it on for exactly the next key press
(modifiers and `OSM()` do not use it up; as with QMK, a long press on its own
also arms it and a quick second tap cancels it; with one-shot keys off it is
only a hold).
A runtime-owned `MT()` consumes it on a qualifying tap release, even when the
tap output waits for repeated taps; its selected action keeps the press layer.
Holds and modifier-only taps leave it armed, and synthetic output never uses
it up again. Every `LT()`, `MT()` and `OSM()` key is decided by the firmware,
with or without a row, exactly as with an empty row: held past the dual-role
time, `LT(layer, kc)` holds its layer through the userspace layer ownership (so
releasing it no longer turns off a locked layer) and `MT()`/`OSM()` hold their
modifiers. Tapped and then pressed and held again at once, such a key starts a
new hold rather than repeating its tap as QMK did. `LM(layer,
mods)` holds its layer and its modifiers from press to release. `DF()` and
`PDF()` are refused: this firmware keeps layer 0 as the base. For a switchable
base such as a game layer, order that layer just above Base and use
`TO(layer)` to switch to it. Inside any helper, `KC_TRNS` means "use the lower active layer's
matching tap, hold, or long-hold behavior here."

The matching key-behavior RGB config in
[`rgb_config.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c#L319)
follows that same model. You do not need every field in every profile; this
example shows the vocabulary.

```c
const key_behavior_feedback_color_config_t key_behavior_feedback_colors = {
    RGB_TAP_BRANCH_COLORS(
        HSV(169, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS), // double tap
        HSV(222, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS), // triple tap
        HSV(85, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),  // quadruple tap
        HSV(25, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS)   // quintuple tap
    ),
    .tap_committed_color = HSV(0, 0, 150),
    .tap_commit_mode = KEY_FEEDBACK_TAP_COMMIT_NON_BASE_TAPS,
    .hold_active_color = HSV(18, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
    .long_hold_active_color = HSV(148, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
    .locality = RGB_KEY_HALF,
};

static const key_behavior_feedback_led_group_t
    key_behavior_feedback_led_groups_data[] = RGB_LED_GROUP_TABLE(
        {
            .semantic = KEY_FEEDBACK_GROUP_ALL,
            .color = HSV(0, 0, 0),
            .led_group = RGB_LED_GROUP_THUMBS,
        },
    );
```

- `RGB_TAP_BRANCH_COLORS(...)`: the color of the tap branch currently selected
  and not yet entered. Every tap past the base one switches the key to that
  branch's color and holds it until the branch is entered. The base tap stays
  dark
- `tap_committed_color`: a tap action just fired and does not already have a
  layer or pointing-mode state to show; inherited normal-tap repeats from a
  branch that omits `.tap` stay quiet
- `hold_active_color`: the `.hold` tier is pending, active, or committing
- `long_hold_active_color`: the `.long_hold` tier is active or committing
- `tap_commit_mode`: chooses whether tap commits pulse;
  `KEY_FEEDBACK_TAP_COMMIT_OFF` disables pulses and
  `KEY_FEEDBACK_TAP_COMMIT_NON_BASE_TAPS` enables them for double-tap and
  higher authored tap branches
- `locality`: chooses where the feedback paints with `RGB_BOTH_HALVES`,
  `RGB_LEFT_HALF`, `RGB_RIGHT_HALF`, `RGB_KEY_HALF`, or `RGB_KEYS_ONLY`
- key-behavior LED group semantics let named LED groups follow
  `KEY_FEEDBACK_GROUP_TAP_BRANCH_PENDING`,
  `KEY_FEEDBACK_GROUP_TAP_COMMITTED`, `KEY_FEEDBACK_GROUP_HOLD_ACTIVE`,
  `KEY_FEEDBACK_GROUP_LONG_HOLD_ACTIVE`, or `KEY_FEEDBACK_GROUP_ALL`

That behavior-specific feedback is the key-level part of the broader RGB
language. It gives you room to design compact keys without turning the source
into a pile of one-off feature code. [Lighting](#lighting) covers the layer,
pointing-mode, combo, LED-group, preview, and auto-mouse surfaces that use the
same authored RGB model.

### Main files

If you want to adapt the profile, start here:

| File | Use It For |
| --- | --- |
| [`keymap.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c) | layers, combos, macros, custom keys, and `key_behaviors[]` |
| [`rgb_config.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c) | layer colors, LED groups, pointing-mode colors, combo feedback, key feedback, and auto-mouse fade |
| [`config.h`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h) | layer enum, timing defaults, auto-mouse settings, RGB feedback toggles, and pointer policy |
| [`users/noah/config.h`](../users/noah/config.h) | split transport, LED geometry, pointing-device hardware settings, and shared board-level QMK overrides |

Most profile work should stay in the first three files. The shared runtime
under [`users/noah/`](../users/noah/) exists so those authored files can stay
small and data-driven.

## Lighting

RGB is used as feedback, not just decoration.

[`rgb_config.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c)
lets you author the visible language of the board:

- layer colors, either across the board or only on keys used by that layer
- reusable LED groups for thumbs, rows, halves, clusters, or any physical
  group that makes sense on the board
- pointing-mode colors that can paint both halves, one fixed half, the half
  that triggered the mode, or only the exact triggering keys
- combo feedback so chords can light near the keys that made them
- key-behavior feedback for waiting, preview, tap-count, hold, and repeat
  states
- preview overlays that show a pending momentary-layer hold with the same
  authored base color, inherited accents, universal groups, and override order
  used after the layer becomes active, on keys where tap and hold both land on
  that layer (an `LT()` key's tap types a letter, so it shows none)
- auto-mouse timeout feedback that fades as the temporary pointer layer is
  about to clear

`locality` is the RGB word for where a feedback surface paints. Depending on
the table, it can mean both halves, one fixed half, the half that owns the
triggering key or combo, or only the exact triggering keys with options such as
`RGB_BOTH_HALVES`, `RGB_LEFT_HALF`, `RGB_RIGHT_HALF`, `RGB_KEY_HALF`, and
`RGB_KEYS_ONLY`.

My build also has one extra trackball LED at LED `56`, documented in the
[`rgb_config.c` LED map](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c#L53).
It is optional: boards without that physical LED are still compatible with this
userspace; they just will not show trackball-specific LED group accents.

In practice, the lights can answer a few simple questions while you use the
board: which layer is active, which trackball mode is live, which physical keys
created a combo, whether a key is waiting for another tap, which tap-count
branch won, whether a hold or repeat action has committed, and how close the
auto-mouse layer is to timing out.

The auto-mouse RGB timer is a good example of the design style. When the
trackball wakes the pointer layer, the LEDs can start from the authored pointer
layer look and then fade toward the board state that will remain after the
auto-mouse layer drops. In plain terms: the lights can show how much time is
left before the board returns to normal.

## Split sync

A Charybdis has one controller per half, so runtime state cannot just live on
whichever half saw the key first. QMK's normal split settings cover the active
layer set and activity timer; this userspace adds custom split RPCs in
[`users/noah/config.h`](../users/noah/config.h#L54) and
[`runtime_sync.h`](../users/noah/lib/split/runtime_sync.h):

- `PUT_SPLIT_RUNTIME_BASE_SYNC`: auto-mouse RGB progress, active or locked
  pointing-mode IDs, key-local pointing-mode ownership, and preview-layer state
- `PUT_SPLIT_COMBO_FEEDBACK_SYNC`: combo underlay and overlay footprints, so
  `RGB_KEY_HALF` and `RGB_KEYS_ONLY` know which half or exact keys caused the
  combo
- `PUT_SPLIT_KEY_FEEDBACK_SEMANTIC_SYNC` and
  `PUT_SPLIT_KEY_FEEDBACK_BRANCH_SYNC`: key-feedback flash visibility, semantic
  state, broad owner groups, and tap-branch colors
- `PUT_PROFILE_SPLIT_SYNC`: the saved profile, copied and committed on both
  halves together
- `PUT_PROFILE_WORKING_SYNC`: live edits that are not saved yet, and the edit
  mode

You can forget those packet names immediately. The point is that both halves
know the same layers, keys, combos, pointing modes, and feedback state, so the
board behaves and lights up like one device instead of two disconnected halves.

Saved edits use a stricter path than transient lighting and pointing state.
A save reaches both halves as one profile generation: each half validates its
copy and keeps it as a prepared record, the half on USB commits first, and the
new profile becomes active only once the other half holds it too. Transfers
are CRC-checked, range-checked and retried with bounded backoff. Boot,
reconnect and USB-role changes always exchange the saved profile's identity
again, and the newer one wins.

## Live edits, Save and what a replug does

The keyboard keeps a working copy of its profile in RAM on each half. In a
live mode, an edit takes effect on both halves at the next safe boundary:
the keyboard waits while a key or layer is held, so nothing changes under
your fingers. Each keyboard remembers its edit mode outside the profile,
so a backup or review never changes that choice. The default is Live + review.

- **Apply:** Ark keeps edits in its draft until you review and Apply. Apply
  saves the complete profile on both halves and replaces any working copy.
- **Live:** the keyboard saves its working copy after **3 seconds without an
  edit and without a held key**. It does this even after Ark closes or loses
  its connection. Holding a key restarts the quiet interval; it waits a full
  quiet interval after release. Each edited revision gets at most one
  automatic save attempt. A failed save keeps the working copy unsaved and
  reports the failure; Save can retry it.
- **Live + review:** edits work immediately, and stay unsaved until you review
  and **Save**. The keyboard never saves them on its own. **Revert** returns to
  the saved profile without writing flash.

A replug always starts from the saved profile. In Live, unplugging before the
quiet interval and save finish loses the unsaved edits, usually the last few
seconds. In Live + review it loses every edit since Save. Status distinguishes
unsaved edits, a save in progress, a failed save and a completed keyboard save;
wait for completion before unplugging if you need to keep an edit. Ark's Live
controls and its "saved by the keyboard" indication follow this firmware work.

**DPI and RGB adjustment keys are edits too.** They change the current value
immediately and join the working copy on both halves at a safe boundary. In
Apply and Live the keyboard saves them after the same quiet interval; in Live
+ review they appear in review and wait for Save. Repeated presses coalesce
while another edit or save is in progress. An adjustment during a save becomes
a subsequent edit, never a change to the profile already being written. In
Apply, a working copy containing a posted client edit is never automatically
saved just because an adjustment joined it. While an adjustment waits to be
applied (a moment), a new Apply from the app is told the keyboard is busy and
tries again; an Apply already under way finishes first, and an adjustment made
during it follows it. A profile that has no settings of its own (it uses the
built-in ones) gets them, unchanged apart from your adjustment, the first time
you adjust.

The profile owns lighting mode and colour, default layers, keymap options and
DPI. QMK's EEPROM is never written by the adjustment keys and never wins over
the profile at boot. Participation toggles (combos and behaviours on/off) are
RAM overrides beside the stored choice: never saved, never reviewed, cleared
by a replug. RGB's on/off key changes the stored lighting setting like other
RGB adjustments.

Every Save, automatic or explicit, uses the same marker-last storage path on
both halves: a power cut leaves a complete old or new saved profile, never a
mixture. Erases wait for no held key and no matrix change for 50 ms; a key
pressed during a paced save can make it wait longer. A press and release
entirely inside a flash pause can still be missed. Pause diagnostics on both
halves are available for the first keyboard checks.

## Building from source

This userspace is updated for QMK `0.34.6` and builds
against my [BastardKB QMK fork](https://github.com/NoahCLR/bastardkb-qmk)
rather than the older `bkb-master`-based setup, at exactly the commit
[`qmk-pin.json`](../qmk-pin.json) names. Releases use the fork's released
[`noah-userspace-contracts`](https://github.com/NoahCLR/bastardkb-qmk/tree/noah-userspace-contracts)
branch at that commit, tagged with the same version as this repository. On
top of QMK it carries the RP2040 Charybdis 4x6 board definition and a few small
hooks this userspace uses: the auto-mouse timer getter, for the auto-mouse RGB
timeout fade and split-synced progress; the split activity hooks, for activity
coalescing; the split frame CRC, which refuses garbled split messages; and the
physical event queues and record admission hook, for gesture timing.

Use that firmware fork, point `QMK_USERSPACE` at this repo,
and build with:

```sh
qmk compile -kb bastardkb/charybdis/4x6 -km noah
```

That single command builds a generic image **without** the live-profile
owner, because the owner needs a provisioned physical half. The firmware you
flash is the side-specific pair. Build it with the VS Code task
`Build Firmware Pair (flashable)`, or from a shell:

```sh
sh tools/build-firmware-pair.sh
```

That writes numbered left and right images into `../builds/<branch>/`. Add
`--no-owner` for the comparison pair without the live-profile owner.
It builds in Docker, in the same image CI builds releases in
(`tools/build-image`), so the pair you flash and the pair a release publishes
come from the same compiler and are byte-identical for the same commit. Docker
must be running. A note beside each pair (`N_charybdis.build.txt`) records the
firmware and BK commits, the compiler and both files' SHA-256.

All current builds use the 32-slot engine and the flash slot region of D-F29. Use
`sh tools/build-firmware-pair.sh` for the flashable side-specific pair. The
right build uses `NOAH_PHYSICAL_HALF=right` and `FORCE_MASTER=yes`; the left uses
`NOAH_PHYSICAL_HALF=left` and `FORCE_SLAVE=yes`. A plain `qmk compile` produces
a 32-slot factory-only image without the live profile owner.

## Old firmware and profile limits

The VIA app is not supported (decided 2026-10-19). The firmware answers only
Charybdis Ark's Profile Wire, keys and macros live in the saved profile, and a
VIA client finds nothing it can read or write. Use Ark for everything.

Old five-layer firmware is no longer built here. Its storage geometry is
incompatible with current firmware, so retain the old pair and its backups if
you still use it. The old five-layer and six-mode readback bridges are retired:
this repository no longer provides a firmware path for extracting and migrating
a profile from an old-geometry keyboard, and flashing current firmware over
that geometry does not preserve its committed profile. A backup made with
earlier firmware cannot be restored onto this one.

Executable custom combo hooks and unsupported macro content cannot be
represented as profile data; export reports these explicitly instead of
producing an incomplete file. The
[portable profile contract](architecture/portable-profile-v1.md) records format
limits, compatibility, restore ordering and remaining hardware checks.

## Text macros and your keyboard layout

The keyboard sends key positions; your computer's keyboard layout decides which
character each one types. So that macros type the right text, pick the layout
your computer uses in the editor's Settings → Host section. The first set is:

- US (the default; what macros have always assumed);
- macOS ABC, Dutch, Unicode Hex Input, British, German and French;
- Windows US International, United Kingdom, German and French;
- Linux US International, United Kingdom, German and French.

Each character is then typed with that layout's own keys, including accented
letters, curly quotes and € through Option, AltGr or dead keys. The layout you
pick must be the one the computer is using; the keyboard cannot check. On a Mac
that set the keyboard up as ISO, turn on the ISO option there too, or `<` and
`` ` `` swap.

Characters the layout cannot type, such as emoji, need Unicode entry:

- **macOS:** choose the Unicode Hex Input layout and make it the active input
  source. A Mac layout with Option characters, such as Dutch, cannot also take
  Unicode entry, because Option types characters there.
- **Windows:** enable Unicode playback and run WinCompose with Right Alt as
  Compose.
- **Linux:** enable Unicode playback and use an input method or application
  that accepts Ctrl+Shift+U entry, such as IBus. Applications without it are
  unsupported.

A macro with a character neither the layout nor Unicode entry can type does not
play. Auto uses QMK's USB OS guess, which may be wrong through a KVM or switch;
Unicode entry stays off until the OS is known or chosen. Unicode entry is slower
than ordinary key taps. Stored macros share 34,903 bytes of memory across 128
slots; one macro can use all the free space, with no separate playback-length
limit. Writing or resetting the macro memory cancels a playing macro and
releases its held keys. This storage layout resets older on-keyboard data on
first flash, and a backup made with earlier firmware cannot be restored onto
it.
Uninterruptible playback defaults on when Unicode entry is needed, and you can
force it On or Off per macro in Ark. Protected playback starts immediately:
already-held ordinary keys stop repeating, and only the macro's keyboard output
reaches the computer until cleanup finishes. New presses are ignored, releases
still update internal state, and modifiers still held resume afterward. Ordinary
keys remaining held stay inactive until released and pressed again. Ignored
presses are never replayed. Pointing, consumer/media actions and other keyboards
remain outside this protection. With protection Off, avoid typing or holding
ordinary keys during playback. Cancellation can leave text already typed,
including a partial macOS entry.
[QMK's Unicode guide](https://docs.qmk.fm/features/unicode) describes host
setup. Physical acceptance of each layout on its own OS is still pending.
