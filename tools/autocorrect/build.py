#!/usr/bin/env python3
"""Build what the autocorrect list domain (0x80) derives from other sources.

The word characters and their case folding come from the host layouts: every
letter and digit a layout types, and what each folds to. This tool writes them
as the firmware's C table and, with golden vectors of the byte format, as the
fixture tests/fixtures/autocorrect_domain_v2.json. It also encodes a keymap's
authored list, autocorrect.txt, into authored_autocorrect_list.h, the compiled
defaults' domain. See docs/spec/profile-wire-v1/autocorrect-domain.md.

  python3 tools/autocorrect/build.py --write            # fixture and C table
  python3 tools/autocorrect/build.py --check            # fail if either is stale
  python3 tools/autocorrect/build.py --write-authored KEYMAP_DIR
  python3 tools/autocorrect/build.py --check-authored KEYMAP_DIR
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
import unicodedata
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HOST_LAYOUTS = ROOT / "tests" / "fixtures" / "host_layouts_v1.json"
FIXTURE = ROOT / "tests" / "fixtures" / "autocorrect_domain_v2.json"
TABLE = ROOT / "users" / "noah" / "lib" / "profile" / "schema" / "profile_autocorrect_words_v1.c"
AUTHORED_LIST = "autocorrect.txt"
AUTHORED_HEADER = "authored_autocorrect_list.h"

VERSION = 2
MAX_CORRECTIONS = 1024
MAX_TYPOS = 1024
MAX_TEXT = 32  # bytes in one typo
MAX_CORRECTION = 128  # bytes in one text correction
MACRO_SLOTS = 128
KIND_TEXT, KIND_MACRO = 0, 1
MAX_TEXT_BYTES = 20480
HEADER_SIZE, CORRECTION_ROW_SIZE, TYPO_ROW_SIZE, INDEX_ROW_SIZE = 10, 6, 6, 2
# A part-of-word typo of a correction that fires both ways is in both
# indexes, so each may hold a row for every typo.
MAX_SIZE = HEADER_SIZE + CORRECTION_ROW_SIZE * MAX_CORRECTIONS + (TYPO_ROW_SIZE + 2 * INDEX_ROW_SIZE) * MAX_TYPOS + MAX_TEXT_BYTES
MATCHES = ("whole word", "start of word", "end of word", "anywhere")
WHOLE_WORD, START, END, ANYWHERE = range(4)
PART_OF_WORD = (START, END, ANYWHERE)
# How a correction's typos fire (D-F71), stored above its kind in bits 1-2 of
# its row's kind byte.
TRIGGERS = ("as a word ends", "after the leader", "both")
AS_TYPED, AFTER_LEADER, BOTH = range(3)
TRIGGER_SHIFT = 1
APOSTROPHE = "'"


class ListError(ValueError):
    """An autocorrect list the domain cannot hold."""


def fail(message: str) -> None:
    print(f"autocorrect: {message}", file=sys.stderr)
    sys.exit(1)


# --- Word characters ---------------------------------------------------------

def simple_lower(char: str) -> str:
    lowered = char.lower()
    return lowered if len(lowered) == 1 else char


def word_characters() -> dict[str, str]:
    """Every letter and digit a host layout types, and each one's fold, with
    the folds themselves: character -> folded character."""
    layouts = json.loads(HOST_LAYOUTS.read_text(encoding="utf-8"))["layouts"]
    typed = {char for layout in layouts for char in layout["chars"]}
    words = {char for char in typed if unicodedata.category(char) in ("Lu", "Ll", "Lo", "Nd")}
    table = {char: simple_lower(char) for char in words}
    table.update({fold: fold for fold in list(table.values())})
    for char, fold in table.items():
        if table[fold] != fold:
            fail(f"folding {char!r} twice changes it again")
        if ord(char) > 0xFFFF:
            fail(f"{char!r} does not fit the 16-bit word table")
    # A small letter's capital is the one character that folds to it; the
    # case rule of docs/spec/autocorrect-v1.md depends on there being one.
    capitals: dict[str, str] = {}
    for char, fold in table.items():
        if fold != char and capitals.setdefault(fold, char) != char:
            fail(f"{fold!r} has two capitals, {capitals[fold]!r} and {char!r}")
    return dict(sorted(table.items()))


WORDS = word_characters()


def fold(text: str) -> str:
    return "".join(WORDS.get(char, char) for char in text)


# --- Encoding ----------------------------------------------------------------

def check_text(text: str, what: str, most: int = MAX_CORRECTION) -> bytes:
    encoded = text.encode("utf-8")
    if not encoded:
        raise ListError(f"an empty {what}")
    if len(encoded) > most:
        raise ListError(f"{what} {text!r} is over {most} bytes")
    if any(ord(char) < 0x20 or 0x7F <= ord(char) < 0xA0 for char in text):
        raise ListError(f"{what} {text!r} holds a control character")
    return encoded


def check_typo(typo: str) -> bytes:
    """A typo is folded word characters, with apostrophes only between them,
    however its correction fires."""
    encoded = check_text(typo, "typo", MAX_TEXT)
    for index, char in enumerate(typo):
        if char == APOSTROPHE and 0 < index < len(typo) - 1:
            continue
        if char not in WORDS:
            raise ListError(f"typo {typo!r} holds {char!r}, which is not a word character")
        if WORDS[char] != char:
            raise ListError(f"typo {typo!r} is not case-folded")
    return encoded


def pack(corrections: list[tuple[int, int, int, int]], typos: list[tuple[int, int, int, int]], part: list[int], leader: list[int], text: bytes, counts: tuple[int, int, int, int, int] | None = None) -> bytes:
    """The domain's bytes from its rows as given, with no checks: corrections
    are (text offset or macro slot, length, kind, first typo), typos (text
    offset, length, match, correction)."""
    header = counts or (len(corrections), len(typos), len(part), len(leader), len(text))
    out = struct.pack("<HHHHH", *header)
    out += b"".join(struct.pack("<HBBH", *row) for row in corrections)
    out += b"".join(struct.pack("<HBBH", *row) for row in typos)
    out += b"".join(struct.pack("<H", index) for index in part)
    out += b"".join(struct.pack("<H", index) for index in leader)
    return out + text


def macro_slot(entry: dict) -> int:
    slot = entry["macro"]
    if not isinstance(slot, int) or isinstance(slot, bool) or not 0 <= slot < MACRO_SLOTS:
        raise ListError(f"macro {slot!r} is not one of the {MACRO_SLOTS} macro slots")
    return slot


def trigger_of(entry: dict) -> int:
    trigger = entry.get("trigger", TRIGGERS[AS_TYPED])
    if trigger not in TRIGGERS:
        raise ListError(f"trigger {trigger!r} is not one of {', '.join(TRIGGERS)}")
    return TRIGGERS.index(trigger)


def rows(entries: list[dict]) -> tuple[list, list, list, list, bytes]:
    """Canonical rows for a list of entries. An entry is a text correction,
    {"correction": str, "trigger": one of TRIGGERS, "typos": [{"typo": str,
    "match": one of MATCHES}]}, or a macro one, {"macro": slot, ...}, whose
    typos match the whole word. A trigger left out is "as a word ends". Text
    corrections come first, by their bytes; macro ones by slot."""
    by_correction: dict[tuple, list] = {}
    triggers: dict[tuple, int] = {}
    typo_owner: dict[bytes, tuple[int, tuple]] = {}
    for entry in entries:
        if ("macro" in entry) == ("correction" in entry):
            raise ListError("an entry has a correction or a macro, one of them")
        plays = "macro" in entry
        key = (KIND_MACRO, macro_slot(entry)) if plays else (KIND_TEXT, check_text(entry["correction"], "correction"))
        shown = f"macro {entry['macro']}" if plays else repr(entry["correction"])
        if key in by_correction:
            raise ListError(f"correction {shown} is listed twice")
        if not entry["typos"]:
            raise ListError(f"correction {shown} has no typo")
        by_correction[key] = entry["typos"]
        triggers[key] = trigger_of(entry)
        for item in entry["typos"]:
            if item["match"] not in MATCHES:
                raise ListError(f"match {item['match']!r} is not one of {', '.join(MATCHES)}")
            match = MATCHES.index(item["match"])
            typo = check_typo(item["typo"])
            if typo in typo_owner:
                raise ListError(f"typo {item['typo']!r} is listed twice")
            if not plays and typo == key[1]:
                raise ListError(f"typo {item['typo']!r} is its own correction")
            if plays and match in PART_OF_WORD:
                raise ListError(f"typo {item['typo']!r} plays a macro, so it matches the whole word")
            typo_owner[typo] = (match, key)
    if len(by_correction) > MAX_CORRECTIONS:
        raise ListError(f"{len(by_correction)} corrections; the list holds {MAX_CORRECTIONS}")
    if len(typo_owner) > MAX_TYPOS:
        raise ListError(f"{len(typo_owner)} typos; the list holds {MAX_TYPOS}")
    correction_order = sorted(by_correction)
    typo_order = sorted(typo_owner)
    text, correction_rows, typo_rows, part, leader = b"", [], [], [], []
    first_typo = {}
    for index, typo in enumerate(typo_order):
        first_typo.setdefault(typo_owner[typo][1], index)
    for kind, value in correction_order:
        byte = kind | triggers[(kind, value)] << TRIGGER_SHIFT
        if kind == KIND_MACRO:
            correction_rows.append((value, 0, byte, first_typo[(kind, value)]))
            continue
        correction_rows.append((len(text), len(value), byte, first_typo[(kind, value)]))
        text += value
    for index, typo in enumerate(typo_order):
        match, correction = typo_owner[typo]
        typo_rows.append((len(text), len(typo), match, correction_order.index(correction)))
        # The part index serves a word end, the leader index the leader.
        if match in PART_OF_WORD and triggers[correction] != AFTER_LEADER:
            part.append(index)
        if triggers[correction] != AS_TYPED:
            leader.append(index)
        text += typo
    if len(text) > MAX_TEXT_BYTES:
        raise ListError(f"{len(text)} bytes of text; the list holds {MAX_TEXT_BYTES}")
    return correction_rows, typo_rows, part, leader, text


def encode(entries: list[dict]) -> bytes:
    """The one encoding of a list (order of entries and typos does not matter)."""
    return pack(*rows(entries))


# --- The authored list ---------------------------------------------------------

# The words a line may begin with to say how its replacement fires (D-F71);
# a line without one fires as a word ends.
AUTHORED_TRIGGERS = {"leader": AFTER_LEADER, "both": BOTH}


def parse_authored(source: str) -> list[dict]:
    """autocorrect.txt: `typo -> correction` lines in QMK's form. A colon
    before the typo ties it to the start of a word, one after it to the end,
    both to the whole word; with neither it matches anywhere in a word. A
    line `:typo: => N` plays macro slot N instead of typing text. A line that
    begins `leader ` fires only after the Leader key, its typo typed whole,
    so it takes no colons; one that begins `both ` fires as a word ends and
    after the Leader key (D-F71); `leader` or `both` straight before the
    arrow is the typo itself, as typed. Lines naming one replacement fire one
    way.
    `#` starts a comment. Typos are folded; lines naming one correction make
    one entry."""
    entries: dict[str, list] = {}
    macros: dict[int, list] = {}
    fires: dict[tuple, int] = {}
    for number, raw in enumerate(source.splitlines(), 1):
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        word, _, rest = line.partition(" ")
        # A trigger word straight before the arrow is the typo, not a trigger.
        trigger = AS_TYPED if rest.lstrip().startswith(("->", "=>")) else AUTHORED_TRIGGERS.get(word, AS_TYPED)
        if trigger != AS_TYPED:
            line = rest.strip()
        plays = "->" not in line and "=>" in line
        typo, arrow, correction = (part.strip() for part in line.partition("=>" if plays else "->"))
        form = typo.strip(":")
        if not arrow or not form or not correction:
            raise ListError(f"line {number}: expected `typo -> correction` or `:typo: => macro slot`, after `leader` or `both` or alone")
        if trigger == AFTER_LEADER:
            if ":" in typo:
                raise ListError(f"line {number}: after the Leader key alone a typo is typed whole, so it takes no colons")
            match = WHOLE_WORD
        else:
            match = (WHOLE_WORD if typo.endswith(":") else START) if typo.startswith(":") else (END if typo.endswith(":") else ANYWHERE)
        item = {"typo": fold(form), "match": MATCHES[match]}
        if plays:
            if not correction.isdecimal():
                raise ListError(f"line {number}: `=>` is followed by a macro slot, 0 to {MACRO_SLOTS - 1}")
            key = ("macro", int(correction))
            macros.setdefault(int(correction), []).append(item)
        else:
            key = ("correction", correction)
            entries.setdefault(correction, []).append(item)
        if fires.setdefault(key, trigger) != trigger:
            raise ListError(f"line {number}: {correction!r} fires {TRIGGERS[fires[key]]} on another line")
    return [{"correction": correction, "trigger": TRIGGERS[fires[("correction", correction)]], "typos": typos} for correction, typos in entries.items()] + [{"macro": slot, "trigger": TRIGGERS[fires[("macro", slot)]], "typos": typos} for slot, typos in macros.items()]


def render_authored(keymap: Path) -> str:
    try:
        data = encode(parse_authored((keymap / AUTHORED_LIST).read_text(encoding="utf-8")))
    except ListError as error:
        fail(f"{keymap / AUTHORED_LIST}: {error}")
    lines = [
        "// Generated by python3 tools/autocorrect/build.py --write-authored <keymap folder>; do not edit.",
        f"// {AUTHORED_LIST} as profile domain 0x80: the compiled defaults' autocorrect",
        "// list. Real-profile validation refuses a stale copy.",
        "#pragma once",
        "",
        "#define NOAH_AUTHORED_AUTOCORRECT_LIST",
    ]
    text = "\n".join(lines)
    for index, byte in enumerate(data):
        text += (" \\\n    " if index % 16 == 0 else " ") + f"0x{byte:02x}" + ("," if index + 1 < len(data) else "")
    return text + "\n"


# --- Golden vectors ------------------------------------------------------------

def entry(correction: str | int, *typos: str | tuple[str, int], trigger: int = AS_TYPED) -> dict:
    """An entry: a text correction, or for a number the macro slot it plays."""
    fires = {} if trigger == AS_TYPED else {"trigger": TRIGGERS[trigger]}
    return {"macro" if isinstance(correction, int) else "correction": correction, **fires, "typos": [{"typo": t, "match": MATCHES[WHOLE_WORD]} if isinstance(t, str) else {"typo": t[0], "match": MATCHES[t[1]]} for t in typos]}


SMALL = [
    entry("the", "teh", "hte"),
    entry("because", "becuase", "becasue", "beacuse"),
    entry("thing", ("tihng", ANYWHERE)),
    entry("iPhone", "iphone"),
    entry("doesn't", "dosen't", "doesnt"),
    entry("a lot", "alot"),
    entry("über", "ueber"),
    entry("café", "cafe", "cafè"),
    entry("un", ("nu", START)),
    entry("ment", ("mnet", END)),
]


# Corrections that fire after the leader: alone, or both ways, one typo the
# start of another, a part-of-word typo of one that fires both ways (in both
# indexes), letters outside ASCII, a macro, and one that fires as a word ends
# beside them.
LEADER = [
    entry("+31 6 1234 5678", "pn", trigger=AFTER_LEADER),
    entry("Kind regards, Sam", "kr", trigger=AFTER_LEADER),
    entry("alpha", "abc", trigger=AFTER_LEADER),
    entry("delta", "abcd", trigger=AFTER_LEADER),
    entry("the", "teh", "hte", trigger=BOTH),
    entry("receive", ("recieve", START), trigger=BOTH),
    entry("café", "éé", trigger=AFTER_LEADER),
    entry("and", "adn"),
    entry(7, "sig", "sigg", trigger=AFTER_LEADER),
]


# A correction of 128 bytes: text with spaces, digits and punctuation.
LONG = [
    entry("Kind regards, Sam Example | +31 6 1234 5678 | sam@example.com | Example Street 12, 1234 AB Amsterdam, The Netherlands | KvK 0000", "sigoff"),
    entry("+31 6 1234 5678", "pnumber"),
    entry("the", "teh"),
]
assert len(LONG[0]["correction"].encode()) == MAX_CORRECTION
# Macro corrections beside text ones: they follow them, by slot.
MACROS = [
    entry(127, "sigfull"),
    entry("the", "teh"),
    entry(3, "addr", "adres"),
    entry("sam@example.com", "myemail"),
]


def maximum_entries() -> list[dict]:
    """A list at every ceiling at once: 1,024 corrections with one typo each,
    every correction firing both ways and every typo part of a word, so in
    both indexes, one correction of 128 bytes and one typo of 32, and the
    text exactly full."""

    def code(index: int, width: int) -> str:
        return "".join("abcdefghijklmnopqrstuvwxyz"[index // 26**place % 26] for place in reversed(range(width)))

    typos = ["t" + code(index, 7) for index in range(MAX_TYPOS - 1)] + ["z" + "é" * 15 + "z"]
    spare = MAX_TEXT_BYTES - sum(len(typo.encode()) for typo in typos) - MAX_CORRECTION
    corrections = ["Z" + "é" * 63 + "!"]
    for index in range(MAX_CORRECTIONS - 1):
        length = spare // (MAX_CORRECTIONS - 1 - index)
        corrections.append("C" + code(index, 3) + "-" * (length - 4))
        spare -= length
    entries = [{"correction": correction, "trigger": TRIGGERS[BOTH], "typos": [{"typo": typo, "match": MATCHES[START + index % 3]}]} for index, (correction, typo) in enumerate(zip(corrections, typos))]
    assert len(encode(entries)) == MAX_SIZE
    return entries


def invalid_vectors() -> list[dict]:
    """One vector per refusal: the rows of a two-entry list, changed."""
    base = [entry("the", "teh"), entry("thing", ("tihng", ANYWHERE), "thnig")]
    vectors = []

    def add(name: str, description: str, code: str, table: int | None, row: int | None, data: bytes) -> None:
        vectors.append({"name": name, "description": description, "hex": data.hex(), "error": {"code": code, "table": table, "row": row}})

    def changed(change) -> bytes:
        return pack(*change(*rows(base)))

    def raw(corrections: list, typos: list[tuple[str, int, int]], part: list[int] | None = None, first: list[int] | None = None, leader: list[int] | None = None, fires: list[int] | None = None) -> bytes:
        """Rows laid out canonically from texts in the order given, unchecked.
        A correction that is a number is a macro slot; a tuple is a row's
        (slot, length, kind byte) as given. `fires` is each correction's
        trigger, as a word ends where it is left out."""
        triggers = fires or [AS_TYPED] * len(corrections)
        text, correction_rows, typo_rows = b"", [], []
        for index, correction in enumerate(corrections):
            lowest = first[index] if first else next((i for i, typo in enumerate(typos) if typo[2] == index), 0)
            if isinstance(correction, int):
                correction_rows.append((correction, 0, KIND_MACRO | triggers[index] << TRIGGER_SHIFT, lowest))
                continue
            if isinstance(correction, tuple):
                correction_rows.append((*correction, lowest))
                continue
            encoded = correction if isinstance(correction, bytes) else correction.encode()
            correction_rows.append((len(text), len(encoded), KIND_TEXT | triggers[index] << TRIGGER_SHIFT, lowest))
            text += encoded
        for typo, match, correction in typos:
            encoded = typo if isinstance(typo, bytes) else typo.encode()
            typo_rows.append((len(text), len(encoded), match, correction))
            text += encoded
        fired = lambda typo: triggers[typo[2]] if 0 <= typo[2] < len(triggers) else AS_TYPED
        derived = [i for i, typo in enumerate(typos) if typo[1] in PART_OF_WORD and fired(typo) != AFTER_LEADER]
        leading = [i for i, typo in enumerate(typos) if fired(typo) != AS_TYPED]
        return pack(correction_rows, typo_rows, derived if part is None else part, leading if leader is None else leader, text)

    valid = encode(base)
    add("truncated-header", "Shorter than the ten-byte header.", "INVALID_LENGTH", None, None, valid[:9])
    add("trailing-byte", "A byte after the text the header counts.", "INVALID_LENGTH", None, None, valid + b"\0")
    add("truncated-text", "The text ends before the header's count.", "INVALID_LENGTH", None, None, valid[:-1])
    add("too-many-corrections", "A correction count over the ceiling.", "CAPACITY_EXCEEDED", None, None, pack(*rows(base), counts=(MAX_CORRECTIONS + 1, 3, 1, 0, 16)))
    add("too-many-typos", "A typo count over the ceiling.", "CAPACITY_EXCEEDED", None, None, pack(*rows(base), counts=(2, MAX_TYPOS + 1, 1, 0, 16)))
    add("too-much-text", "A text length over the ceiling.", "CAPACITY_EXCEEDED", None, None, pack(*rows(base), counts=(2, 3, 1, 0, MAX_TEXT_BYTES + 1)))
    add("more-part-typos-than-typos", "More part-of-word typos than typos.", "CAPACITY_EXCEEDED", None, None, pack(*rows(base), counts=(2, 3, 4, 0, 14)))
    add("more-leader-typos-than-typos", "More leader typos than typos.", "CAPACITY_EXCEEDED", None, None, pack(*rows(base), counts=(2, 3, 1, 4, 14)))
    add("typos-without-corrections", "Typos need a correction to name.", "INVALID_REFERENCE", None, None, raw([], [("teh", WHOLE_WORD, 0)]))
    add("correction-offset", "A correction's text does not begin where the one before it ended.", "INVALID_OFFSET", 0, 1, changed(lambda c, t, p, l, x: (c[:1] + [(c[1][0] + 1, *c[1][1:])], t, p, l, x)))
    add("typo-offset", "A typo's text does not begin where the one before it ended.", "INVALID_OFFSET", 1, 0, changed(lambda c, t, p, l, x: (c, [(t[0][0] - 1, *t[0][1:])] + t[1:], p, l, x)))
    add("empty-correction", "A correction of no bytes.", "INVALID_TEXT_LENGTH", 0, 0, raw(["", "the"], [("teh", WHOLE_WORD, 1)]))
    add("empty-typo", "A typo of no bytes.", "INVALID_TEXT_LENGTH", 1, 0, raw(["the"], [("", WHOLE_WORD, 0), ("teh", WHOLE_WORD, 0)]))
    add("long-correction", "A correction of 129 bytes.", "INVALID_TEXT_LENGTH", 0, 0, raw(["a" * 129], [("teh", WHOLE_WORD, 0)]))
    add("long-typo", "A typo of 33 bytes.", "INVALID_TEXT_LENGTH", 1, 0, raw(["the"], [("a" * 33, WHOLE_WORD, 0)]))
    add("correction-trigger", "A correction that fires no way there is: trigger 3.", "INVALID_KIND", 0, 0, changed(lambda c, t, p, l, x: ([(c[0][0], c[0][1], 3 << TRIGGER_SHIFT, c[0][3])] + c[1:], t, p, l, x)))
    add("correction-kind-bit", "A correction's kind byte with a bit above the trigger set.", "INVALID_KIND", 0, 0, changed(lambda c, t, p, l, x: ([(c[0][0], c[0][1], 0x08, c[0][3])] + c[1:], t, p, l, x)))
    add("macro-with-length", "A macro correction with a text length.", "INVALID_TEXT_LENGTH", 0, 1, raw(["the", (5, 3, KIND_MACRO)], [("sig", WHOLE_WORD, 1), ("teh", WHOLE_WORD, 0)]))
    add("macro-slot-out-of-range", "A macro correction names a slot past the macro slots.", "INVALID_MACRO", 0, 1, raw(["the", MACRO_SLOTS], [("sig", WHOLE_WORD, 1), ("teh", WHOLE_WORD, 0)]))
    add("macro-order", "Macro corrections out of slot order.", "ORDER", 0, 1, raw([7, 3], [("addr", WHOLE_WORD, 1), ("sig", WHOLE_WORD, 0)]))
    add("duplicate-macro", "One macro slot stored twice.", "ORDER", 0, 1, raw([7, 7], [("addr", WHOLE_WORD, 1), ("sig", WHOLE_WORD, 0)]))
    add("text-after-macro", "A text correction after a macro one.", "ORDER", 0, 1, raw([7, "the"], [("sig", WHOLE_WORD, 0), ("teh", WHOLE_WORD, 1)]))
    add("macro-part-of-word-typo", "A macro correction's typo that matches part of a word.", "INVALID_MATCH", 1, 0, raw([7], [("sig", START, 0)]))
    add("correction-order", "Corrections out of order.", "ORDER", 0, 1, raw(["thing", "the"], [("teh", WHOLE_WORD, 1), ("thnig", WHOLE_WORD, 0)]))
    add("duplicate-correction", "One correction stored twice.", "ORDER", 0, 1, raw(["the", "the"], [("hte", WHOLE_WORD, 0), ("teh", WHOLE_WORD, 1)]))
    add("typo-order", "Typos out of order.", "ORDER", 1, 1, raw(["the"], [("teh", WHOLE_WORD, 0), ("hte", WHOLE_WORD, 0)]))
    add("duplicate-typo", "One typo stored twice, for two corrections.", "ORDER", 1, 1, raw(["the", "then"], [("teh", WHOLE_WORD, 0), ("teh", ANYWHERE, 1)]))
    add("prefix-order", "A typo before the shorter typo it begins with.", "ORDER", 1, 1, raw(["the"], [("tehh", WHOLE_WORD, 0), ("teh", WHOLE_WORD, 0)]))
    add("correction-utf8", "A correction that is not UTF-8.", "INVALID_TEXT", 0, 0, raw([b"caf\xe9"], [("cafe", WHOLE_WORD, 0)]))
    add("correction-overlong-utf8", "A correction with an overlong UTF-8 form.", "INVALID_TEXT", 0, 0, raw([b"th\xc1\xa5"], [("teh", WHOLE_WORD, 0)]))
    add("correction-control", "A correction with a control character.", "INVALID_TEXT", 0, 0, raw(["th\te"], [("teh", WHOLE_WORD, 0)]))
    add("typo-utf8", "A typo cut off inside a UTF-8 character.", "INVALID_TEXT", 1, 0, raw(["café"], [(b"caf\xc3", WHOLE_WORD, 0)]))
    add("typo-space", "A typo with a space.", "NOT_A_WORD", 1, 0, raw(["a lot"], [("a lto", WHOLE_WORD, 0)]))
    add("typo-hyphen", "A typo with a hyphen.", "NOT_A_WORD", 1, 0, raw(["e-mail"], [("e-mial", WHOLE_WORD, 0)]))
    add("typo-leading-apostrophe", "A typo that begins with an apostrophe.", "NOT_A_WORD", 1, 0, raw(["the"], [("'teh", WHOLE_WORD, 0)]))
    add("typo-trailing-apostrophe", "A typo that ends with an apostrophe.", "NOT_A_WORD", 1, 0, raw(["the"], [("teh'", WHOLE_WORD, 0)]))
    add("typo-unknown-letter", "A typo with a letter no host layout types.", "NOT_A_WORD", 1, 0, raw(["the"], [("teж", WHOLE_WORD, 0)]))
    add("typo-capital", "A typo with a capital letter.", "NOT_FOLDED", 1, 0, raw(["the"], [("Teh", WHOLE_WORD, 0)]))
    add("typo-accented-capital", "A typo with an accented capital letter.", "NOT_FOLDED", 1, 0, raw(["über"], [("Üebr", WHOLE_WORD, 0)]))
    add("unknown-match", "A match value above anywhere: 4 is no match.", "INVALID_MATCH", 1, 0, raw(["the"], [("teh", 4, 0)], part=[0]))
    add("leader-space", "A typo with a space, of a correction that fires after the leader.", "NOT_A_WORD", 1, 0, raw(["Kind regards, Sam"], [("k r", WHOLE_WORD, 0)], fires=[AFTER_LEADER]))
    add("leader-capital", "A typo with a capital letter, of a correction that fires after the leader.", "NOT_FOLDED", 1, 0, raw(["+31 6 1234 5678"], [("Pn", WHOLE_WORD, 0)], fires=[AFTER_LEADER]))
    add("leader-index-missing", "A typo of a correction that fires after the leader the leader index leaves out.", "LEADER_INDEX", 1, 0, raw(["+31 6 1234 5678", "the"], [("pn", WHOLE_WORD, 0), ("teh", WHOLE_WORD, 1)], leader=[], fires=[AFTER_LEADER, AS_TYPED]))
    add("leader-index-both-missing", "A part-of-word typo of a correction that fires both ways, in the part index and left out of the leader index.", "LEADER_INDEX", 1, 0, raw(["receive"], [("recieve", START, 0)], leader=[], fires=[BOTH]))
    add("leader-index-word-end", "The leader index lists a typo of a correction that fires as a word ends.", "LEADER_INDEX", 3, 0, raw(["the"], [("teh", WHOLE_WORD, 0)], leader=[0]))
    add("leader-index-wrong-typo", "The leader index names another typo.", "LEADER_INDEX", 1, 0, raw(["+31 6 1234 5678", "the"], [("pn", WHOLE_WORD, 0), ("teh", WHOLE_WORD, 1)], leader=[1], fires=[AFTER_LEADER, AS_TYPED]))
    add("part-index-leader-only", "The part index lists a part-of-word typo of a correction that fires only after the leader.", "PART_INDEX", 2, 0, raw(["receive"], [("recieve", START, 0)], part=[0], fires=[AFTER_LEADER]))
    add("missing-correction", "A typo names a correction that does not exist.", "INVALID_REFERENCE", 1, 1, raw(["the"], [("hte", WHOLE_WORD, 0), ("teh", WHOLE_WORD, 1)]))
    add("first-typo-out-of-range", "A correction's first typo is past the typos.", "INVALID_REFERENCE", 0, 0, raw(["the"], [("teh", WHOLE_WORD, 0)], first=[1]))
    add("unused-correction", "A correction no typo names: its first typo names another.", "FIRST_TYPO", 0, 1, raw(["the", "thing"], [("teh", WHOLE_WORD, 0)], first=[0, 0]))
    add("first-typo-not-first", "A correction's first typo is not the first that names it.", "FIRST_TYPO", 1, 0, raw(["the"], [("hte", WHOLE_WORD, 0), ("teh", WHOLE_WORD, 0)], first=[1]))
    add("typo-is-its-correction", "A typo equal to its correction.", "TYPO_IS_CORRECTION", 1, 0, raw(["the"], [("the", WHOLE_WORD, 0)]))
    add("part-index-missing", "A part-of-word typo the part index leaves out.", "PART_INDEX", 1, 1, raw(["the", "thing"], [("teh", WHOLE_WORD, 0), ("tihng", ANYWHERE, 1)], part=[]))
    add("part-index-whole-word", "The part index lists a whole-word typo.", "PART_INDEX", 2, 0, raw(["the"], [("teh", WHOLE_WORD, 0)], part=[0]))
    add("part-index-wrong-typo", "The part index names another typo.", "PART_INDEX", 1, 1, raw(["the", "thing"], [("teh", WHOLE_WORD, 0), ("tihng", ANYWHERE, 1)], part=[0]))
    return vectors


def build_fixture() -> dict:
    return {
        "description": "Autocorrect list domain 0x80 version 2: golden vectors and the word-character table, for the firmware and its clients. Layout and rules: docs/spec/profile-wire-v1/autocorrect-domain.md. Payload, integers little-endian: header [corrections u16, typos u16, part-of-word typos u16, leader typos u16, text bytes u16]; a 6-byte row per correction [text offset u16, length u8, kind u8 (bit 0: 0 text, 1 macro; bits 1-2 the trigger: 0 as a word ends, 1 after the leader, 2 both; every other bit 0), first typo u16], a macro correction holding its macro slot in place of the offset and a length of 0; a 6-byte row per typo [text offset u16, length u8, match u8 (0 whole word, 1 start, 2 end, 3 anywhere), correction u16]; the part index, one u16 typo index per typo that matches part of a word (match 1 to 3) and whose correction fires as a word ends (trigger 0 or 2), ascending; the leader index, one u16 typo index per typo whose correction fires after the leader (trigger 1 or 2), ascending, so a part-of-word typo of a correction that fires both ways is in both; then the text, every text correction's bytes then every typo's, in row order, back to back. Text corrections come first and ascend strictly by their UTF-8 bytes, macro corrections follow and ascend strictly by slot, and typos ascend strictly by their bytes, so one text is one typo whatever its match. An entry is {correction, trigger, typos} for text or {macro, trigger, typos} for a macro slot, whose typos match the whole word; trigger is one of triggers. wordCharacters lists every character a typo may hold besides an apostrophe between two of them, as [character, folded]; a typo holds folded characters only, and a typed word, or what is typed after the leader, is folded with this table before it is looked up. After the leader a typo is its whole text, whatever its match. Error codes are the firmware's NOAH_PROFILE_AUTOCORRECT_V1_* names, with the table (0 corrections, 1 typos, 2 part index, 3 leader index) and row they name. Test evidence, never a migration source.",
        "generator": "tools/autocorrect/build.py",
        "version": VERSION,
        "ceilings": {"corrections": MAX_CORRECTIONS, "typos": MAX_TYPOS, "bytesPerTypo": MAX_TEXT, "bytesPerCorrection": MAX_CORRECTION, "macroSlots": MACRO_SLOTS, "textBytes": MAX_TEXT_BYTES, "domainBytes": MAX_SIZE},
        "matches": list(MATCHES),
        "triggers": list(TRIGGERS),
        "apostrophe": APOSTROPHE,
        "wordCharacters": [[char, folded] for char, folded in WORDS.items()],
        "valid": [
            {"name": "empty", "description": "No entries: the header alone.", "entries": [], "hex": encode([]).hex()},
            {"name": "one", "description": "One correction with one whole-word typo.", "entries": [entry("the", "teh")], "hex": encode([entry("the", "teh")]).hex()},
            {"name": "small", "description": "Several typos for one correction, every match, a capital in a correction, an apostrophe, a space in a correction and letters outside ASCII.", "entries": SMALL, "hex": encode(SMALL).hex()},
            {"name": "long", "description": "A correction of 128 bytes with spaces, digits and punctuation, beside short ones.", "entries": LONG, "hex": encode(LONG).hex()},
            {"name": "macros", "description": "Macro corrections after the text ones, by slot, each with whole-word typos.", "entries": MACROS, "hex": encode(MACROS).hex()},
            {"name": "leader", "description": "Corrections that fire after the leader alone or both ways: one typo the start of another, a part-of-word typo in both indexes, letters outside ASCII, a macro, and a correction that fires as a word ends beside them.", "entries": LEADER, "hex": encode(LEADER).hex()},
        ],
        "invalid": invalid_vectors(),
    }


def render_table() -> str:
    lines = [
        "// Generated by tools/autocorrect/build.py from tests/fixtures/host_layouts_v1.json.",
        "// Do not edit; see docs/spec/profile-wire-v1/autocorrect-domain.md.",
        "",
        '#include "profile_autocorrect_v1.h"',
        "",
        "// Every letter and digit a host layout types, with what it folds to, and",
        "// the folds themselves, ascending by character.",
        "const uint16_t noah_profile_autocorrect_v1_words[][2] = {",
    ]
    lines += [f"    {{0x{ord(char):04X}u, 0x{ord(folded):04X}u}}," for char, folded in WORDS.items()]
    lines += ["};", "", f"const uint16_t noah_profile_autocorrect_v1_word_count = {len(WORDS)}u;"]
    capitals = sorted((folded, char) for char, folded in WORDS.items() if folded != char)
    lines += [
        "",
        "// Every small letter that has a capital, with that capital: the one",
        "// character that folds to it. Ascending by small letter.",
        "const uint16_t noah_profile_autocorrect_v1_capitals[][2] = {",
    ]
    lines += [f"    {{0x{ord(small):04X}u, 0x{ord(capital):04X}u}}," for small, capital in capitals]
    lines += ["};", "", f"const uint16_t noah_profile_autocorrect_v1_capital_count = {len(capitals)}u;"]
    return "\n".join(lines) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--write", action="store_true")
    group.add_argument("--check", action="store_true")
    group.add_argument("--write-authored", type=Path, metavar="KEYMAP_DIR")
    group.add_argument("--check-authored", type=Path, metavar="KEYMAP_DIR")
    args = parser.parse_args()
    keymap = args.write_authored or args.check_authored
    if keymap:
        outputs = {keymap / AUTHORED_HEADER: render_authored(keymap)}
        remedy = f"python3 tools/autocorrect/build.py --write-authored {keymap}"
    else:
        outputs = {FIXTURE: json.dumps(build_fixture(), ensure_ascii=False, indent=1) + "\n", TABLE: render_table()}
        remedy = "python3 tools/autocorrect/build.py --write"
    if args.write or args.write_authored:
        for path, text in outputs.items():
            path.write_text(text, encoding="utf-8")
        return
    stale = [str(path) for path, text in outputs.items() if not path.exists() or path.read_text(encoding="utf-8") != text]
    if stale:
        fail(f"stale generated files: {', '.join(stale)}; run {remedy}")


if __name__ == "__main__":
    main()
