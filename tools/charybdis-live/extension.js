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

const {ProfileDeviceService} = require("./core/session/profile-device-service");
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

function openPanel() {
    const panel = vscode.window.createWebviewPanel(VIEW_TYPE, "Charybdis Live", vscode.ViewColumn.One, {
        enableScripts: true,
        retainContextWhenHidden: true,
    });

    const session = {service: undefined, notice: undefined};
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
    void panel.webview.postMessage({
        type: "model",
        model: buildDeviceModel({
            capabilities: state.capabilities,
            status: state.status,
            layout: state.layout,
            device: state.devices.find((device) => device.id === state.selectedDeviceId),
        }),
        notice: session.notice,
    });
    session.notice = undefined;
}

async function handleMessage(panel, session, message) {
    try {
        switch (message?.type) {
            case "ready":
            case "refresh":
                await connectAndRead(panel, session);
                return;
            case "updateLayoutKeys":
                await writeLayoutKeys(panel, session, message);
                return;

            // Studio surfaces these against a repository. This app has none,
            // and the domains behind them need the committed payload read.
            // Say so rather than failing silently.
            case "selectProfile":
            case "requestCreateProfile":
            case "requestCloneProfile":
            case "requestRenameProfile":
            case "requestDeleteProfile":
                vscode.window.showInformationMessage(
                    "Charybdis Live edits the connected keyboard and has no profile files. Use Profile Studio for source profiles."
                );
                return;
            case "applyLayerChanges":
            case "saveBehavior":
            case "addBehavior":
            case "addCombo":
                vscode.window.showInformationMessage(
                    "That domain needs the committed profile read, which is not implemented yet. Layout editing works today."
                );
                return;
            default:
                return;
        }
    } catch (error) {
        const text = error instanceof Error ? error.message : String(error);
        vscode.window.showErrorMessage(`Charybdis Live: ${text}`);
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

    // The committed profile is only present on firmware that has one. A
    // keyboard running compiled defaults is a normal state, not an error, so a
    // failure here leaves the layout read standing.
    await vscode.window.withProgress(
        {location: vscode.ProgressLocation.Notification, title: "Reading committed profile"},
        () => service.readCommittedProfile()
    );

    const state = service.snapshot();
    session.notice = state.committed?.state === "read"
        ? `Read the layout and committed profile generation ${state.committed.generation}.`
        : "Read the layout. The keyboard reports no committed profile, so RGB and key behaviours are running compiled defaults.";
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

    session.notice = result.rejected.length
        ? `Wrote ${result.written} keys. Refused ${result.rejected.length}: ${result.rejected.map((entry) => entry.keycode).join(", ")}.`
        : `Wrote ${result.written} key${result.written === 1 ? "" : "s"} to the keyboard.`;
    publish(panel, session);
}

module.exports = {activate, deactivate};
