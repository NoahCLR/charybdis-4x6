"use strict";

function renderDeviceCombos(document, model, post = () => {}) {
    const host = document.getElementById("deviceCombos");
    if (!host) return;
    const node = (tag, text) => {
        const element = document.createElement(tag);
        if (text !== undefined) element.textContent = text;
        return element;
    };
    const read = model.comboReadback;
    const rows = model.combos || [];
    host.replaceChildren(node("h2", read?.state === "read" ? `Combos (${rows.length})` : "Combos"));
    if (read?.state !== "read") {
        host.append(node("p", read?.error?.message || "Read from keyboard to load its combos."));
        return;
    }
    host.append(node("p", `Combos are ${read.enabled ? "enabled" : "disabled"}. ` + (model.draft ? "These are the definitions in your profile draft. Keep edits here, then review and apply the profile to the keyboard." : "These definitions and timing values came from the keyboard. Save updates both halves and verifies the profile readback.")));
    host.append(node("p", read.noTimer ? "Combo timing is disabled." : read.strictTimer ? "The combo window starts at the first input press." : "The combo window is extended by subsequent input presses."));
    if (read.customTrigger || read.customRelease || read.customRepress) host.append(node("p", "Additional firmware conditions affect combo triggering or release; they cannot be described by this readout."));
    const references = (read.layerReferences || []).map((target, layer) => target !== layer ? `Layer ${layer} uses Layer ${target}` : "").filter(Boolean);
    if (references.length) host.append(node("p", "Combo input mapping: " + references.join(" · ")));
    if (read.fixedReference) host.append(node("p", "Inputs use the reference layer directly, without transparent-key inheritance."));
    const writable = read.writable && !read.noTimer && !read.customTrigger && !read.customRelease && !read.customRepress;
    if (!writable) host.append(node("p", "Combo saving needs a complete profile read and firmware with combo write support."));
    const field = (parent, label, value, type = "text") => {
        const wrapper = node("label", label + " ");
        const input = node("input"); input.type = type;
        if (type === "checkbox") input.checked = Boolean(value);
        else input.value = String(value ?? "");
        if (type === "number") {input.min = "0"; input.max = "65535"; input.step = "1";}
        wrapper.append(input); parent.append(wrapper); return input;
    };
    if (writable && rows.length) {
        const shared = node("div");
        if (model.draft) {shared.id = "comboHoldDraft"; shared.setAttribute("data-dirty-section", "");}
        const hold = field(shared, "Shared hold threshold (ms)", rows[0].holdTermMs, "number");
        const save = node("button", model.draft ? "Keep hold threshold" : "Save hold threshold"); save.type = "button";
        if (model.draft) save.setAttribute("data-dirty-button", "");
        save.onclick = () => post({type: "updateComboHoldTerm", holdTermMs: hold.value});
        shared.append(save); host.append(shared);
    }
    const editor = (parent, combo) => {
        const edit = node("button", `Edit ${combo.badge} on layout`); edit.type = "button";
        edit.onclick = () => post({type: "editComboInLayout", id: combo.id}); parent.append(edit);
        const remove = node("button", `Delete ${combo.badge}`); remove.type = "button";
        remove.onclick = () => post({type: "deleteCombo", id: combo.id}); parent.append(remove);
    };
    if (!rows.length) {host.append(node("p", "The keyboard reports no combos.")); return;}
    const table = node("table");
    const head = node("thead"), tr = node("tr");
    for (const title of ["Combo", "Inputs", "Output", "Timing", "Requirements"]) tr.append(node("th", title));
    if (writable) tr.append(node("th", "Edit"));
    head.append(tr); table.append(head);
    const body = node("tbody");
    for (const combo of rows) {
        const row = node("tr");
        const inputs = node("td", combo.inputDisplays.join(" + "));
        inputs.append(node("div", combo.inputs.join(" + ")));
        const output = node("td", combo.outputDisplay);
        output.append(node("div", combo.output));
        const requirements = [combo.mustHold ? "Hold" : "", combo.mustTap ? "Tap only" : "", combo.ordered ? "Press in order" : ""].filter(Boolean);
        row.append(node("td", combo.badge), inputs, output,
            node("td", `${combo.termMs} ms window · ${combo.holdTermMs} ms hold threshold`),
            node("td", requirements.join(" · ") || "Any press order"));
        if (writable) {const actions = node("td"); editor(actions, combo); row.append(actions);}
        body.append(row);
    }
    table.append(body); host.append(table);
}

module.exports = {renderDeviceCombos};
