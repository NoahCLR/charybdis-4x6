// Macros: the keyboard's macro slots, edited as the payload it stores, each
// with an optional name kept in the keyboard's profile. The slots share one
// block of macro memory; the host says which still have room.
//
// The preview parses the payload for reading; the host parses it again for
// real when the slot is staged, and says so if it disagrees.

import {el, esc} from "../lib/dom.mjs";
import {describeStep, macroMatches, macroPeek, parseMacro, serializeMacro, unreleased} from "../view/macro.mjs";
import {macroPlacements} from "../view/keyface.mjs";
import {getModel, layerName, layers, post, render, state, writable} from "../store.mjs";
import * as edits from "../view/edits.mjs";
import {layerSwatch} from "./layerbar.mjs";
import {openPicker} from "./picker.mjs";
import {topbar, unavailable} from "./shell.mjs";

const STEP_KINDS = [
    ["tap", "Tap key or chord"], ["press", "Press and hold"], ["release", "Release"],
    ["text", "Text"], ["delay", "Delay"],
];

export function screenMacros() {
    const model = getModel();
    const bank = model?.viaMacros || [];
    const memory = model?.macroBank;
    const slot = bank.find((row) => row.keycode === state.macroSlot) || bank[0];
    const canEdit = writable() && Boolean(model?.macroEditing?.writable) && slot?.available !== false;

    const main = el(`<div class="main">${topbar(
        "Macros",
        "Macro slots on the keyboard. Give one a name and it shows by that name on keys, in the picker and in the key card; the name is saved on the keyboard with everything else.",
        bank.length ? `<input class="input" id="macroSearch" type="search" placeholder="Search macros by name" style="width:220px" value="${esc(state.macroSearch || "")}">` : "",
    )}</div>`);
    main.querySelector("#macroSearch")?.addEventListener("input", (event) => { state.macroSearch = event.target.value; render(); });

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
    const available = memory?.available ?? bank.length;
    const grid = el(`<div class="card"><div class="card-h"><h3>Slots</h3>
        <span class="right tag" data-tip="${filled} filled">${available} of ${bank.length} available</span></div>
        <div class="card-b"><div class="macro-grid"></div>${memoryMeter(memory)}</div></div>`);
    const cells = grid.querySelector(".macro-grid");
    const query = state.macroSearch || "";
    if (!bank.some((row) => macroMatches(row, query))) {
        cells.replaceWith(el(`<p class="note">No macro is named “${esc(query.trim())}”.</p>`));
    }
    bank.forEach((row, index) => {
        if (!macroMatches(row, query)) return;
        const {steps} = parseMacro(row.payload);
        const peek = macroPeek(row.payload, (name) => model?.qmkKeyLabels?.[name] || name);
        const out = row.available === false;
        const tip = out ? `Slot ${index} · no room: the free macro memory is kept for the lower empty slots. Shorten or clear a macro to open it.`
            : row.empty ? `Slot ${index} · empty` : `Slot ${index} · ${steps.length} step${steps.length === 1 ? "" : "s"} · ${row.program} of ${memory?.programMax ?? 512} bytes to play · ${row.payload}`;
        const cell = el(`<button class="mslot ${row.empty ? "" : "filled"} ${out ? "out" : ""} ${row.playable === false ? "warn" : ""}" data-slot="${esc(row.keycode)}"
            aria-current="${row.keycode === slot?.keycode}" ${out ? "disabled" : ""} data-tip="${esc(tip)}">
            <span class="n">M${index}</span>
            <span class="v">${row.playable === false ? "too long" : row.name ? esc(row.name.length > 13 ? `${row.name.slice(0, 12)}…` : row.name) : row.empty ? (out ? "no room" : "—") : esc(peek.length > 13 ? `${peek.slice(0, 12)}…` : peek)}</span></button>`);
        cell.addEventListener("click", () => { state.macroSlot = row.keycode; render(); });
        cells.append(cell);
    });
    pad.appendChild(grid);
    pad.appendChild(slot ? editor(model, slot, canEdit) : el(`<div class="empty-card"><p class="note">Pick a slot.</p></div>`));
    main.appendChild(content);
    return main;
}

// All slots share one block of macro memory. Each empty slot keeps room for
// a few key taps; when that runs out the highest empty slots close.
function memoryMeter(memory) {
    if (!memory) return "";
    const used = memory.stored, share = Math.min(100, (used / memory.capacity) * 100);
    const closed = memory.slots - memory.available;
    return `<div class="sect-h" style="margin-top:14px"><h4>Macro memory</h4>
            <span class="right note">${used.toLocaleString("en-US")} of ${memory.capacity.toLocaleString("en-US")} bytes used</span></div>
        <div class="meter"><i style="width:${share}%"></i></div>
        <p class="note" style="margin-top:6px">A key tap takes 3 bytes, a typed character 1. Every empty slot keeps room for ${memory.reserveTaps} key taps${closed ? `; ${closed} slot${closed === 1 ? " has" : "s have"} no room left` : ""}.</p>`;
}

function editor(model, slot, canEdit) {
    const draft = state.macroDrafts?.[slot.keycode];
    const payload = draft ?? slot.payload ?? "";
    const {steps, error} = parseMacro(payload, {keys: model?.macroPayloadKeycodes});
    const held = unreleased(steps);
    const dirty = draft !== undefined && draft !== slot.payload;

    const wrap = el(`<div class="stack"></div>`);
    const card = el(`<div class="card">
        <div class="card-h"><h3>${esc(slot.name || `Macro ${slot.keycode.split("_").at(-1)}`)}</h3>
            <span class="tag" data-tip="${esc(slot.keycode)}">Slot ${esc(slot.keycode.split("_").at(-1))}</span>
            <span class="right row" style="gap:8px">
                <span class="chip"><i class="dot ${dirty ? "draft" : "on"}"></i>${dirty ? "edited here" : "as read"}</span>
                <button class="btn tiny ghost" data-act="place" ${writable() ? "" : "disabled"}>Place on a key…</button></span></div>
        <div class="card-b stack">
            ${nameField(model, slot, canEdit)}
            ${placedOn(model, slot)}
            <label class="field"><span>Payload</span>
                <textarea class="input mono" rows="3" style="height:auto;padding:9px 10px;resize:vertical" ${canEdit ? "" : "disabled"}
                    data-tip="Exactly as the keyboard stores it. Text is literal; {KC_A} taps, {+KC_A} presses, {-KC_A} releases, {120} waits. Use {{ and }} for literal braces.">${esc(payload)}</textarea></label>
        </div></div>`);
    const textarea = card.querySelector("textarea");
    const nameInput = card.querySelector("[data-name]");
    nameInput?.addEventListener("input", () => {
        card.querySelector("[data-name-count]").textContent = `${nameInput.value.length} / ${model.macroNameSpace.perName} characters`;
    });
    nameInput?.addEventListener("change", () => post(edits.macroNameMessage(slot.keycode, nameInput.value, model?.macroEditing?.identity)));
    textarea.addEventListener("input", () => {
        state.macroDrafts = {...state.macroDrafts, [slot.keycode]: textarea.value};
        state.macroCursors = {...state.macroCursors, [slot.keycode]: textarea.selectionStart};
    });
    textarea.addEventListener("change", () => stageMacro(model, slot, textarea.value));
    for (const eventName of ["click", "keyup", "select"]) textarea.addEventListener(eventName, () => {
        state.macroCursors = {...state.macroCursors, [slot.keycode]: textarea.selectionStart};
    });
    card.querySelectorAll("[data-goto-layer]").forEach((button) => button.addEventListener("click", () =>
        showOnLayer(slot.keycode, Number(button.dataset.gotoLayer))));
    card.querySelector('[data-act="place"]').addEventListener("click", () => {
        state.placement = {keycode: slot.keycode, label: slot.keycode};
        state.screen = "keys"; state.tab = "key"; render();
    });

    const body = card.querySelector(".card-b");
    // A macro the keyboard will not play says so first, above its steps.
    const max = model?.macroBank?.programMax ?? 512;
    if (slot.playable === false) body.prepend(el(`<div class="unavailable">This macro compiles to ${esc(slot.program)} bytes and the keyboard plays at most ${esc(max)}, so pressing it does nothing. Shorten it by about ${esc(Math.ceil((slot.program - max) / 3))} key taps.</div>`));
    if (slot.available === false) body.prepend(el(`<div class="unavailable">This slot has no room: the free macro memory is kept for the lower empty slots, each with room for ${esc(model?.macroBank?.reserveTaps ?? 10)} key taps. Shorten or clear another macro to open it.</div>`));
    body.append(stepBuilder(model, slot, canEdit, textarea));
    body.append(preview(model, slot, steps, error, held, payload, canEdit));
    body.append(actions(model, slot, canEdit, dirty, payload));
    wrap.append(card);
    wrap.append(recorder(model, slot, canEdit, textarea));
    return wrap;
}

// Every slot can hold a full-length name. The host enforces the rule; the
// field stops at the limit and the counter says where it stands.
function nameField(model, slot, canEdit) {
    const space = model?.macroNameSpace;
    if (!space) return "";
    return `<label class="field"><span>Name</span>
        <input class="input" data-name maxlength="${esc(space.perName)}" value="${esc(slot.name || "")}" placeholder="Macro ${esc(slot.keycode.split("_").at(-1))}" ${canEdit ? "" : "disabled"}
            data-tip="Up to ${esc(space.perName)} plain characters: letters, digits, spaces and punctuation.">
        <span class="note" data-name-count>${esc((slot.name || "").length)} / ${esc(space.perName)} characters</span></label>`;
}

// The layers whose keys carry this macro, each in its layer colour. A click
// opens that layer in Keys with this macro picked in its Macros tab, so the
// board rings its keys and the table marks its row.
function placedOn(model, slot) {
    const placements = macroPlacements(layers(), slot.keycode);
    const chips = placements.map(({layer, at, positions}) => {
        const swatch = layerSwatch(model, layer);
        const keys = `${positions.length} key${positions.length === 1 ? "" : "s"}`;
        return `<button class="layer-chip link" data-goto-layer="${at}"
            data-tip="${esc(swatch.tip)} · ${keys} · show on the board">${swatch.html}
            <span>${esc(layerName(layer))}</span><span class="idx">${layer.index}</span></button>`;
    }).join("");
    return `<div class="field"><span>On layers</span>
        ${chips ? `<div class="row" style="gap:6px;flex-wrap:wrap">${chips}</div>`
            : `<span class="note">No key carries this macro. Place it on a key to use it.</span>`}</div>`;
}

function showOnLayer(keycode, at) {
    const placement = macroPlacements(layers(), keycode).find((entry) => entry.at === at);
    if (!placement) return;
    Object.assign(state, {screen: "keys", tab: "macros", layer: at, selected: placement.positions[0], placement: null});
    state.reachRow.macros = `here:${keycode}`;
    state.groups.macros.here = true;
    render();
}

function stageMacro(model, slot, payload) {
    const parsed = parseMacro(payload, {keys: model?.macroPayloadKeycodes});
    if (parsed.error || unreleased(parsed.steps).length) return false;
    state.macroDrafts = {...state.macroDrafts, [slot.keycode]: payload};
    post(edits.macroMessage(slot.keycode, payload, model?.macroEditing?.identity));
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
            <span class="right note">${steps.length} step${steps.length === 1 ? "" : "s"}</span></div>
    </div>`);
    const max = model?.macroBank?.programMax ?? 512;
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
    // The keyboard compiles a macro before playing it, and plays only one
    // that compiles to at most `max` bytes; the host refuses a longer edit.
    node.append(el(`<div class="meter ${slot.playable === false ? "over" : ""}" style="margin-top:10px"><i style="width:${Math.min(100, ((slot.program ?? 0) / max) * 100)}%"></i></div>`));
    node.append(el(`<p class="note" style="margin-top:6px">${esc(slot.program ?? 0)} of ${esc(max)} bytes the keyboard can play${slot.playable === false ? "" : ` · room for about ${esc(slot.roomTaps)} more key taps`}. Sizes follow the staged macro.</p>`));
    return node;
}

function actions(model, slot, canEdit, dirty, payload) {
    const {steps, error} = parseMacro(payload, {keys: model?.macroPayloadKeycodes});
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
                <span class="right row" style="gap:8px"><button class="btn ghost" data-act="clear-take" ${recording || state.lastTake?.slot === slot.keycode ? "" : "disabled"}>Clear take</button>
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
    // Clear take puts the payload back to what it was before Record. A take
    // still in progress was never staged; a finished one was, so its restore
    // is staged too, rather than only rewriting the text on screen.
    node.querySelector('[data-act="clear-take"]').addEventListener("click", () => {
        const take = state.recording?.slot === slot.keycode ? state.recording
            : state.lastTake?.slot === slot.keycode ? state.lastTake : null;
        if (!take) return;
        const staged = !state.recording;
        document.removeEventListener("keydown", onRecordKey, true);
        document.removeEventListener("keyup", onRecordKey, true);
        state.recording = null;
        state.lastTake = null;
        state.macroDrafts = {...state.macroDrafts, [slot.keycode]: take.before};
        if (staged) stageMacro(model, slot, take.before);
        render();
    });
    node.querySelector('[data-act="record"]').addEventListener("click", () => {
        if (recording) { stopRecording(model, slot); return; }
        startRecording(slot, textarea.value);
    });
    return node;
}

// A take appends to what the payload held when Record was pressed. The
// recording state exists only while keys are being captured, so everything
// that pauses for a recording — undo, the key shortcuts — resumes on Stop.
function startRecording(slot, before) {
    state.lastTake = null;
    state.macroDrafts = {...state.macroDrafts, [slot.keycode]: before};
    state.recording = {slot: slot.keycode, before, last: Date.now(), captured: false};
    document.addEventListener("keydown", onRecordKey, true);
    document.addEventListener("keyup", onRecordKey, true);
    render();
}

function stopRecording(model = getModel(), slot = null) {
    const recordedSlot = state.recording?.slot;
    document.removeEventListener("keydown", onRecordKey, true);
    document.removeEventListener("keyup", onRecordKey, true);
    state.lastTake = state.recording ? {slot: recordedSlot, before: state.recording.before} : null;
    state.recording = null;
    const target = slot || (model?.viaMacros || [])
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
    state.macroDrafts = {...state.macroDrafts, [recording.slot]: edits.recordedPayload(current, {
        keycode, type: event.type, gap, captured: recording.captured,
        delays: state.recordDelays !== false, threshold: state.recordDelayThreshold, round: state.recordDelayRound,
        explicit: state.recordMode === "explicit",
    })};
    state.recording = {...recording, last: now, captured: true};
    render();
}
