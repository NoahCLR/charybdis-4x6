# What a behaviour leaves empty is the standard key

Cited as D-F64. Supersedes D-F33.

A branch is one tap count's tap, hold and long hold. Each branch does only
what it sets, and nothing from one branch reaches another, higher or lower.
Whatever a behaviour leaves empty is the **standard key**: what the key does
with no behaviour at all.

1. The standard key. A plain key or a bare modifier taps itself and holds
   itself from the tap / hold threshold; a lone tap of a modifier sends the
   modifier, and two modifiers tapped together send both. `LT()` and `MT()`
   tap their key and hold their layer or modifiers; `OSM()` arms its
   one-shot. `MO()`, `LM()` and a pointing-mode key hold from the press and
   tap nothing. A custom key does its shortcut, a Tap-toggle or One-shot one
   as it does with no behaviour, and with no shortcut does nothing. A macro
   plays.
2. Transparent is set. A tier set to Transparent sends the key below. A
   single tap set to Transparent is also the standard key's tap: wherever a
   tap is empty, inside the depth and past it, the key types the key below.
3. The single tap. When it sets a tap and leaves its hold and long hold
   empty, a key with a hold of its own keeps it: a plain key or modifier
   holds itself, `LT()` and `MT()` their layer or modifiers, a pointing-mode
   key its mode, a custom key with a shortcut its shortcut. Only a key with
   none (a macro, a custom key without a shortcut) sends the set tap on
   release, however long it was held. With only a long hold set, a release
   before the long hold threshold sends the tap, set or standard.
4. Later tap counts. A set tap fills an empty hold and long hold: the tap is
   sent on release, however long the press was held. A set hold with an
   empty long hold carries the hold on. With only a long hold set, a release
   before the long hold threshold sends the tap, set or standard. An empty
   tap sends the standard key's tap N times at tap count N, never the single
   tap.
5. A branch inside the depth that sets nothing is the standard key pressed N
   times with the last held: tapped, N standard taps; held at its Nth press,
   N - 1 standard taps, then the standard key's hold.
6. Past the deepest tap count, every press of a quick run is the standard
   key, the run's earlier presses included, and the behaviour's branches
   don't apply. Quick presses tap it; a held press holds it, as rule 1 says,
   from the press or the tap / hold threshold.
7. The standard key keeps the behaviour's own thresholds: a hold past the
   depth or in an empty branch starts at the behaviour's tap / hold
   threshold, or the profile's where the behaviour sets none.
8. Taps go out in press order, before a held press does anything, its
   fallback hold included. A key's own taps never settle its own pending
   press into a hold.
9. A behaviour that sets only the single tap doesn't count taps: every press
   is a single tap.

So an Escape whose single tap only long-holds Option+Command+Escape, tapped
three times and held on the fourth, sends Escape three times at the fourth
press and then holds Escape until release; held alone it still sends
Option+Command+Escape at the long hold threshold. A Right Alt whose single
tap locks the arrow mode and whose double tap holds Zoom sends Right Alt
twice for a quick double tap, and held alone holds Right Alt. A Dragscroll
whose single tap is Transparent types two of the key below for a quick
double tap and three for three, and held runs Dragscroll. A thumb key, a
custom key without a shortcut, does nothing past its depth.

D-F33 had an empty tap at a later tap count repeat the first tap, and every
press past the depth send it, a held one there taking the first tap count's
hold. That let one branch's actions turn up at another: Right Alt's double
tap toggled the arrow lock, and Escape's long hold fired from a fourth
press. Its run also went out only on the held press's release, after the
hold. D-F33's amendment for a bare modifier's lone tap is rule 1 here.
Contract: [What keys can do](../guide/keys.md#tap-counts).
