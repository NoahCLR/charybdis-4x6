"use strict";

// Receives the same posted model as the existing editor. Device values only
// become text nodes; this view has no device or filesystem access.
function renderDeviceProfileDetails(document, model, post = () => {}, displayKey = value => model.qmkKeyLabels?.[value] || value) {
    const element = (tag, text, className) => {
        const node = document.createElement(tag);
        if (text !== undefined) node.textContent = text;
        if (className) node.className = className;
        return node;
    };
    const stages = document.getElementById("deviceRgbStages");
    if (stages) {
        const base = model.rgb?.baseEffect;
        stages.replaceChildren(element("h2", "Base RGB effect"));
        stages.append(element("p", base?.state === "read"
            ? `${base.effectName} · Brightness ${base.brightnessPercent}% of the device limit · Hue ${base.hue}, saturation ${base.saturation} · Speed ${base.speed}`
            : base?.message || "Base RGB has not been read from the keyboard."));
        stages.append(element("p", "The layer preview uses the last-read base effect. Brightness is approximate; animations and temporary feedback are not simulated. Use Read from keyboard to refresh.", "muted"));
        stages.append(element("h2", "RGB stages"));
        const states = model.rgb?.stages || [];
        stages.append(element("p", states.length
            ? states.map((stage) => `${stage.label}: ${stage.enabled ? "enabled" : "disabled"}`).join(" · ")
            : "No RGB configuration has been read from the keyboard."));
        if (states.length) {
            const controls = element("div");
            if (model.draft) {controls.id = "rgbStageDraft"; controls.setAttribute("data-dirty-section", "");}
            const inputs = states.map(stage => {
                const label = element("label", stage.label + " ");
                const input = element("input"); input.type = "checkbox"; input.checked = stage.enabled;
                label.append(input); controls.append(label); return {input, bit: stage.bit};
            });
            const save = element("button", model.draft ? "Keep RGB policies" : "Save RGB policies"); save.type = "button";
            if (model.draft) save.setAttribute("data-dirty-button", "");
            save.onclick = () => post({type: "updateRgbStages", stageEnableMask: inputs.reduce((mask, {input, bit}) => mask | (input.checked ? bit : 0), 0)});
            controls.append(save); stages.append(controls);
        }
        stages.append(element("p", model.draft ? "Keep RGB edits in your draft, then review and apply them with your other changes." : "Save writes the edited RGB settings to both halves, preserving the other profile settings.", "muted"));
    }
    const host = document.getElementById("deviceBehaviors");
    if (!host) return;
    const rows = model.keyBehaviors || [];
    host.replaceChildren(element("h2", `Key behaviours (${rows.length})`));
    host.append(element("p", "These rows were read from the keyboard. A timing value of 0 uses the firmware default; the default duration is not reported. Edit a row here or select a key in Layout. Saves preserve the other settings and are verified on both halves.", "muted"));
    if (!rows.length) {
        host.append(element("p", "No key behaviour rows were returned. Check the read status and diagnostics."));
        return;
    }
    const table = element("table");
    const heading = element("tr");
    for (const title of ["Key", "Timing (ms)", "Branches", "Auto-mouse anchor flag", "Edit"]) heading.append(element("th", title));
    const thead = element("thead");
    thead.append(heading);
    table.append(thead);
    const body = element("tbody");
    for (const row of rows) {
        const tr = element("tr");
        const key = element("td");
        const keyLabel = element("code", displayKey(row.keycode));
        keyLabel.title = row.keycode;
        key.append(keyLabel);
        const timing = element("td");
        for (const [label, value] of [["Tap/hold", row.tapHoldTerm], ["Long hold", row.longerHoldTerm], ["Multi-tap", row.multiTapTerm]]) {
            timing.append(element("div", `${label}: ${value}`));
        }
        const branches = element("td");
        for (const step of row.steps) {
            const branch = element("div", step.tapCountName);
            for (const [label, action] of [["Tap", step.tap], ["Hold", step.hold], ["Long hold", step.longHold]]) {
                if (!action) continue;
                const helper = {TAP_SENDS: "Tap", PRESS_AND_HOLD_UNTIL_RELEASE: "Hold until release", TAP_AT_HOLD_THRESHOLD: "Tap when hold starts", TAP_ON_RELEASE_AFTER_HOLD: "Tap on release", REPEAT_WHILE_HELD: `Repeat at ${action.repeatHz} Hz`}[action.helper] || action.helper;
                const description = element("div", `${label}: ${helper} ${displayKey(action.action)}`);
                description.title = `${action.helper}(${action.action})`;
                branch.append(description);
            }
            branches.append(branch);
        }
        tr.append(key, timing, branches, element("td", row.keepsAutoMouseAnchored ? "Set" : "Not set"));
        const actions = element("td");
        const edit = element("button", "Edit behaviour"); edit.type = "button";
        edit.onclick = () => post({type: "editBehavior", keycode: row.keycode});
        actions.append(edit); tr.append(actions);
        body.append(tr);
    }
    table.append(body);
    host.append(table);
}

module.exports = {renderDeviceProfileDetails};
