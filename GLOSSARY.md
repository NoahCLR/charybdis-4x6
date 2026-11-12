# Charybdis firmware

The words for the keyboard itself: what it stores and runs, how its two halves
agree, and what a client says to it. The app's own words (draft, review,
backups, its screens) are in Charybdis Ark's glossary.

## Halves and the host

**Half**:
One of the keyboard's two sides, each with its own controller, flash and copy
of the profile.
_Avoid_: side (for the controller), node

**Master half**:
The half plugged into the host over USB, always the right one; it answers
clients and leads every save.
_Avoid_: USB half, local half, primary half

**Slave half**:
The half reached only through the master half, over the cable between them.
_Avoid_: peer, peer half, secondary half, non-USB half, sibling

**Converged**:
Both halves hold the same saved generation, and the same working copy when
there is one.
_Avoid_: in sync, agreed, matched

**Host**:
The computer the keyboard is plugged into and types into.
_Avoid_: PC, system, your computer (in specs)

**Host layout**:
The keyboard layout the host types with (US, Dutch, German…), through which
macro text becomes key presses.
_Avoid_: layout (alone), locale, language

**Client**:
Any program that speaks Profile Wire to the keyboard; Charybdis Ark is the one
that edits it.
_Avoid_: app (in specs), editor, host (for the program)

## The profile

**Profile**:
Everything the keyboard can be told to do without a reflash, kept as one
versioned whole: keymap, behaviours, combos, macros, custom keys, pointing
modes, lighting, settings, the autocorrect list and the keyboard's name.
_Avoid_: config, configuration, setup, layout, keymap (for the whole)

**Domain**:
One independently versioned part of a profile, such as the keymap, the macros
or the lighting.
_Avoid_: section, area, block, table

**Authored profile**:
The profile written in C in the keymap folder, the only place it exists as
source.
_Avoid_: source profile, C profile, factory profile

**Compiled defaults**:
The authored profile as built into the firmware: what runs when nothing is
saved, and the fallback when a saved profile cannot be used.
_Avoid_: factory defaults, built-in defaults, flashed defaults, firmware
defaults, authored defaults

**Saved profile**:
The profile stored in a half's flash, which the keyboard boots into.
_Avoid_: committed profile, stored profile, durable profile

**Generation**:
One saved profile in the sequence of saves; every Apply or Save makes the
next one, and the newer one wins when the halves meet again.
_Avoid_: version, counter, device commit, revision (for this)

**Working copy**:
The profile a half runs from RAM while it has unsaved edits, begun from the
saved profile by the first live edit.
_Avoid_: live profile, running copy, draft, staging

**Active profile**:
What the keyboard runs right now: the working copy if there is one, otherwise
the saved profile, otherwise the compiled defaults.
_Avoid_: effective profile, running profile, live profile, current profile

**Unsaved edits**:
The difference between the working copy and the saved profile; a replug or a
restart loses them.
_Avoid_: dirty state, pending changes, live changes

**Profile slot**:
One of two fixed flash areas per half, A and B, that hold saved generations;
a save always writes the one not in use.
_Avoid_: slot (alone), bank, record (for the area)

**Capabilities**:
What a firmware build reports it supports and holds, read by a client before
anything else.
_Avoid_: features, feature flags, version check

## Changing the profile

**Edit mode**:
How a client's edits reach the keyboard, stored on both halves outside every
profile, listed in this order: **Live edit** (each edit runs at once and the
keyboard saves after a quiet interval), **Live preview + save** (each edit runs
at once and waits for Save; the default) or **Review and save** (the keyboard
changes only by an Apply, which clients show as Save).
_Avoid_: save mode, sync mode, review mode, auto-save mode, draft mode, Live +
review, Live (for the mode), Apply mode

**Live edit**:
A change to the working copy that takes effect on both halves at the next
safe boundary, without a save. Both live edit modes make them; the mode named
Live edit also has the keyboard save them by itself.
_Avoid_: patch, delta, hot edit

**Session**:
The identity of one boot of the keyboard; a new session means unsaved edits
are gone.
_Avoid_: boot id, connection

**Revision**:
The count of changes to the working copy within a session; every live edit
names the session and revision it was made against.
_Avoid_: version, generation, sequence number

**Save**:
Making the working copy the next saved generation on both halves.
_Avoid_: commit (as a user word), persist, flush

**Revert**:
Dropping the working copy so the keyboard runs its saved profile again,
without writing flash.
_Avoid_: undo, discard, reset, rollback

**Apply**:
Replacing the saved profile with a candidate on both halves, as one
operation a client drives.
_Avoid_: upload, flash, deploy, restore (for this)

**Candidate**:
The complete profile a client sends for an Apply, held and checked by the
master half until it is committed or abandoned.
_Avoid_: staging, staged profile, pending profile

**Commit**:
The point of an Apply or Save after which the new generation will become
active on both halves even if it is interrupted.
_Avoid_: decision, point of no return

**Activation**:
A half switching to run a new generation or working copy, always at a safe
boundary.
_Avoid_: swap, go live, publish (for this)

**Safe boundary**:
A moment when no key, layer, lock, macro or tap is in progress, so the profile
can change under no one's fingers.
_Avoid_: safe point, idle, quiet moment

**Keyboard adjustment**:
A change made by a key on the keyboard itself, such as DPI up or a lighting
key, which joins the working copy like any edit.
_Avoid_: hardware edit, local edit

**Stopped**:
A half that met a fault it cannot safely carry on from; it keeps typing but
refuses Save, Revert and edits until a restart.
_Avoid_: halted, failed, wedged

**Restart**:
Both halves rebooting together at a client's request, exactly as safe as a
replug; the saved profile runs afterwards.
_Avoid_: reset, soft replug, reboot (for this)

## Keys and layers

**Keymap**:
The domain holding the keycode at every key position of every layer.
_Avoid_: layout, key map, matrix

**Base layer**:
The lowest layer, the one every lock must lead back to; it is named Base
unless renamed.
_Avoid_: layer 0, home layer, default layer (for this)

**Default layers**:
The layers the keyboard keeps on beneath everything else, chosen in settings.
_Avoid_: base layers, persistent layers

**Layer priority**:
The order of the layers, which decides which one covers which.
_Avoid_: layer order, layer index, stacking order

**Lock**:
A layer or pointing mode kept on after its key is released, until something
turns it off.
_Avoid_: toggle, latch, sticky

**Source layer**:
The layer that supplied a press's key, after falling through transparent keys.
_Avoid_: origin layer, resolved layer

**Placement**:
A key position on one source layer, where behaviours and combos can be
switched off for that key alone.
_Avoid_: slot, cell, key instance

**Custom key**:
One of 128 keycodes of the user's own, each with a name, an optional shortcut
and an optional behaviour.
_Avoid_: user keycode, owned keycode, custom keycode

**Shortcut**:
A custom key's own action: up to six keys held together, any modifiers and
any layers, each optional, held while the custom key is held and tapped when it
is tapped; its layers act together as one layer key, its layer kind (D-F26).
_Avoid_: chord, hotkey, key combo

**Layer kind**:
The layer key a shortcut's layers act as, all of them together: Hold
(`MO()`), Lock (`TG()`), Move to (`TO()`), Tap-toggle (`TT()`) or One-shot
(`OSL()`) (D-F26).
_Avoid_: layer mode, layer action (of a shortcut)

**Action**:
What a key, behaviour, combo or pointing mode sends, stored as one keycode.
_Avoid_: output (in general), binding, command

## Behaviours and combos

**Behaviour**:
A shared definition of what one keycode does for each tap count and tier,
used by every key and combo output that sends that keycode.
_Avoid_: behavior (in prose), key behavior, tap dance, row

**Tap count**:
Which tap of a quick sequence a press is: single, double, and on up to the
build's tap depth, normally five.
_Avoid_: tap step, tap index, multi-tap level

**Tier**:
How long a press lasted: tap, hold or long hold.
_Avoid_: level, stage, longer hold, second hold

**Branch**:
Everything a behaviour does on one tap count: its tap, hold and long hold.
_Avoid_: tap step, step, row, tap-count entry

**Standard key**:
What a key does with no behaviour: its own tap and hold. A behaviour's
empty tiers and every press past its deepest tap count are the standard key
(D-F64); a single tap set to Transparent is its tap.
_Avoid_: built-in behaviour, default key, fallback key

**Hold style**:
How a hold sends its action: held until release, tapped at the threshold,
tapped on release, or repeated while held.
_Avoid_: hold helper, hold mode

**Tap / hold threshold**:
The press length that separates a tap from a hold.
_Avoid_: tapping term, hold time, dual-role time

**Long hold threshold**:
The press length that separates a hold from a long hold.
_Avoid_: longer hold term, second threshold

**Multi tap window**:
How long after a release the next press still counts as another tap.
_Avoid_: tap window, multi-tap timeout, quick-tap term

**Combo**:
Keys pressed together that send one action instead of their own.
_Avoid_: chord (for this), key combination

**Combo window**:
How close together a combo's keys must be pressed.
_Avoid_: combo term, combo timeout

**Switch key**:
A key that turns combos (`CM_ON`, `CM_OFF`, `CM_TOGG`), behaviours
(`BH_ON`, `BH_OFF`, `BH_TOGG`), auto-mouse (`AM_ON`, `AM_OFF`, `AM_TOGG`) or
autocorrect (`AC_ON`, `AC_OFF`, `AC_TOGG`) on, off or the other way for the whole keyboard until it restarts, never
saved; the stored switch stays as it is.
_Avoid_: toggle key, combo toggle, behaviour key

**Participation**:
Whether a press uses its behaviour and whether it can join a combo, allowed
only when the whole keyboard, the source layer, the definition and the
placement all allow it.
_Avoid_: eligibility, permission, enablement

**Macro**:
A named sequence of steps the keyboard plays back, kept in one of 128 macro
slots.
_Avoid_: VIA macro, text macro, dynamic macro, script

**Macro step**:
One element of a macro: text, a key or chord, a delay, a press or a release.
_Avoid_: action (for this), event, token

**Uninterruptible playback**:
A macro playing with held typing keys suspended and new presses ignored, so
nothing the person does mixes into what it types.
_Avoid_: protected playback, input protection

## Autocorrect

**Text replacement**:
The keyboard replacing what is typed when a word ends: a typo with the right
word, an abbreviation with longer text, a typed form with a macro. It is the
name people see. The code, the domain, the specs, the fixtures and the keys
(`AC_ON`, `AC_OFF`, `AC_TOGG`) were named for **autocorrect** and keep that
name, and they call a replacement a `correction` and a typed form a `typo`.
_Avoid_: text expansion, snippets, abbreviations (for the feature), shortcut
(a custom key's)

**Autocorrect list**:
What the keyboard replaces and with what, kept in the profile as its own
domain.
_Avoid_: dictionary, autocorrect table, trie, word list

**Entry**:
One replacement with every typed form that produces it.
_Avoid_: rule, pair, row (for this), mapping

**Replacement**:
What an entry puts in place of what was typed, stored once: text of up to 128
bytes on one line, typed as written, or a macro slot, played. `correction` in
code, on the wire and in fixtures.
_Avoid_: correction (outside those names), fix, target, output, expansion

**Typed form**:
What is typed to get an entry's replacement, stored case-folded: a
misspelling, or an abbreviation such as `pnumber`. It belongs to exactly one
replacement. People know it as an **input**: the guide says input, as the
app and its files do, and the specs and ADRs here say typed form for the
same thing. `typo` in code, on the wire and in fixtures.
_Avoid_: typo (as the term, outside those names; a misspelling is still a
typo), trigger, pattern, source, abbreviation (as a second term)

**Leader**:
Text replacement's own Leader key, QMK's `QK_LEADER`: after it, what is typed
is held on the keyboard and sent only as the replacement of a typed form that
fires after the leader (D-F71). Not QMK's Leader feature, which the build leaves out.
_Avoid_: leader sequence (for the key), QMK Leader

**Leading**:
The keyboard holding what is typed after the Leader key, showing the computer
none of it, until it sends a replacement or drops what it holds.
_Avoid_: leader mode, waiting (alone)

**How it fires** (a replacement's trigger):
Whether a replacement's typed forms fire as a word ends, after the Leader key
alone, or both (D-F71): the replacement's, never a typed form's, so a client
switches it without touching them. After the leader a typed form is its whole
text, whatever its match. Trigger `0`, `1` or `2` in bits 1–2 of the
correction row's kind byte; Ark writes them As you type, After Leader and
Both. A typed form of a replacement that fires after the leader is what
people call a **leader input**.
_Avoid_: leader sequence (for what is stored), leader typo, shortcut, mode

**Leader timeout**:
How long the leader waits after its last key before it sends a typed form a
longer one begins with, or drops what is held: settings scalar 32.
_Avoid_: leader term, leader window

**Word**:
What text replacement follows as it is typed: letters and digits, with
apostrophes between them. Apostrophes before and after it stay on screen and
are not part of what a typed form matches.
_Avoid_: token, string (for this)

**Word end**:
The character that finishes a word: anything that is not a letter, a digit or
an apostrophe, and Enter or Tab. A typed form is replaced only here.
_Avoid_: delimiter, boundary, trigger key

**Undo a replacement**:
Backspace pressed right after a text replacement that a printing character
ended: the ending character goes and the word comes back as typed. A macro
has none.
_Avoid_: revert (for this), rollback

**Fresh start**:
Text replacement forgetting the word because the cursor may have moved: the
next character starts a new word.

**Suspended**:
Text replacement replacing nothing until the next word end, because it no
longer knows where the word on screen starts.

**Typing stage**:
The step between live key ownership and the keyboard report where text
replacement reads what is typed and holds keys back while a replacement is
typed or its macro plays.
_Avoid_: filter, interceptor, deferral layer

**Where it matches**:
What part of a word a typed form has to be: the whole word, the start of a
word, the end of a word, or anywhere in a word. A stored number; a client may
write the three part-of-word ones with `*` where other letters may go
(`recieve*`, `*mnet`, `*tihng*`). A macro's typed forms match the whole word.
After the leader a typed form is its whole text, whatever its match.
_Avoid_: boundary, anchor, mode, match type

## Pointing

**Pointing mode**:
A role the trackball takes instead of moving the pointer, either scrolling or
directional, while its key is held or locked or one of its linked layers is
on; one runs at a time. Its keys keep the keycode names `PD_MODE_n` and
`PD_MODE_n_LOCK` (D-F50), which the words to avoid do not cover.
_Avoid_: pd mode, PD-mode, pointer mode, trackball mode

**Pointing slot**:
One of 32 fixed, numbered places that holds a pointing mode or is empty; keys
reach the slot, so a slot keeps its keys when its mode changes. `PD_MODE_n`
holds slot `n`'s mode and `PD_MODE_n_LOCK` locks it (D-F50).
_Avoid_: PD slot, `PD_SLOT_n` (retired), pointing-mode slot, mode slot

**Link** (of a pointing slot):
Its linked layers, the layers its key turns on, and what ends it (D-F62). No
layer starts two modes; any number of keys may turn one on.
_Avoid_: binding, layer binding

**Linked layers**:
The set of layers that start a pointing slot's mode while any of them is on.
_Avoid_: mode layers, pointing layers, trigger layers

**Turns on** (of a link):
The set of layers a mode's key turns on while it runs the mode: any layers,
its own linked layers among them or not, and layers another mode starts from.
_Avoid_: direction (retired), chosen layer (retired), brings, brought layer

**Ends on** (of a link):
What ends a mode its linked layers started, until that layer goes off:
nothing, a higher layer (one above the highest linked layer that is on), or
any other layer (one outside both of the link's sets). The pointer layer and a default layer
never end it.
_Avoid_: top only (retired), exclusive, yield

**Source** (of a pointing mode):
What runs a mode: its key, held or locked, or its linked layers. A key source
beats a layer source.
_Avoid_: trigger, origin, owner (for this)

**Movement**:
Whether a pointing mode turns ball motion into scrolling or into directional
actions.
_Avoid_: engine, engine family, kind

**Pointer layer**:
The layer meant for trackball work, turned on by moving the ball.
_Avoid_: mouse layer, auto-mouse layer, pointing layer

**Auto-sniping layers**:
The layers whose being on turns sniping on by itself, behind one master
switch; one of them is an auto-sniping layer.
_Avoid_: sniping layer, the auto-sniping layer (for the set)

**Auto-mouse**:
Turning the pointer layer on when the ball moves and off again after a quiet
timeout.
_Avoid_: auto pointer layer, mouse keys

## Lighting

**Lighting**:
Everything the keyboard's LEDs show, all of it feedback about state.
_Avoid_: RGB (in prose), backlight

**Stage**:
One of the seven lighting layers drawn in order: the background effect, layer
colours, the auto-mouse fade, pointing modes, combo feedback, behaviour
feedback and replacement feedback; each can be switched off.
_Avoid_: layer (for this), pass, overlay

**Behaviour feedback**:
The stage that shows what a key with a behaviour is doing: the tap count it
has reached, a hold or long hold pending or running, and a tap that commits.
_Avoid_: key feedback (the code keeps key_feedback and KEY_BEHAVIOR_FEEDBACK names)

**Replacement feedback**:
The stage that flashes when text replacement changes what was typed: a typed
form replaced with text, replaced by its macro, or a replacement undone, each
in its own colour (D-F70). What the leader sends flashes as a replacement
does (D-F71). `autocorrect feedback` in code.
_Avoid_: autocorrect flash (in prose), correction feedback, typo light

**Background effect**:
QMK's own lighting effect, the bottom stage everything else paints over.
_Avoid_: base lighting, base RGB, RGB mode

**Locality**:
Where a feedback colour paints: both halves, one half, the half of the key
that caused it, or only the keys themselves. For replacement feedback the key
that caused it is the one that ended the word, the Backspace that undid the
replacement, or the key that ended leading, none when the leader timeout did.
_Avoid_: scope, target, region

## Talking to the keyboard

**Profile Wire**:
The protocol a client speaks to the keyboard over Raw HID to read, edit, save
and apply the profile.
_Avoid_: VIA protocol, HID protocol, live protocol

**Split protocol**:
How the halves copy the profile, the working copy and runtime state to each
other over the cable.
_Avoid_: split sync (for the protocol), transport, mirror
