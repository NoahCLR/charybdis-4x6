"""Same internal grouped split vectors in independent Python and C codecs."""
import json
from pathlib import Path
import sys

root = Path(sys.argv[1])
sys.path.insert(0, str(root / 'tools'))
from lighting_reference import decode_event_group, encode_event_group

fixture = json.loads((root / 'tests/fixtures/lighting_event_transport_v1.json').read_text())
lines = []
for vector in fixture['valid']:
    if 'groupHex' not in vector:
        continue
    wire = bytes.fromhex(vector['groupHex'])
    group = decode_event_group(wire)
    assert group['order'] == 0xfedcba9876543210 and group['thing'] == 127
    assert group['qualified'][-2:] == '40'
    assert encode_event_group(group) == wire
    packet = bytes.fromhex(vector['hex'])
    assert len(packet) == 63 and packet[9:] == wire
    lines.append('VALID ' + vector['groupHex'])
for vector in fixture['rejected']:
    wire = bytes.fromhex(vector['groupHex'])
    try:
        decode_event_group(wire)
    except ValueError:
        pass
    else:
        raise AssertionError(vector['name'])
    lines.append('INVALID ' + vector['groupHex'])
Path(sys.argv[2]).write_text('\n'.join(lines) + '\n')
print('lighting internal event group vectors agree with Python reference')
