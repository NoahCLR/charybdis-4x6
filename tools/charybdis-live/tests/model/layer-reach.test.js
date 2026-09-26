"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const {layerReach, draftChecks, LEVELS} = require("../../core/model/layer-reach");
const {CHARYBDIS_4X6_LAYOUT_MATRIX} = require("../../core/data/charybdis-layout");

// A profile as validateSnapshot decodes one, holding only what the walk reads.
// Base answers every key with KC_A; the other layers start transparent.
const KC_A = 0x04, KC_B = 0x05, KC_ESC = 0x29, TRNS = 0x01, NO = 0x00;
const MO = (n) => 0x5220 | n, TG = (n) => 0x5260 | n, TO = (n) => 0x5200 | n, TT = (n) => 0x52c0 | n, DF = (n) => 0x5240 | n;
const LOCK_LAYER = (n) => 0x7e5c + n;
const code = (operand) => ({kind: 1, flags: 0, operand});
function profile({keys = {}, fill = {}, rows = [], combos = [], settings = {}} = {}) {
    const layers = Array.from({length: 8}, (_, layer) => Array(60).fill(layer === 0 ? KC_A : (fill[layer] ?? TRNS)));
    for (const [at, value] of Object.entries(keys)) {
        const [layer, index] = at.split(":").map(Number);
        const [row, column] = CHARYBDIS_4X6_LAYOUT_MATRIX[index];
        layers[layer][row * 6 + column] = value;
    }
    const values = Array(28).fill(0);
    values[23] = 1; values[27] = 0x76543210; values[20] = 1;
    for (const [id, value] of Object.entries(settings)) values[id] = value;
    return {document: {layers}, settings: {values, names: ["Base", "Numbers", "Symbols", "Navigation", "Pointer", "Extra 1", "Extra 2", "Extra 3"]},
        behaviors: {rows}, combos};
}
const traps = (value) => layerReach(value).filter((finding) => finding.level === LEVELS.TRAP);
const kinds = (value) => layerReach(value).map((finding) => finding.kind);

test("a lock whose layer covers its own key, with nothing else to release it, is a trap", () => {
    const [trap, ...rest] = traps(profile({keys: {"0:0": TG(1), "1:0": KC_B}}));
    assert.equal(rest.length, 0);
    assert.deepEqual(trap.layers, [1]);
    assert.equal(trap.title, "Numbers can lock with no way back to Base");
    assert.deepEqual(trap.path, ["Tap TG(1) on Base"]);
    assert.deepEqual(trap.place, {kind: "key", layer: 0, layoutIndex: 0});
    assert.match(trap.fix, /TO\(0\) or TG\(1\) on Numbers/);
});

test("a lock is no trap when its key falls through, or the layer has TO(0), TG or LOCK_LAYER of itself", () => {
    assert.deepEqual(traps(profile({keys: {"0:0": TG(1)}})), [], "the lock key is transparent on its layer");
    assert.deepEqual(traps(profile({keys: {"0:0": TG(1), "1:0": KC_B, "1:5": TO(0)}})), []);
    assert.deepEqual(traps(profile({keys: {"0:0": TG(1), "1:0": KC_B, "1:5": LOCK_LAYER(1)}})), []);
    assert.deepEqual(traps(profile({keys: {"0:0": LOCK_LAYER(1), "1:0": KC_B, "1:5": TG(1)}})), []);
});

test("a layer of KC_NO covers everything under it, so locking it with no exit is a trap", () => {
    assert.equal(traps(profile({keys: {"0:0": TG(1)}, fill: {1: NO}})).length, 1);
});

test("a layer held on top of the lock can carry the way out", () => {
    assert.deepEqual(traps(profile({keys: {"0:0": TG(1), "1:0": KC_B, "1:5": MO(3), "3:6": TG(1)}})), []);
    assert.deepEqual(traps(profile({keys: {"0:0": TG(1), "1:0": KC_B, "1:5": MO(3), "3:6": KC_B}})).map((trap) => trap.layers), [[1]]);
});

test("a chain of locks is followed: the second lock can hide the first layer's way out", () => {
    const [trap] = traps(profile({keys: {"0:0": TG(1), "1:0": KC_B, "1:5": TO(0), "1:6": TG(2), "2:5": KC_B, "2:6": KC_B}}));
    assert.deepEqual(trap.layers, [1, 2]);
    assert.deepEqual(trap.path, ["Tap TG(1) on Base", "Tap TG(2) on Numbers"]);
    assert.equal(trap.title, "Numbers and Symbols can lock with no way back to Base");
});

test("TO(n) locks n alone, so TO() onto a layer with no exit is a trap and TO(0) always gets home", () => {
    assert.deepEqual(traps(profile({keys: {"0:0": TO(2), "2:0": KC_B}})).map((trap) => trap.layers), [[2]]);
    assert.deepEqual(traps(profile({keys: {"0:0": TG(1), "0:1": TG(2), "1:0": KC_B, "2:1": KC_B, "1:9": TO(0), "2:9": TO(0)}})), []);
});

test("TT locks on its fifth tap, unless a behaviour row on it replaces that", () => {
    const [trap] = traps(profile({keys: {"0:0": TT(1), "1:0": KC_B}}));
    assert.deepEqual(trap.path, ["Tap TT(1) 5 times on Base"]);
    const row = {target: code(TT(1)), steps: [{tapIndex: 0, tap: code(KC_B)}]};
    assert.deepEqual(traps(profile({keys: {"0:0": TT(1), "1:0": KC_B}, rows: [row]})), []);
});

test("behaviour branches lock and release like keys, told by the taps that reach them", () => {
    const lock = {target: code(KC_ESC), steps: [{tapIndex: 1, tap: {kind: 3, flags: 0, operand: 2}}]};
    const [trap] = traps(profile({keys: {"0:0": KC_ESC, "2:0": KC_B}, rows: [lock]}));
    assert.deepEqual(trap.path, ["Double-tap KC_ESCAPE on Base (its behaviour sends LOCK_LAYER(2))"]);
    const release = {target: code(KC_B), steps: [{tapIndex: 0, hold: {mode: 2, repeatHz: 0, action: code(TO(0))}}]};
    assert.deepEqual(traps(profile({keys: {"0:0": KC_ESC, "2:0": KC_B}, rows: [lock, release]})), []);
    // A layer hold only holds from a "Press and hold until release" branch.
    const held = {target: code(KC_B), steps: [{tapIndex: 0, hold: {mode: 1, repeatHz: 0, action: {kind: 2, flags: 0, operand: 3}}}]};
    assert.deepEqual(traps(profile({keys: {"0:0": KC_ESC, "2:0": KC_B, "3:0": TO(0)}, rows: [lock, held]})), []);
});

test("a combo on the highest layer can release a lock, while combos are switched on", () => {
    const combo = {inputs: [code(KC_A), code(KC_B)], output: code(TO(0))};
    const keys = {"0:0": TG(1), "1:0": KC_B, "1:1": KC_A, "1:2": KC_B};
    assert.deepEqual(traps(profile({keys, combos: [combo]})), []);
    assert.equal(traps(profile({keys, combos: [combo], settings: {20: 0}})).length, 1);
});

test("the trackball wakes the pointer layer, which can carry the way out unless sniping keeps it off", () => {
    const keys = {"0:0": TG(1), "1:0": KC_B, "4:3": TO(0)};
    assert.equal(traps(profile({keys})).length, 1, "auto-mouse is off");
    assert.deepEqual(traps(profile({keys, settings: {4: 1, 5: 4}})), []);
    assert.equal(traps(profile({keys: {...keys, "0:0": TG(3), "3:0": KC_B}, settings: {4: 1, 5: 4, 8: 1, 9: 3}})).length, 1);
});

test("a layer with keys that nothing reaches is a warning, an empty one is not", () => {
    const findings = layerReach(profile({keys: {"5:0": KC_B}}));
    assert.deepEqual(findings.map((finding) => [finding.kind, finding.level, finding.layers]), [["unreachable", "warning", [5]]]);
    assert.equal(findings[0].title, "Nothing reaches Extra 1");
    assert.deepEqual(kinds(profile({keys: {"0:0": MO(5), "5:0": KC_B}})), []);
    assert.deepEqual(kinds(profile({keys: {"0:0": TG(5), "5:3": KC_B}})), [], "reached through a lock");
});

test("a layer key onto a layer with no keys of its own is a notice", () => {
    const [finding] = layerReach(profile({keys: {"0:0": MO(2)}}));
    assert.equal(finding.kind, "emptyTarget");
    assert.equal(finding.level, "notice");
    assert.match(finding.detail, /^Hold MO\(2\) on Base: Symbols holds only transparent keys and KC_NO/);
});

test("transparent and KC_NO base keys have separate notices and counts", () => {
    const findings = layerReach(profile({keys: {"0:3": TRNS, "0:4": TRNS, "0:5": NO}}));
    const transparent = findings.find(check => check.kind === "deadBase");
    const no = findings.find(check => check.kind === "noBase");
    assert.equal(transparent.count, 2);
    assert.equal(transparent.title, "2 transparent keys on the base layer (Base) do nothing");
    assert.match(transparent.detail, /^A transparent key on the base layer \(Base\)/);
    assert.deepEqual(transparent.place, {kind: "key", layer: 0, layoutIndex: 3});
    assert.equal(no.count, 1);
    assert.equal(no.title, "1 KC_NO key on the base layer (Base) does nothing");
    assert.match(no.detail, /^KC_NO on the base layer \(Base\) explicitly sends nothing/);
    assert.deepEqual(no.place, {kind: "key", layer: 0, layoutIndex: 5});
    const named = profile({keys: {"0:3": TRNS}});
    named.settings.names[0] = "Symbol";
    assert.equal(layerReach(named).find((check) => check.kind === "deadBase").title, "1 transparent key on the base layer (Symbol) does nothing");
});

test("a pointer layer that is always on, or empty, cannot work", () => {
    assert.deepEqual(layerReach(profile({settings: {4: 1, 5: 0}})).map((finding) => [finding.kind, finding.level]), [["pointerLayer", "warning"]]);
    assert.deepEqual(layerReach(profile({settings: {4: 1, 5: 4}})).map((finding) => [finding.kind, finding.level]), [["pointerLayer", "notice"]]);
    assert.deepEqual(kinds(profile({keys: {"4:0": KC_B}, settings: {4: 1, 5: 4}})), []);
});

test("layer keys the keyboard leaves to QMK are named where they sit", () => {
    const [finding] = layerReach(profile({keys: {"0:7": DF(1)}}));
    assert.equal(finding.kind, "unowned");
    assert.equal(finding.title, "DF(1) on Base bypasses layer tracking");
    assert.deepEqual(finding.place, {kind: "key", layer: 0, layoutIndex: 7});
});

test("a draft's findings are new, already on the keyboard, or fixed by the draft", () => {
    const keyboard = profile({keys: {"0:0": TG(1), "1:0": KC_B, "5:0": KC_B}});
    const draft = profile({keys: {"0:0": TG(1), "1:0": KC_B, "1:5": TO(0), "6:0": KC_B}});
    const checks = draftChecks(keyboard, draft);
    assert.deepEqual(checks.map((check) => [check.key, check.status]), [["unreachable:6", "new"], ["trap:1", "fixed"], ["unreachable:5", "fixed"]]);
    assert.deepEqual(draftChecks(keyboard, keyboard).map((check) => check.status), ["existing", "existing"]);
    assert.deepEqual(draftChecks(undefined, draft).map((check) => check.status), ["new"]);
});

test("a profile without the eight-layer bank is not walked", () => {
    assert.deepEqual(layerReach({document: {layers: [[]]}}), []);
});

test("findings are matched layer with layer through the draft's order, so a reorder can make or mend one", () => {
    // The keyboard has nothing reaching Extra 1 (5). The draft swaps 5 into slot 2.
    const keyboard = profile({keys: {"5:0": KC_B}});
    const draft = profile({keys: {"2:0": KC_B}});
    const order = [0, 1, 5, 3, 4, 2, 6, 7];
    assert.deepEqual(draftChecks(keyboard, draft, order).map((check) => [check.key, check.status]), [["unreachable:5", "existing"]]);
    // Made the base, a layer leaves the old base with nothing reaching it: new.
    const based = profile({keys: {"3:0": KC_B}});
    assert.deepEqual(draftChecks(profile(), based, [3, 1, 2, 0, 4, 5, 6, 7]).map((check) => [check.key, check.status]), [["unreachable:0", "new"]]);
});

test("layer keys that hold or toggle Base do nothing, and are counted wherever they sit; TO(0) is not one", () => {
    const toggle = {target: code(KC_ESC), steps: [{tapIndex: 1, tap: {kind: 3, flags: 0, operand: 0}}]};
    const combo = {inputs: [code(KC_A), code(KC_B)], output: code(MO(0))};
    const [finding, ...rest] = layerReach(profile({keys: {"0:0": MO(0), "0:1": TO(0), "0:2": KC_ESC, "3:4": TG(0), "0:3": KC_B}, rows: [toggle], combos: [combo]}))
        .filter((entry) => entry.kind === "idleLayerKey");
    assert.equal(rest.length, 0);
    assert.equal(finding.level, "notice");
    assert.equal(finding.count, 4);
    assert.equal(finding.title, "4 layer keys hold or toggle Base, which is always on");
    assert.match(finding.detail, /^MO\(0\), TG\(0\), LOCK_LAYER\(0\) in the behaviour on KC_ESCAPE and 1 more: Base is always on/);
    assert.deepEqual(finding.place, {kind: "key", layer: 0, layoutIndex: 0});
    assert.deepEqual(kinds(profile({keys: {"0:0": TO(0)}})), []);
});
