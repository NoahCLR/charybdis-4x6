#!/usr/bin/env python3
"""Report and enforce firmware storage policies with RP2040 SRAM accounting."""

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
LAYOUT_SYMBOLS = (
    "__bss_base__",
    "__bss_end__",
    "__data_base__",
    "__data_end__",
    "__heap_base__",
    "__heap_end__",
    "__ram0_base__",
    "__ram0_end__",
    "__ram4_base__",
    "__ram4_end__",
    "__ram4_free__",
    "__ram5_base__",
    "__ram5_end__",
    "__ram5_free__",
    "__ram7_base__",
    "__ram7_end__",
    "__ram7_free__",
)

RP2040_SRAM0_BYTES = 256 * 1024
RP2040_SRAM4_BYTES = 4 * 1024
RP2040_SRAM5_BYTES = 4 * 1024
RP2040_PHYSICAL_SRAM_BYTES = RP2040_SRAM0_BYTES + RP2040_SRAM4_BYTES + RP2040_SRAM5_BYTES


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


def checked_span(name, start, end):
    if end < start:
        raise ValueError("{} end precedes its start".format(name))
    return end - start


def calculate_memory_accounting(layout):
    """Return non-overlapping RP2040 SRAM accounting from linker symbols.

    ram7 is the 256-byte boot window at the end of physical SRAM5, not an
    additional bank. Keeping it separate prevents the overlapping linker
    region from being counted as extra physical RAM.
    """

    ram0_capacity = checked_span("SRAM0-3", layout["__ram0_base__"], layout["__ram0_end__"])
    ram4_capacity = checked_span("SRAM4", layout["__ram4_base__"], layout["__ram4_end__"])
    ram5_capacity = checked_span("SRAM5", layout["__ram5_base__"], layout["__ram5_end__"])
    physical_sram = ram0_capacity + ram4_capacity + ram5_capacity
    if (ram0_capacity, ram4_capacity, ram5_capacity) != (
        RP2040_SRAM0_BYTES,
        RP2040_SRAM4_BYTES,
        RP2040_SRAM5_BYTES,
    ):
        raise ValueError(
            "unexpected RP2040 SRAM bank layout: {}, {}, {} B".format(
                ram0_capacity, ram4_capacity, ram5_capacity
            )
        )
    if physical_sram != RP2040_PHYSICAL_SRAM_BYTES:
        raise ValueError("unexpected RP2040 physical SRAM capacity: {} B".format(physical_sram))
    if layout["__heap_end__"] != layout["__ram0_end__"]:
        raise ValueError("SRAM0-3 linker-managed free span does not end at the bank boundary")
    if layout["__ram7_end__"] != layout["__ram5_end__"]:
        raise ValueError("ram7 boot window does not end at the SRAM5 boundary")
    if not (
        layout["__ram0_base__"]
        <= layout["__data_base__"]
        <= layout["__data_end__"]
        <= layout["__heap_base__"]
        <= layout["__heap_end__"]
    ):
        raise ValueError("data or linker-managed free span falls outside SRAM0-3")
    if not (
        layout["__ram0_base__"]
        <= layout["__bss_base__"]
        <= layout["__bss_end__"]
        <= layout["__heap_base__"]
    ):
        raise ValueError("BSS span falls outside the fixed SRAM0-3 prefix")
    if not layout["__ram4_base__"] <= layout["__ram4_free__"] <= layout["__ram4_end__"]:
        raise ValueError("SRAM4 free marker falls outside the bank")
    if not (
        layout["__ram5_base__"]
        <= layout["__ram5_free__"]
        <= layout["__ram7_base__"]
        <= layout["__ram7_free__"]
        <= layout["__ram7_end__"]
    ):
        raise ValueError("SRAM5 or ram7 boot-window markers overlap unexpectedly")

    static_data = checked_span("static data", layout["__data_base__"], layout["__data_end__"])
    static_bss = checked_span("static BSS", layout["__bss_base__"], layout["__bss_end__"])
    data_bss = static_data + static_bss
    ram0_fixed_prefix = checked_span(
        "SRAM0-3 fixed prefix", layout["__ram0_base__"], layout["__heap_base__"]
    )
    ram0_free = checked_span(
        "SRAM0-3 linker-managed free span", layout["__heap_base__"], layout["__heap_end__"]
    )
    ram4_fixed_prefix = checked_span(
        "SRAM4 fixed prefix", layout["__ram4_base__"], layout["__ram4_free__"]
    )
    ram4_unassigned_tail = checked_span(
        "SRAM4 unassigned tail", layout["__ram4_free__"], layout["__ram4_end__"]
    )
    ram5_fixed_prefix = checked_span(
        "SRAM5 fixed prefix", layout["__ram5_base__"], layout["__ram5_free__"]
    )
    ram5_unassigned_pre_boot = checked_span(
        "SRAM5 unassigned pre-boot tail", layout["__ram5_free__"], layout["__ram7_base__"]
    )
    ram7_linked = checked_span(
        "ram7 linked boot-window prefix", layout["__ram7_base__"], layout["__ram7_free__"]
    )
    ram7_reserved = checked_span(
        "ram7 reserved boot-window remainder", layout["__ram7_free__"], layout["__ram7_end__"]
    )
    fixed_linked = ram0_fixed_prefix + ram4_fixed_prefix + ram5_fixed_prefix + ram7_linked
    unassigned_or_allocator = ram0_free + ram4_unassigned_tail + ram5_unassigned_pre_boot
    if fixed_linked + unassigned_or_allocator + ram7_reserved != physical_sram:
        raise ValueError("RP2040 SRAM bank accounting does not sum to physical capacity")

    return {
        "physical_sram": physical_sram,
        "static_data": static_data,
        "static_bss": static_bss,
        "data_bss": data_bss,
        "ram0_fixed_prefix": ram0_fixed_prefix,
        "ram0_free": ram0_free,
        "ram4_fixed_prefix": ram4_fixed_prefix,
        "ram4_unassigned_tail": ram4_unassigned_tail,
        "ram5_fixed_prefix": ram5_fixed_prefix,
        "ram5_unassigned_pre_boot": ram5_unassigned_pre_boot,
        "ram7_linked": ram7_linked,
        "ram7_reserved": ram7_reserved,
        "fixed_linked": fixed_linked,
    }


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
    parser.add_argument(
        "--max-data-bss",
        "--max-static-ram",
        dest="max_data_bss",
        type=int,
        default=51000,
        help="policy ceiling for .data + .bss; --max-static-ram is a compatibility alias",
    )
    parser.add_argument(
        "--min-sram0-free",
        "--min-heap",
        dest="min_sram0_free",
        type=int,
        default=204800,
        help="minimum SRAM0-3 linker-managed free/core-memory span at boot; --min-heap is a compatibility alias",
    )
    args = parser.parse_args(argv)

    if not args.elf.is_file():
        parser.error("ELF does not exist: {}".format(args.elf))

    try:
        symbols = parse_nm_symbols(run([args.nm, "-S", "--size-sort", str(args.elf)]))
        layout = parse_layout_symbols(run([args.nm, "-S", str(args.elf)]))
        gnu_size_bss = parse_size_bss(run([args.size, str(args.elf)]))
        memory = calculate_memory_accounting(layout)
        toolchain = run([args.nm, "--version"]).splitlines()[0]
    except (OSError, subprocess.CalledProcessError, ValueError) as error:
        print("firmware memory policy: FAIL ({})".format(error), file=sys.stderr)
        return 1

    macro_storage = sum(symbols.values())
    reclaimed = args.baseline_macro_storage - macro_storage
    failures = []
    if macro_storage > args.max_macro_storage:
        failures.append("named macro storage {} exceeds {} B".format(macro_storage, args.max_macro_storage))
    if reclaimed < args.min_reclaimed:
        failures.append("reclaimed {} is below {} B".format(reclaimed, args.min_reclaimed))
    if memory["static_bss"] > args.max_static_bss:
        failures.append("static BSS {} exceeds {} B".format(memory["static_bss"], args.max_static_bss))
    if memory["data_bss"] > args.max_data_bss:
        failures.append(
            ".data + .bss policy span {} exceeds {} B".format(memory["data_bss"], args.max_data_bss)
        )
    if memory["ram0_free"] < args.min_sram0_free:
        failures.append(
            "SRAM0-3 linker-managed free/core-memory span {} is below {} B".format(
                memory["ram0_free"], args.min_sram0_free
            )
        )

    print("firmware memory policy: {}".format("FAIL" if failures else "PASS"))
    print("ELF: {}".format(args.elf))
    print("toolchain: {}".format(toolchain))
    print(
        "RP2040 physical SRAM per MCU: {} B (256 KiB SRAM0-3 + 4 KiB SRAM4 + 4 KiB SRAM5)".format(
            memory["physical_sram"]
        )
    )
    for name in REQUIRED_SYMBOLS + BOOKKEEPING_SYMBOLS:
        if name in symbols:
            print("{}: {} B".format(name, symbols[name]))
    print("named macro storage: {} B (limit {} B)".format(macro_storage, args.max_macro_storage))
    print("reclaimed from {} B baseline: {} B (minimum {} B)".format(args.baseline_macro_storage, reclaimed, args.min_reclaimed))
    print("SRAM0-3 .bss span: {} B (policy limit {} B)".format(memory["static_bss"], args.max_static_bss))
    print("SRAM0-3 .data span: {} B".format(memory["static_data"]))
    print(
        "SRAM0-3 .data + .bss policy span: {} B (policy limit {} B)".format(
            memory["data_bss"], args.max_data_bss
        )
    )
    print(
        "SRAM0-3 fixed linked prefix: {} B; linker-managed free/core-memory span at boot: {} B (policy minimum {} B)".format(
            memory["ram0_fixed_prefix"], memory["ram0_free"], args.min_sram0_free
        )
    )
    print(
        "SRAM4 fixed linked prefix: {} B; linker-unassigned tail: {} B".format(
            memory["ram4_fixed_prefix"], memory["ram4_unassigned_tail"]
        )
    )
    print(
        "SRAM5 fixed linked prefix: {} B; linker-unassigned pre-boot tail: {} B; boot window: {} B linked + {} B reserved".format(
            memory["ram5_fixed_prefix"],
            memory["ram5_unassigned_pre_boot"],
            memory["ram7_linked"],
            memory["ram7_reserved"],
        )
    )
    print("fixed linked occupancy across unique SRAM banks: {} B".format(memory["fixed_linked"]))
    print(
        "GNU size BSS column: {} B (informational; includes NOLOAD reservations)".format(
            gnu_size_bss
        )
    )
    for failure in failures:
        print("error: {}".format(failure), file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
