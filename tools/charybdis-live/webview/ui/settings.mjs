// Settings: the keyboard's global policy, exactly as it reports it.
//
// The sections, fields, hints and limits all come from the device snapshot, so
// this screen renders what the firmware says it has rather than a list this
// app decided on. A field the firmware cannot report stays visible and
// read-only with its reason. Each section names the screen it is edited on:
// this one draws the Settings sections, and Mouse draws its own with the same
// cards.

import * as edits from "../view/edits.mjs";
import {el, esc} from "../lib/dom.mjs";
import {getModel, layers, post, render, state, writable, canEdit as canEditArea} from "../store.mjs";
import {topbar, unavailable} from "./shell.mjs";
import {draftDot, draftMarks} from "../view/review.mjs";
import {marked} from "./marks.mjs";

// The sections a screen draws, by the area the keyboard's model files them in.
export const sectionsIn = (model, area) => (model?.configDefaults || []).filter((section) => section.area === area);

export function screenSettings() {
    const model = getModel();
    const sections = sectionsIn(model, "Settings");
    const canEdit = canEditArea("settings");
    const main = el(`<div class="main">${topbar(
        "Settings",
        "The keyboard's global policy: every value it stores that applies on all layers. Values this firmware cannot report stay read-only rather than disappearing.",
        `<input class="input" id="settingsSearch" placeholder="Search settings" style="width:200px" value="${esc(state.settingsSearch || "")}">`,
    )}</div>`);

    const content = el(`<div class="content"><div class="pad" style="max-width:980px"></div></div>`);
    const pad = content.firstElementChild;
    if (!sections.length) {
        pad.appendChild(el(`<div class="screen-stub"><h3>No settings read</h3>
            <p class="note">${esc(unavailable(model) || "Choose Read keyboard to load this keyboard's policy.")}</p></div>`));
    }
    if (!canEdit && sections.length) {
        pad.appendChild(el(`<div class="unavailable" style="margin-bottom:14px">${esc(unavailable(model)
            || "This firmware reports its settings but cannot save them. Flash the complete-profile pair to edit them here.")}</div>`));
    }

    const query = (state.settingsSearch || "").trim().toLowerCase();
    const matching = (section) => query ? section.fields.filter((field) => matches(field, query)) : section.fields;
    for (const section of sections) {
        const fields = matching(section);
        const timing = section.id === "comboSettings" ? COMBO_TIMING.filter((entry) => !query || matches(entry, query)) : [];
        if (!fields.length && !timing.length) continue;
        const card = sectionCard(model, section, fields, canEdit, Boolean(query));
        if (section.id === "comboSettings") {
            for (const entry of timing) card.querySelector(".rows").appendChild(comboTimingRow(model, entry));
            card.querySelector(".card-h .tag").textContent = `${section.fields.length + COMBO_TIMING.length} settings`;
        }
        pad.appendChild(card);
    }
    // A search for something that moved to Mouse says where it went.
    const elsewhere = query ? sectionsIn(model, "Mouse").reduce((total, section) => total + matching(section).length, 0) : 0;
    if (elsewhere) {
        const note = el(`<div class="callout row" style="gap:10px;align-items:center">
            <span>${elsewhere} matching setting${elsewhere === 1 ? " is" : "s are"} on the Mouse screen.</span>
            <button class="btn tiny" data-act="mouse">Open Mouse</button></div>`);
        note.querySelector('[data-act="mouse"]').addEventListener("click", () => { state.screen = "mouse"; render(); });
        pad.appendChild(note);
    }

    main.appendChild(content);
    const search = main.querySelector("#settingsSearch");
    search.addEventListener("input", () => {
        state.settingsSearch = search.value;
        render();
        const again = document.querySelector("#settingsSearch");
        if (again) { again.focus(); again.setSelectionRange(again.value.length, again.value.length); }
    });
    return main;
}

// The default combo window and the combo hold threshold apply to every combo,
// so they are edited with the other combo settings. The keyboard stores them
// with the combos rather than in the settings domain, so each saves as a combo
// edit. A keyboard that stores them once has them even without combos; an
// older one has no default, and repeats the threshold on every combo, so there
// is nothing to store it on until a combo exists.
// A combo follows the default window unless it has its own, so changing it
// re-times those combos. QMK waits for the hold threshold only on combos that
// must be held or are tap only — the longer of it and the combo's own window
// — and every other combo ignores it.
const COMBO_DEFAULT = {label: "Default combo window", hint: "How close together a combo's keys must be pressed, for every combo without its own window. New combos start on it, and the combos that follow it change with it."};
const COMBO_HOLD = {label: "Combo hold threshold", hint: "How long a combo that must be held, or is tap only, waits to tell a hold from a tap: this, or its combo window if that is longer. Other combos ignore it. 0 adds no wait beyond the window."};
const COMBO_TIMING = [COMBO_DEFAULT, COMBO_HOLD];
function comboTimingRow(model, entry) {
    const readback = model?.comboReadback || {};
    const stores = readback.version === 2;
    const hold = entry === COMBO_HOLD;
    const value = hold ? readback.holdTermMs : readback.defaultTermMs;
    const stored = String(value ?? "");
    const combos = model?.combos || [];
    const editable = canEditArea("combos") && (stores || (hold && combos.length > 0));
    const hint = stores ? entry.hint : hold ? (combos.length ? entry.hint : "Stored with the combos, so it can be set once the keyboard has one. Flash the updated firmware pair to set it without one.")
        : "This keyboard keeps each combo's own window. Flash the updated firmware pair to set a default.";
    const row = el(`<div class="setrow">
        <div><div class="nm">${esc(entry.label)}</div>
            <div class="hint">${esc(hint)}</div></div>
        <div class="control"><div class="input-row"><input class="input mono" id="${hold ? "comboHoldTerm" : "comboDefaultTerm"}" value="${esc(stored)}" ${editable ? "" : "disabled"}></div></div>
    </div>`);
    // Posted on its own rather than riding along with whichever combo is saved
    // next. The keyboard always stores a number, so an emptied field is put
    // back rather than read as some default.
    row.querySelector("input").addEventListener("change", (event) => {
        const written = event.target.value.trim();
        if (!written) { event.target.value = stored; return; }
        if (written === stored) return;
        post(hold ? edits.comboHoldTerm(written, model.profileIdentity) : edits.comboDefaultTerm(written, model.profileIdentity));
    });
    return row;
}

const matches = (field, query) => `${field.label} ${field.hint || ""}`.toLowerCase().includes(query);

export function sectionCard(model, section, fields, canEdit, searching = false) {
    // A folded section stays open once opened, by hand or by the review's
    // Show, instead of folding again on the next render.
    const opened = (state.settingsOpen || []).includes(section.id);
    const open = searching || section.expanded !== false || opened;
    const node = el(`<details class="card settings-group" data-section="${esc(section.id)}" ${open ? "open" : ""}>
        <summary class="card-h" style="cursor:pointer;list-style:none"><h3>${esc(section.label)}${draftMarks(model?.draft?.changes).settings.has(section.id) ? draftDot() : ""}</h3>
            <span class="right tag">${section.fields.length} setting${section.fields.length === 1 ? "" : "s"}</span></summary>
        ${section.description ? `<div class="card-b" style="padding-bottom:0"><p class="note">${esc(section.description)}</p></div>` : ""}
        <div class="rows"></div>
    </details>`);
    const rows = node.querySelector(".rows");
    if (section.expanded === false) node.addEventListener("toggle", () => {
        const others = (state.settingsOpen || []).filter((id) => id !== section.id);
        state.settingsOpen = node.open ? [...others, section.id] : others;
    });

    // A section is saved whole: the core validates the complete set, so every
    // field travels together and one changed value cannot half-write a section.
    const submit = () => post(edits.settingsSection(section, (field) => {
        const input = node.querySelector(`[data-macro="${cssEscape(field.macro)}"]`);
        if (!input) return undefined;
        return field.kind === "toggle" ? input.checked : input.value;
    }, model?.settingsEditing?.identity));

    for (const field of section.fields) {
        const hidden = !fields.includes(field);
        rows.appendChild(fieldRow(field, canEdit, submit, hidden, section.fields.some((entry) => entry.governs)));
    }
    return node;
}

function fieldRow(field, canEdit, submit, hidden, slot) {
    const editable = canEdit && !field.readOnly;
    const row = el(`<div class="setrow ${field.readOnly ? "ro" : ""}" ${hidden ? 'style="display:none"' : ""}>
        <div><div class="nm">${marked(getModel(), field.governs, field.label, {slot})}</div>${field.hint ? `<div class="hint">${esc(field.hint)}</div>` : ""}</div>
        <div class="control"></div>
    </div>`);
    const control = row.querySelector(".control");

    if (field.readOnly) {
        control.appendChild(el(`<span class="chip"
            data-tip="The connected firmware does not report this option, so it is shown as the keyboard has it and cannot be edited here.">read-only · ${esc(field.kind === "toggle" ? (field.enabled ? "on" : "off") : field.value)}</span>`));
        return row;
    }
    if (field.kind === "toggle") {
        const node = el(`<label class="sw"><input type="checkbox" data-macro="${esc(field.macro)}" ${field.enabled ? "checked" : ""} ${editable ? "" : "disabled"}>
            <span class="track"></span><span class="txt muted">${field.enabled ? "On" : "Off"}</span></label>`);
        node.querySelector("input").addEventListener("change", submit);
        control.appendChild(node);
        return row;
    }
    if (field.kind === "layer") {
        const node = el(`<select class="input" data-macro="${esc(field.macro)}" ${editable ? "" : "disabled"}>
            ${layers().map((layer) => `<option value="Layer ${layer.index}" ${field.value === `Layer ${layer.index}` ? "selected" : ""}>${esc(layer.displayName || layer.name)}</option>`).join("")}</select>`);
        node.addEventListener("change", submit);
        control.appendChild(node);
        return row;
    }
    if (field.choices?.length) {
        const choices = field.choices.map((choice) => typeof choice === "object" ? choice : {value: choice, label: String(choice)});
        const known = choices.some((choice) => String(choice.value) === String(field.value));
        const node = el(`<select class="input" data-macro="${esc(field.macro)}" ${editable ? "" : "disabled"}>
            ${known ? "" : `<option value="${esc(field.value)}" selected>${esc(field.value)} · as stored</option>`}
            ${choices.map((choice) => `<option value="${esc(choice.value)}" ${String(choice.value) === String(field.value) ? "selected" : ""}>${esc(choice.label)}</option>`).join("")}</select>`);
        node.addEventListener("change", submit);
        control.appendChild(node);
        return row;
    }
    const node = el(`<div class="input-row"><input class="input mono" data-macro="${esc(field.macro)}" value="${esc(field.value)}" ${editable ? "" : "disabled"}>
        ${/ms\b/.test(field.hint || "") || /\(ms\)/.test(field.label) ? `<span class="chip">ms</span>` : ""}</div>`);
    node.querySelector("input").addEventListener("change", submit);
    control.appendChild(node);
    return row;
}

const cssEscape = (value) => String(value).replace(/["\\]/g, "\\$&");
