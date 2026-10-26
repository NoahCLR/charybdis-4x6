# Pointing modes

The trackball moves the pointer until a **pointing mode** takes it over. A
mode either scrolls or sends keys and shortcuts as the ball moves, and runs
while its key is held or locked, or while one of its linked layers is on. Ark's
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

Mouse buttons 1 to 3 click as usual in every mode. To give a mode keys of its
own, have its key turn on a layer that holds them.

## Linked layers

A slot's link has three parts:

- **Linked layers**, any set of layers. While any of them is on, held or
  locked, the mode runs, with no second key. With linked layers of two slots
  on, the slot whose highest linked layer that is on is higher runs, unless
  what ends it is on too; then the other runs.
- **Layers its key turns on**, a second set, any layers. While the key holds
  or locks the mode those layers are on too, so the keys that mode needs sit
  under my hand. Each goes off with the mode unless something else, a held
  layer key or a lock, still keeps it on. A key that turns on the mode's own
  linked layers makes the mode and its layers one thing, whichever I start,
  and letting go of either key ends both; nothing stays stuck on. Several
  keys may turn on one layer, and a key may turn on a layer that starts
  another mode: that mode waits while the key runs its own, and starts from
  the layer only if something else keeps it on after the key lets go.
- **What ends it**: nothing, a higher layer, or any other layer. Then that
  layer ends the mode its layers started, and it comes back when that layer
  goes off. A higher layer is one above the highest linked layer that is on;
  any other layer is any layer in neither set, below or above. The pointer
  layer and the default layers never end it.

A mode's key always wins over a layer: pressing another mode's key on a
linked layer runs that mode, and letting go of it returns to the layer's
mode; nothing ends a mode its key runs. A mode a layer started never turns
the pointer layer on over that layer, so its keys stay put while the ball
moves.

While a linked mode runs with a layer of its link on, however it started, its
colour lights the board beneath the lowest of those layers, from either set,
and each of them paints its own keys over it: with the layers coloured "only mapped keys" I see
which keys they hold on the mode's colour. A layer above covers both. A layer
starts at most one slot's mode, and the pointer layer or a default layer in a
link does nothing.

## The slots I ship

The compiled defaults fill the first eight slots; 8 to 31 start empty. None
of them has a link.

| Slot | Mode | What the ball does |
| --- | --- | --- |
| 0 | Dragscroll | scrolls |
| 1 | Volume | up and down change the volume |
| 2 | Brightness | up and down change screen brightness |
| 3 | Zoom | up and down send Cmd+= and Cmd+- |
| 4 | Arrow | sends arrow keys |
| 5 | Pinch | scrolls up and down with Left Cmd held |
| 6 | Undo / Redo | left and right send Cmd+Z and Shift+Cmd+Z |
| 7 | Back / Forward | left and right click mouse buttons 4 and 5, once per movement |

Pinch becomes pinch-to-zoom on a Mac with
[BetterMouse](https://better-mouse.com/), which turns Cmd-scroll into zoom;
without it, it is Cmd-scroll. Zoom needs no app.

## The pointer layer and sniping

**Auto-mouse** turns the **pointer layer** on when the ball moves and off
again after a quiet timeout (1.2 s in my profile). Mouse keys, mode keys, the
DPI keys and behaviours marked as mouse keys keep it on. A typing key turns it
off once the ball is still; modifiers, modified keys such as Cmd+C, and
dual-role keys held for their layer or modifiers don't. A mode either keeps
the pointer layer on under it or gives way to the typing layer; Arrow gives
way, so the arrows sit on whatever you were typing on.

**Sniping** slows the pointer for precise work. It comes on by itself while
any of the auto-sniping layers is on (Navigation alone, in my profile), and
strips a separate pointer layer so those layers keep their keys. Switching
auto-sniping off keeps the layers chosen for when it comes back on.

The DPI keys step the pointer speed (400 to 3,400 in steps of 200) and the
sniping speed (100 to 400 in steps of 100). A mode can run at its own DPI.
The DPI keys, auto-mouse and sniping settings are all part of the
[profile](saving.md).
