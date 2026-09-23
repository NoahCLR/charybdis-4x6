"use strict";

// The panel's side of a session: what extension.js used to decide inline.
//
// A panel session is a plain object the host keeps per window — the device
// service, the draft, the layer editor, and the outbox the next model carries
// (a notice, an accepted edit, a request to reset forms). Everything here is
// free of VS Code, so it is tested like the rest of core/; extension.js keeps
// only dialogs, files and progress.

const {summary, reorderLayers} = require("../model/portable-profile");
const {ProfileDraftSession, DRAFT_EDITS} = require("./profile-draft-session");
const {buildDeviceModel} = require("./device-model");

const DRAFT_CONTROLS = new Set([
    "reviewProfileDraft", "undoProfileDraft", "redoProfileDraft", "discardProfileDraft",
    "applyProfileDraft", "rebaseProfileDraft", "closeProfileDraftReview",
]);
const PORTABLE_MESSAGES = new Set([
    "exportPdUpgrade", "exportPortableProfile", "choosePortableProfile", "restorePortableProfile",
    "managePortableLayers", "editPortableLayer", "savePortableLayers", "cancelPortableReview",
]);

// A complete read of an eight-layer keyboard opens the draft, or refreshes the
// one already open against what the keyboard now holds.
function observePortable(session, state) {
    const portable = session.service.portable;
    if (!portable || portable === session.observedPortable || portable.incomplete || !state.connected || state.capabilities?.compiledLayerCount !== 8) return;
    if (!session.draft) session.draft = new ProfileDraftSession(portable, state.selectedDeviceId, state.capabilities);
    else session.draft.observe(portable, state.selectedDeviceId);
    session.observedPortable = portable;
}

// The model the webview renders. With a draft open, the editable surfaces come
// from the draft, while the device header and diagnostics stay the keyboard's
// own: the rail describes the keyboard, not the draft.
function buildPanelModel(session, state) {
    observePortable(session, state);
    const device = state.devices?.find((entry) => entry.id === state.selectedDeviceId);
    const busy = Boolean(state.busy || session.portableBusy);
    const editing = session.draft ? session.draft.editingState(state) : state;
    const model = buildDeviceModel({...editing, busy: editing.busy || session.portableBusy, device});
    if (session.draft) {
        model.draft = {...session.draft.view(state), busy};
        if (model.draft.matching) {
            model.profileIdentity = session.draft.identity();
            const names = session.draft.current.summary.names;
            model.layers?.forEach((layer, index) => {layer.displayName = names[index];});
        }
        const actual = buildDeviceModel({
            capabilities: state.capabilities, status: state.status, layout: state.layout, committed: state.committed,
            baseRgb: state.baseRgb, combos: state.combos, macroView: state.macroView, settingsView: state.settingsView,
            busy, device,
        });
        model.device = actual.device;
        model.diagnostics = actual.diagnostics;
        if (model.draft.dirty) model.device.subtitle = "Showing your local draft · the keyboard still runs the last applied profile";
    }
    model.portable = {
        available: Boolean(state.connected && [5, 8].includes(state.capabilities?.compiledLayerCount) && (state.capabilities?.supportedDomainMask & 15) === 15),
        eightLayers: state.capabilities?.compiledLayerCount === 8,
        legacy: state.capabilities?.compiledLayerCount === 5,
        pdUpgradeAvailable: Boolean(state.capabilities?.featureFlags & (1 << 13)),
        busy,
        progress: state.portableProgress,
        review: session.portableReview ? {incoming: summary(session.portableReview.document), current: session.portableReview.before.summary} : null,
        layers: session.portableLayers ? {key: session.portableLayers.before.fingerprint, order: session.portableLayers.order, names: session.portableLayers.names} : null,
    };
    if (!model.draft?.matching) model.layers?.forEach((layer, index) => {layer.displayName = state.portableSummary?.names[index] || layer.name;});
    return model;
}

// What the next model carries once, then forgets.
function takeOutbox(session) {
    const outbox = {notice: session.notice, acceptedEdit: session.acceptedEdit, resetDraftForms: session.resetDraftForms};
    session.notice = undefined;
    session.acceptedEdit = undefined;
    session.resetDraftForms = undefined;
    return outbox;
}

/**
 * Where a message from the webview goes. Draft edits are staged here, since
 * staging is synchronous; everything else names the handler the host runs:
 * "draft" (review, undo, apply…), "portable" (backups, layers), "read", or
 * "none" for a message that only needs an answer. Every refusal throws, and
 * the host turns it into a notice.
 */
function routeMessage(session, message, state) {
    const type = message?.type;
    if (session.portableBusy) return "none";
    if (session.draft && (DRAFT_EDITS.has(type) || DRAFT_CONTROLS.has(type)) && message.draftId !== session.draft.id) {
        throw new Error("This edit belongs to an older draft. Read the keyboard before continuing.");
    }
    if (DRAFT_EDITS.has(type)) {
        // Every change leaves this window through a reviewed draft. Without one
        // (a keyboard whose profile could not be read, or older firmware) the
        // app is read-only; an edit that reaches here is refused, never written.
        if (!session.draft) {
            throw new Error("This keyboard has no editable draft, so the change was not written. Read the keyboard again; if it stays read-only, update both halves to firmware with profile editing.");
        }
        if (state.busy || !state.connected || state.selectedDeviceId !== session.draft.deviceId) {
            throw new Error("Reconnect the keyboard this draft belongs to and wait for its current operation.");
        }
        session.acceptedEdit = session.draft.stage(message);
        return "staged";
    }
    if (DRAFT_CONTROLS.has(type)) return "draft";
    if (PORTABLE_MESSAGES.has(type)) return "portable";
    if (type === "ready" || type === "refresh") return "read";
    return "none";
}

// ── Edit layers: names and order, staged as one layer-reference rewrite ──

function startLayerEdit(before, revision) {
    return {before, revision, order: Array.from({length: 8}, (_, id) => id), names: [...before.summary.names]};
}

// Applies one message from the Edit layers panel to its local state. A name
// is checked by rewriting the document with it, so an invalid one is refused
// before it is kept.
function applyLayerEdit(edit, message) {
    if (!edit) throw new Error("Read the layers again before editing them.");
    if (message.type === "savePortableLayers" && message.names !== undefined) {
        if (!Array.isArray(message.names) || message.names.length !== 8) throw new Error("Read the layers again before naming them.");
        reorderLayers(edit.before.document, edit.order, edit.order.map((old) => message.names[old]));
        edit.names = [...message.names];
        return edit;
    }
    const id = message.id;
    if (!Number.isInteger(id) || id < 0 || id > 7) throw new Error("Read the layers again before editing them.");
    if (message.name !== undefined) {
        if (typeof message.name !== "string") throw new Error("Enter a layer name.");
        const names = [...edit.names];
        names[id] = message.name;
        reorderLayers(edit.before.document, edit.order, edit.order.map((old) => names[old]));
        edit.names = names;
        return edit;
    }
    // The form posts every name with the move, because the rows are rebuilt
    // from this state afterwards: dropping them here would quietly undo
    // whatever was typed before the move.
    if (Array.isArray(message.names) && message.names.length === edit.names.length && message.names.every((name) => typeof name === "string")) {
        reorderLayers(edit.before.document, edit.order, edit.order.map((old) => message.names[old]));
        edit.names = [...message.names];
    }
    const from = edit.order.indexOf(id), to = from + message.direction;
    if (id === 0 || ![1, -1].includes(message.direction) || to < 1 || to > 7) throw new Error("Base stays at the bottom of the layer order.");
    [edit.order[from], edit.order[to]] = [edit.order[to], edit.order[from]];
    return edit;
}

const layerEditDocument = (edit) => reorderLayers(edit.before.document, edit.order, edit.order.map((old) => edit.names[old]));

module.exports = {DRAFT_CONTROLS, PORTABLE_MESSAGES, applyLayerEdit, buildPanelModel, layerEditDocument, observePortable, routeMessage, startLayerEdit, takeOutbox};
