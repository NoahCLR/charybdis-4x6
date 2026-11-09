#!/usr/bin/env python3
"""Build what the autocorrect list domain (0x80) derives from other sources.

The word characters and their case folding come from the host layouts: every
letter and digit a layout types, and what each folds to. This tool writes them
as the firmware's C table and, with golden vectors of the byte format, as the
fixture tests/fixtures/autocorrect_domain_v1.json. It also encodes a keymap's
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
FIXTURE = ROOT / "tests" / "fixtures" / "autocorrect_domain_v1.json"
TABLE = ROOT / "users" / "noah" / "lib" / "profile" / "schema" / "profile_autocorrect_words_v1.c"
AUTHORED_LIST = "autocorrect.txt"
AUTHORED_HEADER = "authored_autocorrect_list.h"

VERSION = 1
MAX_CORRECTIONS = 1024
MAX_TYPOS = 1024
MAX_TEXT = 32
MAX_TEXT_BYTES = 20480
HEADER_SIZE, CORRECTION_ROW_SIZE, TYPO_ROW_SIZE, PART_ROW_SIZE = 8, 6, 6, 2
MAX_SIZE = HEADER_SIZE + CORRECTION_ROW_SIZE * MAX_CORRECTIONS + (TYPO_ROW_SIZE + PART_ROW_SIZE) * MAX_TYPOS + MAX_TEXT_BYTES
MATCHES = ("whole word", "start of word", "end of word", "anywhere")
WHOLE_WORD, START, END, ANYWHERE = range(4)
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

def check_text(text: str, what: str) -> bytes:
    encoded = text.encode("utf-8")
    if not encoded:
        raise ListError(f"an empty {what}")
    if len(encoded) > MAX_TEXT:
        raise ListError(f"{what} {text!r} is over {MAX_TEXT} bytes")
    if any(ord(char) < 0x20 or 0x7F <= ord(char) < 0xA0 for char in text):
        raise ListError(f"{what} {text!r} holds a control character")
    return encoded


def check_typo(typo: str) -> bytes:
    encoded = check_text(typo, "typo")
    for index, char in enumerate(typo):
        if char == APOSTROPHE and 0 < index < len(typo) - 1:
            continue
        if char not in WORDS:
            raise ListError(f"typo {typo!r} holds {char!r}, which is not a word character")
        if WORDS[char] != char:
            raise ListError(f"typo {typo!r} is not case-folded")
    return encoded


def pack(corrections: list[tuple[int, int, int, int]], typos: list[tuple[int, int, int, int]], part: list[int], text: bytes, counts: tuple[int, int, int, int] | None = None) -> bytes:
    """The domain's bytes from its rows as given, with no checks: corrections
    are (text offset, length, reserved, first typo), typos (text offset,
    length, match, correction)."""
    header = counts or (len(corrections), len(typos), len(part), len(text))
    out = struct.pack("<HHHH", *header)
    out += b"".join(struct.pack("<HBBH", *row) for row in corrections)
    out += b"".join(struct.pack("<HBBH", *row) for row in typos)
    out += b"".join(struct.pack("<H", index) for index in part)
    return out + text


def rows(entries: list[dict]) -> tuple[list, list, list, bytes]:
    """Canonical rows for a list of entries: {"correction": str, "typos":
    [{"typo": str, "match": one of MATCHES}]}."""
    by_correction: dict[bytes, list] = {}
    typo_owner: dict[bytes, tuple[int, bytes]] = {}
    for entry in entries:
        correction = check_text(entry["correction"], "correction")
        if correction in by_correction:
            raise ListError(f"correction {entry['correction']!r} is listed twice")
        if not entry["typos"]:
            raise ListError(f"correction {entry['correction']!r} has no typo")
        by_correction[correction] = entry["typos"]
        for item in entry["typos"]:
            typo = check_typo(item["typo"])
            if typo in typo_owner:
                raise ListError(f"typo {item['typo']!r} is listed twice")
            if typo == correction:
                raise ListError(f"typo {item['typo']!r} is its own correction")
            typo_owner[typo] = (MATCHES.index(item["match"]), correction)
    if len(by_correction) > MAX_CORRECTIONS:
        raise ListError(f"{len(by_correction)} corrections; the list holds {MAX_CORRECTIONS}")
    if len(typo_owner) > MAX_TYPOS:
        raise ListError(f"{len(typo_owner)} typos; the list holds {MAX_TYPOS}")
    correction_order = sorted(by_correction)
    typo_order = sorted(typo_owner)
    text, correction_rows, typo_rows, part = b"", [], [], []
    first_typo = {}
    for index, typo in enumerate(typo_order):
        first_typo.setdefault(typo_owner[typo][1], index)
    for correction in correction_order:
        correction_rows.append((len(text), len(correction), 0, first_typo[correction]))
        text += correction
    for index, typo in enumerate(typo_order):
        match, correction = typo_owner[typo]
        typo_rows.append((len(text), len(typo), match, correction_order.index(correction)))
        if match != WHOLE_WORD:
            part.append(index)
        text += typo
    if len(text) > MAX_TEXT_BYTES:
        raise ListError(f"{len(text)} bytes of text; the list holds {MAX_TEXT_BYTES}")
    return correction_rows, typo_rows, part, text


def encode(entries: list[dict]) -> bytes:
    """The one encoding of a list (order of entries and typos does not matter)."""
    return pack(*rows(entries))


# --- The authored list ---------------------------------------------------------

def parse_authored(source: str) -> list[dict]:
    """autocorrect.txt: `typo -> correction` lines in QMK's form. A colon
    before the typo ties it to the start of a word, one after it to the end,
    both to the whole word; with neither it matches anywhere in a word. `#`
    starts a comment. Typos are folded; lines naming one correction make one
    entry."""
    entries: dict[str, list] = {}
    for number, raw in enumerate(source.splitlines(), 1):
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        typo, arrow, correction = (part.strip() for part in line.partition("->"))
        if not arrow or not typo.strip(":") or not correction:
            raise ListError(f"line {number}: expected `typo -> correction`")
        match = (WHOLE_WORD if typo.endswith(":") else START) if typo.startswith(":") else (END if typo.endswith(":") else ANYWHERE)
        entries.setdefault(correction, []).append({"typo": fold(typo.strip(":")), "match": MATCHES[match]})
    return [{"correction": correction, "typos": typos} for correction, typos in entries.items()]


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

def entry(correction: str, *typos: str | tuple[str, int]) -> dict:
    return {"correction": correction, "typos": [{"typo": t, "match": MATCHES[WHOLE_WORD]} if isinstance(t, str) else {"typo": t[0], "match": MATCHES[t[1]]} for t in typos]}


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


def maximum_entries() -> list[dict]:
    """A list at every ceiling at once: 1,024 corrections with one typo each,
    every typo part of a word, one correction and one typo of 32 bytes, and
    the text exactly full."""

    def code(index: int, width: int) -> str:
        return "".join("abcdefghijklmnopqrstuvwxyz"[index // 26**place % 26] for place in reversed(range(width)))

    typos = ["t" + code(index, 7) for index in range(MAX_TYPOS - 1)] + ["z" + "é" * 15 + "z"]
    spare = MAX_TEXT_BYTES - sum(len(typo.encode()) for typo in typos) - MAX_TEXT
    corrections = ["Z" + "é" * 15 + "!"]
    for index in range(MAX_CORRECTIONS - 1):
        length = spare // (MAX_CORRECTIONS - 1 - index)
        corrections.append("C" + code(index, 3) + "-" * (length - 4))
        spare -= length
    entries = [{"correction": correction, "typos": [{"typo": typo, "match": MATCHES[START + index % 3]}]} for index, (correction, typo) in enumerate(zip(corrections, typos))]
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

    def raw(corrections: list[str], typos: list[tuple[str, int, int]], part: list[int] | None = None, first: list[int] | None = None) -> bytes:
        """Rows laid out canonically from texts in the order given, unchecked."""
        text, correction_rows, typo_rows = b"", [], []
        for index, correction in enumerate(corrections):
            encoded = correction if isinstance(correction, bytes) else correction.encode()
            lowest = next((i for i, typo in enumerate(typos) if typo[2] == index), 0)
            correction_rows.append((len(text), len(encoded), 0, first[index] if first else lowest))
            text += encoded
        for typo, match, correction in typos:
            encoded = typo if isinstance(typo, bytes) else typo.encode()
            typo_rows.append((len(text), len(encoded), match, correction))
            text += encoded
        derived = [i for i, typo in enumerate(typos) if typo[1] != WHOLE_WORD]
        return pack(correction_rows, typo_rows, derived if part is None else part, text)

    valid = encode(base)
    add("truncated-header", "Shorter than the eight-byte header.", "INVALID_LENGTH", None, None, valid[:7])
    add("trailing-byte", "A byte after the text the header counts.", "INVALID_LENGTH", None, None, valid + b"\0")
    add("truncated-text", "The text ends before the header's count.", "INVALID_LENGTH", None, None, valid[:-1])
    add("too-many-corrections", "A correction count over the ceiling.", "CAPACITY_EXCEEDED", None, None, pack(*rows(base), counts=(MAX_CORRECTIONS + 1, 3, 1, 16)))
    add("too-many-typos", "A typo count over the ceiling.", "CAPACITY_EXCEEDED", None, None, pack(*rows(base), counts=(2, MAX_TYPOS + 1, 1, 16)))
    add("too-much-text", "A text length over the ceiling.", "CAPACITY_EXCEEDED", None, None, pack(*rows(base), counts=(2, 3, 1, MAX_TEXT_BYTES + 1)))
    add("more-part-typos-than-typos", "More part-of-word typos than typos.", "CAPACITY_EXCEEDED", None, None, pack(*rows(base), counts=(2, 3, 4, 14)))
    add("typos-without-corrections", "Typos need a correction to name.", "INVALID_REFERENCE", None, None, raw([], [("teh", WHOLE_WORD, 0)]))
    add("correction-offset", "A correction's text does not begin where the one before it ended.", "INVALID_OFFSET", 0, 1, changed(lambda c, t, p, x: (c[:1] + [(c[1][0] + 1, *c[1][1:])], t, p, x)))
    add("typo-offset", "A typo's text does not begin where the one before it ended.", "INVALID_OFFSET", 1, 0, changed(lambda c, t, p, x: (c, [(t[0][0] - 1, *t[0][1:])] + t[1:], p, x)))
    add("empty-correction", "A correction of no bytes.", "INVALID_TEXT_LENGTH", 0, 0, raw(["", "the"], [("teh", WHOLE_WORD, 1)]))
    add("empty-typo", "A typo of no bytes.", "INVALID_TEXT_LENGTH", 1, 0, raw(["the"], [("", WHOLE_WORD, 0), ("teh", WHOLE_WORD, 0)]))
    add("long-correction", "A correction of 33 bytes.", "INVALID_TEXT_LENGTH", 0, 0, raw(["a" * 33], [("teh", WHOLE_WORD, 0)]))
    add("long-typo", "A typo of 33 bytes.", "INVALID_TEXT_LENGTH", 1, 0, raw(["the"], [("a" * 33, WHOLE_WORD, 0)]))
    add("correction-reserved", "A correction row's reserved byte is not zero.", "RESERVED", 0, 0, changed(lambda c, t, p, x: ([(c[0][0], c[0][1], 1, c[0][3])] + c[1:], t, p, x)))
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
    add("unknown-match", "A match value above anywhere.", "INVALID_MATCH", 1, 0, raw(["the"], [("teh", 4, 0)], part=[0]))
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
        "description": "Autocorrect list domain 0x80 version 1: golden vectors and the word-character table, for the firmware and its clients. Layout and rules: docs/spec/profile-wire-v1/autocorrect-domain.md. Payload, integers little-endian: header [corrections u16, typos u16, part-of-word typos u16, text bytes u16]; a 6-byte row per correction [text offset u16, length u8, 0, first typo u16]; a 6-byte row per typo [text offset u16, length u8, match u8, correction u16]; the part index, one u16 typo index per typo whose match is not whole word, ascending; then the text, every correction's bytes then every typo's, in row order, back to back. Corrections and typos each ascend strictly by their UTF-8 bytes. wordCharacters lists every character a typo may hold besides an apostrophe between two of them, as [character, folded]; a typo holds folded characters only, and a typed word is folded with this table before it is looked up. Error codes are the firmware's NOAH_PROFILE_AUTOCORRECT_V1_* names, with the table (0 corrections, 1 typos, 2 part index) and row they name. Test evidence, never a migration source.",
        "generator": "tools/autocorrect/build.py",
        "version": VERSION,
        "ceilings": {"corrections": MAX_CORRECTIONS, "typos": MAX_TYPOS, "bytesPerText": MAX_TEXT, "textBytes": MAX_TEXT_BYTES, "domainBytes": MAX_SIZE},
        "matches": list(MATCHES),
        "apostrophe": APOSTROPHE,
        "wordCharacters": [[char, folded] for char, folded in WORDS.items()],
        "valid": [
            {"name": "empty", "description": "No entries: the header alone.", "entries": [], "hex": encode([]).hex()},
            {"name": "one", "description": "One correction with one whole-word typo.", "entries": [entry("the", "teh")], "hex": encode([entry("the", "teh")]).hex()},
            {"name": "small", "description": "Several typos for one correction, every match, a capital in a correction, an apostrophe, a space in a correction and letters outside ASCII.", "entries": SMALL, "hex": encode(SMALL).hex()},
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
