"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const {applyLayerEdit, buildPanelModel, discardDraftForDevice, layerEditDocument, routeMessage, startLayerEdit, takeOutbox} = require("../../core/session/panel-session");
const {fingerprint, summary, validateSnapshot} = require("../../core/model/portable-profile");
const {document} = require("../fixtures/pd-profile");

const capabilities = {compiledLayerCount: 8, supportedDomainMask: 31, actionAbiDigest: 0x61072732, featureFlags: 1 << 13};
const snapshot = () => {
    const doc = document();
    return {document: doc, fingerprint: fingerprint(doc), summary: summary(doc), limits: {brightnessMax: 200}};
};
const device = {id: "kb", manufacturer: "Bastard Keyboards", product: "Charybdis 4x6"};
const connected = (extra = {}) => ({connected: true, busy: false, selectedDeviceId: "kb", devices: [device], capabilities, ...extra});

// A panel session exactly as extension.js keeps one, with the first complete
// read already in: publishing it opens the draft.
function panelWithDraft() {
    const read = snapshot();
    const session = {service: {portable: read}};
    buildPanelModel(session, connected());
    return session;
}

test("a complete read opens one draft, and later publishes keep it", () => {
    const session = panelWithDraft();
    const draft = session.draft;
    assert.ok(draft, "the draft opens on the first complete read");
    buildPanelModel(session, connected());
    assert.equal(session.draft, draft, "publishing again does not replace it");
    const legacy = {service: {portable: snapshot()}};
    buildPanelModel(legacy, connected({capabilities: {...capabilities, compiledLayerCount: 5}}));
    assert.equal(legacy.draft, undefined, "a five-layer keyboard stays read-only");
});

test("draft edits are staged, and refused when stale, draftless or on the wrong keyboard", () => {
    const session = panelWithDraft();
    const draft = session.draft;
    const edit = {type: "updateLayoutKeys", layer: "Layer 1", changes: [{layoutIndex: 0, keycode: "KC_B"}]};
    assert.equal(routeMessage(session, {...edit, draftId: draft.id, draftRevision: draft.revision}, connected()), "staged");
    assert.equal(draft.dirty, true);
    assert.ok(session.acceptedEdit, "the model reply carries the accepted edit");

    assert.throws(() => routeMessage(session, {...edit, draftId: "older", draftRevision: draft.revision}, connected()), /older draft/);
    assert.throws(() => routeMessage(session, {...edit, draftId: draft.id, draftRevision: draft.revision}, connected({busy: true})), /Reconnect/);
    assert.throws(() => routeMessage(session, {...edit, draftId: draft.id, draftRevision: draft.revision}, connected({selectedDeviceId: "other"})), /Reconnect/);
    assert.throws(() => routeMessage({service: {}}, edit, connected()), /no editable draft/, "no draft means read-only, never a direct write");
    assert.throws(() => routeMessage(session, {type: "undoProfileDraft", draftId: "older"}, connected()), /older draft/, "draft controls are checked too");
});

test("the model carries the draft's steps only while the history sheet is open", () => {
    const session = panelWithDraft();
    assert.equal(buildPanelModel(session, connected()).draft.steps, undefined, "closed, the history stays in the host");
    assert.equal(routeMessage(session, {type: "openProfileDraftHistory"}, connected()), "none");
    const steps = buildPanelModel(session, connected()).draft.steps;
    assert.deepEqual(steps.map((entry) => [entry.label, entry.current]), [["Read from the keyboard", true]]);
    session.readBusy = true;
    routeMessage(session, {type: "closeProfileDraftHistory"}, connected());
    assert.equal(session.historyOpen, false, "closing is heard even while the keyboard is being read");
});

test("every other message names the handler that answers it", () => {
    const session = panelWithDraft();
    const id = session.draft.id;
    assert.equal(routeMessage(session, {type: "undoProfileDraft", draftId: id}, connected()), "draft");
    assert.equal(routeMessage(session, {type: "jumpProfileDraft", draftId: id, step: 0}, connected()), "draft",
        "going to a history entry is a draft control, like undo");
    assert.equal(routeMessage(session, {type: "applyProfileDraft", draftId: id}, connected()), "draft");
    assert.equal(routeMessage(session, {type: "discardProfileDraftChanges", draftId: id, group: 0}, connected()), "draft",
        "discarding part of the draft is a draft control, never an apply");
    assert.equal(routeMessage(session, {type: "choosePortableProfile"}, connected()), "portable");
    assert.equal(routeMessage(session, {type: "refresh"}, connected()), "read");
    assert.equal(session.readBusy, true);
    assert.equal(routeMessage(session, {type: "ready"}, connected()), "none", "a second read cannot overlap the first");
    session.readBusy = false;
    assert.equal(routeMessage(session, {type: "ready"}, connected()), "read");
    session.readBusy = false;
    assert.equal(routeMessage(session, {type: "selectDevice", deviceId: "kb"}, connected()), "read");
    session.readBusy = false;
    session.portableReview = {before: snapshot()};
    session.portableLayers = startLayerEdit(snapshot(), session.draft.revision);
    assert.equal(routeMessage(session, {type: "selectDevice", deviceId: "other"}, connected({devices: [device, {id: "other"}]})), "read");
    assert.equal(session.resetDraftForms, true, "switching devices clears unfinished form text, but keeps the host draft");
    assert.equal(session.portableReview, undefined, "a file review belongs to the previous keyboard");
    assert.equal(session.portableLayers, undefined, "a layer editor belongs to the previous keyboard");
    session.readBusy = false;
    assert.throws(() => routeMessage(session, {type: "selectDevice", deviceId: "missing"}, connected()), /current device list/);
    assert.equal(routeMessage(session, {type: "refresh"}, connected({busy: true})), "none", "another device operation blocks readback");
    assert.equal(routeMessage(session, {type: "somethingNew"}, connected()), "none");
    assert.equal(routeMessage({...session, portableBusy: true}, {type: "refresh"}, connected()), "none", "a busy backup answers without starting anything");
});

test("switching keyboards preserves a dirty draft without showing it as editable on the other device", () => {
    const session = panelWithDraft();
    const original = session.draft;
    const edit = {type: "updateLayoutKeys", layer: "Layer 1", changes: [{layoutIndex: 0, keycode: "KC_B"}]};
    original.stage({...edit, draftRevision: original.revision});
    session.service.portable = snapshot();
    const other = connected({selectedDeviceId: "other", devices: [device, {id: "other", label: "Another Charybdis"}]});
    const model = buildPanelModel(session, other);
    assert.equal(session.draft, original);
    assert.equal(model.draft.matching, false);
    assert.equal(model.selectedDeviceId, "other");
    assert.deepEqual(model.devices.map(({id}) => id), ["kb", "other"]);
    assert.throws(() => routeMessage(session, {...edit, draftId: original.id, draftRevision: original.revision}, other), /Reconnect/);

    original.reset(snapshot());
    session.service.portable = snapshot();
    buildPanelModel(session, other);
    assert.notEqual(session.draft, original, "a clean draft follows the newly selected keyboard");
    assert.equal(session.draft.deviceId, "other");
});

test("the panel marks a retained draft stale after reconnecting to the same device choice", () => {
    const session = {service: {portable: snapshot()}};
    buildPanelModel(session, connected({connectionToken: 1}));
    session.draft.stage({type: "updateLayoutKeys", draftRevision: session.draft.revision, layer: "Layer 1", changes: [{layoutIndex: 0, keycode: "KC_B"}]});
    session.service.portable = snapshot();
    const model = buildPanelModel(session, connected({connectionToken: 2}));
    assert.equal(model.draft.matching, true);
    assert.equal(model.draft.connectionChanged, true);
    assert.equal(model.draft.stale, true);
});

test("a new connection blocks draft edits before portable readback finishes", () => {
    const session = {service: {portable: snapshot()}};
    buildPanelModel(session, connected({connectionToken: 1}));
    session.draft.stage({type: "updateLayoutKeys", draftRevision: session.draft.revision, layer: "Layer 1", changes: [{layoutIndex: 0, keycode: "KC_B"}]});
    session.service.portable = undefined;
    const current = connected({connectionToken: 2});
    const model = buildPanelModel(session, current);
    assert.equal(model.draft.connectionChanged, true);
    assert.equal(model.draft.stale, true);
    assert.throws(() => routeMessage(session, {type: "updateLayoutKeys", draftId: session.draft.id, draftRevision: session.draft.revision}, current), /Reconnect/);
});

test("discarding an old draft on a legacy keyboard clears it without opening an eight-layer draft", () => {
    const session = panelWithDraft();
    const legacy = connected({selectedDeviceId: "old", capabilities: {...capabilities, compiledLayerCount: 5}});
    discardDraftForDevice(session, legacy);
    assert.equal(session.draft, undefined);
    assert.equal(session.resetDraftForms, true);
    assert.equal(buildPanelModel(session, legacy).draft, undefined);
});

test("a clean draft closes automatically when selecting a legacy keyboard", () => {
    const session = panelWithDraft();
    const legacy = connected({selectedDeviceId: "old", capabilities: {...capabilities, compiledLayerCount: 5}});
    assert.equal(buildPanelModel(session, legacy).draft, undefined);
    assert.equal(session.resetDraftForms, true);
});

test("an in-flight read keeps the panel busy across service operation gaps", () => {
    const session = panelWithDraft();
    assert.equal(routeMessage(session, {type: "refresh"}, connected()), "read");
    const model = buildPanelModel(session, connected({busy: false}));
    assert.equal(model.device.health.busy, true);
    assert.equal(model.portable.busy, true);
    assert.equal(model.draft.busy, true);
    assert.equal(routeMessage(session, {type: "refresh"}, connected()), "none");
    session.readBusy = false;
    assert.equal(buildPanelModel(session, connected({busy: false})).device.health.busy, false);
});

test("post-Apply readback keeps the editor visible and reports the current read", () => {
    const session = panelWithDraft();
    session.postApplyReadStep = "layout";
    const model = buildPanelModel(session, connected({busy: true, layout: {state: "reading", progress: {done: 12, total: 56}}}));
    assert.equal(model.load.state, "ready", "the completed editable read stays visible during Apply readback");
    assert.deepEqual(model.postApplyRead, {step: "layout", progress: {done: 12, total: 56}});
    session.postApplyReadStep = "combos";
    assert.deepEqual(buildPanelModel(session, connected({busy: true})).postApplyRead, {step: "combos", progress: null});
});

test("configure screens stay gated until a complete editable read finishes", () => {
    const session = {service: {portable: null}, readBusy: true, readReady: false};
    const partial = connected({layout: {state: "read", layers: []}, committed: {state: "read"}});
    assert.equal(buildPanelModel(session, partial).load.state, "loading", "layout and lighting alone do not expose the board");
    session.readBusy = false;
    assert.equal(buildPanelModel(session, partial).load.state, "unavailable", "a failed complete read remains gated");
    session.service.portable = snapshot();
    session.readReady = true;
    assert.equal(buildPanelModel(session, partial).load.state, "ready", "the complete read opens the editable draft");
    session.readBusy = true;
    session.readReady = false;
    assert.equal(buildPanelModel(session, partial).load.state, "loading", "refresh hides the old board across request gaps");
    session.readBusy = false;
    assert.equal(buildPanelModel(session, partial).load.state, "unavailable", "a failed refresh cannot reveal stale readback");
});

test("the model shows the draft's surfaces but the keyboard's own header", () => {
    const session = panelWithDraft();
    const draft = session.draft;
    draft.stage({type: "updateLayoutKeys", draftId: draft.id, draftRevision: draft.revision, layer: "Layer 1", changes: [{layoutIndex: 0, keycode: "KC_B"}]});
    const model = buildPanelModel(session, connected({status: {committedGeneration: 7}}));
    assert.equal(model.draft.dirty, true);
    assert.equal(model.draft.id, draft.id);
    assert.deepEqual(model.profileIdentity, draft.identity());
    assert.match(model.device.subtitle, /local draft/, "the rail says the keyboard still runs what it ran");
    assert.equal(model.layers[1].displayName, draft.current.summary.names[1]);
    assert.equal(model.portable.available, true);
    assert.equal(model.portable.pdUpgradeAvailable, true);
    assert.equal(model.portable.layers, null, "the layer editor is closed until Rename & Reorder opens it");
});

test("the outbox is carried by one model, then forgotten", () => {
    const session = {notice: "Saved.", acceptedEdit: {revision: 2}, resetDraftForms: true};
    assert.deepEqual(takeOutbox(session), {notice: "Saved.", acceptedEdit: {revision: 2}, resetDraftForms: true});
    assert.deepEqual(takeOutbox(session), {notice: undefined, acceptedEdit: undefined, resetDraftForms: undefined});
});

test("Rename & Reorder renames and reorders, keeps Base at the bottom, and carries names with a move", () => {
    const before = snapshot();
    const edit = startLayerEdit(before, 3);
    assert.deepEqual(edit.order, [0, 1, 2, 3, 4, 5, 6, 7]);
    applyLayerEdit(edit, {type: "editPortableLayer", id: 2, name: "Symbols+"});
    assert.equal(edit.names[2], "Symbols+");
    const typed = [...edit.names];
    typed[4] = "Mouse";
    applyLayerEdit(edit, {type: "editPortableLayer", id: 2, direction: 1, names: typed});
    assert.deepEqual(edit.order.slice(0, 4), [0, 1, 3, 2], "layer 2 moved up one place");
    assert.equal(edit.names[4], "Mouse", "a name typed before the move survives it");
    assert.throws(() => applyLayerEdit(edit, {type: "editPortableLayer", id: 0, direction: 1}), /base stays at the bottom/);
    assert.throws(() => applyLayerEdit(edit, {type: "editPortableLayer", id: 1, direction: -1}), /base stays at the bottom/);
    assert.throws(() => applyLayerEdit(edit, {type: "editPortableLayer", id: 9, name: "x"}), /Read the layers again/);
    assert.throws(() => applyLayerEdit(null, {type: "editPortableLayer", id: 1, name: "x"}), /Read the layers again/);
    assert.throws(() => applyLayerEdit(edit, {type: "savePortableLayers", names: ["only one"]}), /naming them/);

    const saved = validateSnapshot(layerEditDocument(edit));
    assert.equal(saved.settings.names[2], edit.names[3], "the document carries the new order");
    assert.equal(saved.settings.names[3], "Symbols+");
});

test("a layer is dragged to any place above the base, and Make base swaps it with the base", () => {
    const edit = startLayerEdit(snapshot(), 3);
    applyLayerEdit(edit, {type: "editPortableLayer", id: 1, to: 6});
    assert.deepEqual(edit.order, [0, 2, 3, 4, 5, 6, 1, 7], "layer 1 lands in slot 6, the ones between shift down");
    applyLayerEdit(edit, {type: "editPortableLayer", id: 7, to: 1});
    assert.deepEqual(edit.order, [0, 7, 2, 3, 4, 5, 6, 1]);
    assert.throws(() => applyLayerEdit(edit, {type: "editPortableLayer", id: 2, to: 0}), /Make base/);
    assert.throws(() => applyLayerEdit(edit, {type: "editPortableLayer", id: 2, to: 8}), /Make base/);
    assert.throws(() => applyLayerEdit(edit, {type: "editPortableLayer", id: 0, to: 3}), /base stays/);

    applyLayerEdit(edit, {type: "editPortableLayer", id: 5, makeBase: true});
    assert.deepEqual(edit.order, [5, 7, 2, 3, 4, 0, 6, 1], "layer 5 is the base, and the old base takes its slot");
    assert.throws(() => applyLayerEdit(edit, {type: "editPortableLayer", id: 5, makeBase: true}), /already the base/);
    assert.throws(() => applyLayerEdit(edit, {type: "editPortableLayer", id: 5, direction: 1}), /base stays/);
    applyLayerEdit(edit, {type: "editPortableLayer", id: 0, direction: 1});
    assert.deepEqual(edit.order, [5, 7, 2, 3, 4, 6, 0, 1], "the old base moves like any other layer now");
    assert.equal(validateSnapshot(layerEditDocument(edit)).settings.names[0], edit.names[5]);
});
