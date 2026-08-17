# RGB Feedback Flow

How the key-behavior feedback stage walks a tap gesture, and which color it
shows at each position. The color always answers one question: what would
letting go right now send?

Timing names are the authored fields on a `key_behaviors[]` row. All four are
row-level, shared by every `tap_counts[]` entry on that key. Defaults live in
the keymap [`config.h`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h).

## Part one: reaching a tier

Which tap count wins, and then which tier of that count the key is sitting in.

```mermaid
flowchart TD
    ONE["one tap<br>dark"]
    MANY["two or more taps<br>white"]
    PLAIN["single tap confirmed<br>dark"]
    CONF["tap count confirmed<br>colour for that count<br>held for rgb_branch_confirm_term"]
    STILL{"still holding<br>the key?"}
    WAIT["holding<br>not past tap_hold_term yet<br>dark"]
    HOLD["hold tier reached<br>see part two"]
    LONG["long hold tier reached<br>see part two"]
    TSENT["tap tier<br>see part two"]

    ONE -->|"tap again within multi_tap_term"| MANY
    MANY -->|"tap again within multi_tap_term"| MANY

    ONE -->|"multi_tap_term runs out"| PLAIN
    MANY -->|"multi_tap_term runs out"| CONF

    PLAIN --> STILL
    CONF --> STILL

    STILL -->|"no, already let go"| TSENT
    STILL -->|"yes"| WAIT

    WAIT -->|"let go"| TSENT
    WAIT ==>|"keep holding past tap_hold_term"| HOLD

    HOLD ==>|"keep holding past longer_hold_term"| LONG

    style ONE fill:#DDE3EA,stroke:#5A6673,color:#1A1F26
    style MANY fill:#FFFFFF,stroke:#5A6673,color:#1A1F26
    style PLAIN fill:#DDE3EA,stroke:#5A6673,color:#1A1F26
    style CONF fill:#B400FF,stroke:#6E0099,color:#FFFFFF
    style STILL fill:#EDF0F4,stroke:#5A6673,color:#1A1F26
    style WAIT fill:#DDE3EA,stroke:#5A6673,color:#1A1F26
    style HOLD fill:#FF6C00,stroke:#A34500,color:#1A1F26
    style LONG fill:#0084FF,stroke:#00539E,color:#FFFFFF
    style TSENT fill:#00FF00,stroke:#009E00,color:#1A1F26
```

Thick arrows are the board advancing on its own when a term runs out. Thin
arrows are you releasing the key.

The confirmed-count node is filled violet, which is the two-tap color. Three
taps is blue, four is azure, and deeper counts clamp to the last color in
`RGB_TAP_BRANCH_COLORS(...)`. A single tap confirms without any color, because
`branch_confirm_mode` and `tap_commit_mode` are both restricted to non-base
taps.

The hold spine only exists when the key is still down as the count confirms.
Tap twice and let go and the gesture ends in the tap tier; there is no hold
tier to reach. A deeper authored branch being available does not change that.
It keeps the question open while the window is open, and never forces a deeper
answer.

## Part two: what the tier actually does

Reaching a tier is not the whole story. What fires, and what the light does
while you keep holding, is decided by which helper authored that tier on that
tap count. This subtree plugs into every tier node above.

The colors below show the `.hold` tier in orange. The `.long_hold` tier behaves
identically in the long-hold blue. The tap tier has only one helper.

```mermaid
flowchart TD
    T{"which helper authored<br>this tier?"}

    T -->|"TAP_SENDS<br>tap tier only"| S1["action sent on release<br>green pulse"]

    T -->|"TAP_AT_HOLD_THRESHOLD"| A1["action sent once, immediately<br>pulse, then steady tier colour<br>for as long as you hold"]
    A1 -->|"let go"| A2["nothing further sent"]

    T -->|"PRESS_AND_HOLD_UNTIL_RELEASE<br>key, modifier, macro, QMK behaviour"| B1["action registered and stays down<br>flashing tier colour"]
    B1 -->|"let go"| B2["action released<br>dark"]

    T -->|"REPEAT_WHILE_HELD"| D1["action sent now, then again<br>at repeat_hz while you hold<br>flashing tier colour"]
    D1 -->|"let go"| D2["repeating stops<br>dark"]

    T -->|"TAP_ON_RELEASE_AFTER_HOLD"| E1["nothing sent yet<br>steady tier colour<br>while you hold"]
    E1 -->|"let go"| E2["action sent now"]

    T -->|"PRESS_AND_HOLD_UNTIL_RELEASE<br>momentary layer or pointer mode"| C1["that layer or mode takes over<br>its own overlay colour<br>this stage stays dark"]
    C1 -->|"let go"| C2["layer or mode ends"]

    style T fill:#EDF0F4,stroke:#5A6673,color:#1A1F26
    style S1 fill:#00FF00,stroke:#009E00,color:#1A1F26
    style A1 fill:#FF6C00,stroke:#A34500,color:#1A1F26
    style B1 fill:#FF6C00,stroke:#A34500,color:#1A1F26
    style D1 fill:#FF6C00,stroke:#A34500,color:#1A1F26
    style E1 fill:#FF6C00,stroke:#A34500,color:#1A1F26
    style C1 fill:#DDE3EA,stroke:#5A6673,color:#1A1F26
    style A2 fill:#FFE8D6,stroke:#A34500,color:#5C2800
    style B2 fill:#FFE8D6,stroke:#A34500,color:#5C2800
    style D2 fill:#FFE8D6,stroke:#A34500,color:#5C2800
    style E2 fill:#FFE8D6,stroke:#A34500,color:#5C2800
    style C2 fill:#DDE3EA,stroke:#5A6673,color:#1A1F26
```

Three light shapes carry three different meanings, and the distinction is load
bearing:

- **flashing** means an action is registered right now and will be released
  when you let go
- **steady** means a threshold has been crossed and something is still pending
- **a single pulse** means an action just fired and nothing is being held

Flash and pulse both use `RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS`.

`TAP_SENDS(KC_TRNS)` keeps the lower active layer's tap output at that key while
the authored row still owns the timing and branching. The same `KC_TRNS` form
works on the hold-tier helpers and defers to the lower layer's own behavior for
that tier.

## What stays dark, on purpose

Dark is a word in this language, not an absence. These conditions choose it:

- a single tap for the whole of its multi-tap window, because one tap does not
  show intent to enter a tap branch
- any tier a row does not author, so a `.long_hold`-only key shows nothing at
  the hold threshold
- layer actions and pointer-mode actions, whose own overlays are the persistent
  feedback
- any hold tier carrying a layer preview, which routes to the preview overlay
  instead of this stage
- keys inside a currently pressed combo, where the combo footprint speaks
  instead
- the base tap of a multi-tap key on commit, since both branch-confirm and
  tap-commit feedback are limited to non-base taps

## When two states coincide

Higher wins: tap-count confirmed, then tap sent, then long-hold, then hold,
then the uncommitted white. White ranks lowest by design, so the moment
anything is actually known the answer outranks the question.

For the color table, locality, LED groups and render order, see
[RGB_CONFIG.md](./RGB_CONFIG.md). For the engine contract behind these
transitions, see [INTERACTION_MODEL.md](./INTERACTION_MODEL.md).
