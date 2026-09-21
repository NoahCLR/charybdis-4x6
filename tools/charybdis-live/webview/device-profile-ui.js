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
        const overview = element("div", undefined, "rgb-overview-copy");
        overview.append(element("span", "Keyboard lighting", "rgb-overview-eyebrow"));
        overview.append(element("strong", base?.state === "read"
            ? `${base.effectName} · ${base.brightnessPercent}% brightness`
            : base?.message || "Base effect not read", "rgb-overview-title"));
        overview.append(element("span", base?.state === "read"
            ? `Hue ${base.hue} · Saturation ${base.saturation} · Speed ${base.speed}`
            : "Read from keyboard to load the current effect.", "rgb-overview-meta"));
        stages.replaceChildren(overview);
        const states = model.rgb?.stages || [];
        if (states.length) {
            const controls = element("div", undefined, "rgb-stage-controls");
            if (model.draft) {controls.id = "rgbStageDraft"; controls.setAttribute("data-dirty-section", "");}
            const inputs = states.map(stage => {
                const label = element("label", undefined, "toggle-inline rgb-stage-toggle");
                const input = element("input"); input.type = "checkbox"; input.checked = stage.enabled;
                input.setAttribute?.("aria-label", stage.label);
                input.setAttribute?.("role", "switch");
                const track = element("span", undefined, "toggle-switch"); track.setAttribute?.("aria-hidden", "true");
                label.append(input, track, element("span", stage.label, "toggle-label"));
                controls.append(label); return {input, bit: stage.bit};
            });
            const save = element("button", model.draft ? "Keep stages in draft" : "Save stages to keyboard"); save.type = "button";
            save.className = "rgb-stage-save";
            if (model.draft) save.setAttribute("data-dirty-button", "");
            save.onclick = () => post({type: "updateRgbStages", stageEnableMask: inputs.reduce((mask, {input, bit}) => mask | (input.checked ? bit : 0), 0)});
            controls.append(save); stages.append(controls);
        } else {
            stages.append(element("span", "No RGB stage configuration has been read.", "rgb-overview-meta"));
        }
        const note = element("span", "Preview uses the last read from the keyboard; animations and temporary feedback are not simulated.", "rgb-overview-note");
        note.setAttribute?.("title", "Brightness is approximate. Use Read from keyboard to refresh the base effect.");
        stages.append(note);
    }
}

module.exports = {renderDeviceProfileDetails};
