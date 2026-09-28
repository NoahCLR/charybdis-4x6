"use strict";

// The panel's side of a session: what extension.js used to decide inline.
//
// A panel session is a plain object the host keeps per window — the device
// service, the draft, the layer editor, and the outbox the next model carries
// (a notice, an accepted edit, a request to reset forms). Everything here is
// free of VS Code, so it is tested like the rest of core/; extension.js keeps
// only dialogs, files and progress.

const {fingerprint, summary, reorderLayers} = require("../model/portable-profile");
const {profileReview} = require("../model/profile-review");
const {ProfileDraftSession, DRAFT_EDITS} = require("./profile-draft-session");
const {buildDeviceModel, deviceSummary} = require("./device-model");

const DRAFT_CONTROLS = new Set([
    "reviewProfileDraft", "undoProfileDraft", "redoProfileDraft", "jumpProfileDraft", "discardProfileDraft", "discardProfileDraftChanges",
    "applyProfileDraft", "rebaseProfileDraft", "closeProfileDraftReview",
]);
const PORTABLE_MESSAGES = new Set([
    "exportPdUpgrade", "exportPortableProfile", "choosePortableProfile", "restorePortableProfile",
    "managePortableLayers", "editPortableLayer", "savePortableLayers", "cancelPortableReview",
]);

// A complete read of an eight-layer keyboard opens the draft, or refreshes the
// one already open against what the keyboard now holds.
function observePortable(session, state) {
    if (session.draft && !session.draft.dirty && state.connected && state.selectedDeviceId !== session.draft.deviceId) {
        session.draft = undefined;
        session.resetDraftForms = true;
    }
    const portable = session.service.portable;
    if (!portable || portable === session.observedPortable || portable.incomplete || !state.connected || state.capabilities?.compiledLayerCount !== 8) return;
    if (!session.draft) {
        session.draft = new ProfileDraftSession(portable, state.selectedDeviceId, state.capabilities, state.connectionToken);
        session.resetDraftForms = true;
    }
    else session.draft.observe(portable, state.selectedDeviceId, state.connectionToken);
    session.observedPortable = portable;
}

function discardDraftForDevice(session, state, snapshot) {
    session.draft = state.capabilities?.compiledLayerCount === 8
        ? new ProfileDraftSession(snapshot, state.selectedDeviceId, state.capabilities, state.connectionToken)
        : undefined;
    session.portableReview = undefined;
    session.portableLayers = undefined;
    session.resetDraftForms = true;
}

// The model the webview renders. With a draft open, the editable surfaces come
// from the draft, while the device header and diagnostics stay the keyboard's
// own: the rail describes the keyboard, not the draft.
function buildPanelModel(session, state) {
    observePortable(session, state);
    if (session.draft && state.connected && state.selectedDeviceId === session.draft.deviceId) session.draft.noteConnection(state.connectionToken);
    const device = state.devices?.find((entry) => entry.id === state.selectedDeviceId);
    const busy = Boolean(state.busy || session.portableBusy || session.readBusy);
    const editing = session.draft ? session.draft.editingState(state) : state;
    const model = buildDeviceModel({...editing, busy: editing.busy || session.portableBusy || session.readBusy, device});
    model.devices = (state.devices || []).map(({id, label}) => ({id, label}));
    model.selectedDeviceId = state.selectedDeviceId || "";
    if (session.draft) {
        model.draft = {...session.draft.view(state), busy};
        if (session.historyOpen) model.draft.steps = session.draft.steps();
        if (model.draft.matching) {
            model.profileIdentity = session.draft.identity();
            const names = session.draft.current.summary.names;
            model.layers?.forEach((layer, index) => {layer.displayName = names[index];});
        }
        const actual = deviceSummary({
            capabilities: state.capabilities, status: state.status, layout: state.layout, committed: state.committed,
            baseRgb: state.baseRgb, combos: state.combos, macroView: state.macroView, customKeyView: state.customKeyView, settingsView: state.settingsView,
            busy, device,
        });
        model.device = actual.device;
        model.diagnostics = actual.diagnostics;
        if (model.draft.dirty && model.draft.matching) model.device.subtitle = "Showing your local draft · the keyboard still runs the last applied profile";
    }
    // The last Apply's steps: live while it runs, kept when it failed.
    model.apply = state.liveApply && state.liveApply.state !== "idle" ? state.liveApply : null;
    model.postApplyRead = session.postApplyReadStep ? {
        step: session.postApplyReadStep,
        progress: session.postApplyReadStep === "layout" ? state.layout?.progress
            : session.postApplyReadStep === "profile" ? state.committed?.progress : null,
    } : null;
    model.portable = {
        available: Boolean(state.connected && [5, 8].includes(state.capabilities?.compiledLayerCount) && (state.capabilities?.supportedDomainMask & 15) === 15),
        eightLayers: state.capabilities?.compiledLayerCount === 8,
        legacy: state.capabilities?.compiledLayerCount === 5,
        pdUpgradeAvailable: Boolean(state.capabilities?.featureFlags & (1 << 13)),
        busy,
        progress: state.portableProgress,
        review: session.portableReview ? {incoming: summary(session.portableReview.document), current: session.portableReview.before.summary,
            fileName: session.portableReview.fileName || null, differences: importDifferences(session)} : null,
        layers: session.portableLayers ? {key: session.portableLayers.before.fingerprint, order: session.portableLayers.order, names: session.portableLayers.names, keysFollow: session.portableLayers.keysFollow !== false} : null,
    };
    if (!model.draft?.matching) model.layers?.forEach((layer, index) => {layer.displayName = state.portableSummary?.names[index] || layer.name;});
    const editableDraft = Boolean(model.draft?.matching && !model.draft.stale && state.connected);
    const readReady = (session.readReady ?? editableDraft) && editableDraft;
    model.load = {
        state: session.readBusy ? "loading" : readReady ? "ready" : "unavailable",
        phase: state.phase || "idle",
        progress: state.portableProgress || state.layout?.progress || state.committed?.progress || null,
    };
    return model;
}

// A chosen profile file against what the keyboard holds, not the draft: that
// is what applying it would write. Each difference is its unit, what happened
// to it and where it is edited, for the import card to count by category;
// its fields wait for the draft's own review. Nothing, when the keyboard holds an interrupted
// configuration there is no comparing with. Kept with the review, for the
// keyboard it was made against.
function importDifferences(session) {
    const review = session.portableReview, keyboard = session.draft ? session.draft.baseSnapshot : review.before;
    if (!keyboard || keyboard.incomplete) return null;
    if (review.differences?.against !== keyboard.fingerprint) {
        const file = {...keyboard, document: review.document, decoded: undefined, fingerprint: fingerprint(review.document)};
        const where = ({target, ...place}) => place;
        review.differences = {against: keyboard.fingerprint, items: profileReview(keyboard, file).map((item) => ({unit: item.unit, title: item.title,
            status: item.status, place: item.place ? where(item.place) : null}))};
    }
    return review.differences.items;
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
    // The draft history sheet is only a view: while it is open the model
    // carries every step, and only then, since that is the whole history.
    if (type === "openProfileDraftHistory" || type === "closeProfileDraftHistory") {
        session.historyOpen = type === "openProfileDraftHistory";
        return "none";
    }
    if (session.portableBusy || session.readBusy) return "none";
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
        if (state.busy || !state.connected || state.selectedDeviceId !== session.draft.deviceId || (state.connectionToken ?? null) !== session.draft.connectionToken) {
            throw new Error("Reconnect the keyboard this draft belongs to and wait for its current operation.");
        }
        session.acceptedEdit = session.draft.stage(message);
        return "staged";
    }
    if (DRAFT_CONTROLS.has(type)) return "draft";
    if (PORTABLE_MESSAGES.has(type)) return "portable";
    if (type === "ready" || type === "refresh" || type === "selectDevice") {
        if (state.busy) return "none";
        if (type === "selectDevice" && (!state.devices?.some((device) => device.id === message.deviceId) || !message.deviceId)) {
            throw new Error("Choose a keyboard from the current device list.");
        }
        if (type === "selectDevice" && message.deviceId !== state.selectedDeviceId) {
            session.portableReview = undefined;
            session.portableLayers = undefined;
            session.resetDraftForms = true;
        }
        session.readBusy = true;
        session.readReady = false;
        return "read";
    }
    return "none";
}

// ── Rename & Reorder: names and order, staged as one layer-reference rewrite ──

// keysFollow: whether moving a layer renumbers the layer keys that reach it
// (the default), or leaves them reaching whatever layer takes its place.
function startLayerEdit(before, revision) {
    return {before, revision, order: Array.from({length: 8}, (_, id) => id), names: [...before.summary.names], keysFollow: true};
}

// Applies one message from the Rename & Reorder panel to its local state. A name
// is checked by rewriting the document with it, so an invalid one is refused
// before it is kept.
function applyLayerEdit(edit, message) {
    if (!edit) throw new Error("Read the layers again before editing them.");
    if (message.keysFollow !== undefined) {
        if (typeof message.keysFollow !== "boolean") throw new Error("Read the layers again before editing them.");
        edit.keysFollow = message.keysFollow;
        if (message.type === "editPortableLayer" && message.id === undefined) {
            // The form posts every name with the toggle, as with a move.
            if (Array.isArray(message.names) && message.names.length === edit.names.length && message.names.every((name) => typeof name === "string")) {
                reorderLayers(edit.before.document, edit.order, edit.order.map((old) => message.names[old]), {keysFollow: edit.keysFollow});
                edit.names = [...message.names];
            }
            return edit;
        }
    }
    if (message.type === "savePortableLayers" && message.names !== undefined) {
        if (!Array.isArray(message.names) || message.names.length !== 8) throw new Error("Read the layers again before naming them.");
        reorderLayers(edit.before.document, edit.order, edit.order.map((old) => message.names[old]), {keysFollow: edit.keysFollow});
        edit.names = [...message.names];
        return edit;
    }
    const id = message.id;
    if (!Number.isInteger(id) || id < 0 || id > 7) throw new Error("Read the layers again before editing them.");
    if (message.name !== undefined) {
        if (typeof message.name !== "string") throw new Error("Enter a layer name.");
        const names = [...edit.names];
        names[id] = message.name;
        reorderLayers(edit.before.document, edit.order, edit.order.map((old) => names[old]), {keysFollow: edit.keysFollow});
        edit.names = names;
        return edit;
    }
    // The form posts every name with the move, because the rows are rebuilt
    // from this state afterwards: dropping them here would quietly undo
    // whatever was typed before the move.
    if (Array.isArray(message.names) && message.names.length === edit.names.length && message.names.every((name) => typeof name === "string")) {
        reorderLayers(edit.before.document, edit.order, edit.order.map((old) => message.names[old]), {keysFollow: edit.keysFollow});
        edit.names = [...message.names];
    }
    // The bottom slot is the base. A layer reaches it only through Make base,
    // which swaps it with the layer there; a move or a drag stays above it.
    const from = edit.order.indexOf(id);
    if (message.makeBase === true) {
        if (from < 1) throw new Error("That layer is already the base.");
        [edit.order[0], edit.order[from]] = [edit.order[from], edit.order[0]];
        return edit;
    }
    const to = Number.isInteger(message.to) ? message.to : Number.isInteger(message.direction) && [1, -1].includes(message.direction) ? from + message.direction : NaN;
    if (from < 1 || !(to >= 1 && to <= 7)) throw new Error("The base stays at the bottom; use Make base to put another layer there.");
    edit.order.splice(to, 0, ...edit.order.splice(from, 1));
    return edit;
}

const layerEditDocument = (edit) => reorderLayers(edit.before.document, edit.order, edit.order.map((old) => edit.names[old]), {keysFollow: edit.keysFollow !== false});

module.exports = {DRAFT_CONTROLS, PORTABLE_MESSAGES, applyLayerEdit, buildPanelModel, discardDraftForDevice, layerEditDocument, observePortable, routeMessage, startLayerEdit, takeOutbox};
