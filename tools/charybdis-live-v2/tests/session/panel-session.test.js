"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const {applyLayerEdit, buildPanelModel, layerEditDocument, routeMessage, startLayerEdit, takeOutbox} = require("../../core/session/panel-session");
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

test("every other message names the handler that answers it", () => {
    const session = panelWithDraft();
    const id = session.draft.id;
    assert.equal(routeMessage(session, {type: "undoProfileDraft", draftId: id}, connected()), "draft");
    assert.equal(routeMessage(session, {type: "applyProfileDraft", draftId: id}, connected()), "draft");
    assert.equal(routeMessage(session, {type: "choosePortableProfile"}, connected()), "portable");
    assert.equal(routeMessage(session, {type: "refresh"}, connected()), "read");
    assert.equal(routeMessage(session, {type: "ready"}, connected()), "read");
    assert.equal(routeMessage(session, {type: "somethingNew"}, connected()), "none");
    assert.equal(routeMessage({...session, portableBusy: true}, {type: "refresh"}, connected()), "none", "a busy backup answers without starting anything");
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
    assert.equal(model.portable.layers, null, "the layer editor is closed until Edit layers opens it");
});

test("the outbox is carried by one model, then forgotten", () => {
    const session = {notice: "Saved.", acceptedEdit: {revision: 2}, resetDraftForms: true};
    assert.deepEqual(takeOutbox(session), {notice: "Saved.", acceptedEdit: {revision: 2}, resetDraftForms: true});
    assert.deepEqual(takeOutbox(session), {notice: undefined, acceptedEdit: undefined, resetDraftForms: undefined});
});

test("Edit layers renames and reorders, keeps Base at the bottom, and carries names with a move", () => {
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
    assert.throws(() => applyLayerEdit(edit, {type: "editPortableLayer", id: 0, direction: 1}), /Base stays/);
    assert.throws(() => applyLayerEdit(edit, {type: "editPortableLayer", id: 1, direction: -1}), /Base stays/);
    assert.throws(() => applyLayerEdit(edit, {type: "editPortableLayer", id: 9, name: "x"}), /Read the layers again/);
    assert.throws(() => applyLayerEdit(null, {type: "editPortableLayer", id: 1, name: "x"}), /Read the layers again/);
    assert.throws(() => applyLayerEdit(edit, {type: "savePortableLayers", names: ["only one"]}), /naming them/);

    const saved = validateSnapshot(layerEditDocument(edit));
    assert.equal(saved.settings.names[2], edit.names[3], "the document carries the new order");
    assert.equal(saved.settings.names[3], "Symbols+");
});
