# Pointing modes

The trackball moves the pointer until a **pointing mode** takes it over. A
mode either scrolls or sends keys and shortcuts as the ball moves, and runs
while its key is held or locked. Ark's
[Pointing](https://github.com/NoahCLR/charybdis-ark/blob/dev/docs/guide/pointing.md)
page edits every setting below.

## Slots and their keys

There are 32 **pointing slots**, numbered 0 to 31. Each holds a mode or is
empty, and each has two keys: `PD_SLOT_n` runs the mode while held, and
`PD_SLOT_n_LOCK` locks it on until pressed again. Keys point at the slot, not
the mode, so changing or clearing a mode keeps its keys where they are. A key
for an empty slot does nothing.

- One mode runs at a time. Starting or locking another mode ends the one
  before it, lock included.
- Pressing a locked mode's hold key unlocks it and holds it until you let go.
- Mode keys are keys like any other: a behaviour can hold one, lock one on a
  double tap, or tap a letter and hold the mode.

## Movement

A **scrolling** mode turns the ball into the scroll wheel. It sticks to one
axis at a time, so a near-diagonal flick scrolls one way until the other axis
clearly takes over. It can be limited to horizontal or vertical only; drift
on the other axis then does nothing. It can hold modifiers while it scrolls.

A **directional** mode sends an action each time the ball moves far enough in
a direction. It reads the dominant axis, one axis only, or eight directions,
diagonals included. Each direction can send a key, a shortcut, a mouse-button
click or a macro.

- **Every step** sends again for each threshold the ball travels. Large
  movements are capped at four actions per sensor report, and anything past
  a backlog of 32 is dropped, so a fast spin never floods the computer.
- **Once per movement** sends once, however far the ball goes, then again
  after a 150 ms pause, when the mode ends, or when the ball turns back
  against the direction that sent.
- A macro started while another is playing is dropped.

While a mode runs it can also give mouse buttons 1 to 3 another job: pass
through, do nothing, tap a shortcut, or hold modifiers.

## The slots I ship

The compiled defaults fill the first eight slots; 8 to 31 start empty.

| Slot | Mode | What the ball does |
| --- | --- | --- |
| 0 | Dragscroll | scrolls |
| 1 | Volume | up and down change the volume |
| 2 | Brightness | up and down change screen brightness |
| 3 | Zoom | up and down send Cmd+= and Cmd+- |
| 4 | Arrow | sends arrow keys; button 1 holds Shift, 2 copies, 3 pastes |
| 5 | Pinch | scrolls up and down with Left Cmd held |
| 6 | Undo / Redo | left and right send Cmd+Z and Shift+Cmd+Z |
| 7 | Back / Forward | left and right click mouse buttons 4 and 5, once per movement |

Pinch becomes pinch-to-zoom on a Mac with
[BetterMouse](https://better-mouse.com/), which turns Cmd-scroll into zoom;
without it, it is Cmd-scroll. Zoom needs no app.

## The pointer layer and sniping

**Auto-mouse** turns the **pointer layer** on when the ball moves and off
again after a quiet timeout (1.2 s in my profile). Mouse keys, mode keys, the
DPI keys and behaviours marked as mouse keys keep it on. A typing key turns
it off once the ball is still; modifiers, modified keys such as Cmd+C, and
dual-role keys held for their layer or modifiers don't. A mode either keeps the pointer layer on under it or gives way to the typing
layer; Arrow gives way, so the arrows sit on whatever you were typing on.

**Sniping** slows the pointer for precise work. It comes on by itself while
the sniping layer is on (Navigation, in my profile).

The DPI keys step the pointer speed (400 to 3,400 in steps of 200) and the
sniping speed (100 to 400 in steps of 100). A mode can run at its own DPI.
The DPI keys, auto-mouse and sniping settings are all part of the
[profile](saving.md).
