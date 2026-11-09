# Autocorrect

The keyboard fixes the typos on your **autocorrect list** as you type, on
any computer, with nothing installed there. The list is part of your
profile: Ark's Autocorrect page edits it, both halves save it and a backup
holds it. The list that ships is empty.

An **entry** is one **correction**, the right word, with every **typo**
that should become it: `because` for `becuase`, `becasue` and `beacuse`.

## When it corrects

Only when you finish a word. A word is letters and digits, with apostrophes
between them; it ends at a space, Enter, Tab, punctuation or a hyphen. If
the finished word is a typo on the list, the keyboard corrects it before
that ending key reaches the computer:

- It erases only what differs and types the rest: `becuase` to `because` is
  four Backspaces and four letters.
- The ending key then goes through as the real key, so Enter in a chat box
  sends the corrected text and a held Space still repeats.
- A typo inside a word you are still typing is never touched.

Each typo says **where it matches**: the whole word, which is the default,
the start of a word, the end of a word, or anywhere in a word. `tihng` for
`thing`, anywhere, also fixes "something" and "things". When several typos
fit, the longest wins, and a whole-word typo beats a part-of-word one.

## Capitals

Typos match whatever their case, and the correction follows what you typed:

- `Teh` becomes `The`;
- `TEH` becomes `THE`;
- anything else, `teh` or `tEh`, gives the correction as it is written;
- a correction written with capitals keeps them: `iphone` and `Iphone` both
  become `iPhone`.

Caps Lock counts: the keyboard reads it from the computer, and types the
correction so that it comes out right with Caps Lock on or off.

## Taking a correction back

If you did mean what you typed, press Backspace right after the correction.
The keyboard removes the space or punctuation you ended the word with and
puts the word back exactly as you typed it. End the word again and it is
left alone.

This works only as the very next key, and not after Enter or Tab: by then
the message may be sent or the focus moved, so Backspace is just Backspace.

## Only what you type

The keyboard only knows the word if it saw every character of it, so it
stops following the word when it cannot be sure:

- After a shortcut, an arrow key, Home, End, Page Up or Down, Delete, a
  mouse click or a macro, the next character starts a new word.
- After a Backspace past the start of the word, a key held until the
  computer repeats it, or a dead key that composes nothing, it corrects
  nothing until the next word ends.

Layer changes and moving the pointer change nothing. Text a macro types is
never corrected.

## Your layout

Typing is read, and corrections are typed, through the host layout you chose
([Macros and host layouts](macros.md#typing-through-your-host-layout)), dead
keys included. An entry whose correction your layout cannot type is skipped;
the keyboard never falls back to Unicode entry for it.

## Typing fast

A correction takes a moment: about a hundredth of a second for each
Backspace and letter. Keys you press meanwhile are held back and come out
afterwards in the order you pressed them, and a Shift you are holding does
not get into the correction.

## Turning it off

The **Autocorrect** switch in Ark turns it off in the profile. Three
[switch keys](keys.md#where-behaviours-and-combos-run), `AC_ON`, `AC_OFF`
and `AC_TOGG`, turn it on, off or the other way until the keyboard is
unplugged. They never change the switch in Ark, and with autocorrect
switched off there `AC_ON` does not turn it on.

The keyboard cannot see what you are typing into. Put `AC_TOGG` or `AC_OFF`
on a key and use it before a password, in a terminal, or anywhere a real
word on your list would be wrong.
