"use strict";

// The panel's HTML shell. This is host code, not page code: it is the only
// place that turns files under `webview/` into webview URIs, which is why it
// sits beside extension.js rather than inside the directory that must stay
// pure browser modules.

const vscode = require("vscode");

function nonce() {
    const alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    let value = "";
    for (let index = 0; index < 32; index += 1) value += alphabet[Math.floor(Math.random() * alphabet.length)];
    return value;
}

function getHtml(webview, extensionUri) {
    const uri = (...parts) => webview.asWebviewUri(vscode.Uri.joinPath(extensionUri, "webview", ...parts));
    const id = nonce();
    return `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; img-src ${webview.cspSource} data:; style-src ${webview.cspSource} 'unsafe-inline'; script-src 'nonce-${id}';">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Charybdis Live v2</title>
<link rel="stylesheet" href="${uri("styles.css")}">
</head>
<body>
<div id="root"></div>
<script type="module" nonce="${id}" src="${uri("app.mjs")}"></script>
</body>
</html>`;
}

module.exports = {getHtml};
