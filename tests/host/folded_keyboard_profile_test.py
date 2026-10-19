"""The compiled defaults are Noah's keyboard profile (folded in on 2026-10-19).

Decodes the golden compiled profile (which run_profile_compiled_defaults_v1_tests.sh
has just proved is what the firmware compiles) with profile_logical_form and
compares it, domain by domain, with tests/fixtures/folded_keyboard_profile.json.
Only the differences that fixture lists, each with its reason, may remain.

Usage: folded_keyboard_profile_test.py <compiled fixture> <expected fixture>
"""

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import profile_logical_form  # noqa: E402


def differences(path, compiled, expected, found):
    if isinstance(compiled, dict) and isinstance(expected, dict):
        for key in sorted(set(compiled) | set(expected)):
            differences(f"{path}.{key}", compiled.get(key), expected.get(key), found)
    elif isinstance(compiled, list) and isinstance(expected, list) and len(compiled) == len(expected):
        for index, (left, right) in enumerate(zip(compiled, expected)):
            differences(f"{path}[{index}]", left, right, found)
    elif compiled != expected:
        found[path] = (compiled, expected)


def main():
    compiled_fixture, expected_fixture = map(Path, sys.argv[1:3])
    fields = dict(line.split("=", 1) for line in compiled_fixture.read_text().splitlines() if "=" in line)
    compiled = profile_logical_form.logical(profile_logical_form.blob_domains(bytes.fromhex(fields["profile.full.hex"])))
    expected = json.loads(expected_fixture.read_text())
    allowed = {entry["path"]: entry for entry in expected["differences"]}
    assert all(entry.get("why") for entry in allowed.values()), "every listed difference says why"

    assert sorted(compiled) == sorted(expected["domains"]), f"domains {sorted(compiled)} != {sorted(expected['domains'])}"
    failures = []
    for domain in sorted(expected["domains"]):
        found = {}
        differences(domain, compiled[domain], expected["domains"][domain], found)
        for path, (have, want) in found.items():
            entry = allowed.pop(path, None)
            if entry is None or entry.get("compiled") != have or entry.get("backup") != want:
                failures.append(f"{path}: compiled {json.dumps(have)}, backup {json.dumps(want)}")
        print(f"{domain}: {'matches the backup' if not found else f'{len(found)} difference(s)'}")
    # A listed difference the defaults no longer have is stale.
    failures += [f"{path}: listed but not found" for path in allowed]
    if failures:
        print("compiled defaults differ from the folded keyboard profile:", *failures, sep="\n  ", file=sys.stderr)
        sys.exit(1)
    print("folded keyboard profile tests passed")


if __name__ == "__main__":
    main()
