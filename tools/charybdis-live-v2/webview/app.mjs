// Charybdis Live v2 — the webview.
//
// It renders the `model` the host posts and posts typed edits back. It holds
// no device state of its own: the draft lives in the host, so what is drawn is
// always what would be applied.

import {el, esc} from "./lib/dom.mjs";
import {captureContentScroll, restoreContentScroll} from "./lib/scroll.mjs";
import {activateOnKey, captureFocus, focusDialog, restoreFocus, trapTab} from "./lib/focus.mjs";
import {closeComboBuilder, getModel, post, render as rerender, resetDraftForms, setModel, setRenderer, state} from "./store.mjs";
import {historyAction} from "./view/edits.mjs";
import {bindLayerIndex, hideHover, mountHover} from "./ui/hover.mjs";
import {pickerOverlay} from "./ui/picker.mjs";
import {keysShortcut, screenKeys} from "./ui/keys.mjs";
import {closeLayers} from "./ui/layers.mjs";
import {screenLighting} from "./ui/lighting.mjs";
import {screenSettings} from "./ui/settings.mjs";
import {screenPointing} from "./ui/pointing.mjs";
import {screenMacros} from "./ui/macros.mjs";
import {screenProfile} from "./ui/profile.mjs";
import {commitBar, rail, topbar, unavailable} from "./ui/shell.mjs";

const root = document.getElementById("root");
let renderedScreen = null;

const SCREENS = {
    keys: screenKeys,
    lighting: screenLighting,
    settings: screenSettings,
    device: screenDevice,
    pointing: screenPointing,
    macros: screenMacros,
    profile: screenProfile,
};

function screenDevice() {
    const model = getModel();
    const device = model?.device || {};
    const kv = (rows) => `<dl class="kv" style="grid-template-columns:170px 1fr">${rows
        .map(([key, value]) => `<dt>${esc(key)}</dt><dd>${esc(value ?? "—")}</dd>`).join("")}</dl>`;
    const main = el(`<div class="main">${topbar(
        "Device",
        "What the keyboard says about itself. Everything this app shows comes from here — it never reads a firmware repository.",
        `<button class="btn" data-act="read" ${device.health?.busy ? "disabled" : ""}>Read from keyboard</button>`,
    )}
        <div class="content"><div class="pad" style="max-width:940px;display:grid;gap:14px">
            <div class="grid2">
                <div class="card"><div class="card-h"><h3>Connection</h3>
                    <span class="right"><span class="chip"><i class="dot ${device.connected ? "on" : "err"}"></i>${device.connected ? "connected" : "disconnected"}</span></span></div>
                    <div class="card-b">${kv([
                        ["Product", device.label],
                        ["Status", device.subtitle],
                        ["Phase", device.health?.phase],
                    ])}</div></div>
                <div class="card"><div class="card-h"><h3>Committed profile</h3>
                    <span class="right"><span class="chip"><i class="dot ${device.health?.converged ? "on" : "draft"}"></i>${device.health?.converged ? "both halves agree" : "halves not converged"}</span></span></div>
                    <div class="card-b">${kv([
                        ["Generation and digest", device.summary],
                        ["Recovery", device.health?.recoveryPending ? "pending" : "clear"],
                        ["Draft", model?.draft ? `${model.draft.changes.length} change${model.draft.changes.length === 1 ? "" : "s"}` : "none"],
                    ])}</div></div>
            </div>
            <div class="card"><div class="card-h"><h3>What was read</h3></div>
                <div class="list">${(model?.diagnostics || []).map((note) =>
                    `<div class="list-row" style="grid-template-columns:1fr"><span class="note">${esc(note)}</span></div>`).join("")
                    || `<div class="list-row"><span class="note">Nothing has been read yet.</span></div>`}</div></div>
        </div></div></div>`);
    main.querySelector('[data-act="read"]').addEventListener("click", () => post({type: "refresh"}));
    return main;
}

function reviewOverlay() {
    const model = getModel();
    const draft = model?.draft;
    if (state.overlay !== "review" || !draft?.dirty) return null;
    const areas = [...new Set(draft.changes.map((change) => change.area))];
    const node = el(`<div class="scrim"><div class="sheet" role="dialog" aria-modal="true" aria-label="Review changes">
        <div class="sheet-h"><h2>Review ${draft.changes.length} change${draft.changes.length === 1 ? "" : "s"}</h2>
            <span class="right" style="margin-left:auto"><button class="btn ghost" data-act="close">Keep editing</button></span></div>
        <div class="sheet-b">
            ${areas.map((area) => `<div style="padding:14px 18px 4px">
                <div class="sect-h"><h4>${esc(area)}</h4><span class="right tag">${draft.changes.filter((change) => change.area === area).length}</span></div>
                <table class="t"><thead><tr><th style="width:38%">What changes</th><th style="width:31%">On the keyboard</th><th style="width:31%">In your draft</th></tr></thead>
                <tbody>${draft.changes.filter((change) => change.area === area).map((change) => `<tr>
                    <td>${esc(change.label)}</td>
                    <td class="mono del">${esc(change.before)}</td>
                    <td class="mono ins">${esc(change.after)}</td></tr>`).join("")}</tbody></table></div>`).join("")}
            <div style="padding:14px 18px 18px"><div class="callout warn">Apply writes a recovery copy, stages the changed blocks on both halves, then publishes one generation. The keyboard keeps running its saved profile until both halves confirm. If it is interrupted, the recovery copy restores it.</div></div>
        </div>
        <div class="sheet-f"><span class="note">${esc(model?.device?.label || "")} · ${esc(model?.device?.summary || "")}</span>
            <span class="right"><button class="btn" data-act="close">Cancel</button>
                <button class="btn primary" data-act="apply" ${draft.reviewed && draft.connected && !draft.stale ? "" : "disabled"}>Apply to keyboard</button></span></div>
    </div></div>`);
    node.addEventListener("click", (event) => {
        if (event.target === node || event.target.closest('[data-act="close"]')) {
            state.overlay = null;
            post({type: "closeProfileDraftReview"});
        }
        if (event.target.closest('[data-act="apply"]')) {
            state.overlay = null;
            post({type: "applyProfileDraft"});
        }
    });
    return node;
}

function render() {
    const scroll = captureContentScroll(root, renderedScreen, state.screen);
    const focus = captureFocus(root);
    hideHover();
    const model = getModel();
    root.replaceChildren();
    const app = el(`<div class="app"></div>`);
    app.appendChild(rail());
    const screen = (SCREENS[state.screen] || screenKeys)();
    if (!model) {
        screen.querySelector(".content")?.prepend(el(`<div class="pad"><div class="screen-stub">
            <h3>Looking for a keyboard</h3><p class="note">Connect a Charybdis and this panel will read it.</p></div></div>`));
    }
    app.appendChild(screen);
    const bar = commitBar();
    if (bar) screen.appendChild(bar);
    root.appendChild(app);
    restoreContentScroll(root, scroll);
    renderedScreen = state.screen;

    const picker = pickerOverlay();
    if (picker) root.appendChild(picker);
    const review = reviewOverlay();
    if (review) root.appendChild(review);
    restoreFocus(root, focus);
    focusDialog(root);
}

setRenderer(render);
bindLayerIndex(() => state.layer);
mountHover(root);

addEventListener("message", (event) => {
    const message = event.data;
    if (message?.type !== "model") return;
    setModel(message.model);
    // The host reports a refused edit as a notice prefixed "Failed"; that is a
    // failure, so it is shown as one rather than as a neutral message.
    if (message.notice) {
        const failed = /^Failed/.test(message.notice);
        state.error = failed ? message.notice : "";
        state.notice = failed ? "" : message.notice;
    }
    if (message.resetDraftForms) resetDraftForms();
    // A combo builder waiting on Keep or Delete closes when the host accepts
    // the edit, and stays open with its fields when the host refuses it. A
    // builder for a combo that no longer exists closes too.
    if (state.comboAwaiting) {
        if (/^Failed/.test(message.notice || "")) state.comboAwaiting = false;
        else closeComboBuilder();
    }
    if (state.comboEditId !== null && !(message.model?.combos || []).some((combo) => combo.id === state.comboEditId)) closeComboBuilder();
    const layerCount = message.model?.layers?.length || 0;
    if (state.layer >= layerCount) state.layer = 0;
    const positions = message.model?.layers?.[state.layer]?.positions || [];
    if (!positions.some((position) => position.layoutIndex === state.selected)) {
        state.selected = positions[0]?.layoutIndex ?? 0;
    }
    if (state.overlay === "review" && !message.model?.draft?.dirty) state.overlay = null;
    render();
});

// A field that holds text keeps its own undo: ⌘Z there edits the text, not
// the draft. Everywhere else it steps the draft, as the ↺ ↻ buttons do.
const TEXT_INPUTS = new Set(["text", "search", "number", "email", "url", "tel", "password"]);
const editsText = (target) => Boolean(target?.closest?.("textarea, [contenteditable]:not([contenteditable=\"false\"])"))
    || (target?.tagName === "INPUT" && TEXT_INPUTS.has(target.type));

function historyShortcut(event) {
    const action = historyAction(event, {editingText: editsText(event.target), busy: Boolean(state.recording || state.retarget)});
    if (!action) return false;
    event.preventDefault();
    const draft = getModel()?.draft;
    if (!draft || draft.busy) return true;
    if (action === "undo" && draft.canUndo) post({type: "undoProfileDraft"});
    if (action === "redo" && draft.canRedo) post({type: "redoProfileDraft"});
    return true;
}

addEventListener("keydown", (event) => {
    if (trapTab(root, event) || activateOnKey(event)) return;
    if (historyShortcut(event) || keysShortcut(event)) return;
    if (event.key !== "Escape") return;
    hideHover();
    if (state.picker) { state.picker = null; render(); return; }
    if (state.retarget) { state.retarget = null; render(); return; }
    if (state.layersOpen) { closeLayers(); render(); return; }
    if (state.overlay) { state.overlay = null; render(); }
});

render();
post({type: "ready"});
