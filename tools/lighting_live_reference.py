"""Executable, importable Lighting Live v1 wire definitions; no device I/O."""
from __future__ import annotations
import argparse
import json
import pathlib
import struct
import zlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
FIXTURE = ROOT / 'tests/fixtures/lighting_live_v1.json'
SLICE_LENGTHS = [(1, 2), (2, 1), (3, 11), (4, 16), (5, 61), (7, 40),
                 (8, 180), (9, 64), (10, 11), (11, 40), (12, 180),
                 (13, 1), (14, 8), (15, 2), (16, 1), (17, 1), (18, 1)]
PROFILE = struct.pack('<IIIII', 1, 0x11223344, 0x55667788, 0xaabbccdd, 0xeeff0011) + bytes([1, 0])

def capability(*, facts=True, frame=True, leds=58, left=29, slices=SLICE_LENGTHS):
    tlv = sum(3 + length for _, length in slices)
    return struct.pack('<BBBBBBBBBBHHHBBHH', 1, int(facts) | (int(frame) << 1),
                       1, 1, 0x0e, 0x0f, 0x1d, 8, leds, left, leds * 3,
                       2000, tlv, 48, 1, 2000, 500) + bytes(3)

def facts_stream(snapshot=1, captured=100, generation=1, keymap=2, profile=PROFILE):
    data = bytearray()
    offset = 0
    for kind, length in SLICE_LENGTHS:
        data += struct.pack('<BH', kind, length)
        data += bytes((offset + i) & 255 for i in range(length))
        offset += length
    header = struct.pack('<BBBBIIII', 1, 1, 1, 1, snapshot, captured, generation, keymap)
    header += profile + struct.pack('<HBBBB', 48 + len(data), len(SLICE_LENGTHS), 58, 29, 60)
    assert len(header) == 48
    return header + data

def request(command, value, request_id, operation, payload=b''):
    if not 1 <= request_id <= 255 or len(payload) > 27:
        raise ValueError('request identity/size')
    return bytes([command, 0, value, request_id, operation]) + payload + bytes(27 - len(payload))

def response(req, status, payload=b''):
    if len(req) != 32 or len(payload) > 25:
        raise ValueError('report size')
    return req[:5] + bytes([status, len(payload)]) + payload + bytes(25-len(payload))

def decode_response(req, reply):
    if len(req) != 32 or len(reply) != 32 or reply[:5] != req[:5]:
        raise ValueError('correlation/size')
    status, length = reply[5:7]
    if status > 10 or length > 25 or any(reply[7+length:]) or (status and length):
        raise ValueError('status/length/padding')
    return status, reply[7:7+length]

def decode_facts(stream):
    if len(stream) < 48 or stream[:4] != bytes([1, 1, 1, 1]):
        raise ValueError('facts version/source/role/validity')
    total, count, leds, left, keys = struct.unpack_from('<HBBBB', stream, 42)
    if total != len(stream) or not 0 < left < leds <= 64 or not 0 < keys <= 64:
        raise ValueError('facts bounds')
    offset, slices, prior = 48, {}, 0
    for _ in range(count):
        if offset+3 > len(stream):
            raise ValueError('slice header')
        kind, length = struct.unpack_from('<BH', stream, offset)
        offset += 3
        if kind <= prior or not length or offset+length > len(stream):
            raise ValueError('slice identity/bounds')
        slices[kind] = stream[offset:offset+length]
        offset += length
        prior = kind
    if offset != len(stream):
        raise ValueError('trailing facts')
    return {'snapshot': struct.unpack_from('<I', stream, 4)[0], 'profile': stream[20:42], 'slices': slices}

def fixture():
    stream = facts_stream()
    reports = []
    for page, offset in enumerate(range(0, len(stream), 21)):
        req = request(8, 0x0e, page+1, page, struct.pack('<I', 1))
        reports.append({'request': req.hex(), 'response': response(req, 0, struct.pack('<I', 1)+stream[offset:offset+21]).hex()})
    # Status is a coherent main publication, not fresh callback-computed time.
    status_fields = [bytes([1, 1, 1, 0, 0, 0, 0]) + struct.pack('<IIHHBB', 0, 0, 0, 174, 0, 0),
                     bytes(20) + bytes([1]),
                     struct.pack('<IBIIII', 1, 1, 0x55667788, 1, 100, 0)]
    for page, data in enumerate(status_fields):
        assert len(data) == 21
        req = request(8, 0x0f, 60+page, page, struct.pack('<I', 2))
        reports.append({'request': req.hex(), 'response': response(req, 0, struct.pack('<I', 2)+data).hex()})
    for req, status in [(request(8, 0x0e, 41, 0, struct.pack('<I', 2)), 5),
                        (request(8, 0x0e, 42, 35, struct.pack('<I', 1)), 2),
                        (request(8, 0x0e, 43, 0, struct.pack('<I', 1)+b'\x01'), 1),
                        (request(7, 0x1d, 44, 9), 1)]:
        reports.append({'request': req.hex(), 'response': response(req, status).hex()})
    frame = bytes((i*17+3) % 128 for i in range(174))
    owner = bytes(range(10, 18))
    identified = owner + struct.pack('<II', 1, 7)
    begin = request(7, 0x1d, 81, 2, owner + struct.pack('<IIHI', 7, 0x55667788, 174, zlib.crc32(frame)))
    chunk = request(7, 0x1d, 82, 3, identified + struct.pack('<HB', 0, 8) + frame[:8])
    conflict = request(7, 0x1d, 83, 3, identified + struct.pack('<HB', 0, 1) + bytes([frame[0]^1]))
    lifecycle = [(begin, 4, 0), (chunk, 4, 0), (chunk, 0, 0),
                 (conflict, 4, 8), (request(7, 0x1d, 84, 4, identified), 4, 7),
                  ]
    for offset in range(8, 174, 8):
        data = frame[offset:offset+8]
        lifecycle.append((request(7, 0x1d, 85+offset//8, 3, identified+struct.pack('<HB', offset, len(data))+data), 4, 0))
    lifecycle += [(request(7, 0x1d, 120, 4, identified), 4, 0),
                  (request(7, 0x1d, 121, 5, identified+struct.pack('<I', 1)), 4, 0),
                  (request(7, 0x1d, 121, 5, identified+struct.pack('<I', 1)), 0, 0),
                  (request(7, 0x1d, 122, 6, identified), 4, 0),
                  (request(7, 0x1d, 123, 5, identified+struct.pack('<I', 2)), 4, 5)]
    return {'format': 1, 'capabilityPage': 16, 'capability': capability().hex(),
            'metadataBytes': 48, 'factsTlvBytes': 671, 'facts': stream.hex(),
            'slices': [{'id': i, 'bytes': n} for i, n in SLICE_LENGTHS],
            'frame': frame.hex(), 'frameCrc32': zlib.crc32(frame),
            'leaseMs': 2000, 'renewRecommendationMs': 500, 'stagingLeaseMs': 2000,
            'reports': reports, 'lifecycle': [{'request': q.hex(), 'admission': response(q, a).hex(), 'completion': response(q, c).hex()} for q,a,c in lifecycle]}

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--write', action='store_true')
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--vectors')
    args = parser.parse_args()
    value = fixture()
    text = json.dumps(value, indent=2)+'\n'
    assert len(capability()) == 25 and len(facts_stream()) == 719
    assert len(decode_facts(facts_stream())['slices']) == 17
    for case in value['reports']:
        decode_response(bytes.fromhex(case['request']), bytes.fromhex(case['response']))
    for offset in (0, 5, 6, 31):
        req=bytes.fromhex(value['reports'][34]['request']);good=bytes.fromhex(value['reports'][34]['response'])
        bad=bytearray(good);bad[offset]^=128
        try: decode_response(req, bad)
        except ValueError: pass
        else: raise AssertionError('bad response accepted')
    if args.write: FIXTURE.write_text(text)
    if args.check and FIXTURE.read_text() != text: raise SystemExit('lighting live fixture is stale')
    if args.vectors:
        pathlib.Path(args.vectors).write_text(value['capability']+'\n'+value['frame']+'\n'+str(value['frameCrc32'])+'\n'+'\n'.join(c['request']+' '+c['response'] for c in value['reports'])+'\nLIFECYCLE '+str(len(value['lifecycle']))+'\n'+'\n'.join(c['request']+' '+c['admission']+' '+c['completion'] for c in value['lifecycle'])+'\n')
    print('Lighting Live v1: exact25B capability/48B metadata/719B facts/correlated32B reports PASS')

if __name__ == '__main__': main()
