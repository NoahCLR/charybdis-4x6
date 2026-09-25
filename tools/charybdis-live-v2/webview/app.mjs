// Charybdis Live v2 — the webview.
//
// It renders the `model` the host posts and posts typed edits back. It holds
// no device state of its own: the draft lives in the host, so what is drawn is
// always what would be applied.

import {el, esc} from "./lib/dom.mjs";
import {captureContentScroll, restoreContentScroll} from "./lib/scroll.mjs";
import {activateOnKey, captureFocus, focusDialog, restoreFocus, trapTab} from "./lib/focus.mjs";
import {closeComboBuilder, getModel, post, render as rerender, resetDraftForms, setModel, setRenderer, state, writable} from "./store.mjs";
import {historyAction} from "./view/edits.mjs";
import {discardLabel, groupNote, placeState, reviewBlocks, statusSummary, stillShown} from "./view/review.mjs";
import {bindLayerIndex, hideHover, mountHover} from "./ui/hover.mjs";
import {pickerOverlay} from "./ui/picker.mjs";
import {keysShortcut, screenKeys} from "./ui/keys.mjs";
import {reviewColumns, reviewItem} from "./ui/review-items.mjs";
import {historyOverlay} from "./ui/history.mjs";
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
    // Discarding goes back to the keyboard's value; it waits while the draft
    // is out of step with the keyboard or busy, as editing does.
    const canDiscard = writable();
    const item = (entry, index, discard, titleSlot) => reviewItem(model, entry, {discard, titleSlot,
        show: stillShown(entry) && placeState(entry.place, model.layers) ? `<button class="btn tiny ghost" data-show="${index}"
            data-tip="Close the review and open this where it is edited.">Show</button>` : ""});
    const columns = reviewColumns("What changes", "On the keyboard", "In your draft");
    const discardable = draft.changes.every((change) => Number.isInteger(change.group));
    const shown = [];
    const sections = reviewBlocks(draft.changes).map(({area, blocks, count}) => `<section class="rv-sect">
        <div class="sect-h"><h4>${esc(area)}</h4><span class="right tag">${count}</span></div>
        ${columns}
        ${((titleSlot) => blocks.map((block) => {
            const grouped = block.size > 1;
            const discard = discardable ? `<button class="btn tiny ghost" data-discard="${esc(block.group)}" ${canDiscard ? "" : "disabled"}
                data-tip="${esc(grouped ? `Put these ${block.size} changes back to what the keyboard holds. They were made together, so they go back together.` : "Put this change back to what the keyboard holds.")}">${esc(discardLabel(block))}</button>` : "";
            const items = block.items.map((entry) => item(entry, shown.push(entry) - 1, grouped ? "" : discard, titleSlot)).join("");
            return grouped
                ? `<div class="rv-block grouped" data-group="${esc(block.group)}"><div class="rv-group-h"><span class="rv-group-t"><span>${esc(block.title || "Made together")}</span>
                    <span class="note">${esc(groupNote(block))}</span></span><span class="rv-act wide">${discard}</span></div>${items}</div>`
                : `<div class="rv-block">${items}</div>`;
        }).join(""))(blocks.some((block) => block.items.some((entry) => entry.titleMark)))}</section>`).join("");
    const summary = statusSummary(draft.changes);
    const node = el(`<div class="scrim"><div class="sheet" role="dialog" aria-modal="true" aria-label="Review changes">
        <div class="sheet-h"><h2>Review ${draft.changes.length} change${draft.changes.length === 1 ? "" : "s"}</h2>
            ${summary ? `<span class="note">${esc(summary)}</span>` : ""}
            <span class="right" style="margin-left:auto"><button class="btn ghost" data-act="close">Keep editing</button></span></div>
        <div class="sheet-b rv">
            ${sections}
            <div style="padding:14px 18px 18px"><div class="callout warn">Apply writes a recovery copy, stages the changed blocks on both halves, then publishes one generation. The keyboard keeps running its saved profile until both halves confirm. If it is interrupted, the recovery copy restores it.</div></div>
        </div>
        <div class="sheet-f"><span class="note">${esc(model?.device?.label || "")} · ${esc(model?.device?.summary || "")}</span>
            <span class="right"><button class="btn" data-act="close">Cancel</button>
                <button class="btn primary" data-act="apply" ${draft.reviewed && draft.connected && !draft.stale ? "" : "disabled"}>Apply to keyboard</button></span></div>
    </div></div>`);
    // Pointing at a group's Discard lights what it takes back, in every area
    // the group reaches.
    node.querySelectorAll(".rv-block.grouped [data-discard]").forEach((button) => {
        const parts = node.querySelectorAll(`.rv-block.grouped[data-group="${button.dataset.discard}"]`);
        button.addEventListener("mouseenter", () => parts.forEach((part) => part.classList.add("lit")));
        button.addEventListener("mouseleave", () => parts.forEach((part) => part.classList.remove("lit")));
    });
    node.addEventListener("click", (event) => {
        const discard = event.target.closest("[data-discard]");
        if (discard) {
            post({type: "discardProfileDraftChanges", group: Number(discard.dataset.discard)});
            return;
        }
        const show = event.target.closest("[data-show]");
        if (show) {
            Object.assign(state, placeState(shown[Number(show.dataset.show)].place, model.layers), {overlay: null});
            post({type: "closeProfileDraftReview"});
            rerender();
            return;
        }
        // The sheet closes at once; the host's answer only refreshes what it says.
        if (event.target === node || event.target.closest('[data-act="close"]')) {
            state.overlay = null;
            post({type: "closeProfileDraftReview"});
            rerender();
        }
        if (event.target.closest('[data-act="apply"]')) {
            state.overlay = null;
            post({type: "applyProfileDraft"});
            rerender();
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
    const history = historyOverlay();
    if (history) root.appendChild(history);
    restoreFocus(root, focus);
    focusDialog(root);
    reveal();
}

// A place asked for by a jump — the review's Show — is scrolled to and marked
// for a moment, once, so the eye lands on it among its neighbours.
function reveal() {
    if (!state.reveal) return;
    const target = root.querySelector(state.reveal);
    state.reveal = null;
    if (!target) return;
    target.scrollIntoView({block: "center"});
    target.classList.add("revealed");
    setTimeout(() => target.classList.remove("revealed"), 1600);
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
    if (state.combo.awaiting) {
        if (/^Failed/.test(message.notice || "")) state.combo.awaiting = false;
        else closeComboBuilder();
    }
    if (state.combo.editId !== null && !(message.model?.combos || []).some((combo) => combo.id === state.combo.editId)) closeComboBuilder();
    const layerCount = message.model?.layers?.length || 0;
    if (state.layer >= layerCount) state.layer = 0;
    const positions = message.model?.layers?.[state.layer]?.positions || [];
    if (!positions.some((position) => position.layoutIndex === state.selected)) {
        state.selected = positions[0]?.layoutIndex ?? 0;
    }
    if (state.overlay === "review" && !message.model?.draft?.dirty) state.overlay = null;
    if (state.overlay === "history" && !message.model?.draft) { state.overlay = null; post({type: "closeProfileDraftHistory"}); }
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
    if (state.overlay) {
        // Leaving the review by Esc is leaving it by Keep editing.
        if (state.overlay === "review") post({type: "closeProfileDraftReview"});
        if (state.overlay === "history") post({type: "closeProfileDraftHistory"});
        state.overlay = null;
        render();
    }
});

render();
post({type: "ready"});
