# RGB Feedback Model

Lighting shows runtime state: the active layer or pointing mode, a selected tap
branch, a pending release action, or an action that has committed. The renderer
reads the key runtime's state; it does not make a second tap/hold decision.
For colors and authoring helpers, see [RGB configuration](RGB_CONFIG.md).
For state derivation and rendering details, see [RGB implementation](RGB_IMPLEMENTATION.md).

## The surface underneath feedback

QMK's base effect remains visible where userspace does not paint. Layer colors
and auto-mouse fading establish the userspace base scene. Combo feedback,
layer preview and pointing feedback compose over it; key-behavior feedback is
last. A stage's locality and LED groups decide where it paints.

An active combo normally keeps its configured color. A combo whose output
owns a momentary layer hold yields to that layer's lighting, without a commit
flash. A pending hold keeps combo feedback until it resolves. Other combos
keep their own feedback, including on shared keys.

## A tap branch is a selection

Tap counts are one-based for the user; C's `tap_counts[]` indexes are zero-based.
The base single-tap branch paints no branch color. Double-tap selects the first
entry of `RGB_TAP_BRANCH_COLORS(...)`, triple-tap the second, and so on. A deeper
branch clamps to the final configured color.

A selected non-base branch retains its color while it is pending, until a more
specific action state takes over or the series ends. Selecting a branch is not
a commit. The runtime's authored action and hold semantics decide whether a
tap-committed, hold or long-hold state is visible. Missing tiers do not create
extra thresholds, and inherited normal actions need not produce confirmation
feedback.

## Action feedback

| State | Meaning |
| --- | --- |
| Pending tap branch | A non-base tap count is selected but has not entered its action |
| Tap committed | A resolved tap state or eligible confirmation pulse; uses the configured tap color |
| Hold pending | The hold tier has been selected but its action still depends on release or a later tier |
| Active hold | The selected hold action owns the key's current behavior |
| Active long hold | The longer hold tier owns the current behavior |

Pending and active states use the runtime's semantic and flash maps. Repeating
or retained actions can flash; a committed threshold action can become steady.
A confirmation pulse is a separate, bounded channel. It lasts one configured
feedback half-period, and one queued pulse can re-arm it. The default tap
policy pulses authored non-base taps only; single taps and inherited normal
taps stay quiet. Layer/pointing state feedback can replace a redundant pulse.

Colors are profile data. The authored defaults use white for tap committed,
orange for hold and blue for long hold; see the
[generated color tables](KEYMAP-OVERVIEW.md#key-behavior-feedback-leds) for exact
HSV values. Those defaults do not constrain a saved live profile.

## Timing and locality

Feedback follows the [interaction model](INTERACTION_MODEL.md), including
per-row timing, queued physical events, foreign-key settlement and scan-time
threshold evaluation. There is no universal “151 ms feedback delay”: a series
may settle through a timeout, another key or an action that already commits.
RGB refresh timing is separate from gesture timing.

`RGB_KEYS_ONLY` preserves each key's semantics. Broader localities use the
newest active owner for the affected surface, including that owner's real flash
phase; offset holds must not fill one another's off windows. Combo origins
carry the complete physical footprint so combo-output feedback can paint its
member keys or halves.

## Both halves

The master computes semantic, branch, flash and ownership maps. The other half
renders mirrored maps. Each split domain is read as a coherent publication;
a read during publication keeps the last coherent values. Semantic and branch
maps travel in separate RPC domains, so they are not an atomic cross-domain
snapshot. The renderer suppresses stale tap-branch colors on live combo
footprints.

Freshness depends on transport scheduling and retries. There is no one-frame
maximum lag guarantee: disconnection or retry backoff can retain old state
longer. See [runtime flow](architecture/runtime-flow.md) and the split-sync
implementation for scheduling and recovery.
