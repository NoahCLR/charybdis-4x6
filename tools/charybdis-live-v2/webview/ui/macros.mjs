// Macros: both banks the keyboard reports, edited as the payload it stores.
//
// Slots have no name on the device, so they are identified by their number and
// their payload. The preview parses that payload for reading; the host parses
// it again for real when the slot is kept, and says so if it disagrees.

import {el, esc} from "../lib/dom.mjs";
import {describeStep, parseMacro, unreleased} from "../view/macro.mjs";
import {getModel, post, render, state, writable} from "../store.mjs";
import {openPicker} from "./picker.mjs";
import {topbar, unavailable} from "./shell.mjs";

const STEP_KINDS = [
    ["tap", "Tap key or chord"], ["press", "Press and hold"], ["release", "Release"],
    ["text", "Text"], ["delay", "Delay"],
];

export function screenMacros() {
    const model = getModel();
    const via = model?.viaMacros || [];
    const user = model?.hardcodedMacros || [];
    const canEdit = writable() && Boolean(model?.macroEditing?.writable);
    const bank = state.macroBank === "user" ? user : via;
    const slot = bank.find((row) => row.keycode === state.macroSlot) || bank[0];

    const main = el(`<div class="main">${topbar(
        "Macros",
        `${via.length + user.length} slots on the keyboard: ${via.length} VIA slots and ${user.length} user slots. Slots have no name on the keyboard, so they are identified by their number and their payload.`,
        `<div class="seg" data-bank>
            <button data-b="via" aria-pressed="${state.macroBank !== "user"}">VIA bank · ${via.length}</button>
            <button data-b="user" aria-pressed="${state.macroBank === "user"}">User bank · ${user.length}</button></div>`,
    )}</div>`);
    main.querySelectorAll("[data-bank] button").forEach((button) => button.addEventListener("click", () => {
        state.macroBank = button.dataset.b;
        state.macroSlot = null;
        render();
    }));

    const content = el(`<div class="content"><div class="pad" style="display:grid;grid-template-columns:minmax(0,340px) minmax(0,1fr);gap:20px;align-items:start"></div></div>`);
    const pad = content.firstElementChild;
    if (!bank.length) {
        pad.style.display = "block";
        pad.appendChild(el(`<div class="screen-stub"><h3>No macro banks read</h3>
            <p class="note">${esc(unavailable(model) || "This firmware has not reported its macro banks.")}</p></div>`));
        main.appendChild(content);
        return main;
    }

    const filled = bank.filter((row) => !row.empty).length;
    const grid = el(`<div class="card"><div class="card-h"><h3>${state.macroBank === "user" ? "User slots" : "VIA slots"}</h3>
        <span class="right tag">${filled} of ${bank.length} filled</span></div>
        <div class="card-b"><div class="macro-grid"></div>
        <p class="note" style="margin-top:12px">Empty slots stay visible so you can see what the keyboard has room for.</p></div></div>`);
    const cells = grid.querySelector(".macro-grid");
    bank.forEach((row, index) => {
        const {steps} = parseMacro(row.payload);
        const peek = steps.find((step) => step.kind === "text")?.text || (steps[0] ? describeStep(steps[0]) : "");
        const cell = el(`<button class="mslot ${row.empty ? "" : "filled"}" data-slot="${esc(row.keycode)}"
            aria-current="${row.keycode === slot?.keycode}"
            data-tip="${esc(row.keycode)} · ${row.empty ? "empty slot" : `${row.bytes} bytes · ${steps.length} steps`}">
            <span class="n">${state.macroBank === "user" ? "U" : "M"}${index}</span>
            <span class="v">${row.empty ? "—" : esc(peek.length > 9 ? `${peek.slice(0, 8)}…` : peek)}</span></button>`);
        cell.addEventListener("click", () => { state.macroSlot = row.keycode; render(); });
        cells.append(cell);
    });
    pad.appendChild(grid);
    pad.appendChild(slot ? editor(model, slot, canEdit) : el(`<div class="empty-card"><p class="note">Pick a slot.</p></div>`));
    main.appendChild(content);
    return main;
}

function editor(model, slot, canEdit) {
    const draft = state.macroDrafts?.[slot.keycode];
    const payload = draft ?? slot.payload ?? "";
    const {steps, error} = parseMacro(payload);
    const held = unreleased(steps);
    const dirty = draft !== undefined && draft !== slot.payload;

    const wrap = el(`<div class="stack"></div>`);
    const card = el(`<div class="card">
        <div class="card-h"><h3>${state.macroBank === "user" ? "User" : "VIA"} macro ${slot.keycode.split("_").at(-1)}</h3>
            <code class="dim">${esc(slot.keycode)}</code>
            <span class="right row" style="gap:8px">
                <span class="chip"><i class="dot ${dirty ? "draft" : "on"}"></i>${dirty ? "edited here" : "as read"}</span>
                <button class="btn tiny ghost" data-act="place">Place on a key…</button></span></div>
        <div class="card-b stack">
            <label class="field"><span>Payload</span>
                <textarea class="input mono" rows="3" style="height:auto;padding:9px 10px;resize:vertical" ${canEdit ? "" : "disabled"}
                    data-tip="Exactly as the keyboard stores it. Text is literal; {KC_A} taps, {+KC_A} presses, {-KC_A} releases, {120} waits. Use {{ and }} for literal braces.">${esc(payload)}</textarea></label>
        </div></div>`);
    const textarea = card.querySelector("textarea");
    textarea.addEventListener("input", () => {
        state.macroDrafts = {...state.macroDrafts, [slot.keycode]: textarea.value};
        render();
        const again = document.querySelector("textarea");
        if (again) { again.focus(); again.setSelectionRange(again.value.length, again.value.length); }
    });
    card.querySelector('[data-act="place"]').addEventListener("click", () => {
        state.screen = "keys"; state.tab = "key"; render();
    });

    const body = card.querySelector(".card-b");
    body.append(stepBuilder(model, slot, canEdit, textarea));
    body.append(preview(slot, steps, error, held, payload));
    body.append(actions(model, slot, canEdit, dirty, payload));
    wrap.append(card);
    wrap.append(recorder(slot, canEdit, textarea));
    return wrap;
}

function stepBuilder(model, slot, canEdit, textarea) {
    const node = el(`<div class="card" style="background:var(--surface-2)">
        <div class="card-h" style="padding:10px 12px"><h3>Add a step</h3><span class="right note">appended to the payload</span></div>
        <div class="card-b" style="padding:12px;display:grid;grid-template-columns:160px minmax(0,1fr) auto;gap:8px;align-items:end">
            <label class="field"><span>Step</span><select class="input" data-kind ${canEdit ? "" : "disabled"}>
                ${STEP_KINDS.map(([value, text]) => `<option value="${value}">${text}</option>`).join("")}</select></label>
            <label class="field"><span data-label>Keys</span>
                <div class="input-row"><input class="input mono" data-value placeholder="KC_LGUI, KC_D" ${canEdit ? "" : "disabled"}>
                <button class="btn" data-act="pick" ${canEdit ? "" : "disabled"}>Pick…</button></div></label>
            <button class="btn" data-act="insert" ${canEdit ? "" : "disabled"}>Add step</button>
        </div></div>`);
    const kind = node.querySelector("[data-kind]");
    const value = node.querySelector("[data-value]");
    const label = node.querySelector("[data-label]");
    const pick = node.querySelector('[data-act="pick"]');
    const sync = () => {
        const mode = kind.value;
        label.textContent = mode === "text" ? "Text" : mode === "delay" ? "Milliseconds" : "Keys";
        value.placeholder = mode === "text" ? "typed literally" : mode === "delay" ? "120" : "KC_LGUI, KC_D";
        pick.style.display = mode === "text" || mode === "delay" ? "none" : "";
    };
    kind.addEventListener("change", sync);
    sync();
    pick.addEventListener("click", () => openPicker({
        title: "Macro step keys", context: slot.keycode, mode: "list",
        seed: value.value.split(",").map((name) => name.trim()).filter(Boolean),
        onPick: (expression) => { value.value = expression; state.picker = null; render(); },
    }));
    node.querySelector('[data-act="insert"]').addEventListener("click", () => {
        const text = value.value.trim();
        if (!text) return;
        const keys = text.split(",").map((name) => name.trim()).filter(Boolean).join(",");
        const addition = kind.value === "text" ? text
            : kind.value === "delay" ? `{${text.replace(/\D/g, "")}}`
            : kind.value === "press" ? `{+${keys}}`
            : kind.value === "release" ? `{-${keys}}`
            : `{${keys}}`;
        textarea.value += addition;
        state.macroDrafts = {...state.macroDrafts, [slot.keycode]: textarea.value};
        render();
    });
    return node;
}

function preview(slot, steps, error, held, payload) {
    const node = el(`<div>
        <div class="sect-h"><h4>Payload preview</h4>
            <span class="right note">${payload.length} source chars · ${slot.bytes} bytes as read · ${steps.length} step${steps.length === 1 ? "" : "s"}</span></div>
    </div>`);
    if (error) node.append(el(`<div class="unavailable">${esc(error)} The keyboard would refuse this payload, so it cannot be kept until it reads cleanly.</div>`));
    if (!error && held.length) node.append(el(`<div class="unavailable">This macro never releases ${esc(held.join(", "))}. The keyboard would keep holding ${held.length === 1 ? "it" : "them"} after the macro ends.</div>`));
    if (!steps.length) node.append(el(`<p class="note">This slot is empty. Type a payload, add a step, or record one.</p>`));
    for (const step of steps) {
        node.append(el(`<div class="step"><span class="grip">⠿</span><span class="kind">${esc(step.kind)}</span>
            <span class="tok">${esc(describeStep(step))}</span></div>`));
    }
    node.append(el(`<div class="meter" style="margin-top:10px"><i style="width:${Math.min(100, (slot.bytes / 128) * 100)}%"></i></div>`));
    node.append(el(`<p class="note" style="margin-top:6px">The byte count is the keyboard's, from the last read; keeping the slot updates it.</p>`));
    return node;
}

function actions(model, slot, canEdit, dirty, payload) {
    const {error} = parseMacro(payload);
    const node = el(`<div class="row" style="gap:8px">
        <button class="btn primary" data-act="keep" ${canEdit && !error ? "" : "disabled"}>Keep macro in draft</button>
        <button class="btn ghost" data-act="discard" ${dirty ? "" : "disabled"}
            data-tip="Drop this slot's local edits and show the payload last read from the keyboard.">Discard slot changes</button>
        <button class="btn ghost" data-act="clear" ${canEdit ? "" : "disabled"}>Clear payload</button>
    </div>`);
    node.querySelector('[data-act="keep"]').addEventListener("click", () => post({
        type: "updateViaMacro", keycode: slot.keycode, payload,
        expectedFingerprint: model?.macroEditing?.identity,
    }));
    node.querySelector('[data-act="discard"]').addEventListener("click", () => {
        const drafts = {...state.macroDrafts};
        delete drafts[slot.keycode];
        state.macroDrafts = drafts;
        render();
    });
    node.querySelector('[data-act="clear"]').addEventListener("click", () => {
        state.macroDrafts = {...state.macroDrafts, [slot.keycode]: ""};
        render();
    });
    return node;
}

/* ── the recorder ──────────────────────────────────────────────────────── */
const EVENT_CODES = {
    Space: "KC_SPC", Enter: "KC_ENT", Tab: "KC_TAB", Backspace: "KC_BSPC", Escape: "KC_ESC",
    Minus: "KC_MINS", Equal: "KC_EQL", BracketLeft: "KC_LBRC", BracketRight: "KC_RBRC",
    Backslash: "KC_BSLS", Semicolon: "KC_SCLN", Quote: "KC_QUOT", Comma: "KC_COMM",
    Period: "KC_DOT", Slash: "KC_SLSH", Backquote: "KC_GRV",
    ShiftLeft: "KC_LSFT", ShiftRight: "KC_RSFT", ControlLeft: "KC_LCTL", ControlRight: "KC_RCTL",
    AltLeft: "KC_LALT", AltRight: "KC_RALT", MetaLeft: "KC_LGUI", MetaRight: "KC_RGUI",
    ArrowUp: "KC_UP", ArrowDown: "KC_DOWN", ArrowLeft: "KC_LEFT", ArrowRight: "KC_RGHT",
};
const codeToKeycode = (code) => EVENT_CODES[code]
    || (/^Key([A-Z])$/.test(code) ? `KC_${code.slice(3)}` : "")
    || (/^Digit(\d)$/.test(code) ? `KC_${code.slice(5)}` : "")
    || (/^F(\d{1,2})$/.test(code) ? `KC_${code}` : "");

function recorder(slot, canEdit, textarea) {
    const recording = state.recording?.slot === slot.keycode;
    const node = el(`<div class="card">
        <div class="card-h"><h3>Record</h3><span class="right"><span class="tag">${recording ? "recording" : "idle"}</span></span></div>
        <div class="card-b" style="display:grid;grid-template-columns:minmax(0,1fr) auto;gap:18px;align-items:end">
            <div class="row" style="gap:14px">
                <label class="sw"><input type="checkbox" data-delays ${state.recordDelays !== false ? "checked" : ""}>
                    <span class="track"></span><span class="txt">Record delays</span></label>
                <span class="note">${recording ? "Typing here is captured. Modifiers arrive as the host sees them, not as the keyboard sends them." : "Captures this window's key events and appends them to the payload."}</span>
            </div>
            <div class="row" style="gap:8px">
                <button class="btn ghost" data-act="clear-take" ${state.recording?.before !== undefined ? "" : "disabled"}>Clear take</button>
                <button class="btn ${recording ? "" : "primary"}" data-act="record" ${canEdit ? "" : "disabled"}>${recording ? "Stop" : "● Record"}</button>
            </div>
        </div></div>`);

    node.querySelector("[data-delays]").addEventListener("change", (event) => { state.recordDelays = event.target.checked; });
    node.querySelector('[data-act="clear-take"]').addEventListener("click", () => {
        const before = state.recording?.before;
        state.recording = null;
        if (before !== undefined) state.macroDrafts = {...state.macroDrafts, [slot.keycode]: before};
        render();
    });
    node.querySelector('[data-act="record"]').addEventListener("click", () => {
        if (recording) { stopRecording(); return; }
        startRecording(slot, textarea.value);
    });
    return node;
}

function startRecording(slot, before) {
    state.recording = {slot: slot.keycode, before, last: Date.now()};
    document.addEventListener("keydown", onRecordKey, true);
    render();
}

function stopRecording() {
    document.removeEventListener("keydown", onRecordKey, true);
    state.recording = state.recording ? {...state.recording, slot: null} : null;
    render();
}

function onRecordKey(event) {
    const recording = state.recording;
    if (!recording?.slot) return;
    if (event.key === "Escape") { stopRecording(); return; }
    const keycode = codeToKeycode(event.code);
    if (!keycode) return;
    event.preventDefault();
    const now = Date.now();
    const gap = now - recording.last;
    const current = state.macroDrafts?.[recording.slot] ?? "";
    const delay = state.recordDelays !== false && gap > 30 ? `{${Math.round(gap / 10) * 10}}` : "";
    state.macroDrafts = {...state.macroDrafts, [recording.slot]: `${current}${delay}{${keycode}}`};
    state.recording = {...recording, last: now};
    render();
}
