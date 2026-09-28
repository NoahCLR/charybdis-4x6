#!/usr/bin/env python3
"""Print the split transaction ids of the current build as a markdown table.

Capture files name split transactions by number only, and the numbering
follows the build's split features. Run after
`qmk compile --compiledb -kb bastardkb/charybdis/4x6 -km noah`, which writes
compile_commands.json to this repository's root; the script preprocesses
QMK's transactions.c with that build's flags and reads the enum.
"""

import json
import re
import shlex
import subprocess
import sys
from pathlib import Path

DATABASE = Path(__file__).resolve().parent.parent / "compile_commands.json"


def preprocess_command(entry):
    args = entry.get("arguments") or shlex.split(entry["command"])
    command, skip = [], False
    for arg in args:
        if skip:
            skip = False
            continue
        if arg in ("-o", "-MF", "-MT", "-MQ"):
            skip = True
            continue
        # The database repeats compiler built-ins as -D flags for clangd.
        if arg == "-c" or arg.startswith(("-M", "-Wa,", "-D__STDC")):
            continue
        command.append(arg)
    return command + ["-E", "-P"]


def main():
    entries = [e for e in json.loads(DATABASE.read_text()) if e["file"].endswith("split_common/transactions.c")]
    if not entries:
        sys.exit(f"{DATABASE} has no split transactions.c entry; run qmk compile --compiledb first")
    result = subprocess.run(preprocess_command(entries[0]), cwd=entries[0]["directory"], capture_output=True, text=True)
    if result.returncode:
        sys.exit(result.stderr)
    match = re.search(r"enum serial_transaction_id\s*\{(.*?)\}", result.stdout, re.S)
    if not match:
        sys.exit("serial_transaction_id enum not found")
    names = [name.strip() for name in match.group(1).split(",") if name.strip()]
    print("| Id | Transaction |\n| ---: | --- |")
    for index, name in enumerate(names[:-1]):
        print(f"| {index} | `{name}` |")
    print(f"\n{names[-1]} = {len(names) - 1}")


if __name__ == "__main__":
    main()
