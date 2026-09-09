"use strict";

const {validateSnapshot} = require("./portable-profile");
const {decodeProfileBlob, encodeProfileBlob} = require("../schema/profile-blob-v1");
const {encodeSettings, validSetting} = require("../schema/settings-domain-v1");
const fail = message => Object.assign(new Error(message), {code: "INVALID_SETTINGS_EDIT"});
const number = (macro, id, label, hint = "0–65535 ms", extra = {}) => ({macro, id, label, hint, kind: "number", validate: "nonnegative-int", ...extra});
const toggle = (macro, id, label, hint) => ({macro, id, label, hint, kind: "toggle"});
const layer = (macro, id, label) => ({macro, id, label, kind: "layer", validate: "layer"});
const byte = (macro, id, shift, label, hint) => number(macro, id, label, hint || "0–255", {shift, max: 255, validate: "uint8"});

// These are editor definitions, not configuration defaults. Every value below
// is supplied by the complete device snapshot; absent readback stays absent.
const sections = [
    {id: "keyTiming", label: "Key Timing", fields: [
        number("tappingTerm", 0, "Dual-role tap / hold", "Milliseconds before a dual-role key becomes a hold."),
        number("tapHoldTerm", 1, "Behaviour tap / hold", "Used when a behaviour leaves its tap / hold timing empty."),
        number("longerHoldTerm", 2, "Long hold", "Used when a behaviour leaves its long-hold timing empty."),
        number("multiTapTerm", 3, "Repeated taps", "Maximum gap between repeated taps when a behaviour has no override."),
        toggle("combosEnabled", 20, "Combos", "Individual combo timing is available in the keyboard view."),
    ]},
    {id: "normalPointerSpeed", label: "Pointer Speed", fields: [
        number("normalDpi", 18, "Normal pointer DPI", "400–3400, in steps of 200", {choices: Array.from({length: 16}, (_, i) => 400 + i * 200)}),
        number("snipingDpi", 19, "Sniping DPI", "Lower DPI gives finer pointer control.", {choices: [100, 200, 300, 400]}),
    ]},
    {id: "pointingModeSpeeds", label: "Pointing Mode Speeds", fields: [
        number("dragscrollDpi", 10, "Drag-scroll DPI", "Trackball sensitivity while scrolling; this is an explicit DPI value."),
        ...["Volume", "Brightness", "Zoom", "Arrow"].map((name, i) => number(name.toLowerCase() + "Dpi", 11 + i, name + " mode DPI", "0 uses normal pointer DPI.")),
    ]},
    {id: "sniping", label: "Sniping", fields: [
        toggle("autoSniping", 8, "Auto-sniping", "Use sniping speed while the selected layer is active."),
        layer("snipingLayer", 9, "Auto-sniping layer"),
    ]},
    {id: "autoMouse", label: "Auto-mouse", fields: [
        toggle("autoMouse", 4, "Auto-mouse", "Trackball movement automatically activates the selected layer."),
        layer("mouseLayer", 5, "Auto-mouse layer"),
        number("mouseTimeout", 6, "Timeout (ms)", "How long the layer stays active after movement. Must exceed the fade delay."),
        number("mouseDebounce", 7, "Movement debounce (ms)", "0–255 ms", {max: 255}),
        number("mouseDelay", 25, "Delay after typing (ms)", "Wait this long after a key event before movement can activate auto-mouse."),
        number("mouseThreshold", 26, "Movement threshold", "Movement needed to activate auto-mouse. Higher values require more movement."),
        number("mouseFadeDelay", 16, "Lighting fade delay (ms)", "Wait before fading the auto-mouse lighting. Must be shorter than the timeout."),
    ]},
    {id: "rgbAppearance", label: "Base Lighting", fields: [
        {...toggle("lightingEnabled", 21, "Lighting", "Enable the saved base lighting effect."), shift: 0},
        byte("hue", 22, 0, "Hue"),
        byte("saturation", 22, 8, "Saturation", "0 is white; 255 is fully saturated."),
        byte("brightness", 22, 16, "Brightness", "Saved brightness, 0–255. The keyboard applies its hardware brightness limit."),
        byte("effectSpeed", 21, 16, "Animation speed"),
        number("lightingTimeout", 17, "Idle timeout (ms)", "0 keeps lighting on. Maximum 86400000 ms (one day).", {max: 86400000}),
    ]},
    {id: "lightingFeedback", label: "Lighting Feedback", fields: [
        number("feedbackFlash", 15, "Key feedback flash interval (ms)", "Time for each on or off phase; a full blink takes twice this value.", {min: 1, validate: "positive-int"}),
    ]},
];

const optionLabels = [
    ["swapControlCaps", "Swap Left Control and Caps Lock"],
    ["capsToControl", "Use Caps Lock as Left Control"],
    ["swapLeftAltGui", "Swap left Alt / Option and Windows / Command"],
    ["swapRightAltGui", "Swap right Alt / Option and Windows / Command"],
    ["disableGui", "Disable Windows / Command keys"],
    ["swapGraveEscape", "Swap backtick and Escape"],
    ["swapBackslashBackspace", "Swap backslash and Backspace"],
    ["nkro", "Allow more than six simultaneous keys"],
    ["swapLeftControlGui", "Swap left Control and Windows / Command"],
    ["swapRightControlGui", "Swap right Control and Windows / Command"],
    ["oneshot", "One-shot modifiers and layers"],
    ["swapEscapeCaps", "Swap Escape and Caps Lock"],
    ["autocorrect", "Autocorrect"],
];
function settingValue(field, values) {
    const value = values[field.id];
    if (field.bitMask) return Number(Boolean(value & field.bitMask));
    return field.shift === undefined ? value : (value >>> field.shift) & ((1 << (field.width || 8)) - 1);
}
function settingsSections(snapshot, settings) {
    const options = snapshot.options;
    const result = sections.map(section => ({...section, fields: section.fields.map(field => ({...field}))}));
    const rgb = result.find(section => section.id === "rgbAppearance");
    const currentEffect = (settings.values[21] >>> 8) & 255, currentFlags = settings.values[21] >>> 24;
    const effects = options?.effects.map(effect => ({value: effect.id, label: effect.name.toLowerCase().replace(/_/g, " ").replace(/^./, c => c.toUpperCase())})) || [];
    if (!effects.some(effect => effect.value === currentEffect)) effects.push({value: currentEffect, label: `Current effect (${currentEffect})`});
    const ledNames = [[1, "Modifiers"], [2, "Underlighting"], [4, "Keys"], [8, "Indicators"]];
    const flags = [{value: 255, label: "All LEDs"}, {value: 0, label: "No LEDs"}];
    if (options) for (let mask = 1; mask < 16; mask++) if (!(mask & ~options.ledFlags)) flags.push({value: mask, label: ledNames.filter(([bit]) => mask & bit).map(([, name]) => name).join(" + ")});
    if (!flags.some(choice => choice.value === currentFlags)) flags.push({value: currentFlags, label: "Current custom LED selection"});
    rgb.fields.splice(1, 0,
        {...byte("effectMode", 21, 8, "Lighting effect"), choices: effects, readOnly: !options, hint: options ? "Effects available on this keyboard. Layer colours can override the base effect." : "Update both halves to report the available lighting effects."},
        {...byte("effectLeds", 21, 24, "Apply base effect to"), choices: flags, readOnly: !options, hint: options ? "Choose which LED classes receive the base effect. Layer colours and feedback have their own policies." : "Update both halves to report the LED classes."});
    const name = i => settings.names[i] || `Layer ${i}`;
    result.push({id: "startupLayers", label: "Startup Layers", expanded: false, description: "Choose the layers active when the keyboard starts. Keep at least one selected; higher layers take priority.", fields:
        Array.from({length: 8}, (_, i) => ({...toggle(`startupLayer${i}`, 23, name(i), `Layer ${i}`), bitMask: 1 << i}))});
    result.push({id: "comboReferences", label: "Combo Layer Matching", expanded: false, description: "Choose which layer supplies the key assignments used to match combos on each layer. Select the same layer to keep its combos independent.", fields:
        Array.from({length: 8}, (_, i) => ({...layer(`comboReference${i}`, 27, `Combos on ${name(i)}`), shift: i * 4, width: 4}))});
    result.push({id: "keyboardOptions", label: "Key Options", expanded: false, description: options ? "Keyboard-wide remapping and typing options. These apply across all layers." : "Update both halves to report their supported key options.", fields:
        options ? optionLabels.map(([macro, label], i) => ({...toggle(macro, 24, label), bitMask: options.keymapMasks[i], readOnly: !(options.supportedKeymapOptions & (1 << i)), hint: options.supportedKeymapOptions & (1 << i) ? "" : "This option is not enabled in the running firmware."})) : []});
    return result;
}

function settingsEditorView(snapshot) {
    if (!snapshot?.document || snapshot.incomplete) return null;
    const {settings} = validateSnapshot(snapshot.document);
    return {identity: snapshot.fingerprint,
        sections: settingsSections(snapshot, settings).map(section => ({...section, fields: section.fields.map(field => {
            const value = settingValue(field, settings.values);
            const brightness = field.macro === "brightness";
            const max = brightness ? snapshot.limits?.brightnessMax : field.max;
            return {...field, max, readOnly: field.readOnly || (brightness && max === undefined),
                hint: brightness ? (max === undefined ? "Update both halves to report their brightness limit before editing brightness." : `0–${max}, the brightness limit reported by this keyboard.`) : field.hint,
                value: field.kind === "layer" ? `Layer ${value}` : String(value), enabled: Boolean(value)};
        })})),
        timing: Object.fromEntries(["tappingTerm", "tapHoldTerm", "longerHoldTerm", "multiTapTerm"].map((key, id) => [key, String(settings.values[id])]))};
}

function editSettings(snapshot, message, capabilities) {
    if (!snapshot?.document || !message.expectedFingerprint || message.expectedFingerprint !== snapshot.fingerprint) throw fail("The keyboard changed since these settings were opened. Read the keyboard and review your changes before saving again.");
    const value = validateSnapshot(snapshot.document, capabilities);
    const section = settingsSections(snapshot, value.settings).find(section => section.id === message.sectionId);
    if (!section || !Array.isArray(message.fields) || message.fields.length !== section.fields.length) throw fail("Choose a complete settings section reported by the keyboard.");
    const seen = new Set();
    for (const input of message.fields) {
        const field = section.fields.find(field => field.macro === input?.macro);
        if (!field || seen.has(input.macro)) throw fail("Unknown or repeated settings field.");
        seen.add(input.macro);
        let number;
        if (field.kind === "toggle") {
            if (typeof input.enabled !== "boolean") throw fail(`${field.label} must be enabled or disabled.`);
            number = Number(input.enabled);
        } else {
            const text = field.kind === "layer" ? /^Layer ([0-7])$/.exec(input.value)?.[1] : input.value;
            if (typeof text !== "string" || !/^\d+$/.test(text)) throw fail(`${field.label} needs a whole number${field.kind === "layer" ? " identifying a layer" : ""}.`);
            number = Number(text);
            if (field.macro === "brightness" && number !== ((value.settings.values[22] >>> 16) & 255)) {
                if (snapshot.limits?.brightnessMax === undefined) throw fail("Read the keyboard's brightness limit before changing brightness. Update both halves if this field is unavailable.");
                if (number > snapshot.limits.brightnessMax) throw fail(`Brightness must be between 0 and ${snapshot.limits.brightnessMax}, the keyboard's reported limit.`);
            }
            if (number < (field.min ?? 0) || number > (field.max ?? 65535) || (field.choices && !field.choices.some(choice => (typeof choice === "object" ? choice.value : choice) === number))) throw fail(`${field.label} is outside the keyboard's supported range${field.hint ? ": " + field.hint : "."}`);
        }
        const previous = settingValue(field, value.settings.values);
        if (field.readOnly && number !== previous) throw fail(`${field.label} cannot be changed with this firmware. ${field.hint || ""}`);
        if (field.macro === "effectMode" && number !== previous && !snapshot.options?.effects.some(effect => effect.id === number)) throw fail("Choose an effect reported by this keyboard.");
        const mask = (1 << (field.width || 8)) - 1;
        value.settings.values[field.id] = field.bitMask ? ((value.settings.values[field.id] & ~field.bitMask) | (number ? field.bitMask : 0)) >>> 0
            : field.shift === undefined ? number : ((value.settings.values[field.id] & ~(mask << field.shift)) | (number << field.shift)) >>> 0;
    }
    if (!value.settings.values[23]) throw fail("Keep at least one startup layer selected.");
    if (!value.settings.values.every((number, id) => validSetting(id, number))) throw fail("A setting is outside the keyboard's supported range.");
    if (value.settings.values[6] <= value.settings.values[16]) throw fail("Auto-mouse timeout must be longer than its lighting fade delay. Adjust both in Auto-mouse before saving.");
    const domains = decodeProfileBlob(value.profile).domains.map(domain => domain.id === 0x40 ? {...domain, payload: encodeSettings(value.settings)} : domain);
    const document = {...value.document, profile: encodeProfileBlob({domains}).toString("base64")};
    validateSnapshot(document, capabilities);
    return document;
}

module.exports = {settingsEditorView, editSettings};
