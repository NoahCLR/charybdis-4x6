"""Every tracked userspace C source is in the source manifest, and every
manifest entry exists.

The firmware build and the feature-gate compile take their sources from the
lists users/noah/source_manifest.mk defines, so a .c file left out of every
list is silently never built. A file built another way is allowlisted below
with its reason. The lists are read the way noah_source_manifest.sh reads
them: make includes the manifest and prints each variable.
"""

import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
USER_PATH = "users/noah"
KEYMAP_PATH = "keyboards/bastardkb/charybdis/4x6/keymaps/noah"

# Tracked userspace sources the manifest does not list, each with how it is
# built instead.
BUILT_ANOTHER_WAY: dict[str, str] = {}

PRINT_LISTS = """\
include $(ROOT)/users/noah/source_manifest.mk
print-lists: ; @$(foreach v,$(sort $(filter NOAH_%_SOURCES,$(.VARIABLES))),printf '%s %s\\n' '$(v)' '$($(v))';)
"""


def manifest_lists(root: Path) -> dict[str, list[str]]:
    result = subprocess.run(
        ["make", "-s", "-f", "-", "print-lists", f"ROOT={root}", f"USER_PATH={USER_PATH}", f"KEYMAP_PATH={KEYMAP_PATH}"],
        input=PRINT_LISTS, capture_output=True, text=True, check=True,
    )
    lists = {}
    for line in result.stdout.splitlines():
        name, *sources = line.split()
        lists[name] = sources
    return lists


def problems(root: Path, built_another_way: dict[str, str] = BUILT_ANOTHER_WAY) -> list[str]:
    lists = manifest_lists(root)
    if not lists:
        return [f"{USER_PATH}/source_manifest.mk defines no NOAH_*_SOURCES list"]
    found = []
    listed = set()
    for name, sources in lists.items():
        for source in sources:
            # Keymap-owned entries are named from the root through KEYMAP_PATH.
            path = source if source.startswith(f"{KEYMAP_PATH}/") else f"{USER_PATH}/{source}"
            listed.add(path)
            if not (root / path).is_file():
                found.append(f"{name} lists {path}, which does not exist")
    tracked = subprocess.run(
        ["git", "-C", str(root), "ls-files", "-z", "--", USER_PATH],
        capture_output=True, check=True,
    ).stdout.decode().split("\0")
    for path in sorted(p for p in tracked if p.endswith(".c")):
        if path not in listed and path not in built_another_way:
            found.append(f"{path} is in no source list of {USER_PATH}/source_manifest.mk, so no build compiles it")
    for path in sorted(built_another_way):
        if path in listed:
            found.append(f"{path} is allowlisted as built another way but the manifest lists it")
        elif path not in tracked:
            found.append(f"{path} is allowlisted as built another way but is not tracked")
    return found


class SourceManifestTest(unittest.TestCase):
    def test_repository(self):
        self.assertEqual(problems(ROOT), [])

    def fixture(self, directory: str) -> Path:
        root = Path(directory)
        (root / USER_PATH / "lib").mkdir(parents=True)
        (root / KEYMAP_PATH).mkdir(parents=True)
        (root / USER_PATH / "source_manifest.mk").write_text(
            "NOAH_COMMON_SOURCES := \\\n    hooks.c \\\n    lib/a.c\n\n"
            "NOAH_PROFILE_KEYMAP_SOURCES := \\\n    $(KEYMAP_PATH)/pd_config.c\n"
        )
        for path in (f"{USER_PATH}/hooks.c", f"{USER_PATH}/lib/a.c", f"{KEYMAP_PATH}/pd_config.c"):
            (root / path).write_text("")
        subprocess.run(["git", "init", "-q", str(root)], check=True)
        subprocess.run(["git", "-C", str(root), "add", "."], check=True)
        return root

    def test_fixture_passes(self):
        with tempfile.TemporaryDirectory() as directory:
            self.assertEqual(problems(self.fixture(directory), {}), [])

    def test_unlisted_source_is_named(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.fixture(directory)
            (root / USER_PATH / "lib/forgotten.c").write_text("")
            # Untracked scratch files are not sources.
            self.assertEqual(problems(root, {}), [])
            subprocess.run(["git", "-C", str(root), "add", "."], check=True)
            found = problems(root, {})
            self.assertEqual(len(found), 1)
            self.assertIn(f"{USER_PATH}/lib/forgotten.c", found[0])
            self.assertEqual(problems(root, {f"{USER_PATH}/lib/forgotten.c": "test"}), [])
            manifest = root / USER_PATH / "source_manifest.mk"
            manifest.write_text(manifest.read_text().replace("lib/a.c\n", "lib/a.c \\\n    lib/forgotten.c\n"))
            self.assertEqual(problems(root, {}), [])

    def test_source_list_of_its_own_counts(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.fixture(directory)
            (root / USER_PATH / "lib/feature.c").write_text("")
            subprocess.run(["git", "-C", str(root), "add", "."], check=True)
            manifest = root / USER_PATH / "source_manifest.mk"
            manifest.write_text(manifest.read_text() + "\nNOAH_FEATURE_SOURCES := \\\n    lib/feature.c\n")
            self.assertEqual(problems(root, {}), [])

    def test_missing_entry_is_named(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.fixture(directory)
            (root / USER_PATH / "lib/a.c").unlink()
            (root / KEYMAP_PATH / "pd_config.c").unlink()
            self.assertEqual(problems(root, {}), [
                f"NOAH_COMMON_SOURCES lists {USER_PATH}/lib/a.c, which does not exist",
                f"NOAH_PROFILE_KEYMAP_SOURCES lists {KEYMAP_PATH}/pd_config.c, which does not exist",
            ])

    def test_stale_allowlist_entry_is_named(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.fixture(directory)
            self.assertEqual(problems(root, {f"{USER_PATH}/lib/a.c": "test", f"{USER_PATH}/lib/gone.c": "test"}), [
                f"{USER_PATH}/lib/a.c is allowlisted as built another way but the manifest lists it",
                f"{USER_PATH}/lib/gone.c is allowlisted as built another way but is not tracked",
            ])


if __name__ == "__main__":
    unittest.main()
