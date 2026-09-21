#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM

node - "$ROOT" "$BUILD_DIR/corpus.bin" <<'JS'
const fs = require("node:fs");
const root = process.argv[2];
const fixture = require(root + "/tests/fixtures/pd_mode_domain_v1.json");
const {decodePdDomain, encodePdDomain} = require(root + "/tools/charybdis-live/core/schema/pd-mode-domain-v1");
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
