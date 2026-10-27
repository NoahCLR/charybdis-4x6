# Macros and host layouts

A **macro** is a named sequence the keyboard plays back: text, keys and
chords, delays, and key presses and releases of their own. There are 128
macro slots, `MACRO_0` to `MACRO_127`. A macro can sit on a key, be a
behaviour's tap or hold, a combo's output, or a pointing mode's direction. Ark's
[Macros](https://github.com/NoahCLR/charybdis-ark/blob/dev/docs/guide/macros.md)
page writes and records them.

## Playing

- One macro plays at a time. Triggering one while another plays does
  nothing; it is dropped, not queued.
- A macro plays alongside everything else: the trackball, the lighting and
  your other keys keep working while it types and waits.
- As a hold that is held until release, a macro plays once at the threshold.
- Every key a macro presses it also releases, and it never lets go of a key
  something else is holding. A macro that would leave a key down, release one
  it never pressed, or hold more than 16 keys at once doesn't play.
- The 128 macros share 34,903 bytes, and one macro can use all the free
  space.

## Typing through your host layout

The keyboard sends key positions; your computer's keyboard layout decides
which character each one types. So that macro text comes out right, the
profile names the **host layout** the computer uses, and every character is
typed with that layout's own keys, accented letters and dead keys included.

| Host | Layouts |
| --- | --- |
| any | US (the default; plain ASCII only) |
| macOS | ABC, Dutch, Unicode Hex Input, British, German, French |
| Windows | US International, United Kingdom, German, French |
| Linux | US International, United Kingdom, German, French |

The layout you pick must be the one the computer is using; the keyboard can't
check. If macOS set the keyboard up as ISO, turn on the ISO option too, or
the keys left of 1 and left of Z swap.

The host can be **Auto**, which uses QMK's guess at the operating system. A
KVM or USB switch can fool it; pick the host yourself if so.

## Unicode entry

A character the layout can't type, such as an emoji, needs the host's
Unicode entry:

- **macOS**: make Unicode Hex Input the Mac's input source, and choose that
  layout (or US with Unicode playback on). The keyboard holds Option and
  types the hex code. A layout with Option characters, such as Dutch, can't
  take Unicode entry, since Option types characters there.
- **Windows**: turn on Unicode playback and run
  [WinCompose](https://github.com/samhocevar/wincompose) with Right Alt as
  the Compose key.
- **Linux**: turn on Unicode playback and use an input method or app that
  accepts Ctrl+Shift+U, such as IBus.

A macro with a character neither the layout nor Unicode entry can type
doesn't play at all. Unicode entry is slower than typing.

## Uninterruptible playback

A macro can play **uninterruptibly**: only the macro types until it ends. New
key presses are ignored, not queued, and keys you were already holding stop
typing; modifiers you still hold come back afterwards, and other keys need
to be pressed again. The trackball, media keys and other keyboards are not
held back. It is on by default for a macro that needs Unicode entry, and can
be forced on or off per macro in Ark. With it off, keep your hands off the
keys while a macro plays. Stopping a macro part way leaves the text it has
already typed.
