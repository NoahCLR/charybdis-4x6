"use strict";

// Charybdis Live — extension host.
//
// Thin on purpose. It owns the VS Code surface (command, panel, message relay)
// and nothing else. Device, protocol and profile decisions live in core/, which
// has no vscode import so this shell can be replaced by a standalone app later
// without touching it. Nothing here parses a firmware repository; see
// docs/LIVE_EDIT_APP_DIRECTION.md.
//
// The webview is Profile Studio's editing UI, ported verbatim. It renders a
// `model` and posts typed edits back, so this file's job is to build that model
// from the keyboard and turn those edits into device writes.

const vscode = require("vscode");

const {validateSnapshot, summary, reorderLayers} = require("./core/session/portable-profile-session");
const {ProfileDeviceService} = require("./core/session/profile-device-service");
const {ProfileDraftSession, DRAFT_EDITS} = require("./core/session/profile-draft-session");
const {buildDeviceModel} = require("./core/session/device-model");
const {getStudioHtml} = require("./webview/studio-ui");

const VIEW_TYPE = "charybdisLive.panel";

function activate(context) {
    context.subscriptions.push(
        vscode.commands.registerCommand("charybdisLive.open", () => openPanel(context))
    );

    const status = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, 2);
    status.text = "$(radio-tower) Charybdis Live";
    status.tooltip = "Open Charybdis Live — edit the connected keyboard";
    status.command = "charybdisLive.open";
    status.show();
    context.subscriptions.push(status);
}

function deactivate() {}

function openPanel(context) {
    const panel = vscode.window.createWebviewPanel(VIEW_TYPE, "Charybdis Live", vscode.ViewColumn.One, {
        enableScripts: true,
        retainContextWhenHidden: true,
    });

    const session = {service: undefined, notice: undefined, recoveryRoot: context.globalStorageUri};
    session.service = new ProfileDeviceService({
        onChange: () => publish(panel, session),
    });

    panel.webview.html = getStudioHtml();
    panel.webview.onDidReceiveMessage((message) => handleMessage(panel, session, message));
    panel.onDidDispose(() => {
        void session.service.close();
    });
}

function publish(panel, session) {
    const state = session.service.snapshot();
    const portable = session.service.portable;
    if (portable && portable !== session.observedPortable && !portable.incomplete && state.connected && state.capabilities?.compiledLayerCount === 8) {
        if (!session.draft) session.draft = new ProfileDraftSession(portable, state.selectedDeviceId, state.capabilities);
        else session.draft.observe(portable, state.selectedDeviceId);
        session.observedPortable = portable;
    }
    const editing = session.draft ? session.draft.editingState(state) : state;
    const modelState = {
            ...editing,
            busy: editing.busy || session.savingBehavior || session.portableBusy,
            device: state.devices.find((device) => device.id === state.selectedDeviceId),
        };
    const model = buildDeviceModel(modelState);
    if (session.draft) {
        model.draft = {...session.draft.view(state), busy: Boolean(state.busy || session.portableBusy)};
        if (model.draft.matching) {
            model.profileIdentity = session.draft.identity();
            const names = session.draft.current.summary.names;
            model.layers?.forEach((layer, i) => {layer.displayName = names[i];});
        }
        const actual = buildDeviceModel({
            capabilities: state.capabilities,
            status: state.status,
            layout: state.layout,
            committed: state.committed,
            baseRgb: state.baseRgb,
            combos: state.combos,
            macroView: state.macroView,
            settingsView: state.settingsView,
            busy: state.busy || session.savingBehavior || session.portableBusy,
            device: state.devices.find((device) => device.id === state.selectedDeviceId),
        });
        model.device = actual.device;
        model.diagnostics = actual.diagnostics;
        if (model.draft.dirty) model.device.subtitle = "Showing your local draft · the keyboard still runs the last applied profile";
    }
    model.portable = {
        available: state.connected && [5, 8].includes(state.capabilities?.compiledLayerCount) && (state.capabilities?.supportedDomainMask & 15) === 15,
        eightLayers: state.capabilities?.compiledLayerCount === 8,
        legacy: state.capabilities?.compiledLayerCount === 5,
        busy: state.busy || session.portableBusy,
        progress: state.portableProgress,
        review: session.portableReview ? {incoming: summary(session.portableReview.document), current: session.portableReview.before.summary} : null,
        layers: session.portableLayers ? {key: session.portableLayers.before.fingerprint, order: session.portableLayers.order, names: session.portableLayers.names} : null,
    };
    if (!model.draft?.matching) model.layers?.forEach((layer, index) => {layer.displayName = state.portableSummary?.names[index] || layer.name;});
    void panel.webview.postMessage({type: "model", model,
        notice: session.notice,
        savedBehavior: session.savedBehavior,
        savedMacro: session.savedMacro,
        savedSettings: session.savedSettings,
        acceptedEdit: session.acceptedEdit,
        resetDraftForms: session.resetDraftForms,
    });
    session.notice = undefined;
    session.savedBehavior = undefined;
    session.savedMacro = undefined;
    session.savedSettings = undefined;
    session.acceptedEdit = undefined;
    session.resetDraftForms = undefined;
}

// The webview sets its own "Working..." status on every message it posts, and
// only a model reply clears it. So every path through here must publish,
// including the ones that decline or fail — otherwise the panel sits on
// "Working..." with no way to know anything went wrong.
async function handleMessage(panel, session, message) {
    try {
        if (session.portableBusy) {publish(panel, session); return;}
        if (session.draft && (DRAFT_EDITS.has(message?.type) || /^(?:review|undo|redo|discard|apply|rebase|close)ProfileDraft/.test(message?.type || "")) && message.draftId !== session.draft.id) throw new Error("This edit belongs to an older draft. Read the keyboard before continuing.");
        if (session.draft && DRAFT_EDITS.has(message?.type)) {
            const state = session.service.snapshot();
            if (state.busy || !state.connected || state.selectedDeviceId !== session.draft.deviceId) throw new Error("Reconnect the keyboard this draft belongs to and wait for its current operation.");
            session.acceptedEdit = session.draft.stage(message);
            if (message.reviewAfter) session.draft.review(session.draft.revision);
            session.notice = "Changes kept in this app. Review changes to apply them to the keyboard.";
            publish(panel, session);
            return;
        }
        if (["reviewProfileDraft", "undoProfileDraft", "redoProfileDraft", "discardProfileDraft", "applyProfileDraft", "rebaseProfileDraft", "closeProfileDraftReview"].includes(message?.type)) {
            await draftMessage(panel, session, message);
            return;
        }
        switch (message?.type) {
            case "exportPortableProfile":
            case "choosePortableProfile":
            case "restorePortableProfile":
            case "managePortableLayers":
            case "editPortableLayer":
            case "savePortableLayers":
            case "cancelPortableReview":
                await portableMessage(panel, session, message);
                return;
            case "ready":
            case "refresh":
                await connectAndRead(panel, session);
                return;
            // The keyboard view's apply posts updateLayoutKeys; the header's
            // Apply all posts applyAllChanges with the same layout edits under
            // a different field, plus layer structure this app cannot change.
            case "updateLayoutKeys":
                await writeLayoutKeys(panel, session, {layers: message.layers, changes: message.changes, layer: message.layer});
                return;
            case "applyAllChanges":
                await writeLayoutKeys(panel, session, {
                    layers: message.layoutGroups,
                    structural: (message.adds?.length || 0) + (message.deletes?.length || 0),
                });
                return;

            // Studio surfaces these against a repository. This app has none,
            // and the domains behind them need the committed payload read.
            // Say so rather than failing silently.
            case "selectProfile":
            case "requestCreateProfile":
            case "requestCloneProfile":
            case "requestRenameProfile":
            case "requestDeleteProfile":
                session.notice = "Charybdis Live edits the connected keyboard and has no profile files. Use Profile Studio for source profiles.";
                publish(panel, session);
                return;
            case "applyLayerChanges":
                session.notice = "This edit is not connected to the profile writer yet.";
                publish(panel, session);
                return;
            case "saveBehavior":
            case "addBehavior":
            case "deleteBehavior":
                session.savingBehavior = true;
                try {
                    await vscode.window.withProgress(
                        {location: vscode.ProgressLocation.Notification, title: "Saving behaviour to both halves"},
                        () => session.service.saveProfileEdit(message)
                    );
                    session.savedBehavior = message.type === "addBehavior" ? "new" : message.behavior?.keycode || message.keycode;
                    session.notice = "Saved to both halves and verified by reading the profile back.";
                } finally {session.savingBehavior = false;}
                publish(panel, session);
                return;
            case "updateConfigDefaults":
                session.portableBusy = true;
                try {
                    await vscode.window.withProgress(
                        {location: vscode.ProgressLocation.Notification, title: "Saving settings to both halves"},
                        () => session.service.saveSettingsEdit(message, {saveRecovery: document => saveRecoveryFile(session, document)})
                    );
                    await session.service.readCommittedProfile();
                    await session.service.readBaseRgb();
                    await session.service.readCombos();
                    session.savedSettings = {sectionId: message.sectionId, fields: message.fields, expectedFingerprint: message.expectedFingerprint};
                    session.notice = "Settings saved to both halves and verified. Recovery copy: " + session.lastRecovery.fsPath;
                } finally {session.portableBusy = false;}
                publish(panel, session);
                return;
            case "updateViaMacro":
                session.portableBusy = true;
                try {
                    await vscode.window.withProgress(
                        {location: vscode.ProgressLocation.Notification, title: "Saving macro to both halves"},
                        () => session.service.saveMacroEdit(message, {saveRecovery: document => saveRecoveryFile(session, document)})
                    );
                    await session.service.readCommittedProfile();
                    session.savedMacro = {keycode: message.keycode, payload: message.payload, expectedFingerprint: message.expectedFingerprint};
                    session.notice = "Macro saved to both halves and verified. Recovery copy: " + session.lastRecovery.fsPath;
                } finally {session.portableBusy = false;}
                publish(panel, session);
                return;
            case "addCombo":
            case "saveCombo":
            case "deleteCombo":
            case "updateComboHoldTerm":
            case "saveRgbReusableLedGroup":
            case "deleteRgbReusableLedGroup":
            case "updateLayerColor":
            case "updatePdModeColor":
            case "updateAutomouseFade":
            case "updateComboFeedback":
            case "updateKeyBehaviorFeedback":
            case "addRgbLedGroup":
            case "deleteRgbLedGroup":
            case "updateRgbStages":
                await vscode.window.withProgress(
                    {location: vscode.ProgressLocation.Notification, title: "Saving keyboard profile to both halves"},
                    () => session.service.saveProfileEdit(message)
                );
                session.notice = "Saved to both halves and verified by reading the profile back.";
                publish(panel, session);
                return;
            default:
                // Even an unrecognised message has already put the panel into
                // "Working...", so answer it.
                publish(panel, session);
                return;
        }
    } catch (error) {
        const text = error instanceof Error ? error.message : String(error);
        const code = error?.code ? ` [${error.code}]` : "";
        vscode.window.showErrorMessage(`Charybdis Live: ${text}`);
        // Put it in the panel too. A toast is easy to miss and disappears,
        // and the panel is where someone looks when it seems stuck.
        session.notice = `Failed${code}: ${text}`;
        publish(panel, session);
    }
}

// Connect, learn what the keyboard is, and read what it is running. Studio's
// Reload button maps onto this: there it re-read the files, here the device.
async function connectAndRead(panel, session) {
    const service = session.service;
    await service.enumerate();
    const devices = service.snapshot().devices;
    if (!devices.length) {
        session.notice = "No Charybdis Raw HID interface found. Connect the keyboard and reload.";
        publish(panel, session);
        return;
    }

    if (!service.snapshot().connected) {
        let deviceId = devices[0].id;
        if (devices.length > 1) {
            const picked = await vscode.window.showQuickPick(
                devices.map((device) => ({
                    label: [device.manufacturer, device.product].filter(Boolean).join(" ") || "Charybdis",
                    description: device.id,
                    id: device.id,
                })),
                {title: "Several Charybdis interfaces matched"}
            );
            if (!picked) {
                return;
            }
            deviceId = picked.id;
        }
        await service.connect(deviceId);
    }

    await service.refresh();
    await vscode.window.withProgress(
        {location: vscode.ProgressLocation.Notification, title: "Reading layout from the keyboard"},
        () => service.readLayout()
    );

    // A keyboard with no committed profile is a normal state, so a failure here
    // must not discard the layout read that already succeeded.
    try {
        await vscode.window.withProgress(
            {location: vscode.ProgressLocation.Notification, title: "Reading keyboard profile"},
            () => service.readCommittedProfile()
        );
    } catch (error) {
        const text = error instanceof Error ? error.message : String(error);
        session.notice = `Read the layout. The committed profile could not be read: ${text}`;
        publish(panel, session);
        return;
    }

    await service.readBaseRgb();
    await service.readCombos();
    let macroFailure = "";
    if ((service.capabilities?.supportedDomainMask & 15) === 15) {
        try {await service.readPortableProfile();}
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
    publish(panel, session);
}

async function writeLayoutKeys(panel, session, message) {
    const service = session.service;
    if (!service.snapshot().connected) {
        throw new Error("Connect to a keyboard before changing its layout.");
    }
    const groups = Array.isArray(message.layers)
        ? message.layers
        : [{layer: message.layer, changes: message.changes || []}];

    const result = await vscode.window.withProgress(
        {location: vscode.ProgressLocation.Notification, title: "Writing keys to the keyboard"},
        () => service.writeLayoutKeys(groups)
    );

    const notes = [`Wrote ${result.written} key${result.written === 1 ? "" : "s"} to the keyboard.`];
    if (result.rejected.length) {
        notes.push(`Refused ${result.rejected.length}: ${result.rejected.map((entry) => `${entry.keycode} (${entry.reason})`).join(", ")}.`);
    }
    if (message.structural) {
        // Layer count is a compiled capability, so adding or removing layers is
        // firmware work rather than something this app can apply.
        notes.push(`Ignored ${message.structural} layer structure change${message.structural === 1 ? "" : "s"}: the layer count is compiled into the firmware.`);
    }
    session.notice = notes.join(" ");
    publish(panel, session);
}

module.exports = {activate, deactivate};

async function saveRecoveryFile(session, document) {
    await vscode.workspace.fs.createDirectory(session.recoveryRoot);
    const suffix = document.format === "charybdis-profile" ? ".charybdis.json" : ".diagnostic.json";
    const uri = vscode.Uri.joinPath(session.recoveryRoot, "recovery-" + new Date().toISOString().replace(/[:.]/g, "-") + suffix);
    await vscode.workspace.fs.writeFile(uri, Buffer.from(JSON.stringify(document, null, 2) + "\n"));
    session.lastRecovery = uri;
    return uri.fsPath;
}

async function portableMessage(panel, session, message) {
    session.portableBusy = true;
    try {
        const service = session.service;
        if (message.names !== undefined) {
            const draft = session.portableLayers;
            if (!draft || !Array.isArray(message.names) || message.names.length !== 8) throw new Error("Read the layers again before naming them.");
            reorderLayers(draft.before.document, draft.order, draft.order.map(old => message.names[old]));
            draft.names = [...message.names];
        }
        const saveRecovery = document => saveRecoveryFile(session, document);
        if (message.type === "cancelPortableReview") {
            session.portableReview = undefined; session.portableLayers = undefined;
        } else if (message.type === "exportPortableProfile") {
            const snapshot = await service.readPortableProfile();
            const uri = await vscode.window.showSaveDialog({title: "Export complete keyboard profile", saveLabel: "Export profile", filters: {"Charybdis profile": ["charybdis.json", "json"]}});
            if (uri) {
                await vscode.workspace.fs.writeFile(uri, Buffer.from(JSON.stringify(snapshot.document, null, 2) + "\n"));
                session.notice = "Complete keyboard profile exported to " + uri.fsPath;
            }
        } else if (message.type === "choosePortableProfile") {
            const files = await vscode.window.showOpenDialog({title: "Choose a keyboard profile", canSelectMany: false, filters: {"Charybdis profile": ["charybdis.json", "json"]}});
            if (files?.length) {
                if ((await vscode.workspace.fs.stat(files[0])).size > 100000) throw new Error("This profile file is too large.");
                const value = validateSnapshot(Buffer.from(await vscode.workspace.fs.readFile(files[0])).toString("utf8"), service.capabilities);
                session.portableReview = {document: value.document, before: session.draft?.current || await service.readPortableProfile({forRestore: true}), revision: session.draft?.revision};
                session.portableLayers = undefined;
            }
        } else if (message.type === "managePortableLayers") {
            const before = session.draft?.current || await service.readPortableProfile();
            session.portableReview = undefined;
            session.portableLayers = {before, revision: session.draft?.revision, order: Array.from({length: 8}, (_, id) => id), names: [...before.summary.names]};
        } else if (message.type === "editPortableLayer") {
            const draft = session.portableLayers, id = message.id;
            if (!draft || !Number.isInteger(id) || id < 0 || id > 7) throw new Error("Read the layers again before editing them.");
            if (message.name !== undefined) {
                if (typeof message.name !== "string") throw new Error("Enter a layer name.");
                const names = [...draft.names]; names[id] = message.name;
                reorderLayers(draft.before.document, draft.order, draft.order.map(old => names[old]));
                draft.names = names;
            } else {
                const from = draft.order.indexOf(id), to = from + message.direction;
                if (id === 0 || ![1, -1].includes(message.direction) || to < 1 || to > 7) throw new Error("Base stays at the bottom of the layer order.");
                [draft.order[from], draft.order[to]] = [draft.order[to], draft.order[from]];
            }
        } else {
            const review = session.portableReview, draft = session.portableLayers;
            const before = message.type === "savePortableLayers" ? draft?.before : review?.before;
            if (!before) throw new Error("Review the profile before restoring it.");
            const document = message.type === "savePortableLayers" ? reorderLayers(before.document, draft.order, draft.order.map(old => draft.names[old])) : review.document;
            if (session.draft) {
                session.draft.replace(document, message.type === "savePortableLayers" ? draft.revision : review.revision);
                session.portableReview = undefined; session.portableLayers = undefined;
                session.resetDraftForms = true;
                session.notice = "Profile changes kept in this app. Review changes to apply them to the keyboard.";
                return;
            }
            await service.restorePortableProfile(document, {expectedFingerprint: before.fingerprint, saveRecovery});
            session.portableReview = undefined; session.portableLayers = undefined;
            await service.readLayout(); await service.readCommittedProfile(); await service.readCombos(); await service.readBaseRgb();
            session.notice = "Complete profile saved to both halves and verified. " + (before.incomplete ? "Interrupted data retained for diagnosis: " : "Recovery copy: ") + session.lastRecovery.fsPath;
        }
    } finally {session.portableBusy = false; publish(panel, session);}
}

async function draftMessage(panel, session, message) {
    const draft = session.draft, service = session.service;
    if (!draft) throw new Error("Read a complete keyboard profile before editing.");
    session.portableBusy = true;
    try {
        if (message.type === "reviewProfileDraft") draft.review(message.draftRevision);
        else if (message.type === "closeProfileDraftReview") {draft.assertRevision(message.draftRevision, {allowStale: true}); draft.reviewedRevision = null;}
        else if (message.type === "undoProfileDraft") {draft.undo(message.draftRevision); session.resetDraftForms = true;}
        else if (message.type === "redoProfileDraft") {draft.redo(message.draftRevision); session.resetDraftForms = true;}
        else if (message.type === "discardProfileDraft") {
            draft.assertRevision(message.draftRevision, {allowStale: true});
            const state = service.snapshot();
            if (!state.connected) throw new Error("Reconnect and read the keyboard before discarding its draft.");
            const snapshot = await service.readPortableProfile();
            session.draft = new ProfileDraftSession(snapshot, state.selectedDeviceId, state.capabilities);
            session.portableReview = undefined; session.portableLayers = undefined;
            session.resetDraftForms = true;
            session.notice = "Draft discarded. Showing the saved keyboard configuration.";
        } else if (message.type === "rebaseProfileDraft") {
            draft.assertRevision(message.draftRevision, {allowStale: true});
            if (service.snapshot().selectedDeviceId !== draft.deviceId) throw new Error("Reconnect the keyboard this draft belongs to.");
            const snapshot = await service.readPortableProfile({forRestore: true});
            draft.observe(snapshot, draft.deviceId);
            draft.rebase(message.draftRevision);
            session.resetDraftForms = true;
            session.notice = "Review now compares your draft with the latest keyboard state. Apply will replace the differences shown.";
        } else {
            await vscode.window.withProgress({location: vscode.ProgressLocation.Notification, title: "Applying the complete profile to both halves"},
                () => draft.apply(service, message.draftRevision, document => saveRecoveryFile(session, document)));
            session.resetDraftForms = true;
            await service.readLayout(); await service.readCommittedProfile(); await service.readCombos(); await service.readBaseRgb();
            session.notice = "Complete profile applied to both halves and verified. Recovery copy: " + session.lastRecovery.fsPath;
        }
    } finally {session.portableBusy = false; publish(panel, session);}
}
