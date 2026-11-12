# Lighting

The LEDs are feedback: they show which layer and pointing mode are on, which
keys made a combo, which tap count a key is on, whether a hold has started,
when text replacement changed what you typed, and when the leader is waiting
for what you type after it.
Ark's
[Lighting](https://github.com/NoahCLR/charybdis-ark/blob/dev/docs/guide/lighting.md)
page edits all of it and paints its board the way the keyboard will.

LED 56, on the right half, is a trackball LED I soldered in. A board without
it works the same; that LED just never lights.

## Stages

Lighting is drawn in **stages**, each painting over the one before:

1. **Background effect**: QMK's own lighting effect.
2. **Layer colours**: each layer on, from the base layer up.
3. **Auto-mouse fade**: replaces the layer colours while the pointer layer
   times out.
4. **Pointing modes**: the running mode's colour. A mode whose link has a
   layer on paints beneath the lowest such layer instead, inside stage 2, so
   the link's layers show their keys over the mode's colour ([linked
   layers](pointing.md#linked-layers)).
5. **Combo feedback**: the keys of a combo you are holding.
6. **Behaviour feedback**: tap counts, holds and long holds.
7. **Replacement feedback**: a flash when text replacement replaces what you
   typed, or puts it back, and a colour while the leader waits.

Stages 2 to 7 can each be switched off without losing their colours. A
combo whose action is shown by the stage above it (it previews a layer or
runs a pointing mode) paints just before the layer preview, so the preview
and the mode colour stay on top of it; every other combo paints after the
pointing mode.

### When nothing else draws

- For 5 seconds after it starts, each half lights every LED white (brightness
  150) and draws nothing else.
- The **idle timeout** turns the lighting off after a stretch with no input
  on either half: 10 minutes in my profile, up to 24 hours, or 0 to keep it
  on. Any key or ball movement on either half brings both back.
- **All lighting** off turns every LED off.

## Layer colours

Every layer on paints its colour, lowest first, the base layer included: a
base layer with a colour paints under everything. A layer either paints all
its keys or only the keys it maps (anything but transparent and `KC_NO`), so
lower layers show through its transparent keys. A layer whose colour has
saturation and brightness both at 0 has no colour: it paints nothing, and on
the base layer that lets the background effect show.

### Layer preview

While you hold a key whose hold turns a layer on, the keys light in that
layer's colours before the layer comes on, but only when every outcome of
the press lands on that layer: a thumb key that locks a layer on tap and holds
it on hold previews; an `LT()` key, whose tap types a letter, doesn't. The
preview is part of the layer colours: it follows that stage's switch, not
behaviour feedback's.

## Pointing modes and combos

The running pointing mode paints its colour where you choose (see
[Where feedback paints](#where-feedback-paints)). A combo you hold paints the
combo colour the same way, until you let go. A combo whose action holds a
layer shows only that layer's colours while it holds it. A mode's or the
combo colour with saturation and brightness both at 0 is passthrough: it
paints nothing and what is beneath shows, though its LED groups in a colour
of their own still paint. To turn its LEDs off instead, dark over what is beneath, give it saturation with brightness 0.

## Behaviour feedback

Behaviour feedback shows what a key with a behaviour is doing. Each case
below, each tap count among them, has its own colour and its own place to
paint (see [Where feedback paints](#where-feedback-paints)).

- A single tap paints nothing. From the double tap on, the key takes the
  colour of the tap count it has reached until that branch fires; the colours
  run from the double to the quintuple tap.
- A hold that is pending or running shows the hold colour, a long hold the
  long hold colour. A hold that stays down (held until release, or repeated)
  flashes at an interval you set (200 ms in my profile).
- A tap that fires from a double tap or later can flash once in the tap
  colour, or never. Taps that are layer or pointing-mode actions never flash,
  since the layer or mode shows itself.
- Holding a branch that has a tap and no hold shows the tap colour once the
  multi tap window has passed, whether or not taps flash.
- A case whose colour has saturation and brightness both at 0 is
  passthrough: it paints nothing and what is beneath shows, and the other
  cases still light. To turn its LEDs off instead, dark over what is beneath, give it saturation with brightness 0.

## Replacement feedback

When [text replacement](autocorrect.md) changes what you typed, the keyboard
flashes once, for a length you set apart from the behaviour feedback flash
interval (200 ms in my profile). The flash begins as the replacement starts
to be typed.

- An input replaced with **text** flashes one colour, one replaced by a
  **macro** another, and a replacement you **undo** with Backspace a third:
  green, magenta and red in my profile.
- A colour with saturation and brightness both at 0 is passthrough: that
  case paints nothing, and the others still flash.
- A word that is left as you typed it never flashes, whatever the reason.
- One flash shows at a time. An undo right after a replacement takes its
  place.

The key it follows is the one that ended the word (Space, Enter, a
punctuation key) or, for an undo, the Backspace. If you type on while an
earlier replacement is still being typed, a combo that ends the next word may
flash on one of its keys only.

While [the leader](autocorrect.md#the-leader) waits for what you type after
it, a fourth colour, **leading**, shows and stays, blue in my profile:

- It shows from the Leader key until the leader is done, however long that
  takes. Pressing Leader again or taking a character back keeps it.
- When the leader sends, the text or macro flash takes its place at once;
  when it sends nothing, the colour just goes.
- Its key is the Leader key you pressed: it paints the Leader key's half
  under the half of the key, the Leader key alone under only the key, and
  otherwise the whole keyboard or the half you chose.
- Each of the four, text, macro, undo and leading, paints where you choose
  for it, as a layer or a pointing mode does. One shows at a time.
- With a passthrough colour it paints nothing, but an LED group for it in a
  colour of its own still paints, so leading can show on a few LEDs alone.
- It shows with text replacement switched off too, since the leader works
  then. A lighting idle timeout shorter than the leader timeout can turn it
  dark.

## Where feedback paints

Each pointing mode, combo feedback, and each case of behaviour feedback (each
tap count, tap, hold and long hold) and of replacement feedback (text, macro,
undo and leading) chooses where it paints:

- **both halves**;
- **the left half** or **the right half**;
- **the half of the key** that caused it. A combo with keys on both halves
  paints both;
- **only the keys** that caused it, every key of a combo.

My profile paints every case on the half of the key. Several keys can show
behaviour feedback at once, in different cases. Then the wider places paint
first and the narrower over them, both halves, then one half, then the half
of the key, then only the keys, so a tap count shown on its key stays seen
over a hold that lights the whole keyboard. Where two cases share a place,
the newest key's case paints it, flashing with it when it flashes.

## LED groups

An **LED group** paints chosen LEDs, such as the thumb keys or the trackball
LED, over a stage's own colour, while a given layer, pointing mode, combo
feedback, kind of behaviour feedback or kind of replacement feedback is
showing (or any of them). A group colour with saturation and brightness at 0
takes the stage's colour; any other colour paints as itself. Each tap count
of behaviour feedback has its own: a group for the triple tap shows while any
key is on the triple tap, not on the double tap. A group for any tap count
lights while a run of taps waits on any of them, in the newest key's tap
count colour when it takes the stage's colour, and not for a tap, hold or
long hold; a group for any kind follows the newest key's tap count too, and
lights for those as well. Rows paint in order, so a later row wins an LED
they share. In behaviour feedback and replacement feedback, rows for any kind
of feedback paint first, then in behaviour feedback rows for any tap count,
so rows for one kind paint over them.

A profile holds up to 128 different LED sets and 640 rows in all, at most 255
for any one of layers, pointing modes, combo feedback, behaviour feedback
and replacement feedback. Rows
that light the same LEDs share one set. A row that is not showing costs the
keyboard nothing to draw.

## Brightness and the lighting keys

No colour can be brighter than the keyboard's limit, 200 of 255, which keeps
the current safe. The QMK lighting keys (on/off, effect, speed, hue,
saturation, brightness) change the profile like any edit and are kept or
lost by [the same rules](saving.md#keyboard-adjustments).

## Auto-mouse fade

When the pointer layer is about to time out, its colours fade toward what
will show once it drops: the real colours underneath, an end colour where the
background effect would show, or an end colour on every key. The colour holds
for the first part of the timeout (a third in my profile) before it fades.
There is no fade with auto-mouse off or while a pointing mode is locked.
