"""A build with VIA switched on stops with D-F31; an ordinary one is unchanged.

users/noah/rules.mk is read here as QMK reads it, after the keymap's rules,
with a stand-in for the real-profile validation it runs, so the test also
proves the refusal comes before validation.
"""

import os
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
USER_PATH = ROOT / "users/noah"
KEYMAP_PATH = "keyboards/bastardkb/charybdis/4x6/keymaps/noah"


class ViaRefusalBuildTest(unittest.TestCase):
    def build(self, keymap_rules: str = "", *arguments: str) -> tuple[subprocess.CompletedProcess, list[str], bool]:
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            validated = workspace / "validated"
            validation = workspace / "tests/host/run_real_profile_validation_tests.sh"
            validation.parent.mkdir(parents=True)
            validation.write_text(f"touch '{validated}'\n")
            (workspace / "keymap_rules.mk").write_text(keymap_rules)
            (workspace / "probe.mk").write_text("all:\n\t@echo $(SRC)\n")
            env = {key: value for key, value in os.environ.items()
                   if not key.startswith("NOAH_") and key != "VIA_ENABLE"}
            result = subprocess.run(
                ["make", "--no-print-directory",
                 "-f", str(workspace / "keymap_rules.mk"), "-f", str(USER_PATH / "rules.mk"), "-f", str(workspace / "probe.mk"),
                 f"QMK_USERSPACE={workspace}", f"USER_PATH={USER_PATH}", f"KEYMAP_PATH={KEYMAP_PATH}", *arguments],
                capture_output=True, text=True, env=env,
            )
            return result, result.stdout.split(), validated.exists()

    def test_via_is_refused_before_validation(self):
        for keymap_rules, arguments in (("VIA_ENABLE = yes\n", ()), ("", ("VIA_ENABLE=yes",)), ("VIA_ENABLE = yes \n", ())):
            with self.subTest(keymap_rules=keymap_rules, arguments=arguments):
                result, _, validated = self.build(keymap_rules, *arguments)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("VIA is not supported (D-F31)", result.stderr)
                self.assertFalse(validated)

    def test_ordinary_build_is_unchanged(self):
        for keymap_rules, arguments in (("", ()), ("VIA_ENABLE = no\n", ()), ("", ("VIA_ENABLE=no",))):
            with self.subTest(keymap_rules=keymap_rules, arguments=arguments):
                result, sources, validated = self.build(keymap_rules, *arguments)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertTrue(validated)
                self.assertIn("hooks.c", sources)
                self.assertNotIn("D-F31", result.stderr)


if __name__ == "__main__":
    unittest.main()
