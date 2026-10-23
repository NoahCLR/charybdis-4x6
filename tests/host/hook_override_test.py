"""An override of a weak userspace hook calls its noah_* helper, or says it
replaces it.

users/noah/hooks.c defines QMK's *_user hooks weakly, each calling the noah_*
helper that does the runtime's work (process_record_user also calls
noah_process_record_user_finalize). Any other tracked userspace or keymap C
file that defines one of those hooks replaces the default, and with it that
work, unless the override calls the same helpers. An override that replaces
the behaviour on purpose says so in its body, naming the first helper, with a
comment of exactly this form (docs/guide/hooks.md):

    // Replaces noah_matrix_scan_user(): <why>

Where the helper is called inside the override is not checked.
"""

import re
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HOOKS = "users/noah/hooks.c"
SCANNED = ("users/noah", "keyboards/bastardkb/charybdis/4x6/keymaps")

COMMENT = re.compile(r"//[^\n]*|/\*.*?\*/", re.S)
WEAK_HOOK = re.compile(r"__attribute__\s*\(\(\s*weak\b[^)]*\)\)[^;{}()]*?\b(\w+_user)\s*\([^)]*\)\s*\{")
HELPER_CALL = re.compile(r"\b(noah_\w+)\s*\(")


def blank_comments(source: str) -> str:
    # Same length and lines, so offsets in the code are offsets in the source.
    return COMMENT.sub(lambda match: re.sub(r"[^\n]", " ", match.group()), source)


def body_end(code: str, open_brace: int) -> int:
    depth = 0
    for index in range(open_brace, len(code)):
        if code[index] == "{":
            depth += 1
        elif code[index] == "}":
            depth -= 1
            if depth == 0:
                return index
    return len(code)


def weak_hooks(root: Path) -> dict[str, list[str]]:
    """Each hook hooks.c defines weakly, and the noah_* helpers it calls, in order."""
    code = blank_comments((root / HOOKS).read_text())
    hooks = {}
    for match in WEAK_HOOK.finditer(code):
        body = code[match.end():body_end(code, match.end() - 1)]
        helpers = list(dict.fromkeys(HELPER_CALL.findall(body)))
        if helpers:
            hooks[match.group(1)] = helpers
    return hooks


def problems(root: Path) -> list[str]:
    hooks = weak_hooks(root)
    if not hooks:
        return [f"{HOOKS} defines no weak *_user hook that calls a noah_* helper"]
    definition = re.compile(r"\b(" + "|".join(map(re.escape, hooks)) + r")\s*\([^;{})]*\)\s*\{")
    tracked = subprocess.run(
        ["git", "-C", str(root), "ls-files", "-z", "--", *SCANNED],
        capture_output=True, check=True,
    ).stdout.decode().split("\0")
    found = []
    for path in sorted(p for p in tracked if p.endswith((".c", ".h")) and p != HOOKS):
        source = (root / path).read_text(errors="replace")
        code = blank_comments(source)
        for match in definition.finditer(code):
            hook, helpers = match.group(1), hooks[match.group(1)]
            start, end = match.end(), body_end(code, match.end() - 1)
            missing = [helper for helper in helpers if not re.search(rf"\b{helper}\s*\(", code[start:end])]
            if not missing or re.search(rf"//\s*Replaces {helpers[0]}\(\): \S", source[start:end]):
                continue
            line = code.count("\n", 0, match.start()) + 1
            found.append(f"{path}:{line}: {hook}() overrides the userspace default without calling "
                         f"{', '.join(h + '()' for h in missing)} or a `// Replaces {helpers[0]}(): <why>` comment in its body")
    return found


class HookOverrideTest(unittest.TestCase):
    def test_repository(self):
        self.assertEqual(problems(ROOT), [])

    def test_hooks_and_helpers_are_read_from_hooks_c(self):
        hooks = weak_hooks(ROOT)
        self.assertEqual(hooks["matrix_scan_user"], ["noah_matrix_scan_user"])
        self.assertEqual(hooks["process_record_user"], ["noah_process_record_user", "noah_process_record_user_finalize"])
        self.assertEqual(hooks["process_record_admit_user"], ["noah_record_admission_admit"])
        self.assertIn("matrix_slave_scan_user", hooks)

    def check(self, keymap: str) -> list[str]:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            files = {
                HOOKS: "// Weak defaults.\n"
                       "__attribute__((weak)) void matrix_scan_user(void) {\n    noah_matrix_scan_user();\n}\n\n"
                       "__attribute__((weak, noinline)) bool process_record_user(uint16_t keycode, keyrecord_t *record) {\n"
                       "    bool keep = noah_process_record_user(keycode, record);\n"
                       "    if (!keep) { noah_process_record_user_finalize(keycode, record, false); }\n    return keep;\n}\n",
                "keyboards/bastardkb/charybdis/4x6/keymaps/test/keymap.c": keymap,
            }
            for path, text in files.items():
                (root / path).parent.mkdir(parents=True, exist_ok=True)
                (root / path).write_text(text)
            subprocess.run(["git", "init", "-q", str(root)], check=True)
            subprocess.run(["git", "-C", str(root), "add", "."], check=True)
            return problems(root)

    def test_override_without_helper_is_named(self):
        self.assertEqual(self.check("#include QMK_KEYBOARD_H\n\nvoid matrix_scan_user(void) {\n    // noah_matrix_scan_user();\n}\n"), [
            "keyboards/bastardkb/charybdis/4x6/keymaps/test/keymap.c:3: matrix_scan_user() overrides the userspace "
            "default without calling noah_matrix_scan_user() or a `// Replaces noah_matrix_scan_user(): <why>` comment in its body",
        ])

    def test_chained_override_passes(self):
        self.assertEqual(self.check("void matrix_scan_user(void) {\n    if (x) { y(); }\n    noah_matrix_scan_user();\n}\n"), [])
        self.assertEqual(self.check(
            "bool process_record_user(uint16_t keycode, keyrecord_t *record) {\n"
            "    if (!noah_process_record_user(keycode, record)) {\n"
            "        noah_process_record_user_finalize(keycode, record, false);\n        return false;\n    }\n    return true;\n}\n"), [])

    def test_override_missing_one_of_its_helpers_is_named(self):
        self.assertEqual(self.check(
            "bool process_record_user(uint16_t keycode, keyrecord_t *record) {\n"
            "    if (!noah_process_record_user(keycode, record)) {\n        return false;\n    }\n    return true;\n}\n"), [
            "keyboards/bastardkb/charybdis/4x6/keymaps/test/keymap.c:1: process_record_user() overrides the userspace default "
            "without calling noah_process_record_user_finalize() or a `// Replaces noah_process_record_user(): <why>` comment in its body",
        ])

    def test_marked_replacement_passes(self):
        self.assertEqual(self.check(
            "void matrix_scan_user(void) {\n    // Replaces noah_matrix_scan_user(): this keymap scans nothing.\n}\n"), [])
        # The marker names the hook's own helper and gives a reason.
        self.assertEqual(len(self.check("void matrix_scan_user(void) {\n    // Replaces noah_process_record_user(): no.\n}\n")), 1)
        self.assertEqual(len(self.check("void matrix_scan_user(void) {\n    // Replaces noah_matrix_scan_user():\n}\n")), 1)

    def test_declarations_calls_and_other_hooks_are_not_overrides(self):
        self.assertEqual(self.check(
            "void matrix_scan_user(void);\n"
            "void keyboard_post_init_user(void) {\n    matrix_scan_user();\n}\n"), [])


if __name__ == "__main__":
    unittest.main()
