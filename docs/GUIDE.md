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

Charybdis Ark has **Pointing modes** with eight slots and eight matching
RGB configurations. Dragscroll, Volume, Brightness, Zoom, Arrow and Pinch occupy
the first six slots; slot 6 provides Undo / Redo and slot 7 starts empty. Create directional key or
shortcut actions with the shared keycode picker, duplicate a mode, or configure
scrolling with optional held modifiers. The everyday flow shows name, movement,
DPI and actions; pointer policy, thresholds, timing, modifier rules and mouse
buttons are under **Advanced**. Keep changes in the shared draft, bind its
hold/toggle action, set RGB, then review and Apply. See
[Pointer modes](POINTER_MODES.md).

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
layouts and layer names, each custom key's name, each macro's name beside its
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
- VIA macro defaults
- custom keys and their names
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

`CUSTOM_KEY_0` through `CUSTOM_KEY_63` are custom keys: named keys that do only
what their behavior row says. Each is named in the `CUSTOM_KEYS(KEY)` table in
`keymap.c` (at most 20 printable ASCII characters, `""` for an unused slot).
Without a row a custom key does nothing, and no behavior step can send one;
keymap validation and Charybdis Ark both refuse that.

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

`action` can be a normal keycode, a modified keycode such as `S(KC_1)`, a VIA
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
authored row on a `TT()` key replaces those taps. `OSL(layer)` holds its layer
while down, and a tap turns it on for exactly the next key press (modifiers and
`OSM()` do not use it up; as with QMK, a long press on its own also arms it
and a quick second tap cancels it; with one-shot keys off it is only a hold). A plain `LT(layer, kc)` without an authored row keeps
QMK's own tap/hold timing; only its hold goes through the userspace layer
ownership, so releasing it no longer turns off a locked layer. `LM(layer,
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
  used after the layer becomes active
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
- `PUT_VIA_KEYMAP_SYNC`: durable reconciliation of committed VIA keymap,
  encoder, macro, validity, and layout-option storage

You can forget those packet names immediately. The point is that both halves
know the same layers, keys, combos, pointing modes, and feedback state, so the
board behaves and lights up like one device instead of two disconnected halves.

VIA edits use a stricter path than transient lighting and pointing state. The
receiving half never trusts or replays an inbound VIA command. Instead, the
firmware marks local storage dirty before QMK changes it, reads back the
committed storage afterward, and reconciles a versioned snapshot with the
other half. Transfers are CRC-checked, range-checked, retried with bounded
backoff, and considered complete only after the receiver verifies the complete
digest and acknowledges it. Boot, reconnect, and USB-role changes always
exchange metadata again. A newer clean generation wins; if equal generations
have different contents, the current USB master wins deterministically and
publishes a new generation. RGB and VIA-macro caches refresh only after local
storage has committed.

## Building from source

This userspace is updated for QMK `0.32.5` and builds
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
Release images are built the same way by CI; the exact byte sizes may differ
between CI and local toolchains.

All current builds use the eight-slot engine and schema-2 EEPROM geometry. Use
`sh tools/build-firmware-pair.sh` for the flashable side-specific pair. The
right build uses `NOAH_PHYSICAL_HALF=right` and `FORCE_MASTER=yes`; the left uses
`NOAH_PHYSICAL_HALF=left` and `FORCE_SLAVE=yes`. A plain `qmk compile` produces
an eight-slot factory-only image without the live profile owner.

## Old firmware and profile limits

Old five-layer firmware is no longer built here. Its storage geometry is
incompatible with current firmware, so retain the old pair and its backups if
you still use it. The old five-layer and six-mode readback bridges are retired:
this repository no longer provides a firmware path for extracting and migrating
a profile from an old-geometry keyboard, and flashing schema-2 firmware over
that geometry does not preserve its committed profile. Existing schema-2
backups still restore through Ark.

Executable custom combo hooks and unsupported macro content cannot be
represented as profile data; export reports these explicitly instead of
producing an incomplete file. The
[portable profile contract](architecture/portable-profile-v1.md) records format
limits, compatibility, restore ordering and remaining hardware checks.
