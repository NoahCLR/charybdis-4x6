"use strict";
const assert = require("node:assert/strict");
const test = require("node:test");
const {keyLabel, keyLabelWithName, profileKeyNames} = require("../../core/model/key-names");
const {decodedDeviceProfile} = require("../fixtures/device-profile");
const keycodes = require("../../core/data/keycode-catalog");
const {PD_BINDINGS} = require("../../core/data/pd-bindings");

const PD_SLOT_0 = PD_BINDINGS[0].holdCode;

test("semantic target aliases link the native ABI and stay absent for an unknown one", () => {
    const behaviors = decodedDeviceProfile().domains.keyBehaviors.rows;
    const {aliases} = profileKeyNames({behaviors, actionsKnown: true});
    assert.equal(aliases[keycodes.resolve(PD_SLOT_0).name], "PD_SLOT_0");
    assert.equal(aliases[keycodes.resolve(PD_BINDINGS[5].holdCode).name], "PD_SLOT_5");
    assert.deepEqual(profileKeyNames({behaviors, actionsKnown: false}).aliases, {});
});

test("a key reads by the profile's name for it, under every name it is known by", () => {
    const names = profileKeyNames({
        actionsKnown: true,
        macros: [{keycode: "VIA_MACRO_0", name: "Copy line"}, {keycode: "VIA_MACRO_1", name: ""}],
        pdModes: [{id: 0, kind: 2, name: "Dragscroll"}, {id: 1, kind: 0, name: ""}],
    });
    assert.equal(keyLabel(names, PD_SLOT_0), "Dragscroll · hold", "not the catalogue's bare user slot");
    assert.equal(names.labels.PD_SLOT_0, "Dragscroll · hold");
    assert.equal(keyLabel(names, PD_BINDINGS[1].holdCode), "Slot 1 · hold (empty)", "an empty slot says so");
    assert.equal(keyLabel(names, PD_BINDINGS[0].lockCode), "Dragscroll · toggle");
    assert.equal(names.labels.VIA_MACRO_0, "Copy line");
    assert.equal(names.labels.VIA_MACRO_1, "Macro 1", "an unnamed macro reads by its number");
    assert.equal(keyLabel(names, 0x04), "A", "the rest reads as the catalogue names it");
    assert.equal(keyLabel(profileKeyNames(), PD_SLOT_0), keycodes.resolve(PD_SLOT_0).label, "no profile, no renaming");
});

test("every custom key reads by its name, or by its number without one", () => {
    const names = profileKeyNames({actionsKnown: true, customKeys: [{slot: 0, name: "Right Thumb"}, {slot: 1, name: ""}]});
    assert.equal(keyLabel(names, 0x7e40), "Right Thumb");
    assert.equal(names.labels.CUSTOM_KEY_0, "Right Thumb");
    assert.equal(keyLabel(names, 0x7e41), "Custom key 1");
    assert.equal(keyLabel(names, 0x7e7f), "Custom key 63", "all 64 exist, named or not");
    assert.equal(names.custom.length, 64);
    assert.equal(names.aliases[keycodes.resolve(0x7e40).name], "CUSTOM_KEY_0");
    assert.deepEqual(profileKeyNames({actionsKnown: false}).custom, [], "an unknown ABI numbers its keys otherwise");
});

test("a key's name is added only where it tells two alike labels apart", () => {
    assert.equal(keyLabelWithName({}, 0x1e), "1 (KC_1)");
    assert.equal(keyLabelWithName({}, 0x59), "1 (KC_KP_1)");
    const unknown = keycodes.resolve(0xfffe);
    assert.equal(keyLabelWithName({}, 0xfffe), unknown.label === unknown.name ? unknown.name : `${unknown.label} (${unknown.name})`);
});

test("a layer key names its layer by name; only the raw keycode keeps the number", () => {
    const names = profileKeyNames({layers: ["Base", "Numbers", "Symbols", "Navigation"], actionsKnown: true});
    assert.equal(keyLabel(names, 0x5223), "Hold Navigation", "MO(3)");
    assert.equal(keyLabel(names, 0x5261), "Lock Numbers", "TG is a lock on this firmware");
    assert.equal(keyLabel(names, 0x5282), "One-shot Symbols");
    assert.equal(keyLabel(names, 0x52c1), "Tap-toggle Numbers");
    assert.equal(keyLabel(names, 0x5200), "Move to Base");
    assert.equal(names.labels["LOCK_LAYER(2)"], "Lock Symbols");
    assert.equal(keyLabel(names, 0x4338), "/ / Navigation", "LT(3,KC_SLASH): the tap key and the layer, both named");
    assert.equal(keyLabelWithName(names, 0x4338), "/ / Navigation (LT(3,KC_SLASH))", "the raw keycode, where one is shown, keeps the number");
    assert.equal(keyLabel(profileKeyNames({layers: ["Base", ""]}), 0x5221), "Hold Layer 1", "an unnamed layer reads as the layer tabs name it");
    assert.equal(profileKeyNames({layers: []}).labels["LOCK_LAYER(1)"], undefined, "a layer lock needs the action ABI");
    const lock = keycodes.resolve(require("../../core/schema/compiled-profile-v1").resolveNativeQmkExpression("LOCK_LAYER(2)", {})).name;
    assert.equal(names.aliases[lock], "LOCK_LAYER(2)", "the user slot a layer lock is stored as is known as the lock");
});
