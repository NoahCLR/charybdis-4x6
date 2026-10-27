# Lighting

The LEDs are feedback: they show which layer and pointing mode are on, which
keys made a combo, which tap count a key is on and whether a hold has started.
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
6. **Key feedback**: tap counts, holds and long holds.

Stages 2 to 6 can each be switched off without losing their colours. A
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
preview is part of the layer colours: it follows that stage's switch, not key
feedback's.

## Pointing modes and combos

The running pointing mode paints its colour where you choose (see
[Where feedback paints](#where-feedback-paints)). A combo you hold paints the
combo colour the same way, until you let go. A combo whose action holds a
layer shows only that layer's colours while it holds it.

## Key feedback

Key feedback shows what a key with a behaviour is doing.

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

## Where feedback paints

Pointing modes, combo feedback and key feedback each choose where they paint:

- **both halves**;
- **the left half** or **the right half**;
- **the half of the key** that caused it. A combo with keys on both halves
  paints both;
- **only the keys** that caused it, every key of a combo.

## LED groups

An **LED group** paints chosen LEDs, such as the thumb keys or the trackball
LED, over a stage's own colour, while a given layer, pointing mode, combo
feedback or kind of key feedback is showing (or any of them). A group colour
with saturation and brightness at 0 takes the stage's colour; any other colour
paints as itself. Rows paint in order, so a later row wins an LED they share.
In key feedback, rows for any kind of feedback paint first, so rows for one
kind paint over them.

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
