# Gesture timing across combo and tap/hold arbitration

Status: local implementation and automated acceptance; physical acceptance remains open.
Nothing has been pushed, flashed or applied by this work.

## Goal and scope

Preserve the current authored layout and its intended gestures. Middle click,
double-hold resize, Pinch + middle-click Click Spam, the nested mouse chords,
layer access and the thumb sequences must coexist predictably. Removing combos,
moving keys or raising every timing value is not the acceptance criterion.
The connected profile remains the runtime authority; authored `keymap.c` is the
baseline for intended layout, not permission to overwrite a user's live edits.

Investigate physical input → QMK combo arbitration → QMK tapping → userspace
behavior → owned output, including release and feedback. The selected clocks and ownership rules now live in
[INTERACTION_MODEL](../INTERACTION_MODEL.md#physical-gestures-and-buffered-delivery)
and firmware decision D-F01. Ark owns its warning decision D-L48 in its repository.

## Report and evidence

The reported gesture was `MS_BTN3` press–release–press-and-hold. Only the double
hold was authored, sending `MS_BTN7` until release; tap/hold and repeated-tap
terms were both 100 ms. The observed output was Button 3 instead of Button 7.
Changing **both** terms to 300 ms in Ark made it work, as confirmed by the user.
That establishes a timing-sensitive failure, not which of the two changes was
necessary. The compiled row still uses 100/100; the user's live workaround is
not a change to the intended layout.

Investigation established:

1. Read-only Raw HID reads of the attached keyboard found the exact sparse
   behavior row: target Button 3, tap/hold 100, default long hold, repeated taps
   100, double-hold mode 1 targeting Button 7. Status reported that committed
   profile active, peer known and converged, with no pending activation. An
   unapplied draft or wrong saved action does not explain that readback.
2. Native combo readback showed combos enabled and `PD_SLOT_5 + MS_BTN3 →
   CUSTOM_KEY_2` with a 50 ms window. The source identifies the output as Click
   Spam. Thus Button 3 is also a buffered combo input.
3. The direct runtime integration reproduction selects Button 7 for first
   press/gap pairs 40/40 and 99/99 ms. It selects Button 3 for 101/40 and
   40/101 ms. These are runtime-delivery intervals. The added test is
   `test_ordinary_mouse_button_double_tap_hold` in
   [pd_mode_key_runtime_integration_test.c](../../tests/host/pd_mode_key_runtime_integration_test.c).
   It does **not** run QMK's actual combo or tapping engine.
4. QMK `quantum/process_keycode/process_combo.c` queues a combo member press,
   releases queued records on disambiguation/release or after its timer, and
   sends them through `action_tapping_process`. The runtime's
   `key_runtime_core_observe_process_record_event` in
   [runtime.c](../../users/noah/lib/key/runtime/reducer/runtime.c) uses
   `timer_read()` at delivery, not the record's physical event timestamp.
5. Existing protection for delayed **combo outputs** is different: the runtime
   consults `noah_qmk_combo_origin_pressed_combo_matches` to preserve a matching
   output's tap series. It does not generally preserve a standalone combo
   member's second physical press while that member is queued.

The original source exposed the following failure mechanism, now reproduced
using the real QMK combo and tapping implementations: a first quick tap
can flush on release, while a held second press waits for combo disambiguation.
A 70 ms physical gap followed by roughly 50 ms of buffering can reach the
runtime after its 100 ms series deadline. It is then a new first press, whose
fallback hold is Button 3. Waiting ends on a scan, so 50 ms is not an exact
hardware latency or a universal bound; overlapping candidates and other input
can change it.

**Evidence limit:** no physical-event/USB-output trace was captured for the
reported attempts. The 70 + 50 example is explanatory, not a measurement. The
earlier conversational claim that the combo delay conclusively explained the
user's particular attempts was too strong. A slow first press, the physical gap,
and combo delivery delay cannot be separated retrospectively. The automated
pipeline reproduction proves the buffering defect, not the exact timing of
those physical attempts.
Do not label all risks below confirmed hardware failures.

Source inspected: firmware `5d8ba16b160d7ca541e6ebe83fcba490f716f011`, with the
new uncommitted boundary test; QMK `c69104c423180613ba276faf5514b30d54f992d2`.
QMK had pre-existing untracked/submodule dirt, preserved during implementation.
The local fix now changes its combo/tapping queue inspection and tap-key hook.
These identify the inspected code, not proof of the attached image's revision.
No trace capture or build provenance was recovered from the device.

## Audit of the intended profile

Read [keymap.c](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c),
[config.h](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h), and
[KEYMAP.md](../KEYMAP.md) together. Defaults are behavior hold/repeat 150 ms,
long hold 400 ms, QMK tapping 200 ms, and combo window 50 ms; the two four-key
Cmd+N chords use 100 ms. Live settings can differ. Windows in this table are
candidate windows, not measured end-to-end delays.

| Surface / intended behavior | Interaction needing acceptance | Evidence status |
| --- | --- | --- |
| Pointer Button 3: middle click; double-hold Button 7 resize | 100/100 behavior and 50 ms Click Spam combo membership; first-press qualification and second-press expiry | Reported failure; direct boundary reproduction; buffering mechanism identified |
| `PD_SLOT_5`: transparent tap, Pinch hold, second tap macro 6, second hold Zoom (`PD_SLOT_3`) | Same Click Spam combo; deferred first Pinch hold must not start while chord is undecided or leak into Zoom | Source-exposed timing risk; not a new hardware finding |
| `PD_SLOT_1`: transparent tap, Volume hold, double-tap Mute | Button 1 + Volume emits Cmd; physical mode key's repeated taps and combo-output Cmd series have different buffering paths | Source-exposed risk; preserve existing combo-output protection |
| `PD_SLOT_0`: transparent tap, scroll hold, double-hold scroll lock | Member of 50 ms three-key Cmd+T and 100 ms four-key Cmd+N mouse chords | Source-exposed repeat/hold latency risk; exercise lock/unlock and consumed unlock press |
| `G(KC_C)` / `G(KC_V)`: normal shortcut, double-tap macros 10 / 7 | Together emit Cmd+A at 50 ms; separately their second presses can wait for that chord | Same family of repeat-window risk as Button 3 |
| `KC_COMM`, `KC_DOT`: normal punctuation / held `<` and `>` | Members of 50 ms Cmd+T and 100 ms Cmd+N alpha chords; a 150 ms hold currently starts at delivery | Source-exposed shifted hold timing; no reported failure yet |
| `LT(NAV, /)`: slash, Nav hold, double-hold Nav lock | Member of both 100 ms four-key chords; QMK tapping 200 ms and authored hold 100 ms add a second arbiter | Existing open `LT()` timing issue plus combo interaction; do not claim thresholds simply add in every path |
| `D + LT(NAV, F) → Tab` | Combo wait and QMK layer-tap decision; ordinary `D`, `F`, Nav access, rollover and interruptions must survive | Acceptance gap; `LT(SYM,J/Z)` are controls for tapping without these combos |
| Mouse chord ladder: Button 1+2 → Button 6; +Scroll → Cmd+T; +Slash/Nav → Cmd+N | Overlapping two/three/four-member candidates; prevent a premature Button 6 drag or smaller chord when the larger wins; release order matters | Source-exposed arbitration requirement, not proof smaller output currently leaks |
| Alpha ladder: N+M → Cmd; M+comma+period → Cmd+T; +Slash/Nav → Cmd+N | Shared members and different windows, longest pending wait, rolling input, modifier acquisition and suppression | Acceptance gap; audit every order rather than just simultaneous presses |
| Physical Cmd and Cmd from N+M / Button 1+Volume | Single Cmd, double-hold Alt, triple-tap one-shot Shift; compare repeated chords with physical key; interrupted and partially released chords | Existing origin/series coverage is a guardrail, not complete physical-path proof |
| Click Spam custom output | 1 ms hold threshold is measured after output delivery; at 100 Hz release must stop repeats without starting constituent Pinch or Button 3 | Existing PD interception tests cover another path; actual chord pipeline still required |
| Drag Window (`CUSTOM_KEY_3`) | Transparent tap and 100 ms Button 6 hold, auto-mouse anchoring; no direct combo membership | Control case; `multi_tap_term=100` does not create a double branch where none exists |
| Thumbs, Esc, Right Alt, Shift, Enter, number/symbol holds, navigation arrows | Multi-depth cycles, fallback modifiers, threshold vs release actions, layer changes and independent pending series | Regression controls; no direct combo membership does not mean immune to queued foreign input |

Pointing-mode precedence is another axis: Arrow can consume Button 1/2/3 for
Shift/copy/paste before their authored behavior. That is intentional routing,
not a timing failure. Test unlocked, held and locked modes, mid-gesture mode
changes, and release ownership. Also keep Button 6/7 host bindings separate:
firmware must prove the USB button down/up; Rectangle Pro's reaction is a
separate host integration check.

The stale Click Spam and Pointer-thumb walkthroughs in KEYMAP have been
corrected to match the authored layout; no layout data was changed.

## Selected implementation and remaining acceptance

The firmware fix preserves physical record timestamps, retains on-time queued
continuations, prevents scan-time holds after a queued release, and gives authored
`LT()` rows one userspace tap/hold decision. QMK owns its existing queues and
combo arbitration; no shadow queue or blanket timing increase was added.
Capability bit 17 lets Ark distinguish the corrected policy. The Ark branch
adds profile-derived warnings in review and the behaviour editor for older
firmware; it preserves legal Apply and the user's tuning.

The real-QMK pipeline regression covers Button 3 repeat-gap boundaries through
100 ms (and rejection at 101 ms), timer wrap, a winning Click Spam chord
consuming the second press, physical hold deadlines, authored Slash/Nav double
hold, another native layer-tap buffering input, and queued short releases. Both
four-key chord families are exercised in all 24 press orders. These are host
events and report sinks, not USB captures. Combo-output origins, modes and
ownership also retain their separate existing host tests.

Still required for physical acceptance:

- Use a backed-up live profile and known paired build, preserving the user's
  current 300 ms workaround until a deliberate trial. Compare 100/100, 100/300,
  300/100 and 300/300 with varied first-press duration and released gap.
- Verify both physical halves, ordinary Button 3 and Button 7 down/up, with
  pointing modes off first, then the intended held/locked modes and mid-gesture
  changes. Arrow's button interception is a separate intentional policy.
- Exercise the full table below/above as applicable: nested chord release
  orders, repeated combo outputs, scroll/Pinch/Zoom locks and unlocks, Click
  Spam cancellation, Cmd ownership, rollover, thumb cycles and host bindings.
- Capture physical/USB timing under the measurement procedure before quoting
  latency. Check RGB and split behaviour on the board; host tests cannot prove it.

The original design questions below explain the investigation. D-F01 and the
timing contract resolve clocks, queue ownership, authored LT authority and
capability recognition; the acceptance questions remain useful. Delete this
plan only after physical acceptance, folding any further lasting rules into
the governing specs.

## Design questions considered

- **Clock definitions.** State precisely what starts tap/hold, long hold and
  inter-tap time: physical press/release, chord completion, or runtime delivery.
  Give standalone keys and equivalent combo outputs understandable semantics.
  Recommended direction: physical gesture eligibility survives arbitration;
  output waits until its ownership is decided. This is now the D-F01 contract.
- **Already-expired physical thresholds.** If a released key is replayed after
  its physical hold threshold, should it emit a held action, a tap, or nothing?
  Never register an action after release without a defined balanced lifecycle.
  A completed combo must suppress its constituents even if their timers elapsed.
- **Pending series expiry.** Merely replacing `timer_read()` with
  `record->event.time` cannot revive a series already flushed by scans. Reserve
  an eligible pending press before expiry, then either consume it as a combo or
  deliver it as a member. Define cancellation, bounded storage and generation
  identity; no indefinitely retained series after a suppressed chord.
- **QMK tapping authority.** Decide who classifies authored `LT()` rows and how
  quick-tap, interrupted tapping and native tap counts interact with userspace.
  Avoid two independent hold decisions for the same physical gesture. Do not
  globally bypass QMK tapping to fix a mouse button.
- **Arbitration policy.** Define nested chord precedence, release order, timer
  restarts, unrelated candidates and maximum waiting. Correct timing cannot
  remove the inherent wait required to distinguish overlapping gestures.
- **Identity and ordering.** Same code at two positions, physical key vs combo
  output, overlapping combo footprints, repeated chords, transparent layer
  resolution, keycode-changing releases, suppressed candidates, timer wrap,
  split scans and multiple events in one scan all need explicit treatment.
  Do not rewind the global runtime clock to process an old record.
- **Safety of outputs.** Preserve modifier/layer/button ownership, no duplicated
  taps or stuck holds, repeat cancellation, PD interception, and RGB feedback
  reflecting the branch that actually wins. Keep compat facts in `lib/compat/`;
  do not create a second runtime owner inside the origin adapter.
- **Compatibility.** A behavior timing change is user-visible even without a
  wire change. Decide how Ark recognizes affected/fixed firmware; the existing
  firmware version alone may not distinguish revisions. If capabilities or
  readback change, specify them and name Ark's pinned-contract follow-up.
- **Cost.** Keep any new pending-event tracking bounded and scan work limited
  to active entries. Use the memory-budget contract before making resource
  claims; this plan does not justify a generic engine rewrite.

## Ark rationale: explain interactions, do not legitimize a firmware defect

Developer inspection of Ark's `core/model/layer-reach.js`, `profile-checks.js`
and `profile-review.js` found reachability, layer traps and whole-profile
validation, not an end-to-end timing/arbitration analysis. A reachable branch
can therefore still be difficult to execute. This was a product gap; the local Ark implementation and D-L48 now own its
presentation and severity.

Propose an advisory on affected firmware for a behavior target that is also a
combo input: identify the key, affected branch, competing combos and effective
windows. Example: “This key also waits for the Click Spam combo. On this
firmware, that wait can make a second press miss the 100 ms repeat window.”
Link the behavior, combo and global timing controls. Explain the physical
gesture as tap–release–press-and-hold; do not imply two full taps plus a third
press. Report inherited defaults as effective values.

The analysis must use the connected profile/native combo readback, current
settings, enabled flags, reference-layer rules and reachable layer stacks; no
firmware-source reads in the app. Separate a static possibility from observed
failure, and distinguish member buffering from combo-output buffering and PD
button overrides. Disabled/inapplicable combos should not generate blanket
warnings. A legal profile stays applicable; do not turn timing advice into a
save blocker or automatically rewrite values.

A formula such as `repeat term - largest combo window` is only an illustrative
simple-case margin, not a guaranteed usable window: timer restarts, overlap,
release-triggered replay and QMK tapping make it path-dependent. If reliable
analysis needs more firmware facts, design the readback contract first. Warnings
must track firmware semantics and disappear or change when the defect is fixed.
300 ms is a demonstrated workaround, not a universally safe recommendation or
a substitute for coherent clocks. It also delays holds and tap settlement.

## Work sequence and acceptance

1. **Reproduce the whole path.** Add a firmware-owned harness using the selected
   QMK combo and tapping implementations, or a narrowly justified equivalent
   with contract checks. Record physical events, queue delivery, runtime branch
   selection and emitted button reports. Make the 100 ms Button 3 case fail
   for the identified buffering reason before changing runtime behavior.
2. **Separate causes.** Exercise 100/100, 100/300, 300/100 and 300/300; first
   duration, released gap and second duration independently. Cover threshold
   minus one, equality and plus one, intermediate scans and timer wrap. Repeat
   with combos disabled, only Click Spam enabled, and the full current profile.
3. **Resolve the decisions above.** Write event timelines and ownership rules
   in INTERACTION_MODEL / KEY_RUNTIME (or a dedicated architecture spec if
   needed), and record the chosen firmware decision in the direction document.
   Review the current keymap matrix against that proposed model before coding.
4. **Implement the smallest sufficient fix.** Preserve authored placements and
   all ten combos. Extend existing combo-output, release and interception
   regressions alongside member-input cases. No compensating default changes
   that hide failed acceptance cases.
5. **Ark work in Ark.** Add tested, firmware-aware findings to keyboard and draft
   review and the relevant editors. Cover new/existing/fixed findings, live
   defaults, disabled combos, layer references and firmware with unknown timing
   semantics. Refresh pinned contracts and run Ark's explicit compatibility gate
   if firmware-facing contracts change.
6. **Hardware acceptance.** With a known side-specific paired build and a backup,
   repeat the audit matrix on both physical halves and relevant layer/mode
   states. Capture input/output evidence under a documented measurement
   procedure if reporting latency numbers. Verify Button 7 alone during resize,
   balanced release, normal Button 3, and no Click Spam/Pinch leakage. Test the
   original intended timing as well as user-tuned settings. Unit success alone
   is insufficient.

Required firmware checkpoints: affected key-runtime, combo-origin/QMK-contract,
PD-mode, layer/modifier ownership, tracing and split/RGB tests; feature-gate
compilation for wiring/compat changes; real-profile validation if authored data
changes. Finish with `sh tests/host/run_all_host_tests.sh`, `git diff --check`,
and, only after host gates pass,
`qmk compile -kb bastardkb/charybdis/4x6 -km noah`. Build the ordinary paired
images for hardware acceptance; the generic compile is not that pair. Firmware
verification must remain independent of an Ark checkout.

Done means the current intended layout passes the matrix under a documented,
consistent timing model, release ownership remains balanced, and Ark explains
remaining intentional ambiguity without portraying broken timing as good
configuration. Fold lasting rules into their governing specs and delete this
plan when complete (D-L07).
