# Macro text is typed through the host's layout

Cited as D-F16.

The keyboard sends key positions and the computer's layout makes them
characters, so a macro types each character with the keys the chosen host
layout uses, dead keys included. Unicode entry is only the fallback for what
the layout cannot type, and a character neither can type is refused before
output. The keyboard cannot see the computer's layout, so the user picks it;
US is the default and types what earlier firmware typed. The catalogue is
data: one versioned fixture, generated from macOS's, Linux's and QMK's Windows
layout definitions, generates the C tables, and clients read the same fixture,
so labels and typing cannot disagree
([`host-layouts-v1.md`](../architecture/host-layouts-v1.md)).
