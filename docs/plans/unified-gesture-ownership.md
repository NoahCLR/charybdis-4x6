# One gesture owner for the board

Status: proposed migration; not implemented by the gesture-timing fixes.

## Problem and goal

Adding or removing a behaviour row can currently change which engine interprets
an otherwise similar key. A person editing the keyboard should not need to know
that distinction or encounter an unexplained change in typing feel.

The proposed direction is one userspace gesture engine with explicit built-in
defaults for supported key types. Ark should show the effective behaviour; an
edit should modify that behaviour without switching classifiers. The connected
profile remains authoritative. Preserve the current layout and live tuning.

This is a proposal, not authorization to replace every QMK convention with an
assumed equivalent. Firmware owns runtime semantics; Ark owns the effective
behaviour presentation and any corresponding app decisions.

## Why the current native path exists

D-F01 preserves physical timing across buffered input. D-F02 routes every
runtime-handled key past native QMK tapping. That includes authored LT/MT/OSM
rows and intrinsic TT/OSL ownership. Unhandled LT/MT/OSM still use native tapping.

That remaining path preserves behaviour the custom runtime does not yet fully
replace:

- LT: tap the encoded key, hold the encoded layer, with interruption and
  overlapping-key rules. For a plain LT, QMK classifies it and userspace already
  owns the resulting momentary layer.
- MT: tap the encoded key versus hold its encoded modifiers, including rollover,
  interrupted input and rapid repetition.
- OSM: one-shot modifier activation, consumption, cancellation and locking.

QMK tapping is more than a threshold. Simply exempting all keycodes would not
implement these defaults. An authored MT row today does not automatically gain
an equivalent native modifier-hold fallback merely because it bypasses tapping.
Keeping the native path is a compatibility boundary, not the proposed final
product model.

## Decide the human interaction contract first

1. Define tap, hold, long hold and repeated taps by physical input and state the
   inclusive/exclusive boundaries. Keep the D-F01 physical-key clocks and
   distinguish eligibility from permission to emit while a combo is undecided.
2. Choose overlapping-key and interruption rules: roll versus chord, another
   key pressed/released before the decision, rapid tap then hold, and whether
   a modifier/layer must be active before a following key is emitted. Inventory
   the selected fork's enabled policies; do not promise every optional QMK mode.
3. Define default LT/MT behaviour and sparse-row inheritance explicitly. Adding
   an unrelated second-tap action must not silently remove a useful first-hold
   default. Distinguish unspecified, deliberately empty and transparent actions.
4. Define OSM/OSL consumption, timeout, cancellation, repeated-tap locking and
   coexistence with physically held modifiers/layers. Separate gesture
   classification from the downstream one-shot action lifecycle.
5. Decide how legacy profiles preserve their behaviour or migrate. A deliberate
   policy change must be visible in Ark; avoid reinterpreting existing rows
   silently. Use an advertised capability/version where semantics differ.

## Combo output is a first-class input

Keep the existing two-stage contract: QMK decides the chord, then its output
keycode enters the same authored behaviour lookup as a physical key. A GUI
combo output is not a request to bypass the GUI behaviour and emit raw Cmd.

The current profile is a required acceptance case:

- Button 1 + Volume-hold slot, within its 50 ms combo window, emits Left GUI.
- First chord tap/hold retains the row's ordinary Cmd fallback.
- Tap the whole chord, release, then form it again and hold: second hold is Alt.
- Three quick chord taps: third output release sends one-shot Shift once.
- Constituent Button 1/Volume must not leak when the chord wins. All outputs and
  ownership must balance on release, interruption and mode changes.

A combo's synthetic output currently starts its hold clock on delivery; pending
combo-origin tracking protects repeat continuation when the chord completes on
time before its output is delivered. Physical keys use physical timestamps.
That protection is load-bearing for C5: Button 1 also belongs to the 100 ms
`G(KC_N)` chord, so QMK holds the GUI output up to 100 ms after completion. A
70 ms release-to-rechord gap then arrives ~170 ms apart, past the 150 ms repeat
term, and only the recorded completion time keeps it a double tap.
Decide whether this distinction remains the desired contract before unifying it.
Do not arbitrarily assign a combo the timestamp of one member.

Preserve stable combo ownership when press order changes, duplicate outputs from
different chords, exact footprints, partial releases, repeated chords, and
cross-half chords. Native QMK combo release policy also matters: an active chord
normally ends when its last constituent releases. Holding one member while
retapping the other is not automatically a new whole-chord tap.

## Implementation sequence

1. Inventory actual default dispatch, native tapping options, owned layer/modifier
   contracts, synthetic action handling, and current authored/live usage.
2. Record chosen rules in INTERACTION_MODEL and the governing runtime specs;
   specify compatibility and effective-behaviour readback needed by Ark.
3. Add failing physical-input regressions for the chosen defaults and editing
   invariants. Include normal typing controls before replacing any classifier.
4. Introduce defaults within existing userspace resolution and ownership modules,
   in small steps. Keep QMK scanning, transport, reports and useful low-level
   action execution. One gesture classifier does not require replacing QMK.
5. Migrate native classification only when its replacement and action lifecycle
   are covered. Add capability/version support and Ark integration deliberately.
6. In Ark's repository, design effective default display and edits with explicit
   inheritance. Extend timing/reachability analysis where the new contract gives
   sufficient facts; label uncertainty rather than claiming complete feasibility.
7. Test on both halves with a backed-up live profile and known paired build.
   Do not equate host assertions with physical feel or USB/host acceptance.

## Acceptance and remaining work

- Adding/removing an equivalent behaviour row preserves the documented first
  tap/hold defaults, timing and rollover; any intentional difference is explicit.
- LT, MT, OSM, TT, OSL, modifiers, ordinary keys, mouse buttons, custom keys and
  pointing-slot gestures each have a documented owner and tested defaults.
- Every hold/repeat/one-shot balances through release, profile changes, suppressed
  combos and interruption; no stuck modifiers, layers or buttons.
- The GUI combo example above and both nested chord families retain their
  authored behaviour, timing, origin and release semantics.
- Native or migrated modifier taps are exercised with real overlapping typing;
  one-shots with next-key consumption, physical modifiers and locked states.
- Ark reports effective values and applicable timing findings from device data,
  without repository access or an unexplained switch of timing engine.
- Required subsystem gates, full host suite, firmware compile, paired builds and
  Ark's explicit compatibility bridge pass before physical acceptance.

No migration in this plan is complete. Fold agreed lasting rules into their
specs and delete the plan after implementation and acceptance (D-L07).
