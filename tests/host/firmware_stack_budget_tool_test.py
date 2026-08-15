#!/usr/bin/env python3

import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
TOOL_PATH = ROOT / "tools" / "check_firmware_stack_budget.py"
SPEC = importlib.util.spec_from_file_location("check_firmware_stack_budget", TOOL_PATH)
stack_budget = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = stack_budget
SPEC.loader.exec_module(stack_budget)


class FirmwareStackBudgetToolTest(unittest.TestCase):
    def record(self, function, frame_bytes, qualifier="linked-disassembly"):
        return stack_budget.StackUsageRecord(
            function=function,
            canonical_function=stack_budget.canonicalize_function_name(function),
            bytes=frame_bytes,
            qualifier=qualifier,
            source=Path("fixture.elf"),
            line_number=0,
        )

    def symbol(self, name, *, value=0x1000, size=4, symbol_type="T"):
        return stack_budget.LinkedSymbol(name, symbol_type, value, size)

    def main_contract(self):
        return stack_budget.StackContract(
            kind="elf_map_symbol",
            symbol="__process_stack_size__",
            minimum_stack_bytes=2048,
            reserve_fraction=0.25,
            minimum_reserve_bytes=512,
        )

    def split_contract(self):
        return stack_budget.StackContract(
            kind="fixed_usable_stack",
            usable_stack_bytes=1024,
            working_area_symbol="waSlaveThread",
            expected_working_area_bytes=1200,
            working_area_fixed_overhead_bytes=172,
            working_area_alignment_bytes=8,
            reserve_fraction=0.0,
            minimum_reserve_bytes=0,
        )

    def context(
        self,
        name="main_process",
        root="main",
        functions=("main", "worker"),
        contract=None,
        indirect_edges=(),
        cycle_bounds=(),
    ):
        return stack_budget.StackContext(
            name=name,
            root=root,
            contract=contract or self.main_contract(),
            paths=(stack_budget.ReviewedPath("fixture path", tuple(functions)),),
            indirect_edges=tuple(indirect_edges),
            cycle_bounds=tuple(cycle_bounds),
        )

    def manifest(self, *contexts):
        return stack_budget.BudgetManifest(
            analysis_kind="reviewed_path_regression",
            indirect_callsite_policy="reviewed_path_adjacencies",
            top_frames=10,
            contexts=tuple(contexts or (self.context(),)),
        )

    def analysis(self, records, edges=(), indirect_callsites=()):
        return stack_budget.LinkedDisassembly(
            records=tuple(records),
            direct_edges=frozenset(edges),
            indirect_callsites=tuple(indirect_callsites),
        )

    def symbols(self, *names):
        return {name: self.symbol(name) for name in names}

    def test_objdump_parser_measures_frame_and_direct_call(self):
        output = """
10000000 <worker>:
10000000:\tb5f0      \tpush\t{r4, r5, r6, r7, lr}
10000002:\tb0a1      \tsub\tsp, #132
10000004:\tf000 f800 \tbl\t10000008 <callee>

10000008 <callee>:
10000008:\t4770      \tbx\tlr
"""
        parsed = stack_budget.parse_objdump_disassembly(output, Path("firmware.elf"))
        frames = {record.function: record for record in parsed.records}
        self.assertEqual(frames["worker"].bytes, 152)
        self.assertTrue(frames["worker"].bounded)
        self.assertIn(stack_budget.CallEdge("worker", "callee"), parsed.direct_edges)

    def test_objdump_parser_rejects_late_stack_allocation(self):
        output = """
10000000 <worker>:
10000000:\tb510      \tpush\t{r4, lr}
10000002:\tf000 f801 \tbl\t10000008 <callee>
10000006:\tb084      \tsub\tsp, #16

10000008 <callee>:
10000008:\t4770      \tbx\tlr
"""
        parsed = stack_budget.parse_objdump_disassembly(output, Path("firmware.elf"))
        worker = next(record for record in parsed.records if record.function == "worker")
        self.assertFalse(worker.bounded)
        self.assertIn("late-stack-allocation", worker.qualifier)

    def test_objdump_parser_records_direct_tail_calls(self):
        output = """
10000000 <outer>:
10000000:\te000      \tb.n\t10000004 <middle>

10000004 <middle>:
10000004:\tf000 b800 \tb.w\t10000008 <leaf>

10000008 <leaf>:
10000008:\t4770      \tbx\tlr
"""
        parsed = stack_budget.parse_objdump_disassembly(output, Path("firmware.elf"))
        self.assertIn(stack_budget.CallEdge("outer", "middle"), parsed.direct_edges)
        self.assertIn(stack_budget.CallEdge("middle", "leaf"), parsed.direct_edges)

    def test_objdump_parser_records_register_indirect_callsite(self):
        output = """
10000000 <dispatcher>:
10000000:\tb510      \tpush\t{r4, lr}
10000002:\t4798      \tblx\tr3
10000004:\tbd10      \tpop\t{r4, pc}
"""
        parsed = stack_budget.parse_objdump_disassembly(output, Path("firmware.elf"))
        self.assertEqual(len(parsed.indirect_callsites), 1)
        self.assertEqual(parsed.indirect_callsites[0].caller, "dispatcher")
        self.assertEqual(parsed.indirect_callsites[0].kind, "indirect")

    def test_false_path_adjacency_fails(self):
        evaluation = stack_budget.evaluate_budget(
            self.manifest(),
            {"main_process": 2048},
            self.analysis((self.record("main", 64), self.record("worker", 64))),
            self.symbols("main", "worker"),
        )
        self.assertFalse(evaluation.passed)
        self.assertTrue(any("false adjacency" in issue for issue in evaluation.issues))

    def test_missing_indirect_assertion_fails(self):
        callsite = stack_budget.IndirectCallsite("main", 0x1000, "blx", "r3")
        evaluation = stack_budget.evaluate_budget(
            self.manifest(),
            {"main_process": 2048},
            self.analysis((self.record("main", 64), self.record("worker", 64)), indirect_callsites=(callsite,)),
            self.symbols("main", "worker"),
        )
        self.assertFalse(evaluation.passed)
        self.assertTrue(any("no linked direct edge or declared indirect edge" in issue for issue in evaluation.issues))

    def test_documented_indirect_adjacency_passes(self):
        assertion = stack_budget.IndirectEdgeAssertion("main", "worker", "fixture callback registry")
        callsite = stack_budget.IndirectCallsite("main", 0x1000, "blx", "r3")
        context = self.context(indirect_edges=(assertion,))
        evaluation = stack_budget.evaluate_budget(
            self.manifest(context),
            {"main_process": 2048},
            self.analysis((self.record("main", 64), self.record("worker", 64)), indirect_callsites=(callsite,)),
            self.symbols("main", "worker"),
        )
        self.assertTrue(evaluation.passed, evaluation.issues)
        self.assertEqual(evaluation.contexts[0].path_results[0].edge_kinds, ("declared-indirect",))

    def test_indirect_assertion_without_linked_callsite_fails(self):
        assertion = stack_budget.IndirectEdgeAssertion("main", "worker", "fixture callback registry")
        context = self.context(indirect_edges=(assertion,))
        evaluation = stack_budget.evaluate_budget(
            self.manifest(context),
            {"main_process": 2048},
            self.analysis((self.record("main", 64), self.record("worker", 64))),
            self.symbols("main", "worker"),
        )
        self.assertFalse(evaluation.passed)
        self.assertTrue(any("no linked register-indirect callsite" in issue for issue in evaluation.issues))

    def test_main_and_split_contexts_use_independent_budgets(self):
        main = self.context(functions=("main", "main_worker"))
        split = self.context(
            name="split_slave_thread",
            root="SlaveThread",
            functions=("SlaveThread", "split_callback"),
            contract=self.split_contract(),
        )
        records = (
            self.record("main", 100),
            self.record("main_worker", 100),
            self.record("SlaveThread", 600),
            self.record("split_callback", 500),
        )
        edges = (
            stack_budget.CallEdge("main", "main_worker"),
            stack_budget.CallEdge("SlaveThread", "split_callback"),
        )
        evaluation = stack_budget.evaluate_budget(
            self.manifest(main, split),
            {"main_process": 2048, "split_slave_thread": 1024},
            self.analysis(records, edges),
            self.symbols("main", "main_worker", "SlaveThread", "split_callback"),
        )
        self.assertFalse(evaluation.passed)
        self.assertTrue(any("1024-byte reviewed-path budget" in issue for issue in evaluation.issues))
        self.assertEqual(evaluation.contexts[0].chain_budget_bytes, 1536)
        self.assertEqual(evaluation.contexts[1].chain_budget_bytes, 1024)

    def test_context_stack_resolution_checks_process_symbol_and_working_area_size(self):
        main = self.context()
        split = self.context(
            name="split_slave_thread",
            root="SlaveThread",
            functions=("SlaveThread",),
            contract=self.split_contract(),
        )
        symbols = {
            "__process_stack_size__": self.symbol(
                "__process_stack_size__", value=2048, size=None, symbol_type="A"
            ),
            "waSlaveThread": self.symbol("waSlaveThread", value=0x20000000, size=1200, symbol_type="b"),
        }
        resolved = stack_budget.resolve_context_stack_bytes(
            self.manifest(main, split), symbols, "__process_stack_size__ = 0x800\n"
        )
        self.assertEqual(resolved, {"main_process": 2048, "split_slave_thread": 1024})

    def test_context_stack_resolution_rejects_wrong_working_area_size(self):
        split = self.context(
            name="split_slave_thread",
            root="SlaveThread",
            functions=("SlaveThread",),
            contract=self.split_contract(),
        )
        symbols = {
            "waSlaveThread": self.symbol("waSlaveThread", value=0x20000000, size=1192, symbol_type="b")
        }
        with self.assertRaisesRegex(stack_budget.StackBudgetInputError, "expected 1200"):
            stack_budget.resolve_context_stack_bytes(self.manifest(split), symbols, "")

    def test_repeated_function_requires_explicit_cycle_bound(self):
        context = self.context(functions=("main", "worker", "main"))
        edges = (
            stack_budget.CallEdge("main", "worker"),
            stack_budget.CallEdge("worker", "main"),
        )
        evaluation = stack_budget.evaluate_budget(
            self.manifest(context),
            {"main_process": 2048},
            self.analysis((self.record("main", 64), self.record("worker", 64)), edges),
            self.symbols("main", "worker"),
        )
        self.assertFalse(evaluation.passed)
        self.assertTrue(any("no explicit cycle bound" in issue for issue in evaluation.issues))

    def test_nm_parser_preserves_working_area_symbol_size(self):
        symbols = stack_budget.parse_nm_symbols(
            "__process_stack_size__ A 800\nwaSlaveThread b 20015658 4b0\n"
        )
        self.assertEqual(symbols["__process_stack_size__"].value, 2048)
        self.assertIsNone(symbols["__process_stack_size__"].size)
        self.assertEqual(symbols["waSlaveThread"].size, 1200)

    def test_old_schema_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "manifest.json"
            path.write_text(json.dumps({"schema_version": 1}), encoding="utf-8")
            with self.assertRaisesRegex(stack_budget.StackBudgetInputError, "schema_version must be 2"):
                stack_budget.load_manifest(path)

    def test_repository_manifest_loads_as_schema_v2(self):
        manifest = stack_budget.load_manifest(ROOT / "tools" / "firmware_stack_budget.json")
        self.assertEqual(manifest.analysis_kind, "reviewed_path_regression")
        self.assertEqual({context.name for context in manifest.contexts}, {"main_process", "split_slave_thread"})


if __name__ == "__main__":
    unittest.main()
