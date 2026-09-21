"use strict";

// Receives the same posted model as the existing editor. Device values only
// become text nodes; this view has no device or filesystem access.
function renderDeviceProfileDetails(document, model) {
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
}

module.exports = {renderDeviceProfileDetails};
