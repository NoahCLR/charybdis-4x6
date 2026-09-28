#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
. "$ROOT/tests/host/noah_host_live_env.sh"
noah_host_export_live_root "$ROOT"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM

node - "$ROOT" "$BUILD_DIR/corpus.bin" <<'JS'
const fs = require("node:fs");
const root = process.argv[2];
const fixture = require(root + "/tests/fixtures/pd_mode_domain_v1.json");
const {decodePdDomain, encodePdDomain} = require(process.env.CHARYBDIS_LIVE_ROOT + "/core/schema/pd-mode-domain-v1");
const golden = Buffer.from(fixture.hex, "hex");
if (!encodePdDomain(fixture.slots).equals(golden)) throw new Error("PD fixture drift");
const chunks = [];
function add(bytes) {
    let valid = 1;
    try {decodePdDomain(bytes);} catch {valid = 0;}
    const header = Buffer.alloc(3); header[0] = valid; header.writeUInt16LE(bytes.length, 1);
    chunks.push(header, bytes);
}
add(golden);
for (let offset = 0; offset < golden.length; offset++) {
    for (const value of [0, 1, 2, 3, 0x7f, 0x80, 0xc0, 0xe0, 0xff]) {
        const bytes = Buffer.from(golden); bytes[offset] = value; add(bytes);
    }
}
for (let length = 0; length < golden.length; length++) add(golden.subarray(0, length));
add(Buffer.concat([golden, Buffer.from([0])]));
// An eight-direction mode: diagonals live in bytes 70..85 and the
// empty-direction policy in byte 86. Every byte of that record is mutated too.
const eightSlots = structuredClone(fixture.slots);
Object.assign(eightSlots[4], {axis: 3, thresholdX: 40, thresholdY: 40, emptyDirection: 1,
    diagonals: {upLeft: {keycode: 0x50, modifierPolicy: 0, mask: 0}, upRight: {keycode: 0x4f, modifierPolicy: 1, mask: 2}, downLeft: {keycode: 0, modifierPolicy: 0, mask: 0}, downRight: {keycode: 0x51, modifierPolicy: 2, mask: 0}}});
const eight = encodePdDomain(eightSlots);
add(eight);
for (let offset = 8 + 4 * 96; offset < 8 + 5 * 96; offset++) {
    for (const value of [0, 1, 2, 3, 4, 0x7f, 0x80, 0xff]) {
        const bytes = Buffer.from(eight); bytes[offset] = value; add(bytes);
    }
}
// Every directional mode carries the empty-direction policy in byte 86.
const dominantSlots = structuredClone(fixture.slots);
Object.assign(dominantSlots[4], {axis: 2, emptyDirection: 2});
const dominant = encodePdDomain(dominantSlots);
add(dominant);
for (let offset = 8 + 4 * 96 + 70; offset < 8 + 4 * 96 + 90; offset++) {
    for (const value of [0, 1, 2, 3, 0xff]) {
        const bytes = Buffer.from(dominant); bytes[offset] = value; add(bytes);
    }
}
for (const name of ["Édition ⌘", "😀".repeat(5), "x".repeat(23)]) {
    const slots = structuredClone(fixture.slots); slots[7].name = name; add(encodePdDomain(slots));
}
fs.writeFileSync(process.argv[3], Buffer.concat(chunks));
JS

build_and_run() {
    name="$1"
    shift
    cc -std=c11 -Wall -Wextra -Werror -pedantic "$@" -I"$ROOT" \
        "$ROOT/tests/host/profile_pd_v1_test.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_pd_v1.c" \
        -o "$BUILD_DIR/$name"
    "$BUILD_DIR/$name" "$BUILD_DIR/corpus.bin"
}
build_and_run normal
build_and_run sanitized -fsanitize=address,undefined -fno-omit-frame-pointer
