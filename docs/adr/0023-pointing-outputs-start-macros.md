# Pointing outputs start macros

Cited as D-F23.

Directions, diagonals and mouse-button tap overrides can start macros 0–127,
played by the existing macro engine. A macro tap must inherit modifiers with
mask zero, since playback outlives the tap that started it. A trigger while a
macro plays is dropped, and only a successful start spends a once-per-movement
movement (D-F07). Rules: [`pd-mode-domain-v1.md`](../spec/pd-mode-domain-v1.md),
"Taps, modifiers and buttons".
