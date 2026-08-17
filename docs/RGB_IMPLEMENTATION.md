# RGB Implementation

What the code actually does, as distinct from what it is meant to do.
[rgbflow.md](./rgbflow.md) is the intended language; this is the machinery behind
it, including the places the two do not line up. For authoring colors, locality
and LED groups see [RGB_CONFIG.md](./RGB_CONFIG.md).

## Where the key-behavior stage sits

Stages composite in a fixed order in
[`rgb_runtime.c`](../users/noah/lib/rgb/core/rgb_runtime.c). Each may overpaint
the ones before it. Key-behavior feedback runs **last**, so it wins over every
other stage wherever it paints.

```mermaid
flowchart LR
    B["base<br>layer colours<br>or automouse fade"] --> CU["combo<br>underlay"]
    CU --> PV["layer<br>preview"]
    PV --> PD["pointer<br>mode"]
    PD --> CO["combo<br>overlay"]
    CO --> KF["key behavior<br>feedback"]

    style B fill:#EDF0F4,stroke:#5A6673,color:#1A1F26
    style CU fill:#EDF0F4,stroke:#5A6673,color:#1A1F26
    style PV fill:#EDF0F4,stroke:#5A6673,color:#1A1F26
    style PD fill:#EDF0F4,stroke:#5A6673,color:#1A1F26
    style CO fill:#EDF0F4,stroke:#5A6673,color:#1A1F26
    style KF fill:#B400FF,stroke:#6E0099,color:#FFFFFF
```

`locality` decides how much it paints. The authored profile uses `RGB_KEY_HALF`,
so one key's feedback state repaints that entire half, over the layer colors and
pointer-mode overlay underneath.

## How truth reaches the LEDs

Per-key truth is computed only on the master. The slave renders from synced
copies and never evaluates the feedback rules itself, which is why the two halves
cannot disagree about the rules, only about freshness.

```mermaid
flowchart TD
    ENG["key runtime state<br>press tokens, tap series, pulses"]
    ENG --> SEM["key_feedback_semantic_map<br>3 bits per key"]
    ENG --> BR["key_feedback_tap_branch_map<br>which count won"]
    ENG --> OWN["key_feedback_broad_owner_map<br>newest owner per surface"]
    SEM --> FV["flash_visibility_bitmap<br>derived from the semantic map"]

    SEM --> RENDER["rgb_key_feedback_stage_render"]
    BR --> RENDER
    OWN --> RENDER
    FV --> RENDER

    SEM --> P1["semantic packet"]
    FV --> P1
    BR --> P2["branch packet"]
    OWN --> P2
    P1 --> SLAVE["slave render<br>same stage, synced inputs"]
    P2 --> SLAVE

    style ENG fill:#EDF0F4,stroke:#5A6673,color:#1A1F26
    style SEM fill:#DDE3EA,stroke:#5A6673,color:#1A1F26
    style BR fill:#DDE3EA,stroke:#5A6673,color:#1A1F26
    style OWN fill:#DDE3EA,stroke:#5A6673,color:#1A1F26
    style FV fill:#DDE3EA,stroke:#5A6673,color:#1A1F26
    style RENDER fill:#B400FF,stroke:#6E0099,color:#FFFFFF
    style P1 fill:#FFE8D6,stroke:#A34500,color:#5C2800
    style P2 fill:#FFE8D6,stroke:#A34500,color:#5C2800
    style SLAVE fill:#D6EAFF,stroke:#00539E,color:#00305C
```

A single `mark_key_feedback_dirty()` marks both packets, so the owner map cannot
lag the semantic map. It fires when a transition plan is non-empty, or when a
plan was empty but `next_feedback_sequence` moved, checked on the press, release
and scan entry points. `should_build_packet` also rebuilds unconditionally while
the last sent packet was active, so an active-to-inactive edge is never missed.

## The tap phase runs on one clock

Both tap-phase colors derive from `last_counted_tap_at + tap_term_ms`. That field
is stamped only where a tap is counted into the series, and deliberately not in
`note_hold_release`, which restamps `last_tap_at` when a held tier releases.

```mermaid
flowchart TD
    OFF["settle offset =<br>now minus last_counted_tap_at plus tap_term_ms"]
    OFF -->|"offset below zero"| W["count still open<br>white, if tap_count above one"]
    OFF -->|"offset zero to branch_confirm_term_ms"| C["count settled<br>colour for that count"]
    OFF -->|"offset beyond that"| Q["tap phase over<br>nothing from this clock"]

    style OFF fill:#EDF0F4,stroke:#5A6673,color:#1A1F26
    style W fill:#FFFFFF,stroke:#5A6673,color:#1A1F26
    style C fill:#B400FF,stroke:#6E0099,color:#FFFFFF
    style Q fill:#DDE3EA,stroke:#5A6673,color:#1A1F26
```

White additionally requires `!branch_confirmed && !branch_confirming`. The count
color additionally fires whenever an action-delay window is open, which is the
overload described at the end of this document.

## The action phase runs on five paths

`branch_confirming` is set from five call sites, each anchoring its window on a
different clock. This is what the display used to inherit, and what the count
clock above now replaces for display purposes.

```mermaid
flowchart TD
    S1["scan flush<br>scan_planner 479"] -->|"anchored at<br>last_tap_at plus tap_term_ms"| BC["branch_confirming<br>action held back for<br>branch_confirm_term_ms"]
    S2["hold threshold<br>scan_planner 490"] -->|"anchored at<br>pressed_at plus tap_hold_term"| BC
    S3["long hold threshold<br>scan_planner 497"] -->|"anchored at<br>pressed_at plus longer_hold_term"| BC
    S4["release hold pending<br>scan_planner 504"] -->|"anchored at<br>pressed_at plus tap_hold_term"| BC
    S5["release path<br>release_planner 502"] -->|"anchored at<br>the release instant"| BC
    BC --> FIRE["window ends<br>action dispatches"]

    style S1 fill:#EDF0F4,stroke:#5A6673,color:#1A1F26
    style S2 fill:#EDF0F4,stroke:#5A6673,color:#1A1F26
    style S3 fill:#EDF0F4,stroke:#5A6673,color:#1A1F26
    style S4 fill:#EDF0F4,stroke:#5A6673,color:#1A1F26
    style S5 fill:#EDF0F4,stroke:#5A6673,color:#1A1F26
    style BC fill:#B400FF,stroke:#6E0099,color:#FFFFFF
    style FIRE fill:#00FF00,stroke:#009E00,color:#1A1F26
```

Which site runs depends on whether the selected branch authors a hold tier and on
whether the key was released. There is no predicate anywhere that answers "has
the count settled" for the action side; that concept exists only in the display
clock above.

## Tier semantics are decided in order

`key_feedback_semantic_for_token` walks these checks top to bottom and returns on
the first match. This is the whole of the hold and long-hold vocabulary.

```mermaid
flowchart TD
    A{"token active,<br>handled, not implicit<br>or fallback hold?"}
    A -->|no| N1["nothing"]
    A -->|yes| B{"an action is<br>registered right now?"}
    B -->|yes| B2{"does that action kind<br>keep registered feedback?"}
    B2 -->|"no, layer or pointer mode"| N2["nothing<br>that overlay owns it"]
    B2 -->|yes| FL["flashing<br>long hold colour if past<br>longer_hold_term, else hold"]
    B -->|no| C{"repeating<br>while held?"}
    C -->|yes| FL
    C -->|no| D{"past longer_hold_term<br>and long hold tier keeps<br>pending feedback?"}
    D -->|yes| ST1["steady<br>long hold colour"]
    D -->|no| E{"release hold pending,<br>and no layer preview<br>on the hold tier?"}
    E -->|yes| ST2["steady<br>hold colour"]
    E -->|no| F{"past tap_hold_term,<br>tier fires at threshold<br>or keeps pending feedback?"}
    F -->|yes| ST2
    F -->|no| N3["nothing"]

    style A fill:#EDF0F4,stroke:#5A6673,color:#1A1F26
    style B fill:#EDF0F4,stroke:#5A6673,color:#1A1F26
    style B2 fill:#EDF0F4,stroke:#5A6673,color:#1A1F26
    style C fill:#EDF0F4,stroke:#5A6673,color:#1A1F26
    style D fill:#EDF0F4,stroke:#5A6673,color:#1A1F26
    style E fill:#EDF0F4,stroke:#5A6673,color:#1A1F26
    style F fill:#EDF0F4,stroke:#5A6673,color:#1A1F26
    style FL fill:#FF6C00,stroke:#A34500,color:#1A1F26
    style ST1 fill:#0084FF,stroke:#00539E,color:#FFFFFF
    style ST2 fill:#FF6C00,stroke:#A34500,color:#1A1F26
    style N1 fill:#DDE3EA,stroke:#5A6673,color:#1A1F26
    style N2 fill:#DDE3EA,stroke:#5A6673,color:#1A1F26
    style N3 fill:#DDE3EA,stroke:#5A6673,color:#1A1F26
```

`keeps_registered_feedback` comes from the action kind table. It is true for
`LITERAL`, `MACRO`, `QMK_BEHAVIOR` and `KEYMAP_CUSTOM`, and false for every layer
and pointer-mode kind. That single flag is why a held modifier flashes and a held
momentary layer does not.

Pulses are a separate one-shot channel lasting one
`FEEDBACK_FLASH_HALF_PERIOD_MS`, with a queue slot so a second pulse arriving
during the first re-arms rather than being dropped. `TAP_COMMITTED` renders green,
`HOLD` renders as steady hold colour, `LONG_HOLD` as steady long-hold colour.

## Precedence

When one key has more than one live semantic, the highest wins.

| Priority | Semantic |
| --- | --- |
| 70 | tap branch committed |
| 60 | tap committed |
| 50 | long hold active, steady or flashing |
| 40 | hold active or hold pending |
| 10 | unresolved tap branch |

## Deliberate silences

- the base single tap through its whole window
- any tier a row does not author
- layer and pointer-mode actions, whose own overlays are the persistent feedback
- any hold tier carrying a layer preview, which routes to the preview stage
- keys inside a currently pressed combo, cleared from the semantic map
- implicit-hold and fallback-hold tokens

## Where this does not match rgbflow.md

1. **The count colour is overloaded.** It fires both when the count clock settles
   and whenever an action-delay window is open. The second meaning is load
   bearing: for a threshold tier it is the only thing shown while the action is
   held back, so removing it leaves the board dark right before a hold or
   long-hold lands. But one colour now answers two questions.

2. **Display and action can describe different moments.** The display moved to the
   count clock; action resolution still runs on the five paths above. On a
   hold-authoring branch released early, the count colour appears when the count
   settles while the action resolved at the release and fires on its own schedule.

3. **The palette has homonyms.** `tap_pending` is the same `#FFFFFF` as
   `LAYER_POINTER`; the three-tap colour is the same `#3C00FF` as `LAYER_NAV`;
   `tap_committed` is the same `#00FF00` as `LAYER_NUM`; the four-tap `#00A2FF`
   is a near miss for `long_hold_active` `#0084FF`. Every collision is reachable
   on keys that use both meanings.

4. **Scale is out of proportion to scope.** `RGB_KEY_HALF` repaints half the board
   for a single key's state, and this stage renders last, so it overrides the
   layer and pointer-mode colours underneath while it is active.
