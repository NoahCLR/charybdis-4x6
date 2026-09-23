"use strict";

// Charybdis Live v2 — extension host.
//
// Thin on purpose. It owns the VS Code surface (command, panel, message relay)
// and nothing else. Device, protocol and profile decisions live in core/, which
// has no vscode import so this shell can be replaced by a standalone app later
// without touching it. Nothing here parses a firmware repository; see
// docs/LIVE_EDIT_APP_DIRECTION.md.
//
// The webview renders a keyboard-backed `model` and posts typed edits back.
// What the model holds and where each message goes is decided in
// core/session/panel-session.js; this file adds the VS Code parts — dialogs,
// files, progress and the panel itself.

const vscode = require("vscode");

const {upgradePdSnapshot, validateSnapshot} = require("./core/session/portable-profile-session");
const {ProfileDeviceService} = require("./core/session/profile-device-service");
const {applyLayerEdit, buildPanelModel, discardDraftForDevice, layerEditDocument, routeMessage, startLayerEdit, takeOutbox} = require("./core/session/panel-session");
const {getHtml} = require("./panel-html");

const VIEW_TYPE = "charybdisLiveV2.panel";

function activate(context) {
    context.subscriptions.push(
        vscode.commands.registerCommand("charybdisLiveV2.open", () => openPanel(context))
    );

    const status = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, 2);
    status.text = "$(radio-tower) Charybdis Live v2";
    status.tooltip = "Open Charybdis Live v2 — edit the connected keyboard";
    status.command = "charybdisLiveV2.open";
    status.show();
    context.subscriptions.push(status);
}

function deactivate() {}

function openPanel(context) {
    const panel = vscode.window.createWebviewPanel(VIEW_TYPE, "Charybdis Live v2", vscode.ViewColumn.One, {
        enableScripts: true,
        retainContextWhenHidden: true,
        localResourceRoots: [vscode.Uri.joinPath(context.extensionUri, "webview")],
    });

    const session = {service: undefined, notice: undefined, recoveryRoot: context.globalStorageUri};
    session.service = new ProfileDeviceService({
        onChange: () => publish(panel, session),
    });

    panel.webview.html = getHtml(panel.webview, context.extensionUri);
    panel.webview.onDidReceiveMessage((message) => handleMessage(panel, session, message));
    panel.onDidDispose(() => {
        void session.service.close();
    });
}

function publish(panel, session) {
    const model = buildPanelModel(session, session.service.snapshot());
    void panel.webview.postMessage({type: "model", model, ...takeOutbox(session)});
}

// Every path through here publishes, including the ones that decline or fail:
// the webview waits on that reply — a combo builder closes when its edit is
// accepted and stays open when it is refused — and a refusal reaches the panel
// as a notice rather than as silence.
async function handleMessage(panel, session, message) {
    try {
        const route = routeMessage(session, message, session.service.snapshot());
        if (route === "draft") await draftMessage(panel, session, message);
        else if (route === "portable") await portableMessage(panel, session, message);
        else if (route === "read") await connectAndRead(panel, session, message.type === "selectDevice" ? message.deviceId : undefined);
        // A staged edit, and even an unrecognised message, is answered, so the
        // panel is never left waiting on a reply.
        else publish(panel, session);
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

// Connect, learn what the keyboard is, and read what it is running.
async function connectAndRead(panel, session, selectedDeviceId) {
    try {
        await readKeyboard(panel, session, selectedDeviceId);
    } finally {
        session.readBusy = false;
        publish(panel, session);
    }
}

async function readKeyboard(panel, session, selectedDeviceId) {
    const service = session.service;
    await service.enumerate();
    const devices = service.snapshot().devices;
    if (!devices.length) {
        session.notice = "No Charybdis Raw HID interface found. Connect the keyboard and reload.";
        return;
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
        if (message.type === "savePortableLayers" && message.names !== undefined) applyLayerEdit(session.portableLayers, message);
        const saveRecovery = document => saveRecoveryFile(session, document);
        if (message.type === "cancelPortableReview") {
            session.portableReview = undefined; session.portableLayers = undefined;
        } else if (message.type === "exportPdUpgrade") {
            const snapshot = await service.readPortableProfile(), upgraded = upgradePdSnapshot(snapshot.document);
            const uri = await vscode.window.showSaveDialog({title: "Save original profile and PD upgrade", saveLabel: "Save both profiles", filters: {"Charybdis profile": ["charybdis.json"]}});
            if (uri) {
                const next = uri.with({path: uri.path.replace(/(?:\.charybdis)?\.json$/i, "") + ".pd8.charybdis.json"});
                let exists = false;
                try {await vscode.workspace.fs.stat(next); exists = true;} catch (error) {if (error.code !== "FileNotFound") throw error;}
                if (exists || next.toString() === uri.toString()) throw new Error("The upgraded backup path already exists. Choose a new backup name.");
                for (const [target, value] of [[uri, snapshot.document], [next, upgraded]]) {
                    const bytes = Buffer.from(JSON.stringify(value, null, 2) + "\n");
                    await vscode.workspace.fs.writeFile(target, bytes);
                    const verified = Buffer.from(await vscode.workspace.fs.readFile(target));
                    if (!verified.equals(bytes)) throw new Error("Backup verification failed. Keep the existing firmware until both backups are saved.");
                    validateSnapshot(verified.toString("utf8"));
                }
                session.notice = "Original and eight-slot profiles saved and verified. Keep the original firmware pair too. After installing the new firmware on both halves, import " + next.fsPath;
            }
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
            session.portableLayers = startLayerEdit(before, session.draft?.revision);
        } else if (message.type === "editPortableLayer") {
            applyLayerEdit(session.portableLayers, message);
        } else {
            const review = session.portableReview, draft = session.portableLayers;
            const before = message.type === "savePortableLayers" ? draft?.before : review?.before;
            if (!before) throw new Error("Review the profile before restoring it.");
            const document = message.type === "savePortableLayers" ? layerEditDocument(draft) : review.document;
            if (session.draft) {
                session.draft.replace(document, message.type === "savePortableLayers" ? draft.revision : review.revision);
                session.portableReview = undefined; session.portableLayers = undefined;
                session.resetDraftForms = true;
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
            const snapshot = state.capabilities?.compiledLayerCount === 8 ? await service.readPortableProfile() : undefined;
            discardDraftForDevice(session, state, snapshot);
            session.notice = state.capabilities?.compiledLayerCount === 8
                ? "Draft discarded. Showing the saved keyboard configuration."
                : "Draft discarded. This keyboard remains read-only.";
        } else if (message.type === "rebaseProfileDraft") {
            draft.assertRevision(message.draftRevision, {allowStale: true});
            if (service.snapshot().selectedDeviceId !== draft.deviceId) throw new Error("Reconnect the keyboard this draft belongs to.");
            const snapshot = await service.readPortableProfile({forRestore: true});
            draft.observe(snapshot, draft.deviceId, service.snapshot().connectionToken);
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
