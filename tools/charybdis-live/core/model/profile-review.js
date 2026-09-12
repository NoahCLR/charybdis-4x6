"use strict";

const {validateSnapshot} = require("./portable-profile");
const {settingsEditorView} = require("./settings-editor");
const {macroEditorView} = require("./macro-editor");
const keycodes = require("../data/keycode-catalog");
const rgbEnums = require("../schema/rgb-domain-v1");
const {KEY_BEHAVIOR_HOLD_MODES} = require("../schema/key-behavior-domain-v1");

const words = text => String(text).replace(/^(RGB_|KEY_FEEDBACK_|PD_MODE_)/, "").replace(/_/g, " ").toLowerCase();
const named = (values, value) => words(Object.entries(values).find(([, id]) => id === value)?.[0] ?? value);
const key = value => keycodes.resolve(value).label;
function action(value) {
    if (!value || value.kind === 0) return "None";
    if (value.kind === 1) return key(value.operand);
    const names = {2: "Momentary layer", 3: "Layer lock", 4: "Pointing mode", 5: "Pointing mode lock", 6: "VIA macro", 7: "User macro"};
    return `${names[value.kind] || "Action"} ${value.operand}`;
}
function behavior(row) {
    if (!row) return "None";
    const hold = value => value ? `${named(KEY_BEHAVIOR_HOLD_MODES, value.mode)}: ${action(value.action)}${value.repeatHz ? ` (${value.repeatHz}/s)` : ""}` : "None";
    return [`Tap/hold ${row.tapHoldTerm} ms; long hold ${row.longerHoldTerm} ms; repeat gap ${row.multiTapTerm} ms`,
        `Keep auto-mouse anchored: ${row.keepsAutoMouseAnchored ? "yes" : "no"}`,
        ...row.steps.map(step => `${step.tapIndex + 1} tap${step.tapIndex ? "s" : ""}: ${action(step.tap)}; hold ${hold(step.hold)}; long hold ${hold(step.longHold)}`)].join("\n");
}
const hsv = c => `HSV(${c.h}, ${c.s}, ${c.v})`;

// Compare complete validated snapshots. The review describes final differences,
// rather than a log that would still show edits the user has already undone.
function profileReview(before, after) {
    const a = validateSnapshot(before.document), b = validateSnapshot(after.document), changes = [];
    const add = (area, label, old, next) => {if (old !== next) changes.push({area, label, before: String(old ?? "None"), after: String(next ?? "None")});};
    const layer = (value, i) => value.settings.names[i] || `Layer ${i}`;
    a.document.layers.forEach((keys, l) => keys.forEach((code, p) => {
        if (code !== b.document.layers[l][p]) add("Layout", `${layer(b, l)} · row ${Math.floor(p / 6) + 1}, column ${p % 6 + 1}`, key(code), key(b.document.layers[l][p]));
    }));
    a.settings.names.forEach((name, i) => add("Layers", `Layer ${i} name`, name || `Layer ${i}`, b.settings.names[i] || `Layer ${i}`));
    const targets = new Map([...a.behaviors.rows, ...b.behaviors.rows].map(row => [JSON.stringify(row.target), row.target]));
    for (const [id, target] of targets) add("Behaviours", action(target), behavior(a.behaviors.rows.find(row => JSON.stringify(row.target) === id)), behavior(b.behaviors.rows.find(row => JSON.stringify(row.target) === id)));
    const combo = row => row ? `${row.inputs.map(action).join(" + ")} → ${action(row.output)}\nWindow ${row.termMs} ms; hold ${row.holdTermMs} ms${row.mustHold ? "; require hold" : ""}${row.mustTap ? "; tap only" : ""}${row.ordered ? "; ordered" : ""}` : "None";
    for (let i = 0; i < Math.max(a.combos.length, b.combos.length); i++) add("Combos", `Combo ${i + 1}`, combo(a.combos[i]), combo(b.combos[i]));
    const macrosA = macroEditorView(before), macrosB = macroEditorView(after);
    for (const bank of ["viaMacros", "hardcodedMacros"]) macrosA[bank].forEach((slot, i) => add("Macros", `${slot.kind === "via" ? "VIA" : "User"} macro ${i}`, slot.payload || "Empty", macrosB[bank][i].payload || "Empty"));
    const fields = (snapshot, settings) => settingsEditorView(snapshot).sections.flatMap(section => section.fields.map(field => {
        let value = field.value;
        if (field.kind === "toggle") value = field.enabled ? "On" : "Off";
        else if (field.kind === "layer") value = settings.names[Number(value.slice(6))] || value;
        else if (field.choices) value = field.choices.find(choice => typeof choice === "object" && String(choice.value) === field.value)?.label || value;
        return {...field, value, raw: field.value, section: section.label};
    }));
    const previous = fields(before, a.settings), next = fields(after, b.settings);
    for (const field of next) {const old = previous.find(item => item.macro === field.macro); if (old?.raw !== field.raw) add("Defaults", `${field.section} · ${field.label}`, old?.value, field.value);}
    const masks = after.options?.keymapMasks.reduce((mask, value) => mask | value, 0) || 0;
    add("Defaults", "Other saved key options", `0x${(a.settings.values[24] & ~masks).toString(16)}`, `0x${(b.settings.values[24] & ~masks).toString(16)}`);
    const rgb = value => {
        const r = value.rgb, result = new Map();
        result.set("Enabled feedback", Object.entries(rgbEnums.RGB_STAGE_BITS).filter(([, bit]) => r.stageEnableMask & bit).map(([name]) => words(name)).join(", ") || "None");
        for (const row of r.layerColors) result.set(`Layer ${row.layerId}`, `${hsv(row.color)} · ${named(rgbEnums.RGB_LAYER_MODES, row.mode)}`);
        for (const row of r.pdModeColors) result.set(`Pointing · ${named(rgbEnums.RGB_PD_MODE_IDS, row.pdModeId)}`, `${hsv(row.color)} · ${named(rgbEnums.RGB_LOCALITIES, row.locality)}`);
        result.set("Auto-mouse fade", `${named(rgbEnums.RGB_AUTOMOUSE_MODES, r.automouseFade.mode)} · ${hsv(r.automouseFade.endColor)}`);
        result.set("Combo feedback", `${hsv(r.comboFeedback.color)} · ${named(rgbEnums.RGB_LOCALITIES, r.comboFeedback.locality)}`);
        result.set("Key feedback", [r.keyFeedback.tapBranchColors.map(hsv).join(", "), hsv(r.keyFeedback.tapCommittedColor), hsv(r.keyFeedback.holdActiveColor), hsv(r.keyFeedback.longHoldActiveColor), named(rgbEnums.RGB_TAP_COMMIT_MODES, r.keyFeedback.tapCommitMode), named(rgbEnums.RGB_LOCALITIES, r.keyFeedback.locality)].join(" · "));
        r.groups.forEach(group => result.set(`LED group ${group.id}`, group.leds.join(", ")));
        for (const [table, label] of [["layerGroupRows", "Layer group"], ["pdModeGroupRows", "Pointing group"], ["comboGroupRows", "Combo group"], ["keyGroupRows", "Key feedback group"]]) r[table].forEach((row, i) => {
            const owner = row.selector === 255 ? "All" : table === "layerGroupRows" ? `Layer ${row.selector}` : table === "pdModeGroupRows" ? named(rgbEnums.RGB_PD_MODE_IDS, row.selector) : table === "keyGroupRows" ? named(rgbEnums.RGB_KEY_SEMANTICS, row.semantic) : "";
            result.set(`${label} assignment ${i + 1}`, `${owner} · LED group ${row.groupId} · ${hsv(row.color)}`);
        });
        return result;
    };
    const rgbA = rgb(a), rgbB = rgb(b);
    for (const label of new Set([...rgbA.keys(), ...rgbB.keys()])) add("RGB", label, rgbA.get(label), rgbB.get(label));
    return changes;
}
module.exports = {profileReview};
