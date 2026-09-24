// Pointing modes: eight device-owned slots. Each one re-reads the trackball as
// scrolling or as directional keys while its key is held or toggled.
//
// The surface leads with what the mode is — name, movement, speed, actions —
// and keeps thresholds, ratios and button overrides under Advanced, where they
// belong for the once-a-year visit.

import {css, isOff} from "../lib/colour.mjs";
import {el, esc} from "../lib/dom.mjs";
import {AXIS, BUTTON, DIAGONALS, DIRECTIONS, EMPTY_DIRECTION, KIND, SCROLL_FIELDS, axisReads, dpiOptions, newMode, readConfig, readsHorizontal, readsVertical, settleButtons, startingRecord} from "../view/pointing-config.mjs";
import {MODIFIER_BITS, keyName, modifierNames} from "../view/keyvalues.mjs";
import {bindingsForSlot} from "../view/keyface.mjs";
import {pdColourRow, stageEnabled} from "../view/lighting.mjs";
import {getModel, post, render, state, writable} from "../store.mjs";
import * as edits from "../view/edits.mjs";
import {openPicker} from "./picker.mjs";
import {topbar, unavailable} from "./shell.mjs";


const AXES = [[2, "Dominant axis"], [3, "Eight directions"], [0, "Vertical only"], [1, "Horizontal only"]];
const INVERT = [[0, "Neither axis"], [1, "Horizontal"], [2, "Vertical"], [3, "Both axes"]];
const POINTER_LAYER = [[0, "Keep the pointer layer active"], [1, "Return to the typing layer"]];
const BUTTON_KINDS = [[BUTTON.PASS_THROUGH, "Pass through"], [BUTTON.CONSUME, "Consume"], [BUTTON.TAP, "Tap a shortcut"], [BUTTON.HOLD_MODIFIERS, "Hold modifiers"]];
const ARROWS = {up: "↑", left: "←", right: "→", down: "↓", upLeft: "↖", upRight: "↗", downLeft: "↙", downRight: "↘"};

// The four scroll fields the Scrolling card shows itself. A field rendered
// twice registers its reader twice, and the second input silently wins, so
// Advanced shows only what the card above does not.
const SCROLL_LEAD = [["divisorH", "Movement per step ↔"], ["divisorV", "Movement per step ↕"],
    ["intervalMs", "Minimum interval", "ms"], ["lockMs", "Axis lock timeout", "ms"]];
// The rest of the scroll record, as Advanced lays it out: single values, then
// the two axis ratios, each as one numerator : denominator pair.
const SCROLL_SINGLES = [["thresholdH", "Horizontal activation threshold"], ["thresholdV", "Vertical activation threshold"],
    ["expireMs", "Gesture expiry", "ms"], ["decayDivisor", "Cross-axis decay divisor"]];
const SCROLL_RATIOS = [["Axis selection ratio", "startNumerator", "startDenominator"],
    ["Axis retention ratio", "sustainNumerator", "sustainDenominator"]];
const BINDINGS = ["DRAGSCROLL", "VOLUME_MODE", "BRIGHTNESS_MODE", "ZOOM_MODE", "ARROW_MODE", "PINCH_MODE"];
const bindingName = (slot) => slot.id < BINDINGS.length ? BINDINGS[slot.id] : `PD_SLOT_${slot.id}`;
const axisLabel = (axis) => (AXES.find(([value]) => value === axis) || [, "Dominant axis"])[1];

// A slot's light, as the keyboard would show it: a stage that is off looks off.
function slotLight(model, slot) {
    const row = pdColourRow(model, slot.id);
    const lit = Boolean(row && !isOff(row.color) && stageEnabled(model, "pd"));
    return {row, lit, swatch: (klass = "") => `<span class="pd-swatch ${klass} ${lit ? "" : "swatch-off"}"
        ${lit ? `style="background:${css(row.color)}"` : ""}></span>`};
}

export function screenPointing() {
    const model = getModel();
    const slots = model?.pdModes || [];
    const canEdit = writable() && Boolean(model?.pdModeEditing?.writable);

    const main = el(`<div class="main">${topbar(
        "Pointing modes",
        "Eight device-owned slots. Each re-reads the trackball while its key is held or toggled — as scrolling, or as directional keys and shortcuts.",
        slots.length && canEdit ? `<span class="note">Changes stage as you make them</span>` : "",
    )}</div>`);
    const content = el(`<div class="content"><div class="pad pd-page"></div></div>`);
    const pad = content.firstElementChild;

    if (!slots.length) {
        pad.classList.remove("pd-page");
        pad.appendChild(el(`<div class="screen-stub"><h3>No pointing modes read</h3>
            <p class="note">${esc(unavailable(model) || "This firmware has fixed pointing modes. The configurable-slot firmware and a migrated profile are needed to edit them.")}</p></div>`));
        main.appendChild(content);
        return main;
    }

    const list = el(`<nav class="pd-slots" aria-label="Pointing slots"></nav>`);
    for (const slot of slots) {
        const {swatch} = slotLight(model, slot);
        const inert = !slot.kind ? bindingsForSlot(model, slot).keys.length : 0;
        const card = el(`<button class="slotcard ${slot.kind ? "" : "empty"}" data-slot="${slot.id}" aria-current="${state.pdSlot === slot.id}">
            ${swatch()}
            <span class="nm">${slot.kind ? esc(slot.name) : "Empty slot"}</span><span class="no">${slot.id + 1}</span>
            <span class="meta">${slot.kind === KIND.SCROLLING ? "Scrolling" : slot.kind === KIND.DIRECTIONAL ? `Directional · ${esc(axisLabel(slot.axis))}` : "Available"}</span>
            <span class="meta mono">${esc(bindingName(slot))}</span>
            ${inert ? `<span class="meta warn">${inert} key${inert === 1 ? " still reaches" : "s still reach"} it · inert</span>` : ""}</button>`);
        card.addEventListener("click", () => { state.pdSlot = slot.id; state.pdKind = null; state.pdButtons = null; render(); });
        list.append(card);
    }
    pad.appendChild(list);

    const slot = slots.find((row) => row.id === state.pdSlot) || slots[0];
    pad.appendChild(slot.kind ? editor(model, slot, canEdit, slots) : emptySlot(model, slot, canEdit, slots));
    main.appendChild(content);
    return main;
}

// A binding for an empty slot is allowed by the keyboard, so the interface
// explains the consequence instead of the app refusing the edit.
function inertNote(model, slot) {
    const {keys, behaviours, layers} = bindingsForSlot(model, slot);
    if (!keys.length && !behaviours.length) return "";
    const parts = [];
    if (keys.length) parts.push(`${keys.length} key${keys.length === 1 ? "" : "s"} on ${esc(layers.join(", "))}`);
    if (behaviours.length) parts.push(`${behaviours.length} behaviour${behaviours.length === 1 ? "" : "s"} (${esc(behaviours.map((row) => row.keycode).join(", "))})`);
    return `<div class="unavailable">${parts.join(" and ")} still reach this slot. The keyboard refuses to activate an empty slot, so they do nothing until it is configured — they do not need to be removed first.</div>`;
}

function emptySlot(model, slot, canEdit, slots) {
    const sources = slots.filter((row) => row.kind);
    const node = el(`<div class="pd-editor stack"><div class="card">
        <div class="card-h">${slotLight(model, slot).swatch("lg")}<h3>Slot ${slot.id + 1} is empty</h3>
            <span class="tag">${esc(bindingName(slot))}</span></div>
        <div class="card-b stack">
        <p class="note" style="max-width:64ch">Nothing is stored here. Start a new mode, or copy a configured one and change what you need — its binding keycode is <code>${esc(bindingName(slot))}</code>, and <code>${esc(bindingName(slot))}_LOCK</code> toggles it.</p>
        ${inertNote(model, slot)}
        <div class="row" style="gap:8px">
            <button class="btn primary" data-new="${KIND.DIRECTIONAL}" ${canEdit ? "" : "disabled"}
                data-tip="Arrow-style keys or shortcuts, starting on the dominant axis with no shortcuts set.">New directional mode</button>
            <button class="btn" data-new="${KIND.SCROLLING}" ${canEdit ? "" : "disabled"}
                data-tip="Scrolling, starting from Dragscroll's tuning.">New scrolling mode</button></div>
        ${sources.length ? `<div class="pd-copy">
            <label class="field"><span>Copy from</span><select class="input" data-source ${canEdit ? "" : "disabled"}>
                ${sources.map((row) => `<option value="${row.id}">Slot ${row.id + 1} · ${esc(row.name)}</option>`).join("")}</select></label>
            <button class="btn" data-act="duplicate" ${canEdit ? "" : "disabled"}>Duplicate into this slot</button></div>`
            : ""}
        ${canEdit ? "" : `<div class="unavailable">${esc(unavailable(model) || "This firmware cannot store configurable pointing modes.")}</div>`}
    </div></div></div>`);
    node.querySelectorAll("[data-new]").forEach((button) => button.addEventListener("click", () =>
        post(edits.pdMode(slot.id, newMode(slot, Number(button.dataset.new)), model.profileIdentity))));
    node.querySelector('[data-act="duplicate"]')?.addEventListener("click", () =>
        post(edits.duplicatePdMode(slot.id, Number(node.querySelector("[data-source]").value), model.profileIdentity)));
    return node;
}

function editor(model, slot, canEdit, slots) {
    // The kind being edited, which is the stored one until the Movement select
    // changes it. The form has to be rebuilt on that change: each section
    // registers the readers for its own fields, so a directional record cannot
    // be read out of a scrolling form.
    const kind = state.pdKind?.slot === slot.id ? state.pdKind.kind : slot.kind;
    if (kind !== slot.kind) slot = startingRecord(slot, kind);
    const scrolling = kind === KIND.SCROLLING;
    const light = slotLight(model, slot);
    const wrap = el(`<div class="pd-editor stack"></div>`);
    const form = {};   // live values, read back whenever a complete field changes
    const disabled = canEdit ? "" : "disabled";
    let stageCurrent = () => {};

    const field = (label, value, key, options = {}) => {
        const node = el(`<label class="field"><span>${esc(label)}</span>
            <span class="affix ${options.unit ? "unit" : ""}"><input class="input mono" value="${esc(value ?? "")}" inputmode="numeric" ${disabled}
            ${options.tip ? `data-tip="${esc(options.tip)}"` : ""}>${options.unit ? `<i>${esc(options.unit)}</i>` : ""}</span></label>`);
        form[key] = () => node.querySelector("input").value;
        return node;
    };
    const select = (label, choices, current, key, options = {}) => {
        const node = el(`<label class="field ${options.inline ? "inline" : ""}"><span>${esc(label)}</span>
            <select class="input" ${disabled} ${options.tip ? `data-tip="${esc(options.tip)}"` : ""}>
            ${choices.map(([value, text]) => `<option value="${value}" ${String(value) === String(current) ? "selected" : ""}>${esc(text)}</option>`).join("")}</select></label>`);
        form[key] = () => Number(node.querySelector("select").value);
        if (options.onChange) node.querySelector("select").addEventListener("change", options.onChange);
        return node;
    };
    const shortcut = (label, code, key, options = {}) => {
        const name = options.name ?? keyName(model, code);
        const node = el(`<label class="field ${options.klass || ""}"><span>${options.labelHtml || esc(label)}</span>
            <div class="input-row"><input class="input mono" value="${esc(name)}" placeholder="nothing" ${disabled}>
            <button class="btn" data-pick ${disabled}>Pick…</button></div></label>`);
        const input = node.querySelector("input");
        form[key] = () => input.value.trim();
        node.querySelector("[data-pick]").addEventListener("click", (event) => {
            event.preventDefault();   // inside a <label>, a click would also focus the input
            openPicker({
                title: label, context: `${slot.name} · slot ${slot.id + 1}`,
                seed: name ? [name] : [],
                onPick: (expression) => {
                    input.value = expression;
                    state.picker = null;
                    stageCurrent();
                    render();
                },
            });
        });
        return node;
    };
    // Eight modifiers as a Left / Right by Ctrl / Shift / Alt / GUI grid, so
    // each switch sits under its name instead of wrapping wherever it lands.
    const modifiers = (label, mask, key) => {
        const columns = [...new Set(MODIFIER_BITS.map(([, name]) => name.replace(/^(Left|Right) /, "")))];
        const cell = ([bit, name]) => `<label class="sw" data-tip="${esc(name)}"><input type="checkbox" data-bit="${bit}" aria-label="${esc(name)}"
            ${mask & bit ? "checked" : ""} ${disabled}><span class="track"></span></label>`;
        const side = (prefix) => MODIFIER_BITS.filter(([, name]) => name.startsWith(prefix));
        const node = el(`<div class="field"><span>${esc(label)}</span>
            <div class="pd-mods" style="grid-template-columns:44px repeat(${columns.length}, 58px)">
                <span></span>${columns.map((name) => `<span class="h">${esc(name)}</span>`).join("")}
                <span class="s">Left</span>${side("Left").map(cell).join("")}
                <span class="s">Right</span>${side("Right").map(cell).join("")}
            </div></div>`);
        form[key] = () => [...node.querySelectorAll("[data-bit]")].reduce((total, input) =>
            total | (input.checked ? Number(input.dataset.bit) : 0), 0);
        return node;
    };

    // ── identity ──────────────────────────────────────────────────────────
    const empties = slots.some((candidate) => !candidate.kind);
    const head = el(`<div class="card">
        <div class="card-h">
            ${light.swatch("lg")}
            <h3>${esc(slot.name || `Slot ${slot.id + 1}`)}</h3><span class="tag">slot ${slot.id + 1} · ${esc(bindingName(slot))}</span>
            <span class="right">
                ${empties ? `<button class="btn tiny" data-act="duplicate" ${disabled}
                    data-tip="Copy this mode into the first empty slot.">Duplicate</button>` : ""}
                <button class="btn tiny" data-act="clear" ${disabled}
                    data-tip="Empty this slot. Keys bound to it stay on the board and do nothing until it is configured again.">Clear slot</button>
            </span></div>
        <div class="card-b pd-identity"></div></div>`);
    const headBody = head.querySelector(".card-b");
    const name = el(`<label class="field"><span>Name</span>
        <input class="input" value="${esc(slot.name)}" maxlength="23" ${disabled}></label>`);
    form.name = () => name.querySelector("input").value.trim();
    headBody.append(name);
    headBody.append(select("Movement", [[KIND.DIRECTIONAL, "Directional keys / shortcuts"], [KIND.SCROLLING, "Scrolling"]], kind, "kind",
        // Switching movement only redraws the form for the other kind: its
        // fields start empty, so staging now would post an invalid record.
        // The slot is staged once a field of the new form is changed.
        {onChange: (event) => { event.stopPropagation(); state.pdKind = {slot: slot.id, kind: Number(event.target.value)}; render(); }}));
    headBody.append(select("Pointer speed while active", dpiOptions(model.pdModeEditing?.dpiChoices, slot.dpi), slot.dpi ?? 0, "dpi",
        {tip: "Sensor DPI while this mode runs. Normal pointer speed keeps the DPI set in Settings."}));
    wrap.append(head);

    // ── movement ──────────────────────────────────────────────────────────
    if (!scrolling) {
        const reads = axisReads(slot.axis);
        const card = el(`<div class="card">
            <div class="card-h"><h3>What each direction sends</h3><span class="right" data-axis></span></div>
            <div class="card-b stack"><div class="pd-cross ${slot.axis === AXIS.EIGHT ? "eight" : reads.length === 4 ? "" : reads.includes("up") ? "vertical" : "horizontal"}"></div></div></div>`);
        card.querySelector("[data-axis]").append(select("Reads", AXES, slot.axis, "axis", {inline: true,
            tip: "Which movement this mode turns into keys. Switching to one axis empties the other axis's shortcuts."}));
        const cross = card.querySelector(".pd-cross");
        for (const [direction, label] of DIRECTIONS) {
            if (reads.includes(direction)) {
                cross.append(shortcut(label, slot.directions?.[direction]?.keycode, `dir:${direction}`, {
                    klass: `arm ${direction}`, labelHtml: `<b>${ARROWS[direction]}</b> ${esc(label)}`,
                }));
            }
            form[`dirPolicy:${direction}`] = () => slot.directions?.[direction]?.modifierPolicy ?? 0;
            form[`dirMask:${direction}`] = () => slot.directions?.[direction]?.mask ?? 0;
        }
        if (slot.axis === AXIS.EIGHT) {
            for (const [diagonal, label] of DIAGONALS) {
                cross.append(shortcut(label, slot.diagonals?.[diagonal]?.keycode, `diag:${diagonal}`, {
                    klass: `arm ${diagonal}`, labelHtml: `<b>${ARROWS[diagonal]}</b> ${esc(label)}`,
                }));
            }
        }
        // Every directional mode: what moving toward a direction with no
        // shortcut does.
        card.querySelector(".card-b").append(select("When a direction is empty", EMPTY_DIRECTION, slot.emptyDirection ?? 0, "emptyDirection",
            {tip: "Moving toward a direction with no shortcut: its neighbours take over its share; both neighbours are sent (a diagonal's two straight directions, a straight direction's two diagonals, so in eight directions only); or the move does nothing."}));
        cross.append(el(`<div class="hub" aria-hidden="true"><svg viewBox="0 0 64 64">
            <circle cx="32" cy="32" r="22"/><circle cx="32" cy="32" r="3"/>
            ${reads.includes("up") ? `<path d="M32 4l-4 5h8z"/>` : ""}${reads.includes("down") ? `<path d="M32 60l-4-5h8z"/>` : ""}
            ${reads.includes("left") ? `<path d="M4 32l5-4v8z"/>` : ""}${reads.includes("right") ? `<path d="M60 32l-5-4v8z"/>` : ""}
            ${slot.axis === AXIS.EIGHT ? `<path d="M12 12l7 1-6 6z"/><path d="M52 12l-7 1 6 6z"/><path d="M12 52l7-1-6-6z"/><path d="M52 52l-7-1 6-6z"/>` : ""}
            </svg><span class="tag">trackball</span></div>`));
        wrap.append(card);
    } else {
        const card = el(`<div class="card">
            <div class="card-h"><h3>Scrolling</h3></div>
            <div class="card-b stack"><div class="pd-grid4"></div><div class="pd-split"></div></div></div>`);
        const steps = card.querySelector(".pd-grid4");
        for (const [key, label, unit] of SCROLL_LEAD) steps.append(field(label, slot.scroll?.[key], `scroll:${key}`, {unit}));
        const lower = card.querySelector(".pd-split");
        lower.append(select("Reverse scrolling", INVERT, slot.scroll?.invert, "invert"));
        lower.append(modifiers("Hold modifiers while scrolling", slot.heldModifiers, "heldModifiers"));
        wrap.append(card);
    }

    // ── how it is reached ─────────────────────────────────────────────────
    const {row} = light;
    const lightText = row
        ? `HSV(${[row.color.h, row.color.s, row.color.v].join(", ")}) · ${esc(String(row.locality).replace(/^RGB_/, "").toLowerCase().replace(/_/g, " "))}${stageEnabled(model, "pd") ? "" : " · stage off"}`
        : "no colour reported";
    const reach = el(`<div class="card">
        <div class="card-h"><h3>How it is reached</h3></div>
        <div class="card-b pd-lines">
            <span class="lbl">Keys</span>
            <div class="val">
                <span class="act"><span class="k">${esc(bindingName(slot))}</span><span class="how">hold</span></span>
                <span class="act"><span class="k">${esc(bindingName(slot))}_LOCK</span><span class="how">toggle</span></span>
                <span class="note">${esc(placedOn(model, slot))}</span></div>
            <button class="btn tiny" data-act="place" ${disabled}>Place on a layer…</button>
            <span class="lbl">Light</span>
            <div class="val">${light.swatch()}<span class="note mono">${lightText}</span></div>
            <button class="btn tiny" data-act="lighting">Edit in Lighting</button>
            <p class="note foot">Clearing the slot leaves its keys where they are. The keyboard keeps its mode keycodes whatever a slot holds, and refuses to activate an empty one — so they do nothing until this slot is configured again.</p>
        </div></div>`);
    reach.querySelector('[data-act="lighting"]').addEventListener("click", () => {
        state.screen = "lighting"; state.stage = "pd"; render();
    });
    reach.querySelector('[data-act="place"]').addEventListener("click", () => {
        state.placement = {keycode: bindingName(slot), label: slot.name || `Pointing slot ${slot.id + 1}`};
        state.screen = "keys";
        state.tab = "key";
        render();
    });
    wrap.append(reach);

    // ── advanced ──────────────────────────────────────────────────────────
    const advanced = el(`<details class="card pd-advanced" ${state.pdAdvanced ? "open" : ""}><summary class="card-h">
        <svg class="chev" viewBox="0 0 10 10" aria-hidden="true"><path d="M3.5 2l3 3-3 3"/></svg><h3>Advanced</h3><span class="right note">thresholds, ratios, mouse buttons</span></summary>
        <div class="card-b stack"></div></details>`);
    const advancedBody = advanced.querySelector(".card-b");
    const section = (title) => {
        const node = el(`<section class="pd-sect"><div class="sect-h"><h4>${esc(title)}</h4></div></section>`);
        advancedBody.append(node);
        return node;
    };

    const behaviour = el(`<div class="pd-grid3"></div>`);
    behaviour.append(select("After this mode ends", POINTER_LAYER, slot.pointerLayer, "pointerLayer"));
    if (!scrolling) {
        // An axis the mode does not read has no threshold to set.
        if (readsHorizontal(slot.axis)) behaviour.append(field("Horizontal movement per tap", slot.thresholdX, "thresholdX"));
        if (readsVertical(slot.axis)) behaviour.append(field("Vertical movement per tap", slot.thresholdY, "thresholdY"));
    }
    section("Behaviour").append(behaviour);

    if (scrolling) {
        const tuning = el(`<div class="pd-grid3"></div>`);
        const drawn = new Set(SCROLL_LEAD.map(([key]) => key));
        for (const [key, label, unit] of SCROLL_SINGLES) {
            tuning.append(field(label, slot.scroll?.[key], `scroll:${key}`, {unit}));
            drawn.add(key);
        }
        for (const [label, top, bottom] of SCROLL_RATIOS) {
            const pair = el(`<div class="field"><span>${esc(label)}</span><div class="pd-ratio"></div></div>`);
            const inputs = pair.querySelector(".pd-ratio");
            for (const [index, key] of [top, bottom].entries()) {
                const input = el(`<input class="input mono" value="${esc(slot.scroll?.[key] ?? "")}" inputmode="numeric"
                    aria-label="${esc(`${label} ${index ? "denominator" : "numerator"}`)}" ${disabled}>`);
                form[`scroll:${key}`] = () => input.value;
                if (index) inputs.append(el(`<i>:</i>`));
                inputs.append(input);
                drawn.add(key);
            }
            tuning.append(pair);
        }
        // Anything the record gains later is still drawn, once.
        for (const [key, label] of SCROLL_FIELDS) {
            if (!drawn.has(key)) tuning.append(field(label, slot.scroll?.[key], `scroll:${key}`));
        }
        section("Scroll tuning").append(tuning);
    }

    const buttons = section("Mouse button overrides");
    if ((slot.buttons || []).length) {
        // An override still being set up is drawn as chosen, not as stored.
        const unfinished = state.pdButtons?.slot === slot.id ? state.pdButtons.rows : {};
        const table = el(`<div class="pd-buttons"><span class="h">Button</span><span class="h">While this mode runs</span>
            <span class="h">Shortcut</span><span class="h">Held modifiers</span></div>`);
        (slot.buttons || []).forEach((stored, index) => {
            const button = unfinished[index] || stored;
            const kindSelect = select(`Button ${index + 1}`, BUTTON_KINDS, button.kind, `button:${index}:kind`);
            kindSelect.querySelector("span").classList.add("sr");
            const tapping = button.kind === BUTTON.TAP, holding = button.kind === BUTTON.HOLD_MODIFIERS;
            const tap = tapping
                ? shortcut(`Button ${index + 1} shortcut`, button.tap?.keycode, `button:${index}:tap`,
                    typeof button.tap?.keycode === "string" ? {name: button.tap.keycode === "0" ? "" : button.tap.keycode} : {})
                : el(`<span class="note blank">—</span>`);
            tap.querySelector?.("span")?.classList.add("sr");
            const held = holding ? modifierNames(button.modifiers).join(", ") || "choose at least one below" : "—";
            table.append(el(`<span class="n">${index + 1}</span>`), kindSelect, tap,
                el(`<span class="note"><span class="narrow">Held modifiers: </span>${esc(held)}</span>`));
            if (holding) {
                const mods = modifiers(`Button ${index + 1} holds`, button.modifiers ?? 0, `button:${index}:modifiers`);
                mods.classList.add("mods");
                table.append(mods);
            }
        });
        buttons.append(table);
    } else {
        buttons.append(el(`<p class="note">This slot stores no button overrides.</p>`));
    }
    wrap.append(advanced);

    // Open stays open from slot to slot, so two modes' tuning can be compared.
    advanced.addEventListener("toggle", () => {
        state.pdAdvanced = advanced.open;
    });

    stageCurrent = () => {
        const {config, pending} = settleButtons(slot, readConfig(slot, form));
        const holding = Object.keys(pending).length > 0, held = Boolean(state.pdButtons);
        state.pdButtons = holding ? {slot: slot.id, rows: pending} : null;
        post(edits.pdMode(slot.id, config, model.profileIdentity));
        // A new kind draws its own field, even when nothing new was stored.
        if (holding || held) render();
    };
    wrap.addEventListener("change", (event) => {
        if (!event.target.matches("input, select")) return;
        stageCurrent();
    });

    head.querySelector('[data-act="clear"]').addEventListener("click", () => {
        state.pdKind = null;
        state.pdButtons = null;
        post(edits.clearPdMode(slot.id, model.profileIdentity));
    });
    head.querySelector('[data-act="duplicate"]')?.addEventListener("click", () => {
        const target = slots.find((candidate) => !candidate.kind);
        if (!target) return;
        state.pdSlot = target.id;
        post(edits.duplicatePdMode(target.id, slot.id, model.profileIdentity));
    });
    return wrap;
}

// Counted through the slot's values rather than its names: a layout position
// carries whatever the keyboard calls the keycode, which is a bare user keycode
// or plain hex, not the name this app prints.
function placedOn(model, slot) {
    const counts = new Map();
    for (const {layer} of bindingsForSlot(model, slot).keys) {
        const name = layer.displayName || layer.name;
        counts.set(name, (counts.get(name) || 0) + 1);
    }
    const places = [...counts].map(([name, count]) => `${name} · ${count} key${count === 1 ? "" : "s"}`);
    return places.length ? `on ${places.join(", ")}` : "not placed on any layer";
}
