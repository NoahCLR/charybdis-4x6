# Noah's Charybdis Userspace

This repo is the shared userspace for my Charybdis 4x6.

For the editor's build, test and compilation-database tasks, see the
[VS Code workflow](.vscode/README.md). Charybdis Ark's editor tasks now belong
to its independent sibling repository.

## Local repositories and worktrees

On Noah's machine, the main checkouts are:

- Firmware: `/Users/noah/dev/charybdis/charybdis-4x6`.
- Ark: [NoahCLR/charybdis-ark](https://github.com/NoahCLR/charybdis-ark)
  (`/Users/noah/dev/charybdis/charybdis-ark` locally).
- Upstream QMK build dependency: `/Users/noah/dev/charybdis/bastardkb-qmk`
  ([NoahCLR/bastardkb-qmk](https://github.com/NoahCLR/bastardkb-qmk):
  development on `noah-userspace-contracts-dev`, released line
  `noah-userspace-contracts`, legacy upstream mirror `main`).
- Work queue: `/Users/noah/dev/charybdis/charybdis-notes`, an Obsidian vault
  (private [NoahCLR/charybdis-notes](https://github.com/NoahCLR/charybdis-notes))
  holding notes, tasks, active plans and keyboard checks for all three
  repositories. Its `AGENTS.md` says how a task is refined and picked up.

Branches: `dev` is the trunk. Each task branches from `dev` in its own
worktree and is squash-landed back onto `dev` locally; `main` is the released
line and only moves when a release promotes `dev` into it. Landed commits, promotions
and release tags carry trailers naming the Ark and QMK commits they were tested
with, so `git log` answers what any build went with. A release is one date tag,
`vYYYY.MM.DD`, on firmware, Ark and the QMK fork together. This repository is a GitHub
fork of Bastard Keyboards' userspace: push only to `NoahCLR/charybdis-4x6`, and
never push or open a pull request upstream. The clone's `gh` default and
pre-push hook enforce that.

Release CI runs the complete host suite before building the side-specific pair,
against the matching QMK tag. Both UF2 files are attached to a draft release;
the shared vault's `release VERSION --push` makes the releases public only after
firmware and Ark CI pass and both firmware assets are present. If publication
is interrupted, rerun the same command: it resumes the saved release commits,
even if `dev` has since advanced. Direct tag pushes leave the release in draft. The
third-party action that attaches the pair runs with write access, so it is pinned
to a reviewed commit hash; move the pin deliberately, after reading the new code.

Development runs the host suite with macOS clang; CI runs it with GCC in the
QMK container, on every `dev` push and pull request (`host_tests.yml`) as well
as before a release build. Host tests put QMK's directories on
`C_INCLUDE_PATH`, so both compilers treat QMK as third-party system headers: a
warning inside QMK cannot fail a build, while warnings in our own sources and
test shims remain errors.

### The BK pin

The UF2 is this userspace and the BK fork compiled together, so this repository
names the exact BK commit it builds with in `qmk-pin.json`: a published commit
on the fork's `noah-userspace-contracts-dev`. Everything uses it:
`tools/build-firmware-pair.sh` refuses a BK checkout that is not at the pin or
has local changes (`NOAH_ALLOW_UNPINNED_QMK=1` allows it for a trial, printed
as such), CI's `Host suite (GCC)` and the release build check BK out at the pin,
and the release build refuses a BK tag that is not the pinned commit. A
non-required CI job (also nightly) runs the host suite against the BK dev head
and reports how far it is ahead of the pin. The vault's `verify` checks BK out
at the pin itself, and its push hook refuses a push whose pin is not on the
published BK dev branch.

Re-pin with `sh tools/pin-qmk.sh [REV]` (default: the published BK dev head).
It lists the BK commits since the old pin and what changed by area: hooks and
core, keycode numbering, VIA, RGB matrix, the Charybdis board and submodules.
Keycode, VIA or RGB changes need an Ark follow-up.

### The firmware contract

`sh tests/host/run_contract_probe.sh` states what this firmware promises a
client: the exact Profile Wire capability pages the keyboard answers once its
live-profile owner is up (versions, feature flags, capacities, the action-ABI
digest and the compiled-default digest), the BK pin, and the SHA-256 of every
fixture under `tests/fixtures`. It builds them from the code the keyboard
runs: the VIA channel and the probe share `lib/compat/qmk_via_profile_capabilities.h`,
the digests come from the compiled-defaults code the owner uses, and the bytes
from the Profile Wire encoder. The host suite runs it; every release build
attaches it as `firmware-contract.json`. Firmware only states the contract; the
client's agreement check judges it.

The pair builder isolates QMK CLI configuration for the compiler and its code
generators, so saved `overlay_dir`/`qmk_home` values cannot redirect a task build
to the main checkout. Explicit keymap paths also override old symlinks inside
the QMK tree. Local `verify` records the full tested inputs and artifact checksums. `land`
reuses that result only while those inputs match, and files its recorded inputs
beside the firmware pair. The installed pre-push hook requires a matching local
verification receipt for new protected-branch commits and a stack certificate
for release tags; a hand-written `Stack-Tested` trailer is insufficient.

Use the worktree assigned to your task. Run `git worktree list` in the relevant
repository to discover its other checkouts; do not assume the main checkout
contains another agent's branch. Relative sibling paths below describe the main
workspace layout and may not hold in a worktree. Select QMK explicitly with
`QMK_ROOT` for host tests and `QMK_HOME` for the QMK CLI, and set `QMK_USERSPACE`
to the firmware worktree being built.

Firmware agents own this checkout's C code, tests and firmware documentation.
Ark agents own the independent app checkout and its documentation; read its
`AGENTS.md` and `README.md` there. The former `tools/charybdis-live/` copy has
been removed; its source history remains in Git. Inspecting another checkout
is allowed;
editing it requires that scope in the task. Coordinate shared QMK build output
and keyboard access with other agents. Firmware tests and builds require no
Ark checkout; Ark owns the optional cross-repository integration tests.

## Firmware overview

It is intentionally Charybdis-specific. The trackball behavior, split sync,
auto-mouse layer, and RGB assumptions are built around this split trackball
board rather than stock QMK conventions.

This is still a personal configuration, but it is not meant to be a pile of
one-off hacks. The point is to keep the interesting behavior centralized and
editable, so the board can grow through authored profile data instead of
scattered runtime rewrites.

> **Opinionated userspace warning:** This repo depends on QMK, but it is not a
> standard copy-paste QMK keymap. I have interpreted some QMK surfaces
> differently and shaped them into a Charybdis-specific userspace with its own
> runtime, data tables, RGB language, and split sync.
> If you are looking for small snippets to drop into a normal keymap, this is
> probably not the easiest place to start. It is more useful as an example of a
> very opinionated firmware model.
>
> **Firmware note:** This userspace is updated for QMK `0.32.5` and builds
> against my [BastardKB QMK fork](https://github.com/NoahCLR/bastardkb-qmk)
> rather than the older `bkb-master`-based setup, at exactly the commit
> [`qmk-pin.json`](qmk-pin.json) names. Releases use the fork's released
> [`noah-userspace-contracts`](https://github.com/NoahCLR/bastardkb-qmk/tree/noah-userspace-contracts)
> branch at that commit, tagged with the same version as this repository. On top of QMK it carries three
> small changes this userspace uses: the auto-mouse timer getters, for the
> auto-mouse RGB timeout fade and split-synced progress; the split activity
> hook, for activity coalescing; and the split frame CRC, which refuses
> garbled split messages.
>
> **Build note:** Use that firmware fork, point `QMK_USERSPACE` at this repo,
> and build with:
>
> ```sh
> qmk compile -kb bastardkb/charybdis/4x6 -km noah
> ```
>
> That single command builds a generic image **without** the live-profile
> owner, because the owner needs a provisioned physical half. The firmware you
> flash is the side-specific pair. Build it with the VS Code task
> `Build Firmware Pair (flashable)`, or from a shell:
>
> ```sh
> sh tools/build-firmware-pair.sh
> ```
>
> That writes numbered left and right images into `../builds/<branch>/`. Add
> `--no-owner` for the comparison pair without the live-profile owner.
> Pushing a `v*` tag makes CI build the same flashable pair and publish a
> GitHub release with separate
> `bastardkb_charybdis_4x6_noah_left.uf2` and
> `bastardkb_charybdis_4x6_noah_right.uf2` assets. The exact byte sizes may
> differ between CI and local toolchains.

This repo is built around the open-source Charybdis from
[BastardKB](https://bastardkb.com/), designed by Quentin. The hardware files
are available in the
[BastardKB Charybdis project](https://github.com/Bastardkb/Charybdis).
Quentin's design, and the many mods the community has built around this board,
are what made this build possible. This keyboard has given me hundreds of
hours of useful firmware and hardware tinkering. If you want to support the
creator, buy the hardware from [BastardKB](https://bastardkb.com/) rather than
from a knockoff seller.

## Configurable pointing modes

The live editor now has **Pointing modes** with eight slots and eight matching
RGB configurations. Dragscroll, Volume, Brightness, Zoom, Arrow and Pinch occupy
the first six slots; slot 6 provides Undo / Redo and slot 7 starts empty. Create directional key or
shortcut actions with the shared keycode picker, duplicate a mode, or configure
scrolling with optional held modifiers. The everyday flow shows name, movement,
DPI and actions; pointer policy, thresholds, timing, modifier rules and mouse
buttons are under **Advanced**. Keep changes in the shared draft, bind its
hold/toggle action, set RGB, then review and Apply. See
[Pointer modes](docs/POINTER_MODES.md).

All current builds use the eight-slot engine and schema-2 EEPROM geometry. Use
`sh tools/build-firmware-pair.sh` for the flashable side-specific pair. The
right build uses `NOAH_PHYSICAL_HALF=right` and `FORCE_MASTER=yes`; the left uses
`NOAH_PHYSICAL_HALF=left` and `FORCE_SLAVE=yes`. A plain `qmk compile` produces
an eight-slot factory-only image without the live profile owner.

The old five-layer and six-mode readback bridges are retired. This repository
no longer provides a firmware path for extracting and migrating a profile from
an old-geometry keyboard. Keep any old firmware and backups you already have;
flashing schema-2 firmware over that geometry does not preserve its committed
profile. Existing schema-2 backups still restore through Ark.

Physical migration, power-interruption, pointing cadence and stack high-water
acceptance remain release gates; see the
[PD-mode domain contract](docs/architecture/pd-mode-domain-v1.md#hardware-acceptance).

## What This Userspace Is For

This repo is my Charybdis 4x6 userspace and profile. The interesting part is
how the profile is authored: key behavior lives in data tables, RGB feedback has
its own authored language, and the shared runtime turns those choices into
firmware behavior.

The two main authoring files are:

- [`keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c):
  what keys, layers, combos, macros, and per-key behaviors exist
- [`rgb_config.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c):
  how layers, pointer modes, combos, key states, and auto-mouse timing are
  shown on the LEDs

The goal is not to copy one exact layout. The useful part is the model: you can
describe what a key should do, describe what the lights should show, and let
the shared userspace handle the timing, split sync, trackball modes, VIA
bridges, and RGB rendering behind that.

## What You Can Build

You can keep a layout readable while still giving the board a lot of
behavior:

- layers for typing, numbers, symbols, navigation, pointer controls, or any
  other surface you want
- combos for simultaneous chords, including chords that emit a keycode handled
  by `key_behaviors[]`
- 64 VIA macro slots, each with a name, editable live or from source defaults
- keys that do one thing on tap, another on hold, another on longer hold, and
  different things again on double-tap or higher tap counts
- pointing-mode keys that can be simple momentary holds, locks, or richer
  tap/hold keys using the same behavior table as the rest of the board

You can also make the trackball change roles instead of only moving the cursor.
The current profile assigns these behaviors to slot keycodes. The slot number
stays fixed when a mode is renamed or reconfigured in Charybdis Ark:

- `PD_SLOT_0` (Dragscroll): ball motion becomes scrolling, as either a momentary hold or a
  lock
- `PD_SLOT_5` (Pinch): command-modified scrolling for pinch-style zoom on macOS; in my
  setup this expects third-party software such as
  [BetterMouse](https://better-mouse.com/) to translate that gesture
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
  button 6); in my setup [Rectangle Pro](https://rectangleapp.com/pro) is what
  binds those buttons to window management
- auto-mouse and auto-sniping layers that keep pointer work available
  automatically while you move between typing and trackball use

Because mode keycodes and their generated `*_LOCK` keycodes are normal actions,
`key_behaviors[]` can make one key hold a mode momentarily and toggle its lock on
a double-tap or double-tap hold. Locks are also easy to leave: pressing or
holding the same runtime-handled mode key while that mode is locked clears the
lock, then behaves as a normal momentary mode until release. Activating or
locking a different pointing mode clears the previous mode lock too.

Those are capabilities, not a fixed layout prescription. `keymap.c` decides
where these ideas live.

## Authored firmware profiles

Edit compiled defaults directly in `keymap.c`, `config.h`, and `rgb_config.c`
under `keyboards/bastardkb/charybdis/4x6/keymaps/<name>/`. Validate authored
changes with the firmware host tests and regenerate their read-only overview
with `python3 tools/profile_introspect.py --keymap <name> --write`.
For the default profile, see [KEYMAP-OVERVIEW.md](docs/KEYMAP-OVERVIEW.md) and
[KEYMAP.md](docs/KEYMAP.md). On a connected keyboard, the committed device
profile is the source of truth; edit it with Charybdis Ark.

## Charybdis Ark

Charybdis Ark edits the connected keyboard over
Raw HID. Its runtime never reads this repository, so it needs no firmware workspace: the
keyboard is the source of truth and the app is its client. Authored C files
remain the firmware's compiled defaults.

The active VS Code installation uses the independent sibling `charybdis-ark`
repository. For a new installation, symlink the sibling folder into VS Code's
extensions, then reload the window:

```sh
cd ../charybdis-ark && npm ci
ln -s "$PWD" ~/.vscode/extensions/noah.charybdis-ark-0.1.0
```

Open it from the **Charybdis Ark** status bar item or `Charybdis: Open
Charybdis Ark`, or press `F5` with the sibling repo's `Run Charybdis Ark`
launch configuration for an Extension Development Host.

It reads everything it edits from the keyboard — layout, key behaviours,
combos, the named VIA macros, custom-key names, lighting, the eight
pointing-mode slots, layers and settings — and shows compiled defaults only
when the keyboard reports no committed profile. While a read runs, all screen
menus are disabled and a loading step replaces the screen. The Configure menus
and board open when the complete profile is ready for editing. After a read
fails, **Device** is available if the keyboard reported its capabilities;
**Profile & backups** opens if the connected keyboard supports complete-profile
backup.
Macro keycodes are shown as `VIA_MACRO_0` through `VIA_MACRO_63` in the live
editor, including slots whose QMK values have no named constant.
**Custom keys** lists the 64 custom keys: rename one, add or edit its
behaviour, place it on a key, and see where it is used; the key picker offers
them in its own Custom keys section. Their names are stored on the keyboard; a
keyboard with none stored reports the names authored in `keymap.c`.
Every edit goes into one local draft with undo, redo and a history, and reaches
the keyboard only through **Review and apply**. Apply
saves a recovery copy, then commits the complete profile to both halves as one
recovery-first logical transaction and verifies the readback. The review shows
warnings in orange, traps and save blockers in red. It checks layer reachability,
combos, inert pointing bindings, and macros the keyboard cannot play. Apply asks
for confirmation when a warning or trap is present and stays disabled until
destination save blockers are resolved.
Removing one combo appears as one deletion in Review and Draft history; later
combo numbers shift because the device stores them in a packed table.

In **Keys → Combos**, **Pick on board** brings the board into view so its keys
can be selected as combo inputs.
The **Key** tab count is the number of mapped keys on the selected layer;
the selected key's layout index appears in its details.
In **Keys → Behaviours**, reach sections open independently. Matching sections
share their expanded or collapsed state across the Behaviours, Combos, Macros
and Pointing modes tabs. The full list scrolls with the Keys page rather than
inside the rail.
The Behaviours, Combos, Macros and Pointing modes tabs start with **On this
view**: what the board reaches with the selected layer and any layers previewed
under it. **On this layer** stays tied to keys stored on the selected layer;
the tab counts use that layer too. Use ⌘-click on layer tabs to change the
composed view. Combo rows use the keyboard's Combo Layer Matching reference
when one is configured.
Every Keys workbench tab is at least as tall as Behaviours, so switching tabs
keeps the page at the same scroll position. If an editor grows taller, that
height stays while switching tabs.

After Apply completes, the editor stays visible while the app reads keys,
profile domains, combos and base lighting back from the keyboard. The bottom
bar names each read and shows its progress; editing resumes when it finishes.

What to expect while it applies:

- Apply waits for held keys, locked layers and pointer modes to clear before
  the commit decision, and says so; after 60 s the save is cancelled and
  nothing changes. Anything that fails before the decision leaves the saved
  profile unchanged. A save the app abandoned before the decision is cancelled
  by the keyboard within about 15 seconds, and the next save can start once
  both halves are connected.
- It then holds key input for the few moments while this half's keys and
  macros are rewritten and the new profile activates. If the app is closed in
  that window, the keyboard finishes the save on its own from the other half's
  copy within about 15 seconds.
- If the cable between the halves comes out after the decision, the USB half
  keeps typing the old profile until the rewrite starts, and the save resumes
  when the cable goes back in. If it comes out during the rewrite, the USB half
  finishes and switches to the new profile, and the app says the other half is
  not connected.
- After power loss in the middle of a save, the USB half types nothing until
  its keys and macros are one complete version again, which may need the other
  half connected.

Drafts live in the editor window; closing it loses unapplied changes. The
transaction and recovery contract is specified in
[`docs/architecture/logical-profile-transaction-v1.md`](docs/architecture/logical-profile-transaction-v1.md).
Physical power-loss acceptance across every decision boundary is still in
progress, so keep the recovery file that Apply creates.

Readback and Apply need the side-specific firmware pair from
`sh tools/build-firmware-pair.sh`, which carries the live-profile owner; the
generic image does not.

The app's current guide is `README.md` in the independent Ark checkout listed
under [local repositories](#local-repositories-and-worktrees); its `docs/`
directory owns app development guidance. Earlier in-tree guides remain in Git
history.
The direction, the decisions behind it, and what is deliberately left
undesigned are in
[`docs/LIVE_EDIT_APP_DIRECTION.md`](./docs/LIVE_EDIT_APP_DIRECTION.md).

The next sections explain the authored keymap and RGB models.

## The Keymap Model

The authored defaults live in the four keymap files. `keymap.c` holds the
layouts and layer names, each custom key's name, each macro's name beside its
payload, the combos with any window of their own (`COMBO_WINDOW`), and the
behaviours; `config.h` the timing and policy values; `pd_config.c` the
pointing modes; `rgb_config.c` the lighting, including LED groups kept for
later. A keyboard with nothing stored reports exactly these; a stored profile
keeps its own.

[`keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c) is the
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
[`keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c#L478).
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
[`rgb_config.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c#L319)
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
into a pile of one-off feature code. The next section covers the layer,
pointing-mode, combo, LED-group, preview, and auto-mouse surfaces that use the
same authored RGB model.

## The RGB Model

RGB is used as feedback, not just decoration.

[`rgb_config.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c)
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
[`rgb_config.c` LED map](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c#L53).
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

## Split Sync

A Charybdis has one controller per half, so runtime state cannot just live on
whichever half saw the key first. QMK's normal split settings cover the active
layer set and activity timer; this userspace adds custom split RPCs in
[`users/noah/config.h`](./users/noah/config.h#L54) and
[`runtime_sync.h`](./users/noah/lib/split/runtime_sync.h):

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

## Main Files

If you want to adapt the profile, start here:

| File | Use It For |
| --- | --- |
| [`keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c) | layers, combos, macros, custom keys, and `key_behaviors[]` |
| [`rgb_config.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c) | layer colors, LED groups, pointing-mode colors, combo feedback, key feedback, and auto-mouse fade |
| [`config.h`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h) | layer enum, timing defaults, auto-mouse settings, RGB feedback toggles, and pointer policy |
| [`users/noah/config.h`](./users/noah/config.h) | split transport, LED geometry, pointing-device hardware settings, and shared board-level QMK overrides |

Most profile work should stay in the first three files. The shared runtime
under [`users/noah/`](./users/noah/) exists so those authored files can stay
small and data-driven.

## Docs Map

Use the docs based on what you want to change:

- [`docs/KEYMAP-OVERVIEW.md`](./docs/KEYMAP-OVERVIEW.md): generated visual
  report of the authored profile
- [`docs/KEYMAP.md`](./docs/KEYMAP.md): prose notes for the current authored
  profile choices
- [`docs/INTERACTION_MODEL.md`](./docs/INTERACTION_MODEL.md): tap, hold,
  longer-hold, and multi-tap semantics
- Gesture timing investigation (plan *Gesture timing and combo arbitration* in
  the work-queue vault):
  Button 3 double-hold failure, combo/tapping interactions, and the current-layout
  acceptance plan
- [`docs/POINTER_MODES.md`](./docs/POINTER_MODES.md): what each trackball mode
  does once active
- [`docs/architecture/pd-mode-domain-v1.md`](./docs/architecture/pd-mode-domain-v1.md):
  eight PD slots: scope, wire contract, schema/ABI migration, capacity and
  remaining hardware acceptance
- [`docs/RGB_CONFIG.md`](./docs/RGB_CONFIG.md): RGB authoring model, render
  order, LED groups, and auto-mouse fade
- [`docs/ADDING_PD_MODE.md`](./docs/ADDING_PD_MODE.md): maintainer guide for
  adding another pointing-device mode
- [`docs/KEY_RUNTIME.md`](./docs/KEY_RUNTIME.md): maintainer map of the
  handled-key runtime and ownership model
- [`docs/HOOK_OVERRIDES.md`](./docs/HOOK_OVERRIDES.md): how to override QMK
  hooks without dropping shared userspace behavior
- [`docs/tooling/PROFILE_INTROSPECT.md`](./docs/tooling/PROFILE_INTROSPECT.md):
  generated profile docs workflow
- [`docs/tooling/VIA_TO_QMK.md`](./docs/tooling/VIA_TO_QMK.md): round-trip VIA
  exports back into source
- [`docs/architecture/README.md`](./docs/architecture/README.md): maintainer
  entry point for runtime ownership and source boundaries
- [`docs/LIVE_EDIT_APP_DIRECTION.md`](./docs/LIVE_EDIT_APP_DIRECTION.md): the
  firmware side of live editing: status, open issues and firmware decisions. The
  product goal, app status and app decisions are in the Charybdis Ark repository.

## AI Workflow Note

I do use AI as part of the workflow around this repo.

The config is still hand-owned daily-driver firmware. Many hours have gone
into tuning the hardware, the layout, the runtime behavior, and the
documentation.

## A Little Show-Off Of My Build

<div align="center">
<video src="https://github.com/user-attachments/assets/fb5749e2-6f30-44de-99d7-9bd47f94659a" controls></video>
</div>

### Complete keyboard profiles

Charybdis Ark's **Export profile** saves the configuration read from the
keyboard: every layer and key position, behaviours, combos, named VIA macros,
lighting and global settings. Flashed defaults and live edits become one
portable file. **Import profile** shows a review, saves a recovery copy, restores
both halves and verifies the complete readback. A failed or interrupted restore
reports the saved recovery file instead of claiming success. Recovery files are
kept in the extension's local storage; the app shows their full path.

The standard firmware reserves eight layers. **Manage layers** names and orders
the overlays, with the highest-priority layer shown first and Base fixed at the
bottom. Moving a layer updates the keys, behaviours, combos, RGB assignments and
pointer settings that refer to it. There is no need to change the layer count
or reflash for ordinary profile editing.
**Make base** swaps an overlay with the base: transparent keys entering the base
become `KC_NO`, `KC_NO` keys leaving it become transparent, and an uncoloured
former base gets the saved base HSV as its own layer colour. The draft review
counts transparent and `KC_NO` base keys in separate notices.

Old five-layer firmware is no longer built here. Its storage geometry is
incompatible with current firmware, so retain the old pair and its backups if
you still use it. Executable custom combo hooks and unsupported macro content
cannot be represented as profile data; export reports these explicitly instead
of producing an incomplete file.
The [portable profile contract](docs/architecture/portable-profile-v1.md) records
format limits, compatibility, restore ordering and remaining hardware checks.

## Split transport comparison builds

The firmware coalesces split activity messages by default: it sends fewer
repeated lighting-activity messages to the other half while still reading its
keys on every scan. The split link always runs at QMK's default 230,400 baud; a
faster link garbled split messages and was removed (D-L43).

```sh
NOAH_SPLIT_ACTIVITY_COALESCE=no sh tools/build-firmware-pair.sh
```

builds the uncoalesced comparison pair, with `_no_activity` in its artifact
names. Every split frame carries a checksum by default, so a garbled message
is refused instead of shown; `NOAH_SPLIT_CRC=no` builds the comparison pair
without it (`_no_crc`). Always flash both halves from the same pair: halves
built with and without the checksum refuse each other at the handshake. For a measurement pair add `NOAH_SPLIT_DIAGNOSTICS=yes` (the ten-second
transaction recorder, `_diagnostic`) and `NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS=yes`
(the pointing-cadence recorder with per-stage loop timing, `_cadence`), then run
`node tools/capture-split-diagnostics.cjs` after flashing; it reads both. Build with Homebrew Python 3.12+ on PATH for the profile tooling.
See [the activity contract](docs/architecture/split-activity-sync.md), and follow
[the capture procedure](measurements/pointing-cadence/README.md) so captures
compare; recorded sets live under [`measurements/`](measurements/README.md).

Firmware builds and host tests do not require Ark. The independent Ark
repo owns the optional `npm run test:compat` integration gate for joint protocol
work. See [the independence contract](docs/architecture/ark-compatibility.md).
Install this repo's diagnostics dependencies with `npm ci --prefix tools` before
using `tools/capture-split-diagnostics.cjs`; its HID dependency is independent
of Ark.

Handled keys preserve physical gesture timing across combo/tapping buffering;
authored layer-tap behaviours use one tap/hold decision. See the
[timing contract](docs/INTERACTION_MODEL.md#physical-gestures-and-buffered-delivery).
This requires the paired QMK fork changes and an Ark client recognizing Profile
Wire feature bit 17. The current layout and timing defaults are unchanged.

Runtime-owned tapping now also covers authored MT/OSM rows and intrinsic TT/OSL
keys (Profile Wire bit 18). Keys pressed while such a key is undecided wait for
its tap or hold, as QMK's tapping does, so rolls keep their order. Native
unhandled dual-role keys remain native. The
[release timing contract](docs/INTERACTION_MODEL.md#release-intervals-and-timing-advice)
also defines the impossible release interval Ark reports.

The unified gesture ownership plan (in the work-queue vault)
explains why native LT/MT/OSM still exist and the proposed migration to one
classifier, including combo-output behaviours as a required acceptance case.

### Protected main promotions

`main` moves only by the shared vault's `release`, so every `main` is a
released, tested stack. GitHub `main` requires a pull request and two checks,
including for administrators: `Promotion from dev` and `Host suite (GCC)`, the
host suite run on the promotion PR itself. `Promotion from dev` accepts only this
repository's `dev` branch and a merge tree identical to that branch. Force pushes
and deletion are blocked. GitHub PR merging uses merge commits; squash and rebase
merging are disabled so the promoted development history stays reachable.

`release VERSION` checks and tests without publishing: its preflight requires
the BK pin in `qmk-pin.json` to be on the BK fork's development branch and not
behind its released line, and its stack test runs exactly what `main` will hold
(this repository's `dev` at its BK pin). `release VERSION --push` then promotes
the BK fork's released line (fast-forwarded to the pin), this repository and the
client in that order: it publishes `dev`, opens or resumes the promotion PR,
waits until GitHub reports every required check passed, merges, and reconciles
local `main` to GitHub's merge identity. The merge message carries the `dev`
tip's stack and verification trailers. Retries resume the frozen preparation.
Direct `main` pushes are rejected by the local hook as well. Normal task
development still lands locally onto `dev`.

`dev` cannot be force-pushed or deleted on GitHub, and published `v*` release
tags cannot be moved or deleted (rulesets without bypass). Merge commits take
the PR's title and body, so a merge from the GitHub page carries the same
verification trailers as one made by `release`.
