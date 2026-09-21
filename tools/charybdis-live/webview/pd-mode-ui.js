"use strict";

// Kept as a standalone function because it is also serialized into the webview.
// The device model already resolves each populated slot to its real firmware
// action and current user-facing name; the picker must not reconstruct either.
function pointingModePickerRows(model, columns = 4) {
    const entries = (model?.qmkKeycodes || []).filter(entry => entry?.group === "Pointing modes");
    const rows = [];
    for (let index = 0; index < entries.length; index += columns) rows.push(entries.slice(index, index + columns));
    return rows;
}

function pdModeDpiChoices(model) {
    const labels = new Map([[0, "Use normal pointer speed"]]);
    for (const section of model?.configDefaults || []) {
        for (const field of section.fields || []) {
            if (!field.choices || !/Dpi$/.test(field.macro || "")) continue;
            for (const choice of field.choices) {
                const value = Number(typeof choice === "object" ? choice.value : choice);
                if (Number.isInteger(value) && value > 0 && value <= 65535) labels.set(value, typeof choice === "object" && choice.label ? choice.label : `${value} DPI`);
            }
        }
    }
    for (const slot of model?.pdModes || []) {
        const value = Number(slot.dpi);
        if (Number.isInteger(value) && value > 0 && value <= 65535 && !labels.has(value)) labels.set(value, `${value} DPI`);
    }
    return [...labels].sort(([a], [b]) => a - b);
}

// Serialized into the webview. Device text is only assigned through textContent.
function renderPdModes(document, model, post, keycodeTools = {}) {
    const host = document.getElementById("pdModes");
    if (!host) return;
    const node = (tag, text) => {const element = document.createElement(tag); if (text !== undefined) element.textContent = text; return element;};
    host.replaceChildren(node("h2", "Pointing modes"));
    host.append(node("p", "Choose a name, movement type, speed and the actions you want. Less common tuning stays under Advanced. Keep changes in your draft, then review and Apply."));
    const slots = model.pdModes || [], writable = Boolean(model.pdModeEditing?.writable && model.draft), dpiChoices = pdModeDpiChoices(model);
    if (slots.length !== 8) {
        host.append(node("p", "This firmware has fixed pointing modes. The configurable-mode firmware and a migrated profile are required to edit these slots."));
        return;
    }
    const directKeyName = code => model.qmkKeycodes?.find(entry => entry.keycode === code)?.value;
    const keyName = code => {
        if (!code) return "";
        const direct = directKeyName(code); if (direct) return direct;
        if (code >= 0x0100 && code <= 0x1fff) {
            const base = directKeyName(code & 0xff), bits = (code >> 8) & 0x1f;
            if (base && bits) {
                const right = Boolean(bits & 0x10), wrappers = right ? [[1, "RCTL"], [2, "RSFT"], [4, "RALT"], [8, "RGUI"]] : [[1, "C"], [2, "S"], [4, "A"], [8, "G"]];
                return wrappers.filter(([bit]) => bits & bit).reduceRight((value, [, wrapper]) => `${wrapper}(${value})`, base);
            }
        }
        return `0x${code.toString(16)}`;
    };
    slots.forEach(slot => {
        const panel = node("details"), title = node("summary", `Slot ${slot.id + 1} · ${slot.name || "Empty"}`);
        panel.className = "panel"; panel.append(title); host.append(panel);
        const form = node("form"); form.id = `pdSlot${slot.id}`; form.dataset.dirtySection = `pdSlot${slot.id}`; form.dataset.dirtyLabel = `Pointing slot ${slot.id + 1}`;
        form.className = "panel-body"; panel.append(form);
        const field = (parent, label, name, value, choices, max = 65535) => {
            const wrapper = node("label"), caption = node("span", label), input = node(choices ? "select" : typeof value === "number" ? "input" : "input");
            wrapper.style.display = "grid"; wrapper.style.gap = "4px"; wrapper.style.marginBottom = "10px";
            input.id = `pd-${slot.id}-${name}`; input.name = name;
            if (choices) for (const [id, text] of choices) {const option = node("option", text); option.value = String(id); input.append(option);}
            else if (typeof value === "number") {input.type = "number"; input.min = "0"; input.max = String(max); input.step = "1";}
            else input.type = "text";
            input.value = String(value ?? ""); input.disabled = !writable;
            wrapper.append(caption, input); parent.append(wrapper); return input;
        };
        const group = (parent, label, open = true) => {const details = node("details"); details.open = open; details.append(node("summary", label)); parent.append(details); return details;};
        const button = (label, callback) => {const control = node("button", label); control.type = "button"; control.disabled = !writable; control.addEventListener("click", callback); form.append(control); return control;};
        const keycode = (parent, label, name, value) => {
            const wrapper = node("label"), caption = node("span", label), row = node("span"), input = node("input"), pick = node("button", "Pick keycode");
            wrapper.style.display = "grid"; wrapper.style.gap = "4px"; wrapper.style.marginBottom = "10px";
            row.className = "input-with-button";
            input.id = `pd-${slot.id}-${name}`; input.name = name; input.type = "text"; input.value = value || ""; input.placeholder = "None or G(KC_Z)"; input.disabled = !writable;
            pick.type = "button"; pick.disabled = !writable; pick.addEventListener("click", () => keycodeTools.open?.(input.id, "single"));
            row.append(input, pick); wrapper.append(caption, row); parent.append(wrapper); return input;
        };
        const identity = field(form, "", "identity", JSON.stringify(model.profileIdentity)); identity.parentElement.hidden = true;
        const name = field(form, "Name", "name", slot.name); name.maxLength = 23;
        const kind = field(form, "Movement", "kind", slot.kind || 1, [[1, "Directional keys / shortcuts"], [2, "Scrolling"]]);
        const dpi = field(form, "DPI", "dpi", slot.dpi, dpiChoices);
        const movement = group(form, "Directional actions");
        const scrolling = group(form, "Scrolling", slot.kind === 2);
        const advanced = group(form, "Advanced", false); advanced.dataset.pdAdvanced = String(slot.id);
        const pointer = field(advanced, "After this mode ends", "pointerLayer", slot.pointerLayer, [[0, "Keep pointer layer active"], [1, "Return to typing layer"]]);
        const directionAdvanced = group(advanced, "Direction tuning and modifier rules", false);
        const axis = field(movement, "Active axes", "axis", slot.kind === 1 ? slot.axis : 2, [[0, "Vertical only"], [1, "Horizontal only"], [2, "Dominant axis"]]);
        const tx = field(directionAdvanced, "Horizontal movement per tap", "thresholdX", slot.thresholdX || 40);
        const ty = field(directionAdvanced, "Vertical movement per tap", "thresholdY", slot.thresholdY || 50);
        const modifierChoices = [[1, "Left Ctrl"], [2, "Left Shift"], [4, "Left Alt / Option"], [8, "Left GUI / Command"], [16, "Right Ctrl"], [32, "Right Shift"], [64, "Right Alt / Option"], [128, "Right GUI / Command"]];
        const modifiers = (parent, label, prefix, mask) => {
            const details = node("details"); details.append(node("summary", label)); parent.append(details);
            const controls = modifierChoices.map(([bit, text]) => {
                const wrapper = node("label", text + " "), input = node("input"); input.type = "checkbox";
                input.id = `pd-${slot.id}-${prefix}-${bit}`; input.name = `${prefix}-${bit}`; input.checked = Boolean(mask & bit); input.disabled = !writable;
                wrapper.style.display = "block"; wrapper.append(input); details.append(wrapper); return [bit, input];
            });
            return () => controls.reduce((mask, [bit, input]) => mask | (input.checked ? bit : 0), 0);
        };
        const tap = (parent, rulesParent, label, prefix, value = {}) => {
            const key = keycode(parent, label, `${prefix}-key`, keyName(value.keycode || 0));
            const rules = group(rulesParent, `${label} modifier handling`, false);
            const policy = field(rules, "Held keyboard modifiers", `${prefix}-policy`, value.modifierPolicy || 0, [[0, "Inherit"], [1, "Ignore selected modifiers"], [2, "Use only this shortcut"]]);
            const mask = modifiers(rules, "Modifiers to ignore", `${prefix}-mask`, value.mask || 0);
            return () => {
                const authored = key.value.trim(), canonical = authored && keycodeTools.canonicalize ? keycodeTools.canonicalize(authored) : authored;
                return {keycode: canonical || "0", modifierPolicy: authored ? Number(policy.value) : 0, mask: authored && Number(policy.value) === 1 ? mask() : 0};
            };
        };
        const outputs = Object.fromEntries(["left", "right", "up", "down"].map(direction => [direction, tap(movement, directionAdvanced, direction[0].toUpperCase() + direction.slice(1), direction, slot.directions?.[direction])]));
        const held = modifiers(scrolling, "Hold modifiers while scrolling", "scrollMods", slot.heldModifiers);
        const inversion = field(scrolling, "Reverse scrolling", "invert", slot.kind === 2 ? slot.scroll.invert : 2, [[0, "Neither axis"], [1, "Horizontal"], [2, "Vertical"], [3, "Both axes"]]);
        const scrollAdvanced = group(advanced, "Scroll tuning", false);
        const scrollFields = [
            ["thresholdH", "Horizontal activation threshold", 2, 65535], ["thresholdV", "Vertical activation threshold", 3, 65535],
            ["divisorH", "Horizontal movement per wheel step", 6, 65535], ["divisorV", "Vertical movement per wheel step", 8, 65535],
            ["intervalMs", "Minimum interval (ms)", 8, 65535], ["expireMs", "Gesture expiry (ms)", 80, 65535], ["lockMs", "Axis lock timeout (ms)", 55, 65535],
            ["startNumerator", "Axis selection ratio: numerator", 7, 255], ["startDenominator", "Axis selection ratio: denominator", 4, 255],
            ["sustainNumerator", "Axis retention ratio: numerator", 5, 255], ["sustainDenominator", "Axis retention ratio: denominator", 4, 255],
            ["decayDivisor", "Cross-axis decay divisor", 4, 255],
        ].map(([key, label, fallback, max]) => [key, field(scrollAdvanced, label, key, slot.kind === 2 ? slot.scroll[key] : fallback, null, max)]);
        const overrides = group(advanced, "Mouse button overrides", false);
        const buttons = [0, 1, 2].map(index => {
            const value = slot.buttons?.[index] || {}, block = node("div"); overrides.append(block);
            const action = field(block, `Button ${index + 1}`, `button${index}-kind`, value.kind || 0, [[0, "Pass through"], [1, "Consume"], [2, "Tap shortcut"], [3, "Hold modifiers"]]);
            const mods = modifiers(block, "Held modifiers", `button${index}-mods`, value.modifiers);
            const output = tap(block, block, "Shortcut", `button${index}-tap`, value.tap);
            return () => ({kind: Number(action.value), ...(Number(action.value) === 3 ? {modifiers: mods()} : Number(action.value) === 2 ? {tap: output()} : {})});
        });
        const hint = slot.id < 6 ? ["DRAGSCROLL", "VOLUME_MODE", "BRIGHTNESS_MODE", "ZOOM_MODE", "ARROW_MODE", "PINCH_MODE"][slot.id] : `PD_SLOT_${slot.id}`;
        form.append(node("p", `Assign ${hint} to hold this mode, or ${hint}_LOCK to toggle it. Its lighting is under RGB → Pointing modes.`));
        const send = message => post({...message, slot: slot.id, expectedBase: JSON.parse(identity.value)});
        const keep = button("Keep mode", () => {
            if (!form.reportValidity()) return;
            const config = {id: slot.id, kind: Number(kind.value), name: name.value.trim(), dpi: Number(dpi.value), pointerLayer: Number(pointer.value), buttons: buttons.map(read => read())};
            if (config.kind === 1) {
                config.axis = Number(axis.value); config.thresholdX = config.axis === 0 ? 0 : Number(tx.value); config.thresholdY = config.axis === 1 ? 0 : Number(ty.value);
                config.directions = Object.fromEntries(Object.entries(outputs).filter(([direction]) => config.axis === 2 || (config.axis === 0 ? ["up", "down"] : ["left", "right"]).includes(direction)).map(([direction, read]) => [direction, read()]));
            } else {
                config.heldModifiers = held(); config.scroll = Object.fromEntries(scrollFields.map(([key, input]) => [key, Number(input.value)])); config.scroll.invert = Number(inversion.value);
            }
            send({type: "savePdMode", config});
        });
        keep.title = "Keep in the shared draft; Apply after reviewing all changes.";
        if (slot.kind) button("Clear slot", () => send({type: "clearPdMode"}));
        else {
            const source = field(form, "Copy from", "source", slots.find(row => row.kind)?.id ?? 0, slots.filter(row => row.kind).map(row => [row.id, `Slot ${row.id + 1} · ${row.name}`]));
            button("Duplicate into this slot", () => send({type: "duplicatePdMode", source: Number(source.value)}));
        }
        form.addEventListener("submit", event => event.preventDefault());
        // Both editors remain mounted so unfinished values survive tab changes.
        const sync = () => {
            movement.hidden = kind.value !== "1"; scrolling.hidden = kind.value !== "2";
            directionAdvanced.hidden = kind.value !== "1"; scrollAdvanced.hidden = kind.value !== "2";
            const activeAxis = Number(axis.value);
            for (const direction of ["left", "right"]) document.getElementById(`pd-${slot.id}-${direction}-key`).parentElement.parentElement.hidden = activeAxis === 0;
            for (const direction of ["up", "down"]) document.getElementById(`pd-${slot.id}-${direction}-key`).parentElement.parentElement.hidden = activeAxis === 1;
        };
        form.addEventListener("restore-pd-controls", sync);
        kind.addEventListener("change", sync); axis.addEventListener("change", sync); sync();
    });
}

module.exports = {pdModeDpiChoices, pointingModePickerRows, renderPdModes};
