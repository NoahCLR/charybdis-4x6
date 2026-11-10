# Text replacement (autocorrect)

The keyboard replaces what you type when you finish a word, on any computer,
with nothing installed there. It fixes a typo (`teh` becomes `the`), types
longer text for you (`pnumber` becomes your phone number) and can play a
macro. Your list is part of your profile: Ark's Text replacement page edits
it, both halves save it and a backup holds it. The list that ships is empty.

An entry is one **replacement** with every **input** that should
produce it: `because` for `becuase`, `becasue` and `beacuse`; an address for
`myaddr`. The firmware's own words for the two are a correction and a typo,
which is what you will see in the keys' names (`AC_` for autocorrect) and in
[the C list](authoring.md#the-autocorrect-list).

## When it replaces

Only when you finish a word. A word is letters and digits, with apostrophes
between them; it ends at a space, Enter, Tab, punctuation or a hyphen. If
the finished word is an input on your list, the keyboard replaces it
before that ending key reaches the computer:

- It erases only what differs and types the rest: `becuase` to `because` is
  four Backspaces and four letters.
- The ending key then goes through as the real key, so Enter in a chat box
  sends the replaced text and a held Space still repeats.
- An input inside a word you are still typing is never touched.

An input is itself always letters and digits, so it is one word:
`pnumber`, `myemail`, `sig2`.

## Part of a word

An input matches the whole word unless it says otherwise. It can also
match the start of a word, the end of a word, or anywhere in one. Ark writes
those with a `*` where other letters may go:

| Input | Matches | So it also fixes |
| --- | --- | --- |
| `recieve*` | the start of a word | `recieved`, `recieving` |
| `*mnet` | the end of a word | `statemnet` |
| `*tihng*` | anywhere in a word | `sometihng`, `tihngs` |

Only the matched part is replaced; the rest of the word stays. When several
inputs fit, the longest wins, and one for the whole word beats one for
part of it.

## What a replacement can be

**Text**, up to 128 bytes on one line: a word, a phone number, an e-mail
address, a sign-off. It may hold spaces, digits and any punctuation your
host layout types.

**A macro**, for anything longer, anything of more than one line, or
anything that is more than text. Pick one of your [macros](macros.md): when
you finish the input the keyboard erases it and plays the macro, then
sends the key you ended the word with. A macro replacement differs from text
in four ways:

- it matches the whole word only, never part of one;
- it plays as it is stored, whatever capitals you typed;
- Backspace does not take it back, since what a macro did cannot be undone
  by erasing;
- if its macro is empty, nothing happens and what you typed stays.

## Capitals

Inputs match whatever their case, and a text replacement follows what
you typed:

- `Teh` becomes `The`;
- `TEH` becomes `THE`;
- anything else, `teh` or `tEh`, gives the replacement as it is written;
- a replacement written with capitals keeps them: `iphone` and `Iphone` both
  become `iPhone`. Digits and punctuation have no capitals, so a phone
  number comes out as written however you typed `pnumber`.

Caps Lock counts: the keyboard reads it from the computer, and types the
replacement so that it comes out right with Caps Lock on or off.

## Taking a replacement back

If you did mean what you typed, press Backspace right after the replacement.
The keyboard removes the space or punctuation you ended the word with and
puts the word back exactly as you typed it. End the word again and it is
left alone.

This works only as the very next key, and not after Enter or Tab: by then
the message may be sent or the focus moved, so Backspace is just Backspace.
It does not work for a macro.

## Only what you type

The keyboard only knows the word if it saw every character of it, so it
stops following the word when it cannot be sure:

- After a shortcut, an arrow key, Home, End, Page Up or Down, Delete, a
  mouse click or a macro, the next character starts a new word.
- After a Backspace past the start of the word, a key held until the
  computer repeats it, or a dead key that composes nothing, it replaces
  nothing until the next word ends.

Layer changes and moving the pointer change nothing. Text a macro types is
never replaced.

## Your layout

Typing is read, and replacements are typed, through the host layout you chose
([Macros and host layouts](macros.md#typing-through-your-host-layout)), dead
keys included. An entry whose replacement your layout cannot type is
skipped; the keyboard never falls back to Unicode entry for it.

One case is left as you typed it: a word you end with a character that
takes a dead key, such as the quote on US-International (the quote key, then
Space). The computer is already composing that character when the word
ends, so the keyboard does not replace the word before it.

## Typing on while it works

A replacement takes a moment: about a hundredth of a second for each
Backspace and each character, so a short fix is over before your next key
and the longest text, 128 characters, takes about a second and a half. You
do not have to wait. Keys you press meanwhile are held back and come out
afterwards in the order you pressed them, and a Shift you are holding does
not get into the replacement.

The keyboard holds 240 key changes back, several times what fast typing makes
during the longest replacement. If you outrun even that, by mashing keys or
during a macro that plays for many seconds, the replacement stops where it
is rather than lose a key: the word is left partly replaced and everything
you typed still arrives in order.

## What not to put in it

Your list is stored on the keyboard as you wrote it, and anyone who connects
the keyboard to Ark can read it. Do not use a replacement for a password.

## Turning it off

The **Text replacement** switch in Ark, under Settings → Host, turns it off
in the profile. Three [switch keys](keys.md#where-behaviours-and-combos-run),
`AC_ON`, `AC_OFF` and `AC_TOGG`, turn it on, off or the other way until the
keyboard is unplugged. They never change the switch in Ark, and with text
replacement switched off there `AC_ON` does not turn it on.

The keyboard cannot see what you are typing into. Put `AC_TOGG` or `AC_OFF`
on a key and use it before a password, in a terminal, or anywhere a real
word on your list would be wrong.
