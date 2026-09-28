#!/bin/sh
# The app's macro program size (macroProgramBytes) must equal what the
# firmware's decoder compiles each VIA macro to, or the app would refuse
# macros the keyboard plays or keep ones it never will.
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
. "$ROOT/tests/host/noah_host_live_env.sh"
noah_host_export_live_root "$ROOT"
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -I"$QMK_ROOT/platforms" -I"$QMK_ROOT/quantum/send_string" \
    -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/macro_program_size_probe.c" \
    "$ROOT/users/noah/lib/macro/macro_payload.c" \
    "$ROOT/users/noah/lib/macro/macro_payload_decode_qmk.c" \
    "$ROOT/users/noah/lib/macro/macro_payload_keycodes.c" \
    "$ROOT/users/noah/lib/macro/macro_payload_parse.c" \
    "$ROOT/users/noah/lib/macro/macro_payload_encode.c" \
    -o "$BUILD_DIR/probe"
node - "$ROOT" "$BUILD_DIR/probe" <<'JS'
const assert = require("node:assert/strict"), {execFileSync} = require("node:child_process");
const {encodeMacroPayload, macroProgramBytes, MACRO_PROGRAM_MAX} = require(process.env.CHARYBDIS_LIVE_ROOT + "/core/schema/macro-payload");
// Fixed edge cases, then seeded random macros built from every step kind.
const payloads = ["", "a", "a".repeat(255), "a".repeat(256), "a".repeat(600), "{KC_A}", "{KC_A}".repeat(170), "{KC_A}".repeat(171),
    "{120}", "{0}", "{65535}", "a{KC_A}b", "{KC_LGUI,KC_N}", "{KC_LCTL,KC_LSFT,KC_4}", "{+KC_LSFT}{KC_A}{-KC_LSFT}",
    "{+KC_LSFT}{KC_A}{KC_B}{-KC_LSFT}", "{+KC_LSFT}{+KC_LCTL}{KC_A}{-KC_LCTL}{-KC_LSFT}", "{+KC_LSFT}{+KC_LCTL}{KC_A}{-KC_LSFT}{-KC_LCTL}",
    "{+KC_LSFT}{KC_A}x{-KC_LSFT}", "{+KC_LSFT}{KC_A}{50}{-KC_LSFT}", "{+KC_LSFT}{+KC_A}{-KC_A}{-KC_LSFT}", "{+KC_A}{KC_B}{KC_C}{-KC_A}{KC_D}"];
let seed = 20260923;
const random = n => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return seed % n; };
const keys = ["KC_A", "KC_B", "KC_C", "KC_ENT", "KC_LSFT", "KC_LCTL", "KC_LGUI", "KC_LALT"];
for (let i = 0; i < 3000; i++) {
    let payload = ""; const held = [];
    for (let steps = 1 + random(40); steps > 0; steps--) {
        const kind = random(7), free = keys.filter(key => !held.includes(key));
        if (kind === 0) payload += "abc xyz".slice(0, 1 + random(7));
        else if (kind === 1) payload += `{${random(2000)}}`;
        else if (kind === 2 && free.length && held.length < 4) { const key = free[random(free.length)]; held.push(key); payload += `{+${key}}`; }
        else if (kind === 3 && held.length) payload += `{-${held.splice(random(held.length), 1)[0]}}`;
        else if (kind === 4 && free.length > 1) payload += `{${free[0]},${free[1]}}`;
        else if (free.length) payload += `{${free[random(free.length)]}}`;
    }
    while (held.length) payload += `{-${held.pop()}}`;
    payloads.push(payload);
}
const macros = payloads.map(payload => encodeMacroPayload(payload, "via"));
const output = execFileSync(process.argv[3], {input: macros.map(bytes => bytes.toString("hex")).join("\n") + "\n"}).toString().trim().split("\n").map(Number);
assert.equal(output.length, macros.length);
let over = 0;
macros.forEach((bytes, index) => {
    const size = macroProgramBytes(bytes);
    // The firmware refuses exactly the macros whose program would not fit.
    if (size > MACRO_PROGRAM_MAX) { over++; assert.equal(output[index], -1, payloads[index]); }
    else assert.equal(size, output[index], payloads[index]);
});
assert.ok(over > 0, "the corpus reaches past the 512-byte program cap");
console.log(`app macro program size matches the firmware decoder for ${macros.length} macros (${over} over the cap)`);
JS
