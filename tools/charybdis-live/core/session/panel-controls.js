"use strict";

// What the panel's controls do, in order: reading the keyboard, the draft's
// own controls (review, undo, discard, rebase, apply) and the portable flows
// (import review, layer editing, restore).
//
// This used to live in extension.js. It is sequencing and session state, not
// VS Code, so it sits here with a test; the host passes in only what a host
// has — a progress indicator, where a recovery copy is written, and a file the
// person chose — as `host`:
//
//   host.progress(title, run)      runs `run` behind a progress indicator
//   host.saveRecovery(document)    writes a recovery copy, returns its path
//   host.chooseProfile()           resolves to a chosen file's text, or undefined

const {validateSnapshot} = require("./portable-profile-session");
const {applyLayerEdit, discardDraftForDevice, layerEditDocument, startLayerEdit} = require("./panel-session");
const {IDENTITY} = require("../model/layer-order");

const run = (host, title, work) => host.progress ? host.progress(title, work) : work();
// The other half confirmed its copy before the keyboard switched, but the
// cable between the halves may have come out since; say so instead of
// claiming a readback of both halves that did not happen.
const savedNotice = (result, verb) => result?.peerUnseen
    ? `Complete profile ${verb} both halves and active. The other half confirmed its copy but is not connected now; plug the cable between the halves back in.`
    : `Complete profile ${verb} both halves and verified.`;

// After the keyboard stores a new profile, everything the panel shows about
// it is read again, in the order the reads depend on.
async function rereadKeyboard(service) {
    await service.readLayout();
    await service.readCommittedProfile();
    await service.readCombos();
    await service.readBaseRgb();
}

// Connect if needed, then read what the keyboard runs. The notice says what
// was read and what could not be; a keyboard with no committed profile is a
// normal state, so that failure keeps the layout that was read.
async function readKeyboard(session, selectedDeviceId, host = {}) {
    const service = session.service;
    await service.enumerate();
    const devices = service.snapshot().devices;
    if (!devices.length) {
        session.notice = "No Charybdis Raw HID interface found. Connect the keyboard and choose Read keyboard again.";
        return false;
    }
    if (selectedDeviceId && !devices.some((device) => device.id === selectedDeviceId)) {
        throw new Error("The selected keyboard is no longer available. Read the device list again.");
    }
    if (!service.snapshot().connected || (selectedDeviceId && service.snapshot().selectedDeviceId !== selectedDeviceId)) {
        const deviceId = selectedDeviceId || devices[0].id;
        await service.connect(deviceId);
        const connected = service.snapshot();
        if (!connected.connected || connected.selectedDeviceId !== deviceId) {
            throw new Error(connected.error?.message || "Could not connect to the selected keyboard.");
        }
    }
    await service.refresh();
    await run(host, "Reading layout from the keyboard", () => service.readLayout());
    try {
        await run(host, "Reading keyboard profile", () => service.readCommittedProfile());
    } catch (error) {
        session.notice = `Read the layout. The committed profile could not be read: ${error instanceof Error ? error.message : String(error)}`;
        return false;
    }
    await service.readBaseRgb();
    await service.readCombos();
    let macroFailure = "", portable;
    if ((service.capabilities?.supportedDomainMask & 15) === 15) {
        try {portable = await service.readPortableProfile();}
        catch (error) {macroFailure = " Macros and global settings could not be read: " + error.message;}
    }
    const state = service.snapshot();
    if (state.error || state.committed?.state !== "read") {
        session.notice = `The keyboard profile could not be read: ${state.error?.message || "no verified profile was returned"}.`;
    } else {
        const description = state.committed.source === "compiled"
            ? "the keyboard's compiled defaults"
            : `committed profile generation ${state.committed.generation}`;
        const failures = state.committed.failures?.length || 0;
        session.notice = `Read the layout and ${description}.` + (failures ? ` ${failures} domain(s) could not be decoded; see diagnostics.` : "");
    }
    if (macroFailure) session.notice += macroFailure;
    return Boolean(!state.error && state.committed?.state === "read" && state.layout?.state === "read"
        && state.capabilities?.compiledLayerCount === 8 && portable && !portable.incomplete);
}

// The draft's own controls. Each leaves the session's outbox (notice, form
// reset) saying what happened; the caller publishes.
async function draftControl(session, message, host = {}) {
    const draft = session.draft, service = session.service;
    if (!draft) throw new Error("Read a complete keyboard profile before editing.");
    const revision = message.draftRevision;
    switch (message.type) {
        case "reviewProfileDraft": draft.review(revision); return;
        case "closeProfileDraftReview": draft.closeReview(revision); return;
        case "undoProfileDraft": draft.undo(revision); session.resetDraftForms = true; return;
        case "redoProfileDraft": draft.redo(revision); session.resetDraftForms = true; return;
        case "jumpProfileDraft": draft.jump(revision, message.step); session.resetDraftForms = true; return;
        case "discardProfileDraftChanges": draft.discard(revision, message.group); session.resetDraftForms = true; return;
        case "discardProfileDraft": {
            // The draft's own keyboard, unchanged: discarding is an undoable
            // step. Otherwise the keyboard is read again and the draft starts
            // over from it.
            if (!draft.stale && !draft.base.incomplete && service.snapshot().selectedDeviceId === draft.deviceId) {
                draft.discardAll(revision);
                session.resetDraftForms = true;
                session.notice = "Draft discarded. Undo (⌘Z) brings it back.";
                return;
            }
            draft.assertRevision(revision, {allowStale: true});
            const state = service.snapshot();
            if (!state.connected) throw new Error("Reconnect and read the keyboard before discarding its draft.");
            const eight = state.capabilities?.compiledLayerCount === 8;
            discardDraftForDevice(session, state, eight ? await service.readPortableProfile() : undefined);
            session.notice = eight ? "Draft discarded. Showing the saved keyboard configuration." : "Draft discarded. This keyboard remains read-only.";
            return;
        }
        case "rebaseProfileDraft": {
            draft.assertRevision(revision, {allowStale: true});
            if (service.snapshot().selectedDeviceId !== draft.deviceId) throw new Error("Reconnect the keyboard this draft belongs to.");
            const snapshot = await service.readPortableProfile({forRestore: true});
            draft.observe(snapshot, draft.deviceId, service.snapshot().connectionToken);
            draft.rebase(revision);
            session.resetDraftForms = true;
            session.notice = "Review now compares your draft with the latest keyboard state. Apply will replace the differences shown.";
            return;
        }
        case "applyProfileDraft": {
            // The interface asks about active traps and warnings; the host
            // enforces that decision before writing a recovery copy or HID.
            if (draft.hasBlockers()) throw new Error("Resolve the blockers in Checks before applying this profile.");
            if (draft.hasChecksToConfirm() && message.confirmChecks !== true) throw new Error("This draft has warnings or traps. Confirm them in the review before applying.");
            let recovery = "";
            const result = await run(host, "Applying the complete profile to both halves",
                () => draft.apply(service, revision, async (document) => (recovery = await host.saveRecovery(document))));
            session.resetDraftForms = true;
            await rereadKeyboard(service);
            session.notice = `${savedNotice(result, "applied to")} Recovery copy: ${recovery}`;
            return;
        }
        default: throw new Error("Unsupported draft control.");
    }
}

// The portable flows that are not just file dialogs: reviewing an import,
// editing layer names and order, and keeping either in the draft — or, on a
// keyboard with no draft, restoring it to the keyboard.
async function portableControl(session, message, host = {}) {
    const service = session.service;
    if (message.type === "savePortableLayers" && message.names !== undefined) applyLayerEdit(session.portableLayers, message);
    switch (message.type) {
        case "cancelPortableReview":
            session.portableReview = undefined; session.portableLayers = undefined;
            return;
        case "choosePortableProfile": {
            // The host answers with the file's text, and its name when it has one.
            const chosen = await host.chooseProfile?.();
            if (chosen === undefined) return;
            const value = validateSnapshot(typeof chosen === "string" ? chosen : chosen.text, service.capabilities);
            session.portableReview = {document: value.document, fileName: typeof chosen === "string" ? null : chosen.name || null,
                before: session.draft?.current || await service.readPortableProfile({forRestore: true}), revision: session.draft?.revision,
                deviceId: service.snapshot().selectedDeviceId, draftId: session.draft?.id};
            session.portableLayers = undefined;
            return;
        }
        case "managePortableLayers": {
            const before = session.draft?.current || await service.readPortableProfile();
            session.portableReview = undefined;
            session.portableLayers = startLayerEdit(before, session.draft?.revision);
            session.portableLayers.deviceId = service.snapshot().selectedDeviceId;
            session.portableLayers.draftId = session.draft?.id;
            return;
        }
        case "editPortableLayer":
            applyLayerEdit(session.portableLayers, message);
            return;
        case "savePortableLayers":
        case "restorePortableProfile": {
            const layers = message.type === "savePortableLayers";
            const review = session.portableReview, edit = session.portableLayers;
            const before = layers ? edit?.before : review?.before;
            if (!before) throw new Error("Review the profile before restoring it.");
            const editorDeviceId = layers ? edit.deviceId : review.deviceId;
            const selectedDeviceId = service.snapshot().selectedDeviceId;
            if (editorDeviceId !== undefined && editorDeviceId !== selectedDeviceId) {
                throw new Error("This profile review belongs to another keyboard. Open it again on the selected keyboard.");
            }
            if (session.draft && ((layers ? edit.revision : review.revision) !== session.draft.revision ||
                (layers ? edit.draftId : review.draftId) !== session.draft.id)) {
                throw new Error("This profile review belongs to an older draft. Open it again on the selected keyboard.");
            }
            if (session.draft && selectedDeviceId !== session.draft.deviceId) {
                throw new Error("This profile review belongs to another keyboard. Open it again on the selected keyboard.");
            }
            const document = layers ? layerEditDocument(edit) : review.document;
            if (session.draft) {
                // An import has no layer order to keep: its layers are
                // compared slot by slot with the keyboard's.
                if (layers) session.draft.editLayers(document, edit.revision, edit.order);
                else session.draft.replace(document, review.revision, "edit", "Imported a profile", IDENTITY);
                session.portableReview = undefined; session.portableLayers = undefined;
                session.resetDraftForms = true;
                return;
            }
            let recovery = "";
            const result = await service.restorePortableProfile(document, {expectedFingerprint: before.fingerprint,
                saveRecovery: async (copy) => (recovery = await host.saveRecovery(copy))});
            session.portableReview = undefined; session.portableLayers = undefined;
            await rereadKeyboard(service);
            session.notice = savedNotice(result, "saved to") + " " + (before.incomplete ? "Interrupted data retained for diagnosis: " : "Recovery copy: ") + recovery;
            return;
        }
        default: throw new Error("Unsupported portable control.");
    }
}

module.exports = {readKeyboard, draftControl, portableControl, rereadKeyboard};
