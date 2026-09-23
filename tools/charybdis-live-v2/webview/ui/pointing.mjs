// Pointing modes: eight device-owned slots. Each one re-reads the trackball as
// scrolling or as directional keys while its key is held or toggled.
//
// The surface leads with what the mode is — name, movement, speed, actions —
// and keeps thresholds, ratios and button overrides under Advanced, where they
// belong for the once-a-year visit.

import {css, isOff} from "../lib/colour.mjs";
import {el, esc} from "../lib/dom.mjs";
import {DIRECTIONS, KIND, SCROLL_FIELDS, readConfig, startingRecord} from "../view/pointing-config.mjs";
import {MODIFIER_BITS, keyName, modifierNames} from "../view/keyvalues.mjs";
import {bindingsForSlot} from "../view/keyface.mjs";
import {pdColourRow, stageEnabled} from "../view/lighting.mjs";
import {getModel, post, render, state, writable} from "../store.mjs";
import * as edits from "../view/edits.mjs";
import {openPicker} from "./picker.mjs";
import {topbar, unavailable} from "./shell.mjs";


const AXES = [[2, "Dominant axis"], [0, "Vertical only"], [1, "Horizontal only"]];
const INVERT = [[0, "Neither axis"], [1, "Horizontal"], [2, "Vertical"], [3, "Both axes"]];
const POINTER_LAYER = [[0, "Keep the pointer layer active"], [1, "Return to the typing layer"]];
const BUTTON_KINDS = [[0, "Pass through"], [1, "Consume"], [2, "Tap a shortcut"], [3, "Hold modifiers"]];
const MODIFIER_POLICY = [[0, "Inherit held modifiers"], [1, "Ignore the selected modifiers"], [2, "Use only this shortcut"]];

// The four scroll fields the Scrolling card shows itself. A field rendered
// twice registers its reader twice, and the second input silently wins, so
// Advanced shows only what the card above does not.
const SCROLL_LEAD = ["divisorH", "divisorV", "intervalMs", "lockMs"];
const BINDINGS = ["DRAGSCROLL", "VOLUME_MODE", "BRIGHTNESS_MODE", "ZOOM_MODE", "ARROW_MODE", "PINCH_MODE"];
const bindingName = (slot) => slot.id < BINDINGS.length ? BINDINGS[slot.id] : `PD_SLOT_${slot.id}`;

export function screenPointing() {
    const model = getModel();
    const slots = model?.pdModes || [];
    const canEdit = writable() && Boolean(model?.pdModeEditing?.writable);

    const main = el(`<div class="main">${topbar(
        "Pointing modes",
        "Eight device-owned slots. Each re-reads the trackball while its key is held or toggled — as scrolling, or as directional keys and shortcuts.",
        "",
    )}</div>`);
    const content = el(`<div class="content"><div class="pad" style="display:grid;grid-template-columns:300px minmax(0,1fr);gap:20px;align-items:start"></div></div>`);
    const pad = content.firstElementChild;

    if (!slots.length) {
        pad.style.display = "block";
        pad.appendChild(el(`<div class="screen-stub"><h3>No pointing modes read</h3>
            <p class="note">${esc(unavailable(model) || "This firmware has fixed pointing modes. The configurable-slot firmware and a migrated profile are needed to edit them.")}</p></div>`));
        main.appendChild(content);
        return main;
    }

    const list = el(`<div class="stack" style="gap:6px"></div>`);
    for (const slot of slots) {
        const row = pdColourRow(model, slot.id);
        const lit = row && !isOff(row.color) && stageEnabled(model, "pd");
        const card = el(`<button class="slotcard" data-slot="${slot.id}" aria-current="${state.pdSlot === slot.id}">
            <div class="top">
                <span class="swatch-lg ${lit ? "" : "swatch-off"}" style="width:16px;height:16px;border-radius:5px;${lit ? `background:${css(row.color)}` : ""}"></span>
                <span class="nm">${slot.kind ? esc(slot.name) : "Empty slot"}</span><span class="no">${slot.id + 1}</span>
            </div>
            <div class="note">${slot.kind === KIND.SCROLLING ? "Scrolling" : slot.kind === KIND.DIRECTIONAL ? `Directional · ${axisLabel(slot.axis)}` : "Available"}</div>
            <div class="note mono dim">${esc(bindingName(slot))}</div>
            ${!slot.kind && bindingsForSlot(model, slot).keys.length
                ? `<div class="note">${bindingsForSlot(model, slot).keys.length} key${bindingsForSlot(model, slot).keys.length === 1 ? " still reaches" : "s still reach"} it · inert</div>` : ""}</button>`);
        card.addEventListener("click", () => { state.pdSlot = slot.id; state.pdKind = null; render(); });
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
    if (keys.length) parts.push(`${keys.length} key${keys.length === 1 ? "" : "s"} on ${layers.join(", ")}`);
    if (behaviours.length) parts.push(`${behaviours.length} behaviour${behaviours.length === 1 ? "" : "s"} (${behaviours.map((row) => row.keycode).join(", ")})`);
    return `<div class="unavailable">${parts.join(" and ")} still reach this slot. The keyboard refuses to activate an empty slot, so they do nothing until it is configured — they do not need to be removed first.</div>`;
}

const axisLabel = (axis) => (AXES.find(([value]) => value === axis) || [, "Dominant axis"])[1];

function emptySlot(model, slot, canEdit, slots) {
    const sources = slots.filter((row) => row.kind);
    const node = el(`<div class="card"><div class="card-b" style="display:grid;gap:14px;justify-items:start">
        <h3>Slot ${slot.id + 1} is empty</h3>
        <p class="note" style="max-width:60ch">Nothing is stored here. Copy a configured mode and change what you need — its binding keycode is <code>${esc(bindingName(slot))}</code>, and <code>${esc(bindingName(slot))}_LOCK</code> toggles it.</p>
        ${inertNote(model, slot)}
        ${sources.length ? `<div class="row" style="gap:8px;align-items:end">
            <label class="field" style="width:220px"><span>Copy from</span><select class="input" data-source>
                ${sources.map((row) => `<option value="${row.id}">Slot ${row.id + 1} · ${esc(row.name)}</option>`).join("")}</select></label>
            <button class="btn primary" data-act="duplicate" ${canEdit ? "" : "disabled"}>Duplicate into this slot</button></div>`
            : `<p class="note">No configured slot to copy from yet.</p>`}
        ${canEdit ? "" : `<div class="unavailable">${esc(unavailable(model) || "This firmware cannot store configurable pointing modes.")}</div>`}
    </div></div>`);
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
    const row = pdColourRow(model, slot.id);
    const wrap = el(`<div class="stack"></div>`);
    const form = {};   // live values, read back whenever a complete field changes
    let stageCurrent = () => {};

    const field = (label, value, key, options = {}) => {
        const node = el(`<label class="field"><span>${esc(label)}</span>
            <input class="input mono" value="${esc(value ?? "")}" ${canEdit ? "" : "disabled"}
            ${options.tip ? `data-tip="${esc(options.tip)}"` : ""}></label>`);
        form[key] = () => node.querySelector("input").value;
        return node;
    };
    const select = (label, choices, current, key, options = {}) => {
        const node = el(`<label class="field"><span>${esc(label)}</span>
            <select class="input" ${canEdit ? "" : "disabled"} ${options.tip ? `data-tip="${esc(options.tip)}"` : ""}>
            ${choices.map(([value, text]) => `<option value="${value}" ${String(value) === String(current) ? "selected" : ""}>${esc(text)}</option>`).join("")}</select></label>`);
        form[key] = () => Number(node.querySelector("select").value);
        if (options.onChange) node.querySelector("select").addEventListener("change", options.onChange);
        return node;
    };
    const shortcut = (label, code, key) => {
        const name = keyName(model, code);
        const node = el(`<label class="field"><span>${esc(label)}</span>
            <div class="input-row"><input class="input mono" value="${esc(name)}" placeholder="nothing" ${canEdit ? "" : "disabled"}>
            <button class="btn" data-pick ${canEdit ? "" : "disabled"}>Pick…</button></div></label>`);
        const input = node.querySelector("input");
        form[key] = () => input.value.trim();
        node.querySelector("[data-pick]").addEventListener("click", () => openPicker({
            title: label, context: `${slot.name} · slot ${slot.id + 1}`,
            seed: name ? [name] : [],
            onPick: (expression) => {
                input.value = expression;
                state.picker = null;
                stageCurrent();
                render();
            },
        }));
        return node;
    };
    const modifiers = (label, mask, key) => {
        const node = el(`<div class="field"><span>${esc(label)}</span>
            <div class="row" style="gap:10px;flex-wrap:wrap">${MODIFIER_BITS.map(([bit, name]) =>
                `<label class="sw"><input type="checkbox" data-bit="${bit}" ${mask & bit ? "checked" : ""} ${canEdit ? "" : "disabled"}>
                <span class="track"></span><span class="txt">${esc(name)}</span></label>`).join("")}</div></div>`);
        form[key] = () => [...node.querySelectorAll("[data-bit]")].reduce((total, input) =>
            total | (input.checked ? Number(input.dataset.bit) : 0), 0);
        return node;
    };

    // ── identity ──────────────────────────────────────────────────────────
    const head = el(`<div class="card">
        <div class="card-h">
            <span class="swatch-lg ${row && !isOff(row.color) ? "" : "swatch-off"}" style="width:18px;height:18px;border-radius:5px;${row && !isOff(row.color) ? `background:${css(row.color)}` : ""}"></span>
            <h3>${esc(slot.name || `Slot ${slot.id + 1}`)}</h3><span class="tag">slot ${slot.id + 1}</span>
            <span class="right row" style="gap:8px">
                ${slots.some((candidate) => !candidate.kind) ? `<button class="btn ghost" data-act="duplicate" ${canEdit ? "" : "disabled"}
                    data-tip="Copy this mode into the first empty slot.">Duplicate mode</button>` : ""}
                <button class="btn ghost" data-act="clear" ${canEdit ? "" : "disabled"}
                    data-tip="Empty this slot. Keys bound to it stay on the board and do nothing until it is configured again.">Clear slot</button>
                <span class="note">changes stage automatically</span></span></div>
        <div class="card-b grid3"></div></div>`);
    const headBody = head.querySelector(".card-b");
    const name = el(`<label class="field"><span>Name</span>
        <input class="input" value="${esc(slot.name)}" maxlength="23" ${canEdit ? "" : "disabled"}></label>`);
    form.name = () => name.querySelector("input").value.trim();
    headBody.append(name);
    headBody.append(select("Movement", [[KIND.DIRECTIONAL, "Directional keys / shortcuts"], [KIND.SCROLLING, "Scrolling"]], kind, "kind",
        // Switching movement only redraws the form for the other kind: its
        // fields start empty, so staging now would post an invalid record.
        // The slot is staged once a field of the new form is changed.
        {onChange: (event) => { event.stopPropagation(); state.pdKind = {slot: slot.id, kind: Number(event.target.value)}; render(); }}));
    headBody.append(field("Pointer speed while active", slot.dpi, "dpi", {tip: "DPI used while this mode runs. 0 uses the normal pointer speed."}));
    wrap.append(head);

    // ── movement ──────────────────────────────────────────────────────────
    if (!scrolling) {
        const card = el(`<div class="card">
            <div class="card-h"><h3>What each direction sends</h3><span class="right" id="axis"></span></div>
            <div class="card-b" style="display:grid;grid-template-columns:260px minmax(0,1fr);gap:20px"></div></div>`);
        card.querySelector("#axis").append(select("", AXES, slot.axis, "axis"));
        const body = card.querySelector(".card-b");
        const pad = el(`<div class="dpad"></div>`);
        const cell = (direction) => {
            const value = slot.directions?.[direction];
            const name2 = keyName(model, value?.keycode);
            return `<div class="cell"><span>${esc(name2 || "—")}</span><span class="c">${esc(direction)}</span></div>`;
        };
        pad.innerHTML = `<div class="cell mid"></div>${cell("up")}<div class="cell mid"></div>
            ${cell("left")}<div class="cell mid" style="display:grid;place-items:center"><span class="tag">trackball</span></div>${cell("right")}
            <div class="cell mid"></div>${cell("down")}<div class="cell mid"></div>`;
        const fields = el(`<div class="stack" style="gap:10px"></div>`);
        for (const [direction, label] of DIRECTIONS) {
            fields.append(shortcut(label, slot.directions?.[direction]?.keycode, `dir:${direction}`));
            form[`dirPolicy:${direction}`] = () => slot.directions?.[direction]?.modifierPolicy ?? 0;
            form[`dirMask:${direction}`] = () => slot.directions?.[direction]?.mask ?? 0;
        }
        body.append(pad, fields);
        wrap.append(card);
    } else {
        const card = el(`<div class="card">
            <div class="card-h"><h3>Scrolling</h3></div>
            <div class="card-b" style="display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1fr);gap:20px"></div></div>`);
        const body = card.querySelector(".card-b");
        const left = el(`<div class="stack" style="gap:12px"></div>`);
        left.append(select("Reverse scrolling", INVERT, slot.scroll?.invert, "invert"));
        left.append(modifiers("Hold modifiers while scrolling", slot.heldModifiers, "heldModifiers"));
        const right = el(`<div class="grid2"></div>`);
        right.append(field("Movement per wheel step ↔", slot.scroll?.divisorH, "scroll:divisorH"));
        right.append(field("Movement per wheel step ↕", slot.scroll?.divisorV, "scroll:divisorV"));
        right.append(field("Minimum interval (ms)", slot.scroll?.intervalMs, "scroll:intervalMs"));
        right.append(field("Axis lock timeout (ms)", slot.scroll?.lockMs, "scroll:lockMs"));
        body.append(left, right);
        wrap.append(card);
    }

    // ── how it is reached ─────────────────────────────────────────────────
    const reach = el(`<div class="card">
        <div class="card-h"><h3>How it is reached</h3></div>
        <div class="card-b" style="display:grid;gap:10px">
            <div class="row" style="gap:8px;flex-wrap:wrap">
                <span class="act"><span class="k">${esc(bindingName(slot))}</span><span class="how">hold</span></span>
                <span class="act"><span class="k">${esc(bindingName(slot))}_LOCK</span><span class="how">toggle</span></span>
                <span class="note">${esc(placedOn(model, slot))}</span>
                <button class="btn tiny ghost" data-act="place" ${canEdit ? "" : "disabled"}>Place on a layer…</button></div>
            ${slot.kind ? `<p class="note">Clearing the slot leaves those keys where they are. The keyboard keeps its mode keycodes whatever a slot holds, and refuses to activate an empty one — so they do nothing until this slot is configured again.</p>` : ""}
            <div class="row" style="gap:10px">
                <span class="swatch-lg ${row && !isOff(row.color) ? "" : "swatch-off"}" style="${row && !isOff(row.color) ? `background:${css(row.color)}` : ""}"></span>
                <div><div style="font-size:12.5px">Lights while active</div>
                    <div class="note mono">${row ? `HSV(${[row.color.h, row.color.s, row.color.v].join(", ")}) · ${esc(String(row.locality).replace(/^RGB_/, "").toLowerCase().replace(/_/g, " "))}` : "no colour reported"}</div></div>
                <button class="btn tiny ghost" style="margin-left:auto" data-act="lighting">Edit in Lighting</button></div>
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
    const advanced = el(`<details class="card" ${state.pdAdvanced === slot.id ? "open" : ""}><summary class="card-h" style="cursor:pointer;list-style:none">
        <h3>Advanced</h3><span class="right muted" style="font-size:11.5px">thresholds, ratios, modifier rules, mouse buttons</span></summary>
        <div class="card-b stack" style="gap:16px"></div></details>`);
    const advancedBody = advanced.querySelector(".card-b");
    const behaviour = el(`<div class="grid3"></div>`);
    behaviour.append(select("After this mode ends", POINTER_LAYER, slot.pointerLayer, "pointerLayer"));
    if (!scrolling) {
        behaviour.append(field("Horizontal movement per tap", slot.thresholdX, "thresholdX"));
        behaviour.append(field("Vertical movement per tap", slot.thresholdY, "thresholdY"));
    }
    advancedBody.append(behaviour);
    if (scrolling) {
        const grid = el(`<div class="grid3"></div>`);
        for (const [key, label] of SCROLL_FIELDS) {
            if (SCROLL_LEAD.includes(key)) continue;
            grid.append(field(label, slot.scroll?.[key], `scroll:${key}`));
        }
        advancedBody.append(grid);
    }
    const buttons = el(`<div class="stack" style="gap:12px"><div class="sect-h"><h4>Mouse button overrides</h4></div></div>`);
    (slot.buttons || []).forEach((button, index) => {
        const box = el(`<div class="grid3" style="align-items:end"></div>`);
        box.append(select(`Button ${index + 1}`, BUTTON_KINDS, button.kind, `button:${index}:kind`));
        box.append(shortcut(`Button ${index + 1} shortcut`, button.tap?.keycode, `button:${index}:tap`));
        box.append(el(`<div class="field"><span>Held modifiers</span>
            <div class="note">${esc(modifierNames(button.modifiers).join(", ") || "none")}</div></div>`));
        form[`button:${index}:modifiers`] = () => button.modifiers ?? 0;
        buttons.append(box);
    });
    advancedBody.append(buttons);
    wrap.append(advanced);

    advanced.addEventListener("toggle", () => {
        state.pdAdvanced = advanced.open ? slot.id : null;
    });

    stageCurrent = () => {
        const config = readConfig(slot, form);
        post(edits.pdMode(slot.id, config, model.profileIdentity));
    };
    wrap.addEventListener("change", (event) => {
        if (!event.target.matches("input, select")) return;
        stageCurrent();
    });

    head.querySelector('[data-act="clear"]').addEventListener("click", () => {
        state.pdKind = null;
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
