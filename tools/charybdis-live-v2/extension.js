"use strict";

// Charybdis Live v2 — extension host.
//
// Thin on purpose. It owns the VS Code surface (command, panel, message relay)
// and nothing else. Device, protocol and profile decisions live in core/, which
// has no vscode import so this shell can be replaced by a standalone app later
// without touching it. Nothing here parses a firmware repository; see
// docs/LIVE_EDIT_APP_DIRECTION.md.
//
// The webview renders a keyboard-backed `model` and posts typed edits back, so
// this file's job is to build that model and turn those edits into device
// writes.

const vscode = require("vscode");

const {upgradePdSnapshot, validateSnapshot, summary, reorderLayers} = require("./core/session/portable-profile-session");
const {ProfileDeviceService} = require("./core/session/profile-device-service");
const {ProfileDraftSession, DRAFT_EDITS} = require("./core/session/profile-draft-session");
const {buildDeviceModel} = require("./core/session/device-model");
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
            busy: editing.busy || session.portableBusy,
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
            busy: state.busy || session.portableBusy,
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
        pdUpgradeAvailable: Boolean(state.capabilities?.featureFlags & (1 << 13)),
        busy: state.busy || session.portableBusy,
        progress: state.portableProgress,
        review: session.portableReview ? {incoming: summary(session.portableReview.document), current: session.portableReview.before.summary} : null,
        layers: session.portableLayers ? {key: session.portableLayers.before.fingerprint, order: session.portableLayers.order, names: session.portableLayers.names} : null,
    };
    if (!model.draft?.matching) model.layers?.forEach((layer, index) => {layer.displayName = state.portableSummary?.names[index] || layer.name;});
    void panel.webview.postMessage({type: "model", model,
        notice: session.notice,
        acceptedEdit: session.acceptedEdit,
        resetDraftForms: session.resetDraftForms,
    });
    session.notice = undefined;
    session.acceptedEdit = undefined;
    session.resetDraftForms = undefined;
}

// Every path through here publishes, including the ones that decline or fail:
// the webview waits on that reply — a combo builder closes when its edit is
// accepted and stays open when it is refused — and a refusal reaches the panel
// as a notice rather than as silence.
async function handleMessage(panel, session, message) {
    try {
        if (session.portableBusy) {publish(panel, session); return;}
        if (session.draft && (DRAFT_EDITS.has(message?.type) || /^(?:review|undo|redo|discard|apply|rebase|close)ProfileDraft/.test(message?.type || "")) && message.draftId !== session.draft.id) throw new Error("This edit belongs to an older draft. Read the keyboard before continuing.");
        if (session.draft && DRAFT_EDITS.has(message?.type)) {
            const state = session.service.snapshot();
            if (state.busy || !state.connected || state.selectedDeviceId !== session.draft.deviceId) throw new Error("Reconnect the keyboard this draft belongs to and wait for its current operation.");
            session.acceptedEdit = session.draft.stage(message);
            if (message.reviewAfter) session.draft.review(session.draft.revision);
            publish(panel, session);
            return;
        }
        // Every change leaves this window through a reviewed draft. Without one
        // (a keyboard whose profile could not be read, or older firmware) the
        // app is read-only; an edit that reaches here is refused, never written.
        if (!session.draft && DRAFT_EDITS.has(message?.type)) {
            throw new Error("This keyboard has no editable draft, so the change was not written. Read the keyboard again; if it stays read-only, update both halves to firmware with profile editing.");
        }
        if (["reviewProfileDraft", "undoProfileDraft", "redoProfileDraft", "discardProfileDraft", "applyProfileDraft", "rebaseProfileDraft", "closeProfileDraftReview"].includes(message?.type)) {
            await draftMessage(panel, session, message);
            return;
        }
        switch (message?.type) {
            case "exportPdUpgrade":
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
            default:
                // Even an unrecognised message is answered, so the panel is
                // never left waiting on a reply.
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

// Connect, learn what the keyboard is, and read what it is running.
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
                // The form posts every name with the move, because the rows are
                // rebuilt from this draft afterwards: dropping them here would
                // quietly undo whatever was typed before the move.
                if (Array.isArray(message.names) && message.names.length === draft.names.length
                    && message.names.every(name => typeof name === "string")) draft.names = [...message.names];
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
