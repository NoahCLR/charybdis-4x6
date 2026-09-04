"use strict";

// Charybdis Live — extension host.
//
// This file is deliberately thin. It owns the VS Code surface only: the
// command, the panel, and message relay. Every decision about the device, the
// protocol, and the profile lives in live-link/, which has no vscode import so
// that this shell can be replaced by a standalone app later without touching
// it. Nothing here parses a firmware repository; see
// docs/LIVE_EDIT_APP_DIRECTION.md.

const path = require("node:path");
const vscode = require("vscode");

const {ProfileDeviceService} = require("./live-link/profile-device-service");

const VIEW_TYPE = "charybdisLive.panel";

function activate(context) {
    context.subscriptions.push(
        vscode.commands.registerCommand("charybdisLive.open", () => openPanel(context))
    );
}

function deactivate() {}

function openPanel(context) {
    const panel = vscode.window.createWebviewPanel(
        VIEW_TYPE,
        "Charybdis Live",
        vscode.ViewColumn.One,
        {
            enableScripts: true,
            retainContextWhenHidden: true,
            localResourceRoots: [vscode.Uri.file(path.join(context.extensionPath, "media"))],
        }
    );

    const service = new ProfileDeviceService({
        onChange: (snapshot) => post(panel, {type: "snapshot", snapshot}),
    });

    panel.webview.html = renderHtml(panel.webview, context);
    panel.webview.onDidReceiveMessage((message) => handleMessage(panel, service, message));
    panel.onDidDispose(() => {
        void service.close();
    });

    // Publish the empty snapshot immediately so the view renders before any
    // device work happens.
    post(panel, {type: "snapshot", snapshot: service.snapshot()});
}

async function handleMessage(panel, service, message) {
    try {
        switch (message?.type) {
            case "scan":
                await service.enumerate();
                return;
            case "connect":
                await service.connect(message.deviceId);
                return;
            case "refresh":
                await service.refresh();
                return;
            case "disconnect":
                await service.disconnect();
                return;
            default:
                return;
        }
    } catch (error) {
        // The service records its own error state and emits a snapshot, so the
        // panel stays truthful. Surface it once here for the operations a user
        // started explicitly.
        const text = error instanceof Error ? error.message : String(error);
        vscode.window.showErrorMessage(`Charybdis Live: ${text}`);
    }
}

function post(panel, message) {
    void panel.webview.postMessage(message);
}

function renderHtml(webview, context) {
    const asset = (...parts) =>
        webview.asWebviewUri(vscode.Uri.file(path.join(context.extensionPath, ...parts)));
    const nonce = createNonce();
    const csp = [
        "default-src 'none'",
        `style-src ${webview.cspSource}`,
        `script-src 'nonce-${nonce}'`,
    ].join("; ");

    return `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta http-equiv="Content-Security-Policy" content="${csp}">
<meta name="viewport" content="width=device-width, initial-scale=1">
<link rel="stylesheet" href="${asset("media", "main.css")}">
<title>Charybdis Live</title>
</head>
<body>
<main id="app"></main>
<script nonce="${nonce}" type="module" src="${asset("media", "main.js")}"></script>
</body>
</html>`;
}

function createNonce() {
    const alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    let nonce = "";
    for (let index = 0; index < 32; index += 1) {
        nonce += alphabet[Math.floor(Math.random() * alphabet.length)];
    }
    return nonce;
}

module.exports = {activate, deactivate};
