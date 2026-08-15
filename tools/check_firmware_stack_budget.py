#!/usr/bin/env python3
"""Check reviewed firmware paths against their linked stack contexts.

This is deliberately a reviewed-path regression check, not a proof of the
global worst-case stack depth.  It uses final linked disassembly for frame
bounds and direct call edges, and requires every non-direct adjacency in the
manifest to be an explicit, documented indirect-edge assertion.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import re
import shutil
import subprocess
import sys
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Mapping, Sequence


_CLONE_SUFFIX_RE = re.compile(
    r"(?:"
    r"\.(?:constprop|isra|part|clone|lto_priv)\.\d+"
    r"|\.cold(?:\.\d+)?"
    r")+$"
)
_HEX_OR_DECIMAL_RE = r"(?:0[xX][0-9a-fA-F]+|[0-9]+)"
_OBJDUMP_FUNCTION_RE = re.compile(r"^(?P<address>[0-9a-fA-F]+) <(?P<function>[^>]+)>:$")
_OBJDUMP_INSTRUCTION_RE = re.compile(
    r"^\s*(?P<address>[0-9a-fA-F]+):\s+"
    r"(?:(?:[0-9a-fA-F]{2}|[0-9a-fA-F]{4}|[0-9a-fA-F]{8})\s+)+"
    r"(?P<mnemonic>\.?[a-zA-Z][a-zA-Z0-9.]*)"
    r"(?:\s+(?P<operands>.*?))?\s*$"
)
_OBJDUMP_WORD_RE = re.compile(
    r"^\s*(?P<address>[0-9a-fA-F]+):\s+[0-9a-fA-F]{8}\s+\.word\s+0x(?P<value>[0-9a-fA-F]+)\s*$"
)
_REGISTER_RE = r"(?:r(?:1[0-5]|[0-9])|ip|lr|pc|sp)"
_TARGET_SYMBOL_RE = re.compile(r"<(?P<symbol>[^>]+)>")
_TARGET_ADDRESS_RE = re.compile(r"^(?:0x)?(?P<address>[0-9a-fA-F]+)(?:\s|$)")
_DIRECT_CALL_MNEMONICS = frozenset({"bl", "bl.w", "blx", "blx.w"})
_DIRECT_TAIL_MNEMONICS = frozenset({"b", "b.n", "b.w"})


class StackBudgetInputError(ValueError):
    """Raised when linked artifacts or the manifest cannot prove a result."""


@dataclass(frozen=True)
class StackUsageRecord:
    function: str
    canonical_function: str
    bytes: int
    qualifier: str
    source: Path
    line_number: int

    @property
    def bounded(self) -> bool:
        return self.qualifier == "linked-disassembly"


@dataclass(frozen=True)
class LinkedSymbol:
    name: str
    symbol_type: str
    value: int
    size: int | None


@dataclass(frozen=True, order=True)
class CallEdge:
    caller: str
    callee: str


@dataclass(frozen=True)
class IndirectCallsite:
    caller: str
    address: int
    mnemonic: str
    operands: str
    kind: str = "indirect"


@dataclass(frozen=True)
class DisassemblyInstruction:
    address: int
    mnemonic: str
    operands: str
    comment: str


@dataclass(frozen=True)
class LinkedDisassembly:
    records: tuple[StackUsageRecord, ...]
    direct_edges: frozenset[CallEdge]
    indirect_callsites: tuple[IndirectCallsite, ...]


@dataclass(frozen=True)
class StackContract:
    kind: str
    reserve_fraction: float
    minimum_reserve_bytes: int
    symbol: str | None = None
    minimum_stack_bytes: int | None = None
    usable_stack_bytes: int | None = None
    working_area_symbol: str | None = None
    expected_working_area_bytes: int | None = None
    working_area_fixed_overhead_bytes: int | None = None
    working_area_alignment_bytes: int | None = None

    def reserve_bytes(self, stack_bytes: int) -> int:
        return max(self.minimum_reserve_bytes, math.ceil(stack_bytes * self.reserve_fraction))


@dataclass(frozen=True)
class IndirectEdgeAssertion:
    caller: str
    callee: str
    reason: str


@dataclass(frozen=True)
class CycleBound:
    function: str
    max_occurrences: int
    reason: str


@dataclass(frozen=True)
class ReviewedPath:
    name: str
    functions: tuple[str, ...]


@dataclass(frozen=True)
class StackContext:
    name: str
    root: str
    contract: StackContract
    paths: tuple[ReviewedPath, ...]
    indirect_edges: tuple[IndirectEdgeAssertion, ...]
    cycle_bounds: tuple[CycleBound, ...]


@dataclass(frozen=True)
class BudgetManifest:
    analysis_kind: str
    indirect_callsite_policy: str
    top_frames: int
    contexts: tuple[StackContext, ...]


@dataclass(frozen=True)
class PathResult:
    context: str
    name: str
    frames: tuple[tuple[str, StackUsageRecord], ...]
    edge_kinds: tuple[str, ...]

    @property
    def total_bytes(self) -> int:
        return sum(record.bytes for _, record in self.frames)


@dataclass(frozen=True)
class ContextEvaluation:
    name: str
    stack_bytes: int
    reserve_bytes: int
    chain_budget_bytes: int
    path_results: tuple[PathResult, ...]


@dataclass(frozen=True)
class BudgetEvaluation:
    contexts: tuple[ContextEvaluation, ...]
    issues: tuple[str, ...]

    @property
    def passed(self) -> bool:
        return not self.issues


def canonicalize_function_name(name: str) -> str:
    """Normalize GCC clone suffixes while preserving the source symbol."""

    normalized = name.strip()
    while True:
        stripped = _CLONE_SUFFIX_RE.sub("", normalized)
        if stripped == normalized:
            return normalized
        normalized = stripped


def _base_target_symbol(name: str) -> str:
    return re.split(r"[+-]0x[0-9a-fA-F]+$", name.strip(), maxsplit=1)[0]


def _parse_immediate(text: str) -> int:
    value = text.strip()
    if value.startswith("#"):
        value = value[1:]
    return int(value, 0)


def _parse_linker_integer(text: str) -> int:
    value = text.strip()
    return int(value, 16 if value.lower().startswith("0x") else 10)


def _signed_32(value: int) -> int:
    value &= 0xFFFFFFFF
    return value - 0x100000000 if value & 0x80000000 else value


def _register_count(register_list: str) -> int:
    aliases = {"ip": 12, "sp": 13, "lr": 14, "pc": 15}

    def register_number(name: str) -> int:
        normalized = name.strip().lower()
        if normalized in aliases:
            return aliases[normalized]
        if re.fullmatch(r"r(?:1[0-5]|[0-9])", normalized):
            return int(normalized[1:])
        raise StackBudgetInputError(f"cannot parse register in stack instruction: {name!r}")

    count = 0
    for item in register_list.split(","):
        item = item.strip()
        if "-" not in item:
            register_number(item)
            count += 1
            continue
        start_text, end_text = item.split("-", 1)
        start = register_number(start_text)
        end = register_number(end_text)
        if end < start:
            raise StackBudgetInputError(f"descending register range in stack instruction: {item!r}")
        count += end - start + 1
    return count


def _instruction_target(
    instruction: DisassemblyInstruction,
    address_to_function: Mapping[int, str],
) -> str | None:
    symbol_match = _TARGET_SYMBOL_RE.search(instruction.operands)
    if symbol_match:
        return _base_target_symbol(symbol_match.group("symbol"))
    address_match = _TARGET_ADDRESS_RE.match(instruction.operands)
    if address_match:
        return address_to_function.get(int(address_match.group("address"), 16))
    return None


def _is_register_operand(operands: str) -> bool:
    return re.fullmatch(_REGISTER_RE, operands.strip().lower()) is not None


def _analyze_function_frame(
    function: str,
    instructions: Sequence[DisassemblyInstruction],
    words: Mapping[int, int],
    elf_path: Path,
) -> StackUsageRecord:
    """Conservatively derive a linked frame bound.

    The target gate enforces ``-fno-shrink-wrap``.  A new allocation after a
    nested call is nevertheless rejected so a toolchain/flag regression cannot
    silently move part of a prologue beyond the analyzed entry frame.
    """

    current_depth = 0
    maximum_depth = 0
    constants: dict[str, int] = {}
    nested_call_seen = False
    unknown_stack_operation: str | None = None

    def apply_depth_delta(delta: int, instruction: DisassemblyInstruction) -> bool:
        nonlocal current_depth, maximum_depth, unknown_stack_operation
        if delta > 0 and nested_call_seen:
            unknown_stack_operation = f"late-stack-allocation@0x{instruction.address:x}"
            return False
        current_depth = max(0, current_depth + delta)
        maximum_depth = max(maximum_depth, current_depth)
        return True

    for instruction in instructions:
        mnemonic = instruction.mnemonic
        operands = instruction.operands
        if mnemonic in {".word", ".short", ".byte"}:
            continue

        push_match = re.fullmatch(r"\{([^}]+)\}", operands) if mnemonic == "push" else None
        stmdb_match = re.fullmatch(r"sp!,\s*\{([^}]+)\}", operands) if mnemonic == "stmdb" else None
        if push_match or stmdb_match:
            if not apply_depth_delta(4 * _register_count((push_match or stmdb_match).group(1)), instruction):
                break
            continue

        pop_match = re.fullmatch(r"\{([^}]+)\}", operands) if mnemonic == "pop" else None
        ldmia_match = re.fullmatch(r"sp!,\s*\{([^}]+)\}", operands) if mnemonic == "ldmia" else None
        if pop_match or ldmia_match:
            if not nested_call_seen:
                current_depth = max(0, current_depth - 4 * _register_count((pop_match or ldmia_match).group(1)))
            continue

        direct_stack_match = re.fullmatch(r"sp,\s*(#[+-]?(?:0[xX][0-9a-fA-F]+|[0-9]+))", operands)
        if direct_stack_match and mnemonic in {"sub", "subs", "add", "adds"}:
            immediate = _parse_immediate(direct_stack_match.group(1))
            delta = immediate if mnemonic.startswith("sub") else -immediate
            if not apply_depth_delta(delta, instruction):
                break
            continue

        register_stack_match = re.fullmatch(rf"sp,\s*({_REGISTER_RE})", operands)
        if register_stack_match and mnemonic in {"sub", "subs", "add", "adds"}:
            register = register_stack_match.group(1).lower()
            if register not in constants:
                unknown_stack_operation = f"unknown-stack-register@0x{instruction.address:x}"
                break
            register_value = constants[register]
            delta = -register_value if mnemonic.startswith("add") else register_value
            if not apply_depth_delta(delta, instruction):
                break
            continue

        literal_load = re.fullmatch(rf"({_REGISTER_RE}),\s*\[pc,\s*#[^]]+\]", operands)
        if mnemonic == "ldr" and literal_load:
            target_match = re.search(r"\((?:0x)?([0-9a-fA-F]+)(?:\s|\))", instruction.comment)
            if target_match:
                target = int(target_match.group(1), 16)
                if target in words:
                    constants[literal_load.group(1).lower()] = _signed_32(words[target])
            continue

        move_immediate = re.fullmatch(rf"({_REGISTER_RE}),\s*(#[+-]?(?:0[xX][0-9a-fA-F]+|[0-9]+))", operands)
        if mnemonic in {"mov", "movs"} and move_immediate:
            constants[move_immediate.group(1).lower()] = _parse_immediate(move_immediate.group(2))
            continue

        move_register = re.fullmatch(rf"({_REGISTER_RE}),\s*({_REGISTER_RE})", operands)
        if mnemonic in {"mov", "movs"} and move_register:
            destination, source = (group.lower() for group in move_register.groups())
            if destination == "sp":
                if not nested_call_seen:
                    unknown_stack_operation = f"unknown-stack-restore@0x{instruction.address:x}"
                    break
                continue
            if source in constants:
                constants[destination] = constants[source]
            else:
                constants.pop(destination, None)
            continue

        shift_immediate = re.fullmatch(rf"({_REGISTER_RE}),\s*({_REGISTER_RE}),\s*(#[0-9]+)", operands)
        if mnemonic in {"lsl", "lsls"} and shift_immediate:
            destination, source, shift_text = shift_immediate.groups()
            destination = destination.lower()
            source = source.lower()
            if source in constants:
                constants[destination] = _signed_32(constants[source] << _parse_immediate(shift_text))
            else:
                constants.pop(destination, None)
            continue

        register_immediate = re.fullmatch(
            rf"({_REGISTER_RE}),\s*(#[+-]?(?:0[xX][0-9a-fA-F]+|[0-9]+))", operands
        )
        if mnemonic in {"add", "adds", "sub", "subs"} and register_immediate:
            register, immediate_text = register_immediate.groups()
            register = register.lower()
            if register in constants:
                immediate = _parse_immediate(immediate_text)
                constants[register] += immediate if mnemonic.startswith("add") else -immediate
            continue

        if mnemonic in _DIRECT_CALL_MNEMONICS:
            nested_call_seen = True
            constants.clear()
            continue

        if re.match(r"^sp(?:,|!)", operands) and mnemonic not in {"cmp", "cmn", "tst"}:
            unknown_stack_operation = f"unknown-stack-operation@0x{instruction.address:x}"
            break

        destination_match = re.match(rf"^({_REGISTER_RE})(?:,|$)", operands)
        if destination_match and mnemonic not in {
            "b", "b.n", "b.w", "beq", "bne", "bcs", "bcc", "bhs", "blo", "bmi", "bpl",
            "bvs", "bvc", "bhi", "bls", "bge", "blt", "bgt", "ble", "bx", "cmp", "cmn",
            "str", "strb", "strh", "tst",
        }:
            constants.pop(destination_match.group(1).lower(), None)

    return StackUsageRecord(
        function=function,
        canonical_function=canonicalize_function_name(function),
        bytes=maximum_depth,
        qualifier=unknown_stack_operation or "linked-disassembly",
        source=elf_path,
        line_number=0,
    )


def parse_objdump_disassembly(output: str, elf_path: Path) -> LinkedDisassembly:
    words: dict[int, int] = {}
    for raw_line in output.splitlines():
        match = _OBJDUMP_WORD_RE.match(raw_line)
        if match:
            words[int(match.group("address"), 16)] = int(match.group("value"), 16)

    functions: list[tuple[int, str, list[DisassemblyInstruction]]] = []
    current_address: int | None = None
    current_name: str | None = None
    current_instructions: list[DisassemblyInstruction] = []
    for raw_line in output.splitlines():
        function_match = _OBJDUMP_FUNCTION_RE.match(raw_line.strip())
        if function_match:
            if current_name is not None and current_address is not None:
                functions.append((current_address, current_name, current_instructions))
            current_address = int(function_match.group("address"), 16)
            current_name = function_match.group("function")
            current_instructions = []
            continue
        if current_name is None:
            continue
        instruction_match = _OBJDUMP_INSTRUCTION_RE.match(raw_line)
        if not instruction_match:
            continue
        operands_and_comment = (instruction_match.group("operands") or "").strip()
        if "@" in operands_and_comment:
            operands, comment = operands_and_comment.split("@", 1)
        else:
            operands, comment = operands_and_comment, ""
        current_instructions.append(
            DisassemblyInstruction(
                address=int(instruction_match.group("address"), 16),
                mnemonic=instruction_match.group("mnemonic").lower(),
                operands=operands.strip(),
                comment=comment.strip(),
            )
        )
    if current_name is not None and current_address is not None:
        functions.append((current_address, current_name, current_instructions))
    if not functions:
        raise StackBudgetInputError("objdump output contains no function disassembly")

    address_to_function = {address: function for address, function, _ in functions}
    records: list[StackUsageRecord] = []
    direct_edges: set[CallEdge] = set()
    indirect_callsites: list[IndirectCallsite] = []

    for _address, function, instructions in functions:
        records.append(_analyze_function_frame(function, instructions, words, elf_path))
        canonical_caller = canonicalize_function_name(function)
        for instruction in instructions:
            mnemonic = instruction.mnemonic
            operands = instruction.operands.strip()
            if mnemonic in _DIRECT_CALL_MNEMONICS:
                if mnemonic.startswith("blx") and _is_register_operand(operands):
                    indirect_callsites.append(
                        IndirectCallsite(canonical_caller, instruction.address, mnemonic, operands)
                    )
                    continue
                target = _instruction_target(instruction, address_to_function)
                if target is None:
                    indirect_callsites.append(
                        IndirectCallsite(canonical_caller, instruction.address, mnemonic, operands, "unresolved-direct")
                    )
                    continue
                canonical_target = canonicalize_function_name(target)
                if canonical_target != canonical_caller:
                    direct_edges.add(CallEdge(canonical_caller, canonical_target))
                continue

            if mnemonic in _DIRECT_TAIL_MNEMONICS:
                target = _instruction_target(instruction, address_to_function)
                if target is not None:
                    canonical_target = canonicalize_function_name(target)
                    if canonical_target != canonical_caller:
                        direct_edges.add(CallEdge(canonical_caller, canonical_target))
                continue

            if mnemonic == "bx" and operands.lower() != "lr":
                # A register-restored return is indistinguishable locally from
                # an indirect tail. It is not accepted as manifest evidence;
                # only BLX-register callsites can justify callback assertions.
                indirect_callsites.append(
                    IndirectCallsite(canonical_caller, instruction.address, mnemonic, operands, "indirect-tail")
                )
            elif mnemonic in {"mov", "movs"} and re.fullmatch(r"pc,\s*(?!lr\b)" + _REGISTER_RE, operands):
                indirect_callsites.append(
                    IndirectCallsite(canonical_caller, instruction.address, mnemonic, operands, "indirect-tail")
                )
            elif mnemonic == "ldr" and operands.lower().startswith("pc,"):
                indirect_callsites.append(
                    IndirectCallsite(canonical_caller, instruction.address, mnemonic, operands, "indirect-tail")
                )

    return LinkedDisassembly(
        records=tuple(records),
        direct_edges=frozenset(direct_edges),
        indirect_callsites=tuple(indirect_callsites),
    )


def read_linked_disassembly(elf_path: Path, objdump_command: str) -> LinkedDisassembly:
    executable = shutil.which(objdump_command) if os.sep not in objdump_command else objdump_command
    if not executable:
        raise StackBudgetInputError(f"objdump executable not found: {objdump_command}")
    try:
        completed = subprocess.run(
            [executable, "-d", "--demangle", str(elf_path)],
            check=False,
            capture_output=True,
            text=True,
        )
    except OSError as exc:
        raise StackBudgetInputError(f"could not execute {objdump_command}: {exc}") from exc
    if completed.returncode != 0:
        detail = completed.stderr.strip() or completed.stdout.strip() or f"exit {completed.returncode}"
        raise StackBudgetInputError(f"{objdump_command} could not disassemble {elf_path}: {detail}")
    return parse_objdump_disassembly(completed.stdout, elf_path)


def parse_nm_symbols(output: str) -> dict[str, LinkedSymbol]:
    """Parse ``nm --format=posix --print-size`` output."""

    symbols: dict[str, LinkedSymbol] = {}
    for line_number, line in enumerate(output.splitlines(), start=1):
        if not line.strip():
            continue
        columns = line.split()
        if len(columns) < 3 or len(columns) > 4:
            raise StackBudgetInputError(f"malformed nm output at line {line_number}: {line!r}")
        name, symbol_type, value_text = columns[:3]
        try:
            value = int(value_text, 16)
            size = int(columns[3], 16) if len(columns) == 4 else None
        except ValueError as exc:
            raise StackBudgetInputError(f"invalid nm value at line {line_number}: {line!r}") from exc
        symbols[name] = LinkedSymbol(name=name, symbol_type=symbol_type, value=value, size=size)
    return symbols


def read_elf_symbols(elf_path: Path, nm_command: str) -> dict[str, LinkedSymbol]:
    executable = shutil.which(nm_command) if os.sep not in nm_command else nm_command
    if not executable:
        raise StackBudgetInputError(f"nm executable not found: {nm_command}")
    if not elf_path.is_file():
        raise StackBudgetInputError(f"ELF does not exist: {elf_path}")
    try:
        completed = subprocess.run(
            [executable, "-P", "-S", "--defined-only", str(elf_path)],
            check=False,
            capture_output=True,
            text=True,
        )
    except OSError as exc:
        raise StackBudgetInputError(f"could not execute {nm_command}: {exc}") from exc
    if completed.returncode != 0:
        detail = completed.stderr.strip() or completed.stdout.strip() or f"exit {completed.returncode}"
        raise StackBudgetInputError(f"{nm_command} could not inspect {elf_path}: {detail}")
    return parse_nm_symbols(completed.stdout)


def parse_map_symbol(map_text: str, symbol: str) -> int:
    escaped_symbol = re.escape(symbol)
    assignment = re.compile(rf"\b{escaped_symbol}\b\s*=\s*(?P<value>{_HEX_OR_DECIMAL_RE})\b")
    values = {_parse_linker_integer(match.group("value")) for match in assignment.finditer(map_text)}
    if not values:
        raise StackBudgetInputError(f"linker map does not define {symbol}")
    if len(values) != 1:
        rendered = ", ".join(str(value) for value in sorted(values))
        raise StackBudgetInputError(f"linker map contains conflicting definitions for {symbol}: {rendered}")
    return values.pop()


def read_map_text(map_path: Path) -> str:
    try:
        return map_path.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        raise StackBudgetInputError(f"cannot read linker map {map_path}: {exc}") from exc


def reconcile_stack_symbol(symbol: str, linked_symbols: Mapping[str, LinkedSymbol], map_stack_bytes: int) -> int:
    linked = linked_symbols.get(symbol)
    if linked is None:
        raise StackBudgetInputError(f"linked ELF does not define {symbol}")
    if linked.value != map_stack_bytes:
        raise StackBudgetInputError(
            f"stack symbol mismatch: ELF reports {linked.value} bytes, linker map reports {map_stack_bytes} bytes"
        )
    return linked.value


def _manifest_integer(value: object, field: str, *, minimum: int = 0) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < minimum:
        raise StackBudgetInputError(f"manifest field {field!r} must be an integer >= {minimum}")
    return value


def _manifest_fraction(value: object, field: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not 0.0 <= float(value) < 1.0:
        raise StackBudgetInputError(f"manifest field {field!r} must be >= 0 and < 1")
    return float(value)


def _manifest_string(value: object, field: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise StackBudgetInputError(f"manifest field {field!r} must be a non-empty string")
    return value.strip()


def _load_stack_contract(document: object, context_name: str) -> StackContract:
    if not isinstance(document, dict):
        raise StackBudgetInputError(f"context {context_name!r} stack must be an object")
    kind = _manifest_string(document.get("kind"), f"{context_name}.stack.kind")
    reserve = document.get("reserve")
    if not isinstance(reserve, dict):
        raise StackBudgetInputError(f"context {context_name!r} stack.reserve must be an object")
    fraction = _manifest_fraction(reserve.get("fraction"), f"{context_name}.stack.reserve.fraction")
    minimum_reserve = _manifest_integer(
        reserve.get("minimum_bytes"), f"{context_name}.stack.reserve.minimum_bytes"
    )
    if kind == "elf_map_symbol":
        return StackContract(
            kind=kind,
            reserve_fraction=fraction,
            minimum_reserve_bytes=minimum_reserve,
            symbol=_manifest_string(document.get("symbol"), f"{context_name}.stack.symbol"),
            minimum_stack_bytes=_manifest_integer(
                document.get("minimum_bytes"), f"{context_name}.stack.minimum_bytes", minimum=1
            ),
        )
    if kind == "fixed_usable_stack":
        usable_stack_bytes = _manifest_integer(
            document.get("usable_stack_bytes"), f"{context_name}.stack.usable_stack_bytes", minimum=1
        )
        expected_working_area_bytes = _manifest_integer(
            document.get("expected_working_area_bytes"),
            f"{context_name}.stack.expected_working_area_bytes",
            minimum=1,
        )
        fixed_overhead_bytes = _manifest_integer(
            document.get("working_area_fixed_overhead_bytes"),
            f"{context_name}.stack.working_area_fixed_overhead_bytes",
        )
        alignment_bytes = _manifest_integer(
            document.get("working_area_alignment_bytes"),
            f"{context_name}.stack.working_area_alignment_bytes",
            minimum=1,
        )
        computed_working_area_bytes = (
            (usable_stack_bytes + fixed_overhead_bytes + alignment_bytes - 1) // alignment_bytes
        ) * alignment_bytes
        if computed_working_area_bytes != expected_working_area_bytes:
            raise StackBudgetInputError(
                f"context {context_name!r} working-area math yields {computed_working_area_bytes} bytes, "
                f"not expected_working_area_bytes {expected_working_area_bytes}"
            )
        return StackContract(
            kind=kind,
            reserve_fraction=fraction,
            minimum_reserve_bytes=minimum_reserve,
            usable_stack_bytes=usable_stack_bytes,
            working_area_symbol=_manifest_string(
                document.get("working_area_symbol"), f"{context_name}.stack.working_area_symbol"
            ),
            expected_working_area_bytes=expected_working_area_bytes,
            working_area_fixed_overhead_bytes=fixed_overhead_bytes,
            working_area_alignment_bytes=alignment_bytes,
        )
    raise StackBudgetInputError(
        f"context {context_name!r} stack kind must be 'elf_map_symbol' or 'fixed_usable_stack'"
    )


def load_manifest(path: Path) -> BudgetManifest:
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise StackBudgetInputError(f"cannot read stack-budget manifest {path}: {exc}") from exc
    if not isinstance(document, dict):
        raise StackBudgetInputError("stack-budget manifest root must be an object")
    if document.get("schema_version") != 2:
        raise StackBudgetInputError("stack-budget manifest schema_version must be 2")
    analysis_kind = _manifest_string(document.get("analysis_kind"), "analysis_kind")
    if analysis_kind != "reviewed_path_regression":
        raise StackBudgetInputError("manifest analysis_kind must be 'reviewed_path_regression'")
    indirect_callsite_policy = _manifest_string(
        document.get("indirect_callsite_policy"), "indirect_callsite_policy"
    )
    if indirect_callsite_policy != "reviewed_path_adjacencies":
        raise StackBudgetInputError(
            "manifest indirect_callsite_policy must be 'reviewed_path_adjacencies'"
        )
    top_frames = _manifest_integer(document.get("top_frames", 10), "top_frames", minimum=1)
    context_documents = document.get("contexts")
    if not isinstance(context_documents, list) or not context_documents:
        raise StackBudgetInputError("manifest contexts must be a non-empty array")

    contexts: list[StackContext] = []
    seen_context_names: set[str] = set()
    for context_index, context_document in enumerate(context_documents):
        if not isinstance(context_document, dict):
            raise StackBudgetInputError(f"manifest context at index {context_index} must be an object")
        name = _manifest_string(context_document.get("name"), f"contexts[{context_index}].name")
        if name in seen_context_names:
            raise StackBudgetInputError(f"duplicate manifest context name: {name}")
        seen_context_names.add(name)
        root = _manifest_string(context_document.get("root"), f"{name}.root")
        contract = _load_stack_contract(context_document.get("stack"), name)

        path_documents = context_document.get("paths")
        if not isinstance(path_documents, list) or not path_documents:
            raise StackBudgetInputError(f"context {name!r} paths must be a non-empty array")
        paths: list[ReviewedPath] = []
        seen_path_names: set[str] = set()
        for path_index, path_document in enumerate(path_documents):
            if not isinstance(path_document, dict):
                raise StackBudgetInputError(f"context {name!r} path at index {path_index} must be an object")
            path_name = _manifest_string(path_document.get("name"), f"{name}.paths[{path_index}].name")
            if path_name in seen_path_names:
                raise StackBudgetInputError(f"duplicate path name in context {name!r}: {path_name}")
            seen_path_names.add(path_name)
            functions_document = path_document.get("functions")
            if not isinstance(functions_document, list) or not functions_document:
                raise StackBudgetInputError(f"path {name}/{path_name!r} functions must be a non-empty array")
            functions = tuple(
                _manifest_string(function, f"{name}.{path_name}.functions") for function in functions_document
            )
            if canonicalize_function_name(functions[0]) != canonicalize_function_name(root):
                raise StackBudgetInputError(f"path {name}/{path_name!r} must start at context root {root!r}")
            paths.append(ReviewedPath(path_name, functions))

        indirect_documents = context_document.get("indirect_edges", [])
        if not isinstance(indirect_documents, list):
            raise StackBudgetInputError(f"context {name!r} indirect_edges must be an array")
        indirect_edges: list[IndirectEdgeAssertion] = []
        seen_indirect: set[CallEdge] = set()
        for edge_index, edge_document in enumerate(indirect_documents):
            if not isinstance(edge_document, dict):
                raise StackBudgetInputError(f"context {name!r} indirect edge at index {edge_index} must be an object")
            caller = _manifest_string(edge_document.get("caller"), f"{name}.indirect_edges[{edge_index}].caller")
            callee = _manifest_string(edge_document.get("callee"), f"{name}.indirect_edges[{edge_index}].callee")
            reason = _manifest_string(edge_document.get("reason"), f"{name}.indirect_edges[{edge_index}].reason")
            pair = CallEdge(canonicalize_function_name(caller), canonicalize_function_name(callee))
            if pair in seen_indirect:
                raise StackBudgetInputError(f"duplicate indirect edge in context {name!r}: {caller} -> {callee}")
            seen_indirect.add(pair)
            indirect_edges.append(IndirectEdgeAssertion(caller, callee, reason))

        cycle_documents = context_document.get("cycle_bounds", [])
        if not isinstance(cycle_documents, list):
            raise StackBudgetInputError(f"context {name!r} cycle_bounds must be an array")
        cycle_bounds: list[CycleBound] = []
        seen_cycle_functions: set[str] = set()
        for bound_index, bound_document in enumerate(cycle_documents):
            if not isinstance(bound_document, dict):
                raise StackBudgetInputError(f"context {name!r} cycle bound at index {bound_index} must be an object")
            function = _manifest_string(bound_document.get("function"), f"{name}.cycle_bounds[{bound_index}].function")
            canonical = canonicalize_function_name(function)
            if canonical in seen_cycle_functions:
                raise StackBudgetInputError(f"duplicate cycle bound in context {name!r}: {function}")
            seen_cycle_functions.add(canonical)
            cycle_bounds.append(
                CycleBound(
                    function=function,
                    max_occurrences=_manifest_integer(
                        bound_document.get("max_occurrences"),
                        f"{name}.cycle_bounds[{bound_index}].max_occurrences",
                        minimum=2,
                    ),
                    reason=_manifest_string(
                        bound_document.get("reason"), f"{name}.cycle_bounds[{bound_index}].reason"
                    ),
                )
            )

        contexts.append(
            StackContext(
                name=name,
                root=root,
                contract=contract,
                paths=tuple(paths),
                indirect_edges=tuple(indirect_edges),
                cycle_bounds=tuple(cycle_bounds),
            )
        )
    return BudgetManifest(
        analysis_kind=analysis_kind,
        indirect_callsite_policy=indirect_callsite_policy,
        top_frames=top_frames,
        contexts=tuple(contexts),
    )


def resolve_context_stack_bytes(
    manifest: BudgetManifest,
    linked_symbols: Mapping[str, LinkedSymbol],
    map_text: str,
) -> dict[str, int]:
    resolved: dict[str, int] = {}
    for context in manifest.contexts:
        contract = context.contract
        if contract.kind == "elf_map_symbol":
            assert contract.symbol is not None
            map_value = parse_map_symbol(map_text, contract.symbol)
            resolved[context.name] = reconcile_stack_symbol(contract.symbol, linked_symbols, map_value)
            continue
        assert contract.kind == "fixed_usable_stack"
        assert contract.working_area_symbol is not None
        assert contract.expected_working_area_bytes is not None
        assert contract.usable_stack_bytes is not None
        working_area = linked_symbols.get(contract.working_area_symbol)
        if working_area is None:
            raise StackBudgetInputError(
                f"context {context.name!r}: linked ELF does not define working area {contract.working_area_symbol}"
            )
        if working_area.size is None:
            raise StackBudgetInputError(
                f"context {context.name!r}: linked symbol {contract.working_area_symbol} has no size"
            )
        if working_area.size != contract.expected_working_area_bytes:
            raise StackBudgetInputError(
                f"context {context.name!r}: {contract.working_area_symbol} is {working_area.size} bytes; "
                f"expected {contract.expected_working_area_bytes} for the configured "
                f"{contract.usable_stack_bytes}-byte usable stack"
            )
        resolved[context.name] = contract.usable_stack_bytes
    return resolved


def _index_stack_records(records: Sequence[StackUsageRecord]) -> dict[str, tuple[StackUsageRecord, ...]]:
    indexed: dict[str, list[StackUsageRecord]] = {}
    for record in records:
        indexed.setdefault(record.canonical_function, []).append(record)
    return {name: tuple(matches) for name, matches in indexed.items()}


def _resolve_frame(function: str, indexed_records: Mapping[str, Sequence[StackUsageRecord]]) -> StackUsageRecord:
    canonical = canonicalize_function_name(function)
    matches = indexed_records.get(canonical, ())
    if not matches:
        raise StackBudgetInputError(f"no linked target frame resolves manifest function {function!r}")
    unbounded = sorted({record.qualifier for record in matches if not record.bounded})
    if unbounded:
        raise StackBudgetInputError(
            f"manifest function {function!r} has unknown target stack usage: {', '.join(unbounded)}"
        )
    return max(matches, key=lambda record: (record.bytes, record.function, str(record.source)))


def evaluate_budget(
    manifest: BudgetManifest,
    context_stack_bytes: Mapping[str, int],
    disassembly: LinkedDisassembly,
    linked_symbols: Mapping[str, LinkedSymbol],
) -> BudgetEvaluation:
    issues: list[str] = []
    context_results: list[ContextEvaluation] = []
    linked_canonical = {canonicalize_function_name(name) for name in linked_symbols}
    indexed_records = _index_stack_records(disassembly.records)
    direct_edges = {
        CallEdge(canonicalize_function_name(edge.caller), canonicalize_function_name(edge.callee))
        for edge in disassembly.direct_edges
    }
    indirect_callers = {
        canonicalize_function_name(callsite.caller)
        for callsite in disassembly.indirect_callsites
        if callsite.kind == "indirect"
    }

    for context in manifest.contexts:
        if context.name not in context_stack_bytes:
            issues.append(f"context {context.name!r}: no resolved stack size")
            continue
        stack_bytes = context_stack_bytes[context.name]
        reserve_bytes = context.contract.reserve_bytes(stack_bytes)
        chain_budget = stack_bytes - reserve_bytes
        if context.contract.minimum_stack_bytes is not None and stack_bytes < context.contract.minimum_stack_bytes:
            issues.append(
                f"context {context.name!r}: configured stack is {stack_bytes} bytes, below required minimum "
                f"{context.contract.minimum_stack_bytes} bytes"
            )
        if chain_budget < 0:
            issues.append(
                f"context {context.name!r}: required reserve {reserve_bytes} bytes exceeds stack {stack_bytes} bytes"
            )

        asserted_edges = {
            CallEdge(canonicalize_function_name(edge.caller), canonicalize_function_name(edge.callee)): edge
            for edge in context.indirect_edges
        }
        cycle_bounds = {
            canonicalize_function_name(bound.function): bound for bound in context.cycle_bounds
        }
        used_assertions: set[CallEdge] = set()
        path_results: list[PathResult] = []

        for path in context.paths:
            prefix = f"context {context.name!r}, path {path.name!r}"
            frames: list[tuple[str, StackUsageRecord]] = []
            edge_kinds: list[str] = []
            path_failed = False
            canonical_functions = tuple(canonicalize_function_name(function) for function in path.functions)
            if canonical_functions[0] != canonicalize_function_name(context.root):
                issues.append(f"{prefix}: does not start at context root {context.root!r}")
                path_failed = True

            occurrences = Counter(canonical_functions)
            for function, count in sorted(occurrences.items()):
                if count <= 1:
                    continue
                bound = cycle_bounds.get(function)
                if bound is None:
                    issues.append(f"{prefix}: repeated function {function!r} has no explicit cycle bound")
                    path_failed = True
                elif count > bound.max_occurrences:
                    issues.append(
                        f"{prefix}: function {function!r} occurs {count} times, exceeding cycle bound "
                        f"{bound.max_occurrences}"
                    )
                    path_failed = True

            for function, canonical in zip(path.functions, canonical_functions):
                if canonical not in linked_canonical:
                    issues.append(f"{prefix}: function {function!r} is not present in the linked ELF")
                    path_failed = True
                    continue
                try:
                    frames.append((function, _resolve_frame(function, indexed_records)))
                except StackBudgetInputError as exc:
                    issues.append(f"{prefix}: {exc}")
                    path_failed = True

            for caller, callee in zip(canonical_functions, canonical_functions[1:]):
                edge = CallEdge(caller, callee)
                if edge in direct_edges:
                    edge_kinds.append("direct")
                    continue
                assertion = asserted_edges.get(edge)
                if assertion is None:
                    issues.append(
                        f"{prefix}: false adjacency {caller!r} -> {callee!r}; no linked direct edge or "
                        "declared indirect edge"
                    )
                    path_failed = True
                    edge_kinds.append("unresolved")
                    continue
                used_assertions.add(edge)
                if caller not in indirect_callers:
                    issues.append(
                        f"{prefix}: declared indirect edge {caller!r} -> {callee!r} has no linked "
                        "register-indirect callsite in its caller"
                    )
                    path_failed = True
                    edge_kinds.append("unresolved")
                    continue
                edge_kinds.append("declared-indirect")

            if path_failed:
                continue
            result = PathResult(
                context=context.name,
                name=path.name,
                frames=tuple(frames),
                edge_kinds=tuple(edge_kinds),
            )
            path_results.append(result)
            if result.total_bytes > chain_budget:
                issues.append(
                    f"{prefix}: uses {result.total_bytes} bytes, exceeding the {chain_budget}-byte reviewed-path budget"
                )

        for edge, assertion in asserted_edges.items():
            if edge not in used_assertions:
                issues.append(
                    f"context {context.name!r}: unused indirect-edge assertion "
                    f"{assertion.caller!r} -> {assertion.callee!r}"
                )

        context_results.append(
            ContextEvaluation(
                name=context.name,
                stack_bytes=stack_bytes,
                reserve_bytes=reserve_bytes,
                chain_budget_bytes=chain_budget,
                path_results=tuple(path_results),
            )
        )

    return BudgetEvaluation(contexts=tuple(context_results), issues=tuple(issues))


def render_report(
    manifest: BudgetManifest,
    evaluation: BudgetEvaluation,
    records: Sequence[StackUsageRecord],
) -> str:
    lines = [
        "Firmware reviewed-path stack regression check",
        "  Scope: explicit reviewed paths only; PASS is not a proof of global maximum stack use.",
        "  Indirect-call scope: only adjacencies named by reviewed paths are resolved.",
        "  Frame source: final linked target disassembly (post-LTO)",
        "  Edge source: linked direct calls/tails plus documented indirect-edge assertions",
        "",
        "Top linked frames:",
    ]
    indexed = _index_stack_records(records)
    top_records = sorted(
        (max(matches, key=lambda record: record.bytes) for matches in indexed.values()),
        key=lambda record: (-record.bytes, record.canonical_function),
    )[: manifest.top_frames]
    for record in top_records:
        lines.append(f"  {record.bytes:5d} B  {record.canonical_function} [{record.qualifier}]")

    for context in evaluation.contexts:
        lines.extend(
            [
                "",
                f"Context: {context.name}",
                f"  usable/configured stack: {context.stack_bytes} B",
                f"  required reserve: {context.reserve_bytes} B",
                f"  reviewed-path budget: {context.chain_budget_bytes} B",
                "  Reviewed paths:",
            ]
        )
        for result in context.path_results:
            status = "PASS" if result.total_bytes <= context.chain_budget_bytes else "FAIL"
            lines.append(f"    {status}  {result.total_bytes:5d} B  {result.name}")
            for index, (requested_name, record) in enumerate(result.frames):
                edge_suffix = ""
                if index < len(result.edge_kinds):
                    edge_suffix = f" --{result.edge_kinds[index]}-->"
                rendered_name = record.function if record.function != requested_name else requested_name
                lines.append(f"            {record.bytes:5d} B  {rendered_name}{edge_suffix}")
        if context.path_results:
            worst = max(context.path_results, key=lambda result: result.total_bytes)
            lines.append(f"  Worst reviewed path: {worst.name} ({worst.total_bytes} B)")

    if evaluation.issues:
        lines.extend(["", "Failures:"])
        lines.extend(f"  - {issue}" for issue in evaluation.issues)
        lines.extend(["", "RESULT: FAIL"])
    else:
        lines.extend(["", "RESULT: PASS (reviewed paths only)"])
    return "\n".join(lines)


def build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True, help="schema-v2 reviewed-path manifest")
    parser.add_argument("--elf", type=Path, required=True, help="fresh target ELF")
    parser.add_argument("--map", dest="map_path", type=Path, required=True, help="matching linker map")
    parser.add_argument("--nm", default=os.environ.get("NM", "arm-none-eabi-nm"), help="target nm executable")
    parser.add_argument(
        "--objdump",
        default=os.environ.get("OBJDUMP", "arm-none-eabi-objdump"),
        help="target objdump executable",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_argument_parser().parse_args(argv)
    try:
        manifest = load_manifest(args.manifest)
        disassembly = read_linked_disassembly(args.elf, args.objdump)
        linked_symbols = read_elf_symbols(args.elf, args.nm)
        map_text = read_map_text(args.map_path)
        context_stack_bytes = resolve_context_stack_bytes(manifest, linked_symbols, map_text)
        evaluation = evaluate_budget(manifest, context_stack_bytes, disassembly, linked_symbols)
        print(render_report(manifest, evaluation, disassembly.records))
        return 0 if evaluation.passed else 1
    except StackBudgetInputError as exc:
        print(f"firmware stack-budget input error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
