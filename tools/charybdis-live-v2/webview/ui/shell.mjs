// The shell: who the keyboard is, where you are, and the one gate every change
// passes through on its way to the device.

import {el, esc} from "../lib/dom.mjs";
import {getModel, post, render, state} from "../store.mjs";

const ICONS = {
    keys: '<svg viewBox="0 0 16 16"><rect x="1.5" y="3.5" width="13" height="9" rx="2"/><path d="M4 6.5h.01M6.5 6.5h.01M9 6.5h.01M11.5 6.5h.01M5 9.5h6"/></svg>',
    pointing: '<svg viewBox="0 0 16 16"><circle cx="8" cy="8" r="5.5"/><path d="M8 2.5v11M2.5 8h11"/></svg>',
    lighting: '<svg viewBox="0 0 16 16"><path d="M8 1.8v1.6M13 3l-1.1 1.1M14.2 8h-1.6M3 13l1.1-1.1M2 8h1.6M3 3l1.1 1.1"/><circle cx="8" cy="8.4" r="3.2"/></svg>',
    macros: '<svg viewBox="0 0 16 16"><path d="M2.5 4.5h11M2.5 8h7M2.5 11.5h9"/></svg>',
    settings: '<svg viewBox="0 0 16 16"><path d="M2.5 5h11M2.5 11h11"/><circle cx="6" cy="5" r="1.8"/><circle cx="10.5" cy="11" r="1.8"/></svg>',
    profile: '<svg viewBox="0 0 16 16"><path d="M8 1.8v8.4M5 7.4 8 10.4l3-3M2.8 12.2v1.4h10.4v-1.4"/></svg>',
    device: '<svg viewBox="0 0 16 16"><rect x="2.5" y="2.5" width="11" height="11" rx="2.5"/><path d="M6 6.5h4v3H6z"/></svg>',
};

export const SCREENS = [
    {group: "Configure", items: [
        {id: "keys", label: "Keys", icon: "keys"},
        {id: "lighting", label: "Lighting", icon: "lighting"},
        {id: "macros", label: "Macros", icon: "macros"},
        {id: "pointing", label: "Pointing modes", icon: "pointing"},
        {id: "settings", label: "Settings", icon: "settings"},
    ]},
    {group: "Keyboard", items: [
        {id: "profile", label: "Profile & backups", icon: "profile"},
        {id: "device", label: "Device", icon: "device"},
    ]},
];

export function rail() {
    const model = getModel();
    const device = model?.device || {};
    const health = device.health || {};
    const draft = model?.draft;
    const devices = model?.devices || [];
    const selector = devices.length > 1 ? `<label class="rail-picker-label" for="rail-device-picker">Keyboard</label>
            <select class="input rail-picker" id="rail-device-picker" data-act="select-device" ${health.busy ? "disabled" : ""}>
                ${devices.map((entry) => `<option value="${esc(entry.id)}" ${entry.id === model.selectedDeviceId ? "selected" : ""}>${esc(entry.label)} · ${esc(entry.id)}</option>`).join("")}
            </select>` : "";
    const line = (tone, text, tip) =>
        `<div class="stat" data-tip="${esc(tip)}"><i class="dot ${tone}"></i> ${esc(text)}</div>`;

    const nav = SCREENS.map((group) => `<div class="rail-group">${group.group}</div>` + group.items.map((item) => `
        <button class="nav-item" data-screen="${item.id}" aria-current="${state.screen === item.id}">
            ${ICONS[item.icon]}<span>${item.label}</span>
        </button>`).join("")).join("");

    const node = el(`<aside class="rail">
        <div class="rail-device">
            <div class="rail-mark"><span class="mark-glyph">C</span> <span class="nm">Charybdis Live</span>
                <button class="btn tiny ghost" data-act="refresh" aria-label="Read keyboard" ${health.busy ? "disabled" : ""}
                    data-tip="Read the connected keyboard again while keeping your draft.">Read</button></div>
            ${selector}
            <div class="rail-product">${esc(device.label || "No keyboard connected")}</div>
            <div class="rail-meta">${esc(device.summary || "—")}</div>
            <div class="rail-status">
                ${line(device.connected ? "on" : "err",
                    device.connected ? (health.busy ? health.phase || "Working" : "Connected") : "Disconnected",
                    "Whether this window is talking to a keyboard.")}
                ${line(health.profile === "synced" ? "on" : health.profile === "attention" ? "draft" : "",
                    health.profile === "synced" ? "Both halves agree" : health.profile === "attention" ? "Halves need attention"
                        : health.profile === "unread" ? "Profile not read" : "Profile unavailable",
                    "The committed generation and digest each half reports.")}
                ${line(draft?.dirty ? "draft" : "on",
                    draft ? (draft.dirty ? `${draft.changes.length} change${draft.changes.length === 1 ? "" : "s"} in draft` : "Draft clean") : "No draft",
                    "Edits waiting in this window. The keyboard still runs its saved profile.")}
                ${health.restartNeeded
                    ? line("err", "Restart the keyboard",
                        "The other half did not confirm a cancelled save, so the keyboard refuses new saves. Unplug the USB cable (not the cable between the halves), wait a few seconds and plug it back in. Nothing was lost.")
                    : line(health.recoveryPending ? "draft" : device.connected ? "on" : "",
                        health.recoveryPending ? "Recovery pending" : device.connected ? "Recovery clear" : "Recovery unknown",
                        "A recovery copy is written before every apply.")}
            </div>
        </div>
        <nav class="rail-nav">${nav}</nav>
        ${railMessage()}
        <div class="rail-foot">
            <span class="note">Draft history</span>
            <button class="btn tiny ghost icon" data-act="undo" ${draft?.canUndo ? "" : "disabled"}
                data-tip="Undo the last edit in the draft (⌘Z). The keyboard is not touched until you apply.">↺</button>
            <button class="btn tiny ghost icon" data-act="redo" ${draft?.canRedo ? "" : "disabled"}
                data-tip="Redo the edit you just undid (⇧⌘Z).">↻</button>
        </div>
    </aside>`);

    node.querySelectorAll("[data-screen]").forEach((button) => button.addEventListener("click", () => {
        state.screen = button.dataset.screen;
        render();
    }));
    node.querySelector('[data-act="dismiss"]')?.addEventListener("click", () => { state.notice = ""; state.error = ""; render(); });
    node.querySelector('[data-act="refresh"]').addEventListener("click", () => post({type: "refresh"}));
    node.querySelector('[data-act="select-device"]')?.addEventListener("change", (event) => post({type: "selectDevice", deviceId: event.target.value}));
    node.querySelector('[data-act="undo"]').addEventListener("click", () => post({type: "undoProfileDraft"}));
    node.querySelector('[data-act="redo"]').addEventListener("click", () => post({type: "redoProfileDraft"}));
    return node;
}

export const topbar = (title, subtitle, actions = "") => `<header class="topbar">
    <div><h1>${esc(title)}</h1><p class="sub">${esc(subtitle)}</p></div>
    <div class="topbar-actions">${actions}</div>
</header>`;

// What the keyboard or the host last said — a read, a save, a refusal — sits
// in the rail's free space above the draft history: not over the screen being
// worked in, and below the navigation so nothing moves when it appears.
function railMessage() {
    const model = getModel();
    const error = state.error || model?.device?.health?.error;
    const message = error || state.notice;
    if (!message) return "";
    return `<div class="rail-message ${error ? "err" : ""}" role="status">
        <span>${esc(message)}</span>
        <button class="btn tiny ghost icon" data-act="dismiss" aria-label="Dismiss">✕</button></div>`;
}

// One Apply's steps, as the keyboard reports them. The running step carries
// what it is doing now and, where there is one, how many bytes are through.
function applySteps(apply) {
    const rows = apply.steps.map((step) => {
        const current = step.state === "active";
        const counted = current && apply.bytes
            ? `<span class="ap-bar"><i style="width:${Math.round((apply.bytes.completed / apply.bytes.total) * 100)}%"></i></span>
               <span class="ap-bytes">${apply.bytes.completed} / ${apply.bytes.total} bytes</span>` : "";
        return `<li class="${step.state}"><span class="mark">${step.state === "active" ? '<span class="spin"></span>' : ""}</span>
            <span class="lbl">${esc(step.label)}</span>
            ${current && (apply.detail || counted) ? `<span class="ap-detail">${esc(apply.detail)}${counted}</span>` : ""}</li>`;
    }).join("");
    return `<ol class="ap-steps">${rows}</ol>`;
}

const SAVED_VERDICT = {
    none: "Nothing was saved. The keyboard kept the profile it had.",
    unknown: "The keyboard may have saved part of it. Read the keyboard to see what it runs now.",
};

export function commitBar() {
    const model = getModel();
    const draft = model?.draft;
    const apply = model?.apply;
    if (!draft) return null;

    if (draft.busy && apply?.state === "applying") {
        const index = apply.steps.findIndex((step) => step.state === "active");
        return el(`<div class="commit applying ap">
            <div class="ap-head"><strong>Applying to the keyboard</strong>
                <span class="muted">step ${index + 1} of ${apply.steps.length}</span></div>
            ${applySteps(apply)}
            <div class="note">The keyboard keeps running its saved profile until both halves confirm.</div></div>`);
    }
    // A failed Apply stays on screen until it is dismissed, so where it
    // stopped and why can be read after the fact.
    if (!draft.busy && apply?.state === "failed" && state.applyDismissed !== apply.id) {
        const node = el(`<div class="commit ap failed">
            <div class="ap-head"><i class="dot err"></i><strong>Apply stopped at: ${esc(apply.failure.label)}</strong></div>
            <p class="ap-reason">${esc(apply.failure.reason)}</p>
            <p class="ap-saved">${esc(SAVED_VERDICT[apply.failure.saved] || SAVED_VERDICT.unknown)}</p>
            ${applySteps(apply)}
            <div class="ap-actions"><button class="btn ghost tiny" data-act="dismiss">Dismiss</button>
                <button class="btn tiny" data-act="read">Read keyboard</button></div></div>`);
        node.querySelector('[data-act="dismiss"]').addEventListener("click", () => {
            state.applyDismissed = apply.id;
            render();
        });
        node.querySelector('[data-act="read"]').addEventListener("click", () => post({type: "refresh"}));
        return node;
    }

    if (draft.busy) {
        return el(`<div class="commit applying">
            <span class="n"><span class="spin"></span> <strong>Working</strong>
            <span class="muted">${esc(model?.device?.health?.phase || "talking to the keyboard")}</span></span>
            <span class="sep"></span>
            <span class="note">the keyboard keeps running its saved profile until both halves confirm</span></div>`);
    }
    if (!draft.matching) {
        const node = el(`<div class="commit" style="border-color:var(--draft)">
            <span class="n"><i class="dot draft"></i> <strong>Draft belongs to another keyboard</strong>
            <span class="muted">${draft.changes.length} change${draft.changes.length === 1 ? "" : "s"} kept locally. Select that keyboard to continue${model?.device?.connected ? ", or discard the draft here" : ""}.</span></span>
            <span class="sep"></span>
            ${model?.device?.connected ? '<button class="btn ghost" data-act="discard">Discard draft</button>' : ""}</div>`);
        node.querySelector('[data-act="discard"]')?.addEventListener("click", () => post({type: "discardProfileDraft"}));
        return node;
    }
    if (draft.stale) {
        const node = el(`<div class="commit" style="border-color:var(--draft)">
            <span class="n"><i class="dot draft"></i> <strong>${draft.connectionChanged ? "Keyboard connection changed" : "The keyboard changed"}</strong>
            <span class="muted">${draft.connectionChanged ? "Review the current keyboard before continuing" : "since this draft began"} — your ${draft.changes.length} change${draft.changes.length === 1 ? "" : "s"} are kept</span></span>
            <span class="sep"></span>
            <button class="btn" data-act="rebase">Review against the keyboard</button>
            <button class="btn ghost" data-act="discard">Discard draft</button></div>`);
        node.querySelector('[data-act="rebase"]').addEventListener("click", () => post({type: "rebaseProfileDraft"}));
        node.querySelector('[data-act="discard"]').addEventListener("click", () => post({type: "discardProfileDraft"}));
        return node;
    }
    if (!draft.dirty) return null;

    const areas = [...new Set(draft.changes.map((change) => change.area))];
    const node = el(`<div class="commit">
        <span class="n"><i class="dot draft"></i> <strong>${draft.changes.length} change${draft.changes.length === 1 ? "" : "s"}</strong>
        <span class="muted">in your draft</span></span>
        <span class="peek"><span class="note" style="white-space:nowrap">${esc(areas.join(" · ").toLowerCase())}</span></span>
        <span class="sep"></span>
        <button class="btn ghost tiny" data-act="discard">Discard</button>
        <button class="btn" data-act="review">Review</button>
        <button class="btn primary" data-act="review"
            data-tip="Review the complete draft, then write it to both halves as one generation.">Apply to keyboard</button></div>`);
    node.querySelectorAll('[data-act="review"]').forEach((button) => button.addEventListener("click", () => {
        state.overlay = "review";
        post({type: "reviewProfileDraft"});
    }));
    node.querySelector('[data-act="discard"]').addEventListener("click", () => post({type: "discardProfileDraft"}));
    return node;
}

// Read-only reasons, said plainly where the control is.
export function unavailable(model) {
    if (!model?.device?.connected) return "No keyboard is connected. Connect one and choose Read keyboard.";
    if (!model?.layers?.length) return "Nothing has been read from the keyboard yet. Choose Read keyboard.";
    if (!model?.draft) return "This keyboard's firmware cannot hold a complete eight-layer profile, so edits cannot be drafted here.";
    if (!model.draft.matching) return "The local draft belongs to another keyboard. Select it again or discard the draft here.";
    if (model.draft.stale) return "Review the draft against the current keyboard before editing further.";
    return "";
}
