// Macros: both banks the keyboard reports, edited as the payload it stores.
//
// Slots have no name on the device, so they are identified by their number and
// their payload. The preview parses that payload for reading; the host parses
// it again for real when the slot is staged, and says so if it disagrees.

import {el, esc} from "../lib/dom.mjs";
import {describeStep, macroPeek, parseMacro, serializeMacro, unreleased} from "../view/macro.mjs";
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
        const peek = macroPeek(row.payload, (name) => model?.qmkKeyLabels?.[name] || name);
        const cell = el(`<button class="mslot ${row.empty ? "" : "filled"}" data-slot="${esc(row.keycode)}"
            aria-current="${row.keycode === slot?.keycode}"
            data-tip="${esc(row.keycode)} · ${row.empty ? "empty slot" : `${row.bytes} bytes · ${steps.length} steps · ${esc(row.payload)}`}">
            <span class="n">${state.macroBank === "user" ? "U" : "M"}${index}</span>
            <span class="v">${row.empty ? "—" : esc(peek.length > 13 ? `${peek.slice(0, 12)}…` : peek)}</span></button>`);
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
        state.macroCursors = {...state.macroCursors, [slot.keycode]: textarea.selectionStart};
    });
    textarea.addEventListener("change", () => stageMacro(model, slot, textarea.value));
    for (const eventName of ["click", "keyup", "select"]) textarea.addEventListener(eventName, () => {
        state.macroCursors = {...state.macroCursors, [slot.keycode]: textarea.selectionStart};
    });
    card.querySelector('[data-act="place"]').addEventListener("click", () => {
        state.placement = {keycode: slot.keycode, label: slot.keycode};
        state.screen = "keys"; state.tab = "key"; render();
    });

    const body = card.querySelector(".card-b");
    body.append(stepBuilder(model, slot, canEdit, textarea));
    body.append(preview(model, slot, steps, error, held, payload, canEdit));
    body.append(actions(model, slot, canEdit, dirty, payload));
    wrap.append(card);
    wrap.append(recorder(model, slot, canEdit, textarea));
    return wrap;
}

function stageMacro(model, slot, payload) {
    const parsed = parseMacro(payload);
    if (parsed.error || unreleased(parsed.steps).length) return false;
    state.macroDrafts = {...state.macroDrafts, [slot.keycode]: payload};
    post({
        type: "updateViaMacro", keycode: slot.keycode, payload,
        expectedFingerprint: model?.macroEditing?.identity,
    });
    return true;
}

function stepBuilder(model, slot, canEdit, textarea) {
    const stepDraft = state.macroSteps?.[slot.keycode] || {kind: "tap", value: ""};
    const node = el(`<div class="card" style="background:var(--surface-2)">
        <div class="card-h" style="padding:10px 12px"><h3>Add a step</h3><span class="right note">inserted at the cursor</span></div>
        <div class="card-b" style="padding:12px;display:grid;grid-template-columns:160px minmax(0,1fr) auto;gap:8px;align-items:end">
            <label class="field"><span>Step</span><select class="input" data-kind ${canEdit ? "" : "disabled"}>
                ${STEP_KINDS.map(([value, text]) => `<option value="${value}" ${stepDraft.kind === value ? "selected" : ""}>${text}</option>`).join("")}</select></label>
            <label class="field"><span data-label>Keys</span>
                <div class="input-row"><input class="input mono" data-value value="${esc(stepDraft.value)}" placeholder="KC_LGUI, KC_D" ${canEdit ? "" : "disabled"}>
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
    const remember = () => {
        state.macroSteps = {...state.macroSteps, [slot.keycode]: {kind: kind.value, value: value.value}};
    };
    kind.addEventListener("change", () => { remember(); sync(); });
    value.addEventListener("input", remember);
    sync();
    pick.addEventListener("click", () => openPicker({
        title: "Macro step keys", context: slot.keycode, mode: "list",
        seed: value.value.split(",").map((name) => name.trim()).filter(Boolean),
        onPick: (expression) => {
            state.macroSteps = {...state.macroSteps, [slot.keycode]: {kind: kind.value, value: expression}};
            state.picker = null;
            render();
        },
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
        const cursor = Math.max(0, Math.min(textarea.value.length,
            state.macroCursors?.[slot.keycode] ?? textarea.selectionStart ?? textarea.value.length));
        textarea.value = `${textarea.value.slice(0, cursor)}${addition}${textarea.value.slice(cursor)}`;
        state.macroCursors = {...state.macroCursors, [slot.keycode]: cursor + addition.length};
        stageMacro(model, slot, textarea.value);
        render();
    });
    return node;
}

function preview(model, slot, steps, error, held, payload, canEdit) {
    const node = el(`<div>
        <div class="sect-h"><h4>Payload preview</h4>
            <span class="right note">${payload.length} source chars · ${slot.bytes} bytes as read · ${steps.length} step${steps.length === 1 ? "" : "s"}</span></div>
    </div>`);
    if (error) node.append(el(`<div class="unavailable">${esc(error)} The keyboard would refuse this payload, so it cannot be staged until it reads cleanly.</div>`));
    if (!error && held.length) node.append(el(`<div class="unavailable">This macro never releases ${esc(held.join(", "))}. The keyboard would keep holding ${held.length === 1 ? "it" : "them"} after the macro ends.</div>`));
    if (!steps.length) node.append(el(`<p class="note">This slot is empty. Type a payload, add a step, or record one.</p>`));
    steps.forEach((step, index) => {
        const row = el(`<div class="step"><span class="grip">⠿</span><span class="kind">${esc(step.kind)}</span>
            <span class="tok">${esc(describeStep(step))}</span><span class="right row" style="gap:4px">
                <button class="btn tiny ghost" data-move="-1" ${canEdit && index > 0 ? "" : "disabled"} aria-label="Move step up">↑</button>
                <button class="btn tiny ghost" data-move="1" ${canEdit && index < steps.length - 1 ? "" : "disabled"} aria-label="Move step down">↓</button>
                <button class="btn tiny ghost" data-remove ${canEdit ? "" : "disabled"}>Remove</button></span></div>`);
        row.querySelector("[data-remove]")?.addEventListener("click", () => {
            const next = steps.filter((_, candidate) => candidate !== index);
            const nextPayload = serializeMacro(next);
            stageMacro(model, slot, nextPayload);
            render();
        });
        row.querySelectorAll("[data-move]").forEach((button) => button.addEventListener("click", () => {
            const target = index + Number(button.dataset.move);
            if (target < 0 || target >= steps.length) return;
            const next = steps.slice();
            [next[index], next[target]] = [next[target], next[index]];
            const nextPayload = serializeMacro(next);
            stageMacro(model, slot, nextPayload);
            render();
        }));
        node.append(row);
    });
    node.append(el(`<div class="meter" style="margin-top:10px"><i style="width:${Math.min(100, (slot.bytes / 128) * 100)}%"></i></div>`));
    node.append(el(`<p class="note" style="margin-top:6px">The byte count is the keyboard's, from the last read; staging the slot updates it.</p>`));
    return node;
}

function actions(model, slot, canEdit, dirty, payload) {
    const {steps, error} = parseMacro(payload);
    const blocked = Boolean(error || unreleased(steps).length);
    const node = el(`<div class="row" style="gap:8px">
        <span class="note">${blocked ? "Fix the payload before it can be staged." : "Valid changes are kept in the draft automatically."}</span>
        <button class="btn ghost" data-act="discard" ${dirty ? "" : "disabled"}
            data-tip="Drop text that has not passed validation and show the latest staged payload.">Discard local text</button>
        <button class="btn ghost" data-act="clear" ${canEdit ? "" : "disabled"}>Clear payload</button>
    </div>`);
    node.querySelector('[data-act="discard"]').addEventListener("click", () => {
        const drafts = {...state.macroDrafts};
        delete drafts[slot.keycode];
        state.macroDrafts = drafts;
        render();
    });
    node.querySelector('[data-act="clear"]').addEventListener("click", () => {
        stageMacro(model, slot, "");
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

function recorder(model, slot, canEdit, textarea) {
    const recording = state.recording?.slot === slot.keycode;
    const node = el(`<div class="card">
        <div class="card-h"><h3>Record</h3><span class="right"><span class="tag">${recording ? "recording" : "idle"}</span></span></div>
        <div class="card-b" style="display:grid;gap:14px">
            <div class="row" style="gap:12px;align-items:end;flex-wrap:wrap">
                <label class="field" style="min-width:190px"><span>Capture style</span><select class="input" data-mode ${recording ? "disabled" : ""}>
                    <option value="compact" ${state.recordMode !== "explicit" ? "selected" : ""}>Compact taps</option>
                    <option value="explicit" ${state.recordMode === "explicit" ? "selected" : ""}>Explicit press and release</option>
                </select></label>
                <label class="sw"><input type="checkbox" data-delays ${state.recordDelays !== false ? "checked" : ""}>
                    <span class="track"></span><span class="txt">Record delays</span></label>
                <label class="field" style="width:120px"><span>After (ms)</span><input class="input mono" data-threshold value="${esc(state.recordDelayThreshold)}" ${state.recordDelays === false ? "disabled" : ""}></label>
                <label class="field" style="width:120px"><span>Round to (ms)</span><input class="input mono" data-round value="${esc(state.recordDelayRound)}" ${state.recordDelays === false ? "disabled" : ""}></label>
            </div>
            <div class="row" style="gap:8px">
                <span class="note">${recording ? "Typing in this window is captured. Press Escape or Stop when the take is done." : "Captures this window's key events at the end of the payload."}</span>
                <span class="right row" style="gap:8px"><button class="btn ghost" data-act="clear-take" ${state.recording?.before !== undefined ? "" : "disabled"}>Clear take</button>
                <button class="btn ${recording ? "" : "primary"}" data-act="record" ${canEdit ? "" : "disabled"}>${recording ? "Stop" : "● Record"}</button></span>
            </div>
        </div></div>`);

    node.querySelector("[data-mode]").addEventListener("change", (event) => { state.recordMode = event.target.value; });
    node.querySelector("[data-delays]").addEventListener("change", (event) => { state.recordDelays = event.target.checked; render(); });
    node.querySelector("[data-threshold]").addEventListener("change", (event) => {
        state.recordDelayThreshold = Math.max(0, Number(event.target.value) || 0);
    });
    node.querySelector("[data-round]").addEventListener("change", (event) => {
        state.recordDelayRound = Math.max(1, Number(event.target.value) || 1);
    });
    node.querySelector('[data-act="clear-take"]').addEventListener("click", () => {
        const before = state.recording?.before;
        document.removeEventListener("keydown", onRecordKey, true);
        document.removeEventListener("keyup", onRecordKey, true);
        state.recording = null;
        if (before !== undefined) state.macroDrafts = {...state.macroDrafts, [slot.keycode]: before};
        render();
    });
    node.querySelector('[data-act="record"]').addEventListener("click", () => {
        if (recording) { stopRecording(model, slot); return; }
        startRecording(slot, textarea.value);
    });
    return node;
}

function startRecording(slot, before) {
    state.recording = {slot: slot.keycode, before, last: Date.now()};
    document.addEventListener("keydown", onRecordKey, true);
    document.addEventListener("keyup", onRecordKey, true);
    render();
}

function stopRecording(model = getModel(), slot = null) {
    const recordedSlot = state.recording?.slot;
    document.removeEventListener("keydown", onRecordKey, true);
    document.removeEventListener("keyup", onRecordKey, true);
    state.recording = state.recording ? {...state.recording, slot: null} : null;
    const target = slot || [...(model?.viaMacros || []), ...(model?.hardcodedMacros || [])]
        .find((candidate) => candidate.keycode === recordedSlot);
    if (target) stageMacro(model, target, state.macroDrafts?.[target.keycode] ?? target.payload ?? "");
    render();
}

function onRecordKey(event) {
    const recording = state.recording;
    if (!recording?.slot) return;
    if (event.key === "Escape" && event.type === "keydown") { event.preventDefault(); stopRecording(); return; }
    const keycode = codeToKeycode(event.code);
    if (!keycode) return;
    if (event.repeat) { event.preventDefault(); return; }
    if (state.recordMode !== "explicit" && event.type === "keyup") return;
    event.preventDefault();
    const now = Date.now();
    const gap = now - recording.last;
    const current = state.macroDrafts?.[recording.slot] ?? "";
    const threshold = Math.max(0, Number(state.recordDelayThreshold) || 0);
    const round = Math.max(1, Number(state.recordDelayRound) || 1);
    const delay = state.recordDelays !== false && gap > threshold ? `{${Math.round(gap / round) * round}}` : "";
    const command = state.recordMode === "explicit"
        ? `{${event.type === "keydown" ? "+" : "-"}${keycode}}`
        : `{${keycode}}`;
    state.macroDrafts = {...state.macroDrafts, [recording.slot]: `${current}${delay}${command}`};
    state.recording = {...recording, last: now};
    render();
}
