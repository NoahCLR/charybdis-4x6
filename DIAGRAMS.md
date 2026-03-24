# Firmware Flow Diagrams

Visual overview of how QMK processes input and renders output on this keyboard. For implementation details, see [INTERNALS.md](INTERNALS.md).

## Architecture Overview

QMK runs a main loop that calls user-defined callbacks for key events, trackball motion, and LED rendering. This keymap hooks into those callbacks and ties them together with shared state.

```mermaid
flowchart TD
    subgraph QMK["QMK Main Loop (every scan cycle)"]
        KEY["Key scan"] --> PRU["process_record_user()"]
        TB["Trackball poll (~1kHz)"] --> PDTU["pointing_device_task_user()"]
        LED["RGB frame tick"] --> RGBU["rgb_matrix_indicators_advanced_user()"]
    end

    subgraph STATE["Shared State"]
        FLAGS["pd_mode_flags (uint8_t bitfield)"]
        PRIO["pd_mode_priority[] (resolution order)"]
        ELAPSED["auto-mouse elapsed timer"]
    end

    subgraph SPLIT["Split Transport"]
        SYNC["split_sync.h — 3-byte RPC packet"]
    end

    PRU -- "set/clear mode bits" --> FLAGS
    PRU -- "trigger sync" --> SYNC
    PDTU -- "read mode bits" --> FLAGS
    PDTU -- "check priority" --> PRIO
    RGBU -- "read mode bits" --> FLAGS
    RGBU -- "read elapsed" --> ELAPSED
    RGBU -- "broadcast elapsed + flags" --> SYNC
    SYNC -- "slave receives" --> FLAGS
    SYNC -- "slave receives" --> ELAPSED
```

> **Single translation unit:** All `.h` files are included into `keymap.c`, which is the only `.c` file compiled for this keymap. Static globals in headers are shared across all functions.

## Auto-Mouse Layer Lifecycle

QMK's auto-mouse system activates the pointer layer when the trackball moves and deactivates it after a timeout (1200ms). The keymap hooks into this to render the countdown gradient and sync state to the slave half.

```mermaid
stateDiagram-v2
    [*] --> Inactive

    Inactive --> Active: Trackball movement detected
    Active --> Active: More movement (timer resets to 0)
    Active --> Expired: Timer reaches 1200ms
    Expired --> Inactive: QMK disables pointer layer

    state Active {
        [*] --> DeadTime: LEDs stay white
        DeadTime --> Gradient: 400ms elapsed
        Gradient --> Gradient: Color shifts white → red
    }
```

| Event | What happens |
|-------|-------------|
| Trackball moves | QMK activates pointer layer, resets timer to 0 |
| Timer < 400ms | LEDs white (dead time — no flicker during active use) |
| Timer 400–1200ms | LEDs animate white → red (visual countdown) |
| Timer hits 1200ms | QMK deactivates pointer layer, back to base |
| Drag-scroll toggled on | Timer frozen — pointer layer stays active until toggled off |

## Key Press Processing

QMK calls `process_record_user()` for every key press and release. The function is organized in four stages, evaluated top to bottom. Returning `false` stops further processing.

```mermaid
flowchart TD
    A[Key press/release event] --> B{Stage 1: Mode key?}
    B -- "VOLUME / BRIGHTNESS / ZOOM / ARROW" --> C{Press or release?}
    C -- Press --> C1[Start timer + activate mode]
    C -- Release --> C2{Held < 150ms?}
    C2 -- Yes --> C3[Deactivate mode + send base-layer key]
    C2 -- No --> C4[Deactivate mode]
    C1 --> D[Sync state to slave]
    C3 --> D
    C4 --> D
    D --> STOP([return false])

    B -- "DRAGSCROLL" --> E{Already toggled on?}
    E -- Yes --> F[Unlock dragscroll]
    E -- No --> G[Pass to charybdis firmware]
    F --> STOP
    G --> STOP

    B -- No --> TD{Stage 2: Tap dance key?}
    TD -- "LED 49/45/44/28/53" --> TD1{Count taps}
    TD1 -- "Single tap" --> TD2[Send plain key]
    TD1 -- "Single hold" --> TD3[Send shifted variant / activate layer]
    TD1 -- "Double tap" --> TD4[Send media key]
    TD2 --> STOP
    TD3 --> STOP
    TD4 --> STOP

    TD -- No --> H{Stage 3: Tap/hold key?}
    H -- Yes, pressed --> I[Start hold timer]
    H -- Yes, released --> J{Already fired by matrix_scan?}
    J -- Yes --> STOP
    J -- No --> J2{How long held?}
    J2 -- "< 150ms" --> K[Tap: plain key]
    J2 -- "150-400ms" --> L[Hold: shifted variant]
    J2 -- "> 400ms" --> M[Longer hold: third action]
    K --> STOP
    L --> STOP
    M --> STOP

    H -- No --> N{Stage 4: Release?}
    N -- Yes --> PASS([return true])

    N -- "No (press only)" --> O{Stage 5: Macro?}
    O -- Yes --> P[Send macro shortcut]
    P --> STOP
    O -- No --> PASS
```

## Trackball Motion Pipeline

QMK calls `pointing_device_task_user()` every scan cycle (~1000Hz) with the trackball's motion delta. If a pointing device mode is active, the motion is intercepted and converted to keypresses instead of cursor movement.

```mermaid
flowchart TD
    A["Trackball motion (x, y)"] --> B[pointing_device_task_user]
    B --> C{Walk pd_mode_priority array}

    C -- "Dragscroll active" --> D["Handled by charybdis firmware
    (scroll output)"]
    C -- "Volume active" --> E["handle_volume_mode
    (Y-axis → volume up/down)"]
    C -- "Brightness active" --> F["handle_brightness_mode
    (Y-axis → brightness up/down)"]
    C -- "Zoom active" --> Z["handle_zoom_mode
    (Y-axis → GUI+Plus/Minus)"]
    C -- "Arrow active" --> G["handle_arrow_mode
    (dominant axis → arrow key)"]
    C -- "No mode active" --> H["Pass through unchanged
    (normal cursor movement)"]

    style D fill:#f90,color:#000
    style E fill:#ff0,color:#000
    style F fill:#f0f,color:#000
    style Z fill:#7f0,color:#000
    style G fill:#0ff,color:#000
```

> **Priority:** If multiple modes are held simultaneously, only the highest-priority one runs. The priority order is defined once in `pd_mode_priority[]`: dragscroll > volume > brightness > zoom > arrow.

## RGB Rendering

QMK calls `rgb_matrix_indicators_advanced_user()` in LED chunks. Each layer has a base color; active pointing device modes overlay a color on the right half (where the trackball is).

```mermaid
flowchart TD
    A[RGB frame tick] --> B{Active layer?}

    B -- Base --> C["Default RGB effect
    (no override)"]
    B -- Pointer --> D["Auto-mouse gradient
    white → red over 1.2s"]
    B -- Num --> E["Solid green
    (both halves)"]
    B -- Lower --> F["Solid blue
    (both halves)"]
    B -- Raise --> G["Solid purple
    (both halves)"]

    D --> H{Mode active?}
    E --> H
    F --> H
    G --> H

    H -- Yes --> I["Override right half
    with mode color"]
    H -- No --> J[Done]
    I --> J

    style D fill:#fff,color:#000
    style E fill:#0f0,color:#000
    style F fill:#00f,color:#fff
    style G fill:#808,color:#fff
```

## Split Sync (Master → Slave)

The master half (left) knows the auto-mouse elapsed time and which pointing device modes are active. The slave half (right) needs this to render correct LED colors. A 3-byte RPC packet is sent whenever the state changes.

```mermaid
sequenceDiagram
    participant M as Master (left half)
    participant RPC as QMK Split RPC
    participant S as Slave (right half)

    Note over M: User moves trackball
    M->>M: auto_mouse_get_time_elapsed()
    M->>M: Quantize to 50ms steps
    M->>M: Build packet (elapsed + mode_flags)

    alt Packet changed since last send
        M->>RPC: transaction_rpc_send(PUT_PD_SYNC, 3 bytes)
        RPC->>S: pd_sync_slave_rpc()
        S->>S: Update pd_sync_remote
        S->>S: Update pd_mode_flags
        Note over S: RGB rendering uses<br/>synced values
    else Packet unchanged
        Note over M: Skip — no RPC sent
    end
```

## Auto-Mouse Countdown Gradient

When the trackball triggers the pointer layer, LEDs show a color gradient representing remaining time before the layer deactivates.

```mermaid
flowchart LR
    A["0ms
    Triggered"] -->|Dead time| B["400ms
    Still white"]
    B -->|Gradient starts| C["800ms
    Midpoint"]
    C --> D["1200ms
    Expires"]

    style A fill:#fff,color:#000
    style B fill:#fff,color:#000
    style C fill:#f88,color:#000
    style D fill:#f00,color:#fff
```

| Phase | Time | Color | Purpose |
|-------|------|-------|---------|
| Dead time | 0 – 400ms | White | No flicker during active trackball use |
| Gradient | 400 – 1200ms | White → Red | Visual countdown to layer deactivation |
| Expired | 1200ms | Layer deactivates | Auto-mouse returns to base layer |
