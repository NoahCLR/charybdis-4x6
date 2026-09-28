"""Keep firmware regression data and host checks independent of the app."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[2]
fixtures = root / 'tests/fixtures/client-regression'
manifest = json.loads((fixtures / 'manifest.json').read_text())
assert manifest['format'] == 1
for record in manifest['files']:
    assert hashlib.sha256((fixtures / record['path']).read_bytes()).hexdigest() == record['sha256'], record['path']
# Guard executable host surfaces. This file describes the rule and is not a consumer.
for file in (root / 'tests/host').glob('*.sh'):
    text = file.read_text()
    for forbidden in ['tools/charybdis-live', 'CHARYBDIS_LIVE_ROOT', 'noah_host_live_env']:
        assert forbidden not in text, (file.name, forbidden)
text = (root / 'tools/capture-split-diagnostics.cjs').read_text()
assert 'requireLive' not in text and 'charybdis-live/package.json' not in text
print('Firmware host checks are app-independent; frozen fixture hashes match')
