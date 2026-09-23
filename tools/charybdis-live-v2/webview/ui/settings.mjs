// Settings: the keyboard's global policy, exactly as it reports it.
//
// The sections, fields, hints and limits all come from the device snapshot, so
// this screen renders what the firmware says it has rather than a list this
// app decided on. A field the firmware cannot report stays visible and
// read-only with its reason.

import * as edits from "../view/edits.mjs";
import {el, esc} from "../lib/dom.mjs";
import {getModel, layers, post, render, state, writable} from "../store.mjs";
import {topbar, unavailable} from "./shell.mjs";

export function screenSettings() {
    const model = getModel();
    const sections = model?.configDefaults || [];
    const canEdit = writable() && Boolean(model?.settingsEditing?.writable);
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
    for (const section of sections) {
        const fields = query
            ? section.fields.filter((field) => `${field.label} ${field.hint || ""}`.toLowerCase().includes(query))
            : section.fields;
        if (!fields.length) continue;
        pad.appendChild(sectionCard(model, section, fields, canEdit, Boolean(query)));
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

function sectionCard(model, section, fields, canEdit, searching) {
    const open = searching || section.expanded !== false;
    const node = el(`<details class="card settings-group" ${open ? "open" : ""}>
        <summary class="card-h" style="cursor:pointer;list-style:none"><h3>${esc(section.label)}</h3>
            <span class="right tag">${section.fields.length} setting${section.fields.length === 1 ? "" : "s"}</span></summary>
        ${section.description ? `<div class="card-b" style="padding-bottom:0"><p class="note">${esc(section.description)}</p></div>` : ""}
        <div class="rows"></div>
    </details>`);
    const rows = node.querySelector(".rows");

    // A section is saved whole: the core validates the complete set, so every
    // field travels together and one changed value cannot half-write a section.
    const submit = () => post(edits.settingsSection(section, (field) => {
        const input = node.querySelector(`[data-macro="${cssEscape(field.macro)}"]`);
        if (!input) return undefined;
        return field.kind === "toggle" ? input.checked : input.value;
    }, model?.settingsEditing?.identity));

    for (const field of section.fields) {
        const hidden = !fields.includes(field);
        rows.appendChild(fieldRow(field, canEdit, submit, hidden));
    }
    return node;
}

function fieldRow(field, canEdit, submit, hidden) {
    const editable = canEdit && !field.readOnly;
    const row = el(`<div class="setrow ${field.readOnly ? "ro" : ""}" ${hidden ? 'style="display:none"' : ""}>
        <div><div class="nm">${esc(field.label)}</div>${field.hint ? `<div class="hint">${esc(field.hint)}</div>` : ""}</div>
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
