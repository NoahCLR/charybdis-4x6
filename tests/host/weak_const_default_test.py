#!/usr/bin/env python3
"""A weak const object is never read in the file that defines it.

GCC folds a weak const object's own initializer into the code of the file that
defines it, so a strong definition elsewhere (an authored table) is ignored
there. The host compilers the tests use do not, so no behavioural host test
sees it: the flashed firmware lost Noah's saved LED group this way while every
host test passed (keyboard check, 2026-10-19). Weak defaults therefore live in
files that only define them, such as users/noah/lib/rgb/core/rgb_config_defaults.c,
and their readers see an extern declaration.

Usage: weak_const_default_test.py [root]
"""

import re
import sys
from pathlib import Path

DEFINITION = re.compile(
    r"__attribute__\s*\(\(\s*weak\s*\)\)\s*(?:static\s+)?const\b[^;{}()]*?\b([A-Za-z_]\w*)\s*(?:\[[^\]]*\]\s*)?=",
)
COMMENT = re.compile(r"//[^\n]*|/\*.*?\*/", re.S)


def problems(path: Path) -> list[str]:
    source = COMMENT.sub("", path.read_text(errors="replace"))
    found = []
    for match in DEFINITION.finditer(source):
        name = match.group(1)
        rest = source[: match.start()] + source[match.end():]
        if re.search(rf"\b{re.escape(name)}\b", rest):
            line = source.count("\n", 0, match.start()) + 1
            found.append(f"{path}:{line}: weak const `{name}` is read in the file that defines it")
    return found


def main() -> None:
    root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[2]
    files = sorted(p for base in ("users", "keyboards") for p in (root / base).rglob("*.c"))
    found = [problem for path in files for problem in problems(path)]
    if found:
        print("weak const defaults must live apart from their readers:", *found, sep="\n  ", file=sys.stderr)
        sys.exit(1)
    print(f"weak const default tests passed ({len(files)} files)")


if __name__ == "__main__":
    main()
