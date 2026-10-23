# A scrolling mode can scroll one axis only

Cited as D-F08.

A scrolling mode scrolls both axes, horizontally only or vertically only. The
engine still chooses and holds an axis per gesture as for both axes, and a
gesture held on the excluded axis sends nothing, so a sideways swipe in a
vertical-only mode is dropped whole instead of scrolling by its slight
vertical drift. Excluded motion is never remapped onto the allowed axis.
Layout: [`pd-mode-domain-v1.md`](../spec/pd-mode-domain-v1.md).
