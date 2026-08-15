#!/usr/bin/env python3
"""Report and enforce the target macro-slot storage and total-BSS budgets."""

import argparse
import pathlib
import subprocess
import sys


REQUIRED_SYMBOLS = (
    "hardcoded_macro_slots",
    "via_macro_slots",
    "macro_slot_active_ir",
)
BOOKKEEPING_SYMBOLS = (
    "macro_slot_active_metadata",
    "macro_slot_active_stale",
)
LAYOUT_SYMBOLS = ("__bss_base__", "__bss_end__", "__heap_base__", "__heap_end__")


def canonical_symbol(name):
    return name.split(".lto_priv.", 1)[0]


def parse_nm_symbols(output):
    symbols = {}
    tracked = set(REQUIRED_SYMBOLS + BOOKKEEPING_SYMBOLS)
    for line in output.splitlines():
        fields = line.split()
        if len(fields) < 4 or fields[2] not in "BbDd":
            continue
        name = canonical_symbol(fields[3])
        if name not in tracked:
            continue
        if name in symbols:
            raise ValueError("duplicate tracked symbol: {}".format(name))
        symbols[name] = int(fields[1], 16)

    missing = [name for name in REQUIRED_SYMBOLS if name not in symbols]
    if missing:
        raise ValueError("missing required symbol(s): {}".format(", ".join(missing)))
    return symbols


def parse_size_bss(output):
    for line in output.splitlines():
        fields = line.split()
        if len(fields) >= 4 and all(field.isdigit() for field in fields[:4]):
            return int(fields[2])
    raise ValueError("could not parse GNU size output")


def parse_layout_symbols(output):
    symbols = {}
    for line in output.splitlines():
        fields = line.split()
        if len(fields) < 3 or fields[-1] not in LAYOUT_SYMBOLS:
            continue
        symbols[fields[-1]] = int(fields[0], 16)
    missing = [name for name in LAYOUT_SYMBOLS if name not in symbols]
    if missing:
        raise ValueError("missing layout symbol(s): {}".format(", ".join(missing)))
    return symbols


def run(command):
    return subprocess.run(command, check=True, text=True, capture_output=True).stdout


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", required=True, type=pathlib.Path)
    parser.add_argument("--nm", default="arm-none-eabi-nm")
    parser.add_argument("--size", default="arm-none-eabi-size")
    parser.add_argument("--baseline-macro-storage", type=int, default=41440)
    parser.add_argument("--min-reclaimed", type=int, default=32768)
    parser.add_argument("--max-macro-storage", type=int, default=8192)
    parser.add_argument("--max-static-bss", type=int, default=26000)
    parser.add_argument("--min-heap", type=int, default=204800)
    args = parser.parse_args(argv)

    if not args.elf.is_file():
        parser.error("ELF does not exist: {}".format(args.elf))

    try:
        symbols = parse_nm_symbols(run([args.nm, "-S", "--size-sort", str(args.elf)]))
        layout = parse_layout_symbols(run([args.nm, "-S", str(args.elf)]))
        bss = parse_size_bss(run([args.size, str(args.elf)]))
        toolchain = run([args.nm, "--version"]).splitlines()[0]
    except (OSError, subprocess.CalledProcessError, ValueError) as error:
        print("macro storage budget: FAIL ({})".format(error), file=sys.stderr)
        return 1

    macro_storage = sum(symbols.values())
    reclaimed = args.baseline_macro_storage - macro_storage
    static_bss = layout["__bss_end__"] - layout["__bss_base__"]
    heap = layout["__heap_end__"] - layout["__heap_base__"]
    failures = []
    if macro_storage > args.max_macro_storage:
        failures.append("named macro storage {} exceeds {} B".format(macro_storage, args.max_macro_storage))
    if reclaimed < args.min_reclaimed:
        failures.append("reclaimed {} is below {} B".format(reclaimed, args.min_reclaimed))
    if static_bss > args.max_static_bss:
        failures.append("static BSS {} exceeds {} B".format(static_bss, args.max_static_bss))
    if heap < args.min_heap:
        failures.append("linker heap {} is below {} B".format(heap, args.min_heap))

    print("macro storage budget: {}".format("FAIL" if failures else "PASS"))
    print("ELF: {}".format(args.elf))
    print("toolchain: {}".format(toolchain))
    for name in REQUIRED_SYMBOLS + BOOKKEEPING_SYMBOLS:
        if name in symbols:
            print("{}: {} B".format(name, symbols[name]))
    print("named macro storage: {} B (limit {} B)".format(macro_storage, args.max_macro_storage))
    print("reclaimed from {} B baseline: {} B (minimum {} B)".format(args.baseline_macro_storage, reclaimed, args.min_reclaimed))
    print("static BSS span: {} B (limit {} B)".format(static_bss, args.max_static_bss))
    print("linker heap: {} B (minimum {} B)".format(heap, args.min_heap))
    print("ELF BSS including linker-reserved heap: {} B (informational)".format(bss))
    for failure in failures:
        print("error: {}".format(failure), file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
