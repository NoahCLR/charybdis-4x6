// Charybdis Live v2 — the webview.
//
// It renders the `model` the host posts and posts typed edits back. It holds
// no device state of its own: the draft lives in the host, so what is drawn is
// always what would be applied.

import {el, esc} from "./lib/dom.mjs";
import {css, isOff} from "./lib/colour.mjs";
import {captureContentScroll, restoreContentScroll} from "./lib/scroll.mjs";
import {activateOnKey, captureFocus, focusDialog, restoreFocus, trapTab} from "./lib/focus.mjs";
import {closeComboBuilder, getModel, post, render as rerender, resetDraftForms, setModel, setRenderer, state, writable} from "./store.mjs";
import {historyAction} from "./view/edits.mjs";
import {FIELDS_SHOWN, discardLabel, placeState, reviewBlocks, statusSummary, stillShown} from "./view/review.mjs";
import {bindLayerIndex, hideHover, mountHover} from "./ui/hover.mjs";
import {pickerOverlay} from "./ui/picker.mjs";
import {keysShortcut, screenKeys} from "./ui/keys.mjs";
import {mark, marked} from "./ui/marks.mjs";
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
    // A colour is shown as the keyboard would light it, beside its value; an
    // off colour is drawn off, as it is everywhere else.
    const swatch = (colour) => colour
        ? `<i class="rv-swatch ${isOff(colour) ? "swatch-off" : ""}" style="${isOff(colour) ? "" : `background:${css(colour)}`}"></i>` : "";
    // Everything with a colour of its own carries its mark, drawn as every
    // editor draws it (ui/marks.mjs): a tier's dot, a branch badge, a layer's
    // or pointing mode's light, a combo badge, a stage's on/off dot.
    const value = (text, klass, colour, marker) => text === null ? ""
        : `<span class="rv-val">${swatch(colour)}${mark(model, marker)}<span class="${klass}">${esc(text)}</span></span>`;
    // A behaviour tier is labelled as the grid heads it: the branch badge,
    // the tier's dot, then the tier's name.
    const TIER_NAMES = {tap: "tap", hold: "hold", long: "long hold"};
    // Within an item, marked and unmarked labels share one mark slot, so the
    // words start at one edge and the tier dots line up in a column.
    const fieldLabel = (entry, slot) => {
        const text = entry.labelMark?.kind === "tier" && entry.labelMark.branch ? TIER_NAMES[entry.labelMark.tier] : entry.label;
        return text ? marked(model, entry.labelMark, text, {slot}) : "";
    };
    // Every field is one row of the same three columns — what, on the
    // keyboard, in your draft — so the two sides line up down the whole
    // review. A side that has nothing (an added thing on the keyboard, a
    // removed one in the draft) says so with a dash in its own column.
    const nothing = `<span class="rv-none">—</span>`;
    // Each field's own status sits in a narrow column before it, as a diff
    // marks its lines: + a field the draft adds, − one it removes, nothing for
    // one it changes. So a tier dropped from a behaviour that stays reads as
    // removed, and a behaviour removed whole reads − on every line.
    const SIGNS = {added: "+", removed: "−"};
    const field = (slot) => (entry) => `<span class="rv-sign ${esc(entry.status || "")}" aria-label="${esc(entry.status || "")}">${SIGNS[entry.status] || ""}</span>
        <span class="rv-k">${fieldLabel(entry, slot)}</span>
        <span class="rv-v">${entry.before === null ? nothing : value(entry.before, "del", entry.beforeColour, entry.beforeMark)}</span>
        <span class="rv-v">${entry.after === null ? nothing : value(entry.after, "ins", entry.afterColour, entry.afterMark)}</span>`;
    const fields = (item) => {
        const head = item.fields.slice(0, FIELDS_SHOWN), rest = item.fields.slice(FIELDS_SHOWN);
        const row = field(item.fields.some((entry) => entry.labelMark));
        return `<div class="rv-fields">${head.map(row).join("")}${rest.length
            ? `<details class="rv-more"><summary>Show all ${item.fields.length}</summary><div class="rv-fields">${rest.map(row).join("")}</div></details>` : ""}</div>`;
    };
    // An item is one row of the review's grid: a status gutter, the title,
    // the fields, and two action slots that are always in the same place —
    // Show, then Discard at the edge — whether or not an item has them.
    const item = (entry, block, index, discard, titleSlot) => `<div class="rv-item">
        <span class="rv-gutter"><span class="rv-status ${esc(entry.status)}">${esc(entry.status)}</span></span>
        <div class="rv-title">${titleSlot ? `<span class="mk"><span class="mk-slot title">${mark(model, entry.titleMark)}</span><span class="t">${esc(entry.title)}</span></span>` : `<span class="t">${esc(entry.title)}</span>`}
            ${entry.area !== block.area ? `<span class="rv-meta">${esc(entry.area)}</span>` : ""}</div>
        ${fields(entry)}
        <span class="rv-act">${stillShown(entry) && placeState(entry.place, model.layers) ? `<button class="btn tiny ghost" data-show="${index}"
            data-tip="Close the review and open this where it is edited.">Show</button>` : ""}</span>
        <span class="rv-act">${discard}</span></div>`;
    const columns = `<div class="rv-cols"><span></span><span>What changes</span>
        <div class="rv-fields"><span></span><span></span><span>On the keyboard</span><span>In your draft</span></div><span></span><span></span></div>`;
    const discardable = draft.changes.every((change) => Number.isInteger(change.group));
    const shown = [];
    const sections = reviewBlocks(draft.changes).map(({area, blocks, count}) => `<section class="rv-sect">
        <div class="sect-h"><h4>${esc(area)}</h4><span class="right tag">${count}</span></div>
        ${columns}
        ${((titleSlot) => blocks.map((block) => {
            const grouped = block.items.length > 1;
            const discard = discardable ? `<button class="btn tiny ghost" data-discard="${esc(block.group)}" ${canDiscard ? "" : "disabled"}
                data-tip="${esc(grouped ? `Put these ${block.items.length} changes back to what the keyboard holds. They were made together, so they go back together.` : "Put this change back to what the keyboard holds.")}">${esc(discardLabel(block))}</button>` : "";
            const items = block.items.map((entry) => item(entry, block, shown.push(entry) - 1, grouped ? "" : discard, titleSlot)).join("");
            return grouped
                ? `<div class="rv-block grouped"><div class="rv-group-h"><span class="rv-group-t"><span>${esc(block.title || "Made together")}</span>
                    <span class="note">${block.items.length} changes, discarded together</span></span><span class="rv-act wide">${discard}</span></div>${items}</div>`
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
    // Pointing at a group's Discard lights what it takes back.
    node.querySelectorAll(".rv-block.grouped [data-discard]").forEach((button) => {
        const block = button.closest(".rv-block");
        button.addEventListener("mouseenter", () => block.classList.add("lit"));
        button.addEventListener("mouseleave", () => block.classList.remove("lit"));
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
        state.overlay = null;
        render();
    }
});

render();
post({type: "ready"});
