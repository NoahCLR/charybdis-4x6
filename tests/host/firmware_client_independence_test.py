"""Keep firmware regression data, checks, tools and build wiring independent of the app."""
import hashlib
import json
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[2]
fixtures = root / 'tests/fixtures/client-regression'
manifest = json.loads((fixtures / 'manifest.json').read_text())
assert manifest['format'] == 1
for record in manifest['files']:
    assert hashlib.sha256((fixtures / record['path']).read_bytes()).hexdigest() == record['sha256'], record['path']

# Every tracked executable surface: tests, tools, build files, userspace, editor
# tasks and CI. Prose and frozen fixtures may name the app as history or
# provenance; this file names the rule and is not a consumer.
# The app is Charybdis Ark; its former name stays forbidden too.
forbidden = ['charybdis-ark', 'CHARYBDIS_ARK_ROOT', 'charybdis-live', 'CHARYBDIS_LIVE_ROOT', 'noah_host_live_env', 'requireLive']
prose = {'.md', '.txt'}
# CI's release gate is the one place that may name the app (D-F06): its
# `Agreement with Ark main` job runs the app's own agreement check against this
# firmware before main moves. Firmware code, tests and tools still never read it.
exempt = {'tests/host/firmware_client_independence_test.py', '.github/workflows/ci.yml'}
tracked = subprocess.run(['git', '-C', str(root), 'ls-files', '-z'], check=True, capture_output=True).stdout
checked = 0
for name in tracked.decode().split('\0'):
    if not name or name in exempt or name.startswith(('docs/', 'tests/fixtures/', 'measurements/')):
        continue
    path = root / name
    if path.suffix in prose or not path.is_file():
        continue
    data = path.read_bytes()
    if b'\0' in data:
        continue
    text = data.decode(errors='replace')
    for word in forbidden:
        assert word not in text, (name, word)
    checked += 1
assert checked > 100, f'independence scan covered only {checked} files'
print(f'Firmware is app-independent across {checked} tracked files; frozen fixture hashes match')
