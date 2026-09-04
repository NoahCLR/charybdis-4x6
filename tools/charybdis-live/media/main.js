// Charybdis Live — webview entry point.
//
// Slice 1 shows only what the keyboard says about itself: identity,
// capabilities, schema, and committed profile status. There is no editing yet
// and no authored-source model; everything rendered here was read from the
// device.

import {element} from "./dom.js";
import {renderIdentity} from "./views/identity.js";
import {renderLayout} from "./views/layout.js";
import {renderDevices} from "./views/devices.js";
import {renderStatus} from "./views/status.js";
import {renderDiagnostics} from "./views/diagnostics.js";

const vscode = acquireVsCodeApi();
const app = document.getElementById("app");

let snapshot = null;
let activeLayer = 0;

function send(message) {
    vscode.postMessage(message);
}

function render() {
    if (!snapshot) {
        app.replaceChildren(element("p", {class: "muted"}, "Starting…"));
        return;
    }

    const header = element("header", {class: "bar"});
    header.append(
        element("h1", {}, "Charybdis Live"),
        element("span", {class: `chip chip--${phaseTone(snapshot)}`}, phaseLabel(snapshot))
    );

    const actions = element("div", {class: "bar__actions"});
    actions.append(
        button("Scan", () => send({type: "scan"}), snapshot.busy),
        button("Refresh", () => send({type: "refresh"}), snapshot.busy || !snapshot.connected),
        button("Disconnect", () => send({type: "disconnect"}), snapshot.busy || !snapshot.connected)
    );
    header.append(actions);

    const sections = [header, renderDevices(snapshot, (deviceId) => send({type: "connect", deviceId}))];

    if (snapshot.connected) {
        sections.push(
            renderLayout({...snapshot, activeLayer}, {
                readLayout: () => send({type: "readLayout"}),
                selectLayer: (layer) => {
                    activeLayer = layer;
                    render();
                },
            }),
            renderIdentity(snapshot),
            renderStatus(snapshot)
        );
    } else {
        sections.push(
            element(
                "p",
                {class: "muted"},
                snapshot.scanned
                    ? "Select an interface above to connect."
                    : "Scan to find a connected Charybdis."
            )
        );
    }

    if (snapshot.error) {
        sections.push(
            element("section", {class: "panel panel--error"}, [
                element("h2", {}, "Error"),
                element("p", {}, `${snapshot.error.code || "error"}: ${snapshot.error.message || ""}`),
            ])
        );
    }

    sections.push(renderDiagnostics(snapshot));
    app.replaceChildren(...sections);
}

function phaseLabel(state) {
    if (state.busy) {
        return "Working…";
    }
    if (state.connected) {
        return "Connected";
    }
    return state.scanned ? "Not connected" : "Not scanned";
}

function phaseTone(state) {
    if (state.error) {
        return "error";
    }
    return state.connected ? "ok" : "idle";
}

function button(label, onClick, disabled) {
    const node = element("button", {}, label);
    node.disabled = Boolean(disabled);
    node.addEventListener("click", onClick);
    return node;
}

window.addEventListener("message", (event) => {
    if (event.data?.type === "snapshot") {
        snapshot = event.data.snapshot;
        render();
    }
});

render();
send({type: "scan"});
