"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const {draftControl, portableControl, readKeyboard, rereadKeyboard} = require("../../core/session/panel-controls");
const {buildPanelModel} = require("../../core/session/panel-session");
const {fingerprint, summary} = require("../../core/model/portable-profile");
const {document} = require("../fixtures/pd-profile");

const capabilities = {compiledLayerCount: 8, supportedDomainMask: 31, actionAbiDigest: 0x61072732, featureFlags: 1 << 13};
const snapshot = () => {
    const doc = document();
    return {document: doc, fingerprint: fingerprint(doc), summary: summary(doc), limits: {brightnessMax: 200}};
};
const device = {id: "kb", manufacturer: "Bastard Keyboards", product: "Charybdis 4x6"};

// A device service that records what it is asked, in order.
function fakeService(extra = {}) {
    const calls = [];
    const record = (name, value) => async (...args) => { calls.push(name); return typeof value === "function" ? value(...args) : value; };
    const state = {connected: true, busy: false, selectedDeviceId: "kb", devices: [device], capabilities, committed: {state: "read", source: "device", generation: 7, failures: []}};
    const service = {
        calls, capabilities, portable: snapshot(),
        snapshot: () => state,
        enumerate: record("enumerate"), connect: record("connect"), refresh: record("refresh"),
        readLayout: record("readLayout"), readCommittedProfile: record("readCommittedProfile"),
        readBaseRgb: record("readBaseRgb"), readCombos: record("readCombos"),
        readPortableProfile: record("readPortableProfile", () => service.portable),
        restorePortableProfile: record("restorePortableProfile", async (doc, options) => {
            const path = await options.saveRecovery(doc);
            assert.ok(path, "a recovery copy is written before the keyboard is changed");
            return {...snapshot(), document: doc, fingerprint: fingerprint(doc)};
        }),
        ...extra,
    };
    return service;
}
const host = () => {
    const saved = [];
    return {saved, progress: async (title, work) => work(), saveRecovery: async (doc) => { saved.push(doc); return `/recovery/${saved.length}.json`; }};
};
function sessionWithDraft() {
    const service = fakeService();
    const session = {service};
    buildPanelModel(session, service.snapshot());
    return session;
}
const stage = (session, change) => session.draft.stage({draftId: session.draft.id, draftRevision: session.draft.revision, ...change});

test("reading the keyboard connects, reads in dependency order and says what it read", async () => {
    const service = fakeService();
    const session = {service};
    await readKeyboard(session, undefined, host());
    assert.deepEqual(service.calls, ["enumerate", "refresh", "readLayout", "readCommittedProfile", "readBaseRgb", "readCombos", "readPortableProfile"]);
    assert.match(session.notice, /committed profile generation 7/);
    const empty = {service: fakeService({snapshot: () => ({devices: []})})};
    await readKeyboard(empty, undefined, host());
    assert.match(empty.notice, /No Charybdis Raw HID interface/);
});

test("apply writes a recovery copy through the host, then reads the keyboard again", async () => {
    const session = sessionWithDraft(), draft = session.draft, h = host();
    stage(session, {type: "updateLayoutKeys", layers: [{layer: "Layer 0", changes: [{layoutIndex: 0, keycode: "KC_A"}]}]});
    draft.review(draft.revision);
    session.service.calls.length = 0;
    await draftControl(session, {type: "applyProfileDraft", draftRevision: draft.revision}, h);
    assert.equal(h.saved.length, 1);
    assert.deepEqual(session.service.calls, ["restorePortableProfile", "readLayout", "readCommittedProfile", "readCombos", "readBaseRgb"]);
    assert.match(session.notice, /Recovery copy: \/recovery\/1\.json/);
    assert.equal(session.resetDraftForms, true);
});

test("discarding the draft of an unchanged keyboard is an undoable step, and leaving review un-reviews", async () => {
    const session = sessionWithDraft(), draft = session.draft;
    stage(session, {type: "updateLayoutKeys", layers: [{layer: "Layer 0", changes: [{layoutIndex: 0, keycode: "KC_A"}]}]});
    draft.review(draft.revision);
    await draftControl(session, {type: "closeProfileDraftReview", draftRevision: draft.revision});
    assert.equal(draft.view({}).reviewed, false);
    await draftControl(session, {type: "discardProfileDraft", draftRevision: draft.revision});
    assert.equal(session.draft, draft, "the same draft, one step further");
    assert.equal(draft.dirty, false);
    assert.match(session.notice, /Undo/);
    assert.equal(draft.view({}).canUndo, true);
    await assert.rejects(draftControl(session, {type: "somethingElse", draftRevision: draft.revision}), /Unsupported draft control/);
});

test("layer edits and imports are kept in the draft; without one they restore to the keyboard", async () => {
    const session = sessionWithDraft(), draft = session.draft;
    await portableControl(session, {type: "managePortableLayers"});
    assert.ok(session.portableLayers);
    const names = [...session.portableLayers.names];
    names[4] = "Mouse";
    await portableControl(session, {type: "savePortableLayers", names});
    assert.equal(session.portableLayers, undefined);
    assert.equal(draft.current.summary.names[4], "Mouse", "the rename went into the draft");
    assert.equal(draft.view({}).undoLabel, "Edited layers");

    const text = JSON.stringify(document());
    await portableControl(session, {type: "choosePortableProfile"}, {chooseProfile: async () => text});
    assert.ok(session.portableReview, "a chosen file opens for review first");
    await portableControl(session, {type: "cancelPortableReview"});
    assert.equal(session.portableReview, undefined);

    const bare = {service: fakeService()}, h = host();
    bare.portableReview = {document: document(), before: bare.service.portable};
    await portableControl(bare, {type: "restorePortableProfile"}, h);
    assert.deepEqual(bare.service.calls, ["restorePortableProfile", "readLayout", "readCommittedProfile", "readCombos", "readBaseRgb"]);
    assert.match(bare.notice, /Recovery copy: \/recovery\/1\.json/);
});

test("the layer panel's keys-follow toggle decides whether layer keys are renumbered", async () => {
    // Base gets MO(4) and TG(1); layer 4 then moves down to 1, swapping past 3 and 2.
    const moveLayer4Down = async (keysFollow) => {
        const session = sessionWithDraft(), draft = session.draft;
        stage(session, {type: "updateLayoutKeys", layers: [{layer: "Layer 0", changes: [{layoutIndex: 0, keycode: "MO(4)"}, {layoutIndex: 1, keycode: "TG(1)"}]}]});
        await portableControl(session, {type: "managePortableLayers"});
        assert.equal(session.portableLayers.keysFollow, true, "on by default");
        const layer4Name = session.portableLayers.names[4];
        if (!keysFollow) {
            await portableControl(session, {type: "editPortableLayer", keysFollow: false, names: session.portableLayers.names.map((name, id) => id === 2 ? "Typed" : name)});
            assert.equal(session.portableLayers.keysFollow, false);
            assert.equal(session.portableLayers.names[2], "Typed", "a name typed before the toggle is kept");
        }
        for (let step = 0; step < 3; step++) await portableControl(session, {type: "editPortableLayer", id: 4, direction: -1, names: session.portableLayers.names});
        const before = draft.current.document;
        await portableControl(session, {type: "savePortableLayers", names: session.portableLayers.names});
        const after = draft.current.document;
        assert.deepEqual(after.layers[1].slice(2), before.layers[4].slice(2), "layer 4 moved to 1 with its keys");
        assert.equal(draft.current.summary.names[1], layer4Name, "its name moved with it");
        return after.layers[0].slice(0, 2);
    };
    assert.deepEqual(await moveLayer4Down(true), [0x5221, 0x5262], "following: MO(4) becomes MO(1), TG(1) becomes TG(2)");
    assert.deepEqual(await moveLayer4Down(false), [0x5224, 0x5261], "not following: both keep their numbers");
});

test("a re-read after a save follows the order the reads depend on", async () => {
    const service = fakeService();
    await rereadKeyboard(service);
    assert.deepEqual(service.calls, ["readLayout", "readCommittedProfile", "readCombos", "readBaseRgb"]);
});
