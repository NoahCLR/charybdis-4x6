"use strict";

const {validateSnapshot} = require("./portable-profile");
const {settingsEditorView} = require("./settings-editor");
const {macroEditorView} = require("./macro-editor");
const keycodes = require("../data/keycode-catalog");
const rgbEnums = require("../schema/rgb-domain-v1");
const {KEY_BEHAVIOR_HOLD_MODES} = require("../schema/key-behavior-domain-v1");

const words = text => String(text).replace(/^(RGB_|KEY_FEEDBACK_|PD_MODE_)/, "").replace(/_/g, " ").toLowerCase();
const named = (values, value) => words(Object.entries(values).find(([, id]) => id === value)?.[0] ?? value);
// A label is what a person reads, but many keycodes share one: KC_1 and KC_KP_1
// are both "1". Where a label is shared, the review adds the keycode name, so
// changing one for the other is never a row whose two sides look the same.
let sharedLabels;
function labelsUsedTwice() {
    const seen = new Set(), shared = new Set();
    for (let value = 0; value <= 0xffff; value++) {
        const {label} = keycodes.resolve(value);
        if (seen.has(label)) shared.add(label); else seen.add(label);
    }
    return shared;
}
function key(value) {
    const {label, name} = keycodes.resolve(value);
    sharedLabels ??= labelsUsedTwice();
    return sharedLabels.has(label) && name !== label ? `${label} (${name})` : label;
}
// An action named as the editors name what it reaches: a layer, a pointing
// mode or a macro by its own name in that snapshot, when it has one.
function action(value, names = {}) {
    if (!value || value.kind === 0) return "None";
    if (value.kind === 1) return key(value.operand);
    const id = value.operand;
    if (value.kind === 2 || value.kind === 3) return `${names.layers?.[id] || `Layer ${id}`} layer${value.kind === 3 ? " lock" : ""}`;
    if (value.kind === 4 || value.kind === 5) return `${names.pointing?.[id] || `Pointing slot ${id + 1}`}${value.kind === 5 ? " lock" : ""}`;
    if (value.kind === 6) return `Macro ${id}${names.macros?.[id] ? ` · ${names.macros[id]}` : ""}`;
    return `${value.kind === 7 ? "User macro" : "Action"} ${id}`;
}
// The names an action can be read by in one snapshot.
const namesIn = (value, macros) => ({layers: value.settings.names, pointing: value.pdModes?.map(slot => slot.kind ? slot.name : null) || [],
    macros: macros?.viaMacros.map(slot => slot.name) || []});
// The mark an action carries: what it reaches, when that has a colour of its
// own — a pointing mode's light, a layer's colour.
const actionMark = value => !value ? undefined
    : value.kind === 4 || value.kind === 5 ? {kind: "pointing", slot: value.operand}
    : value.kind === 2 || value.kind === 3 ? {kind: "layer", layer: value.operand} : undefined;
function pointingFields(slot) {
    const result = new Map([["Movement", ["Empty", "Directional keys / shortcuts", "Scrolling"][slot?.kind || 0]], ["Name", slot?.name || "Empty"]]);
    if (!slot?.kind) return result;
    const mods = mask => ["Left Ctrl", "Left Shift", "Left Alt", "Left GUI", "Right Ctrl", "Right Shift", "Right Alt", "Right GUI"].filter((_, bit) => mask & (1 << bit)).join(" + ") || "None";
    const tap = output => !output?.keycode ? "None" : `${key(output.keycode)} · ${output.modifierPolicy === 2 ? "Exact shortcut" : output.modifierPolicy === 1 ? "Ignore " + mods(output.mask) : "Inherit modifiers"}`;
    result.set("DPI", slot.dpi || "Normal pointer speed");
    result.set("Pointer layer", slot.pointerLayer ? "Return to typing layer" : "Keep pointer layer active");
    if (slot.kind === 1) {
        result.set("Axes", ["Vertical only", "Horizontal only", "Dominant axis"][slot.axis]);
        result.set("Horizontal movement per tap", slot.thresholdX);
        result.set("Vertical movement per tap", slot.thresholdY);
        for (const direction of ["left", "right", "up", "down"]) result.set(direction[0].toUpperCase() + direction.slice(1), tap(slot.directions[direction]));
    } else {
        result.set("Scroll modifiers", mods(slot.heldModifiers));
        const labels = {thresholdH: "Horizontal activation threshold", thresholdV: "Vertical activation threshold", divisorH: "Horizontal movement per wheel step", divisorV: "Vertical movement per wheel step", intervalMs: "Minimum interval (ms)", expireMs: "Gesture expiry (ms)", lockMs: "Axis lock timeout (ms)", startNumerator: "Axis selection ratio numerator", startDenominator: "Axis selection ratio denominator", sustainNumerator: "Axis retention ratio numerator", sustainDenominator: "Axis retention ratio denominator", decayDivisor: "Cross-axis decay divisor"};
        for (const [field, label] of Object.entries(labels)) result.set(label, slot.scroll[field]);
        result.set("Reverse scrolling", ["Neither axis", "Horizontal", "Vertical", "Both axes"][slot.scroll.invert]);
    }
    slot.buttons.forEach((button, index) => result.set(`Button ${index + 1}`, button.kind === 3 ? "Hold " + mods(button.modifiers) : button.kind === 2 ? tap(button.tap) : button.kind === 1 ? "Consume" : "Pass through"));
    return result;
}

const hsv = c => `HSV(${c.h}, ${c.s}, ${c.v})`;

// ── what a review item is ───────────────────────────────────────────────
//
// The review lists items, one per thing a person edits: a key on a layer, a
// layer's name, a behaviour, a combo, a macro, a settings section, a pointing
// slot, one lighting record. An item is also the unit a discard puts back
// (profile-revert.js), so it is the smallest part that is valid on its own.
// Each carries whether it was added, changed or removed, only the fields that
// differ (everything it holds, for an added or removed item), and where it
// is edited, so the review can go there.

// The words the editors use, so the review never names a thing differently.
const HOW_IT_RUNS = {PRESS_AND_HOLD_UNTIL_RELEASE: "held until release", TAP_AT_HOLD_THRESHOLD: "tap at hold threshold",
    TAP_ON_RELEASE_AFTER_HOLD: "tap on release after hold", REPEAT_WHILE_HELD: "repeat while held"};
const TIERS = [["tap", "tap"], ["hold", "hold"], ["longHold", "long hold"]];

// A behaviour as fields: its timing, the anchor, then every tier the grid
// shows, named as the grid names it (1× tap, 2× hold).
function behaviourFields(row, defaults, names) {
    if (!row) return new Map();
    // Compared by what is stored, shown with the default it stands for, so a
    // changed default in Settings does not read as a change to every row.
    // Each timing is marked with what it decides, as Settings marks its default.
    const timing = (value, fallback, labelMark) => ({text: value ? `${value} ms` : `default · ${fallback} ms`, key: value, labelMark});
    const fields = new Map([
        ["Tap / hold", timing(row.tapHoldTerm, defaults[1], {kind: "tier", tier: "hold"})],
        ["Long hold", timing(row.longerHoldTerm, defaults[2], {kind: "tier", tier: "long"})],
        ["Repeated taps", timing(row.multiTapTerm, defaults[3], {kind: "branch", count: 2})],
        ["Keeps auto-mouse anchored", row.keepsAutoMouseAnchored ? "yes" : "no"],
    ]);
    for (const step of row.steps) for (const [tier, name] of TIERS) {
        const branch = step[tier];
        if (!branch || (tier === "tap" && !branch.kind)) continue;
        const how = tier === "tap" ? "" : ` · ${HOW_IT_RUNS[Object.entries(KEY_BEHAVIOR_HOLD_MODES).find(([, id]) => id === branch.mode)?.[0]] || "unknown"}${branch.repeatHz ? ` · ${branch.repeatHz} Hz` : ""}`;
        // A tier is named as the grid names it, and carries which branch and
        // tier it is, so the review can colour it as the grid does.
        const reaches = tier === "tap" ? branch : branch.action;
        // Compared by what is stored, so a renamed or cleared pointing mode
        // or layer is a change where it was made, not to every behaviour
        // that reaches it.
        fields.set(`${step.tapIndex + 1}× ${name}`, {text: `${action(reaches, names)}${how}`, key: JSON.stringify([reaches, branch.mode, branch.repeatHz]),
            labelMark: {kind: "tier", tier: tier === "longHold" ? "long" : tier, branch: step.tapIndex + 1}, mark: actionMark(reaches)});
    }
    return fields;
}
// Fields an added or removed thing is described by: what it holds, leaving
// out values that only restate a default.
const EMPTY = new Set(["no", "none", "no name", "empty"]);
const holds = (label, value) => value !== undefined && !EMPTY.has(value) && !/^default · /.test(value) && !(label === "Movement" && value === "Empty");

// A field is shown by its text and compared by its key; most fields are plain
// text and are both. A field may also carry its own label, when the map key is
// an id rather than words.
const text = value => typeof value === "object" && value !== null ? value.text : value;
const compared = value => typeof value === "object" && value !== null ? value.key : value;
const labelOf = (id, ...values) => values.find(value => typeof value === "object" && value?.label)?.label ?? id;

function comboFields(row, names) {
    if (!row) return new Map();
    const options = [row.mustHold && "must be held", row.mustTap && "tap only", row.ordered && "keys in order"].filter(Boolean).join(", ");
    return new Map([["Keys", {text: row.inputs.map(input => action(input, names)).join(" + "), key: JSON.stringify(row.inputs)}],
        ["Sends", {text: action(row.output, names), key: JSON.stringify(row.output), mark: actionMark(row.output)}], ["Window", `${row.termMs} ms`],
        ["Hold threshold", `${row.holdTermMs} ms`], ["Options", options || "none"]]);
}

// Lighting in the words the Lighting screen uses, and every colour as a
// colour: the review draws it as a swatch, and says what an off colour means
// where it means something (a group row inherits its stage's colour).
const STAGE_NAMES = {LAYER: "Layer colours", AUTOMOUSE: "Auto-mouse fade", PD_MODE: "Pointing modes", COMBO: "Combo feedback", KEY_BEHAVIOR: "Key feedback"};
const LOCALITY_NAMES = {RGB_BOTH_HALVES: "both halves", RGB_LEFT_HALF: "left half", RGB_RIGHT_HALF: "right half",
    RGB_KEY_HALF: "the half holding the trigger key", RGB_KEYS_ONLY: "only the trigger key"};
const PAINTS_NAMES = {ALL_KEYS: "all keys", KEYS_MAPPED_ON_THIS_LAYER_ONLY: "keys mapped on this layer only"};
const FADE_NAMES = {FOLLOW_REAL_DESTINATION: "follow the real destination", END_COLOR_WHERE_BASE_EFFECT_WOULD_SHOW: "end colour where the base effect would show",
    END_COLOR_ON_ALL_KEYS: "end colour on all keys"};
const TAP_COMMIT_NAMES = {KEY_FEEDBACK_TAP_COMMIT_OFF: "off", KEY_FEEDBACK_TAP_COMMIT_NON_BASE_TAPS: "non-base taps"};
const SEMANTIC_NAMES = {KEY_FEEDBACK_GROUP_ALL: "every feedback state", KEY_FEEDBACK_GROUP_TAP_BRANCH_PENDING: "tap branch pending",
    KEY_FEEDBACK_GROUP_TAP_COMMITTED: "tap committed", KEY_FEEDBACK_GROUP_HOLD_ACTIVE: "hold active", KEY_FEEDBACK_GROUP_LONG_HOLD_ACTIVE: "long hold active"};
const nameIn = (names, values, value) => names[Object.entries(values).find(([, id]) => id === value)?.[0]] ?? named(values, value);
const colour = (value, off = "off") => ({text: value.v ? hsv(value) : off, key: hsv(value), colour: {h: value.h, s: value.s, v: value.v}});

function lightingRecords(value) {
    const r = value.rgb, records = new Map();
    const layerName = id => value.settings.names[id] || `Layer ${id}`;
    const slotName = id => value.pdModes?.[id]?.kind ? value.pdModes[id].name : id < 6 ? words(Object.keys(rgbEnums.RGB_PD_MODE_IDS)[id]) : `slot ${id + 1}`;
    const record = (unit, title, stage, fields, where = {}, titleMark) => records.set(unit, {title, stage, where, titleMark, fields: new Map(fields)});
    // Each stage's switch sits on its own tab, so the stages record belongs
    // to no single one.
    record("rgb:stages", "Feedback stages", null, Object.entries(rgbEnums.RGB_STAGE_BITS).map(([name, bit]) => {
        const on = Boolean(r.stageEnableMask & bit);
        return [STAGE_NAMES[name] || words(name), {text: on ? "on" : "off", key: on, mark: {kind: "stage", on}}];
    }));
    for (const row of r.layerColors) record(`rgb:layer:${row.layerId}`, `${layerName(row.layerId)} colour`, "layers",
        [["Colour", colour(row.color, row.layerId === 0 ? "off · the base effect shows" : "off")], ["Paints", nameIn(PAINTS_NAMES, rgbEnums.RGB_LAYER_MODES, row.mode)]], {layer: row.layerId}, {kind: "layer", layer: row.layerId});
    for (const row of r.pdModeColors) record(`rgb:pd:${row.pdModeId}`, `Pointing mode colour · ${slotName(row.pdModeId)}`, "pd",
        [["Colour", colour(row.color)], ["Where", nameIn(LOCALITY_NAMES, rgbEnums.RGB_LOCALITIES, row.locality)]], {slot: row.pdModeId}, {kind: "pointing", slot: row.pdModeId});
    record("rgb:automouse", "Auto-mouse fade", "auto", [["Fade", nameIn(FADE_NAMES, rgbEnums.RGB_AUTOMOUSE_MODES, r.automouseFade.mode)], ["End colour", colour(r.automouseFade.endColor)]]);
    record("rgb:combo", "Combo feedback", "combo", [["Colour", colour(r.comboFeedback.color)], ["Where", nameIn(LOCALITY_NAMES, rgbEnums.RGB_LOCALITIES, r.comboFeedback.locality)]]);
    const k = r.keyFeedback;
    record("rgb:key", "Key feedback", "key", [...k.tapBranchColors.map((value, i) => [`${i + 2}× branch`, colour(value)]),
        ["Tap committed", colour(k.tapCommittedColor)], ["Hold active", colour(k.holdActiveColor)], ["Long hold active", colour(k.longHoldActiveColor)],
        ["Tap commit", nameIn(TAP_COMMIT_NAMES, rgbEnums.RGB_TAP_COMMIT_MODES, k.tapCommitMode)], ["Where", nameIn(LOCALITY_NAMES, rgbEnums.RGB_LOCALITIES, k.locality)]]);
    // LED groups are one record with every row that paints them: rows name
    // groups by id, so a row put back without its group could point at a
    // group that no longer exists. Groups read as the Lighting screen shows
    // them, "Group 1 · 3 LEDs", and each row as who it paints for.
    const groups = new Map(r.groups.map(group => [group.id, group]));
    const fields = r.groups.map(group => [`group:${group.id}`, {label: `Group ${group.id}`, text: `LEDs ${group.leds.join(", ")}`, key: group.leds.join(",")}]);
    const TIER_SEMANTICS = {KEY_FEEDBACK_GROUP_TAP_COMMITTED: "tap", KEY_FEEDBACK_GROUP_HOLD_ACTIVE: "hold", KEY_FEEDBACK_GROUP_LONG_HOLD_ACTIVE: "long"};
    for (const [table, owner, ownerMark] of [
        ["layerGroupRows", row => row.selector === 255 ? "all layers" : layerName(row.selector), row => row.selector === 255 ? undefined : {kind: "layer", layer: row.selector}],
        ["pdModeGroupRows", row => row.selector === 255 ? "all pointing modes" : slotName(row.selector), row => row.selector === 255 ? undefined : {kind: "pointing", slot: row.selector}],
        ["comboGroupRows", () => "combos", () => ({kind: "combo"})],
        ["keyGroupRows", row => nameIn(SEMANTIC_NAMES, rgbEnums.RGB_KEY_SEMANTICS, row.semantic),
            row => (tier => tier && {kind: "tier", tier})(TIER_SEMANTICS[Object.entries(rgbEnums.RGB_KEY_SEMANTICS).find(([, id]) => id === row.semantic)?.[0]])],
    ]) r[table].forEach((row, i) => {
        const group = groups.get(row.groupId), leds = group ? `Group ${group.id} · ${group.leds.length} LED${group.leds.length === 1 ? "" : "s"}` : `Group ${row.groupId}`;
        const paint = colour(row.color, "inherits the stage colour");
        fields.push([`${table}:${i}`, {label: `Override · ${owner(row)}`, labelMark: ownerMark(row) || undefined, text: `${leds} · ${paint.text}`,
            key: `${group?.leds.join(",")}|${paint.key}|${owner(row)}`, colour: paint.colour}]);
    });
    record("rgb:groups", "LED overrides", "groups", fields);
    return records;
}

// Compare complete validated snapshots. The review describes final differences,
// rather than a log that would still show edits the user has already undone.
function profileReview(before, after) {
    const a = validateSnapshot(before.document), b = validateSnapshot(after.document), items = [];
    // One item from its fields on each side; `exists` says whether the thing is
    // there at all, which decides added and removed.
    const item = (area, unit, title, old, next, place, exists = [old.size > 0, next.size > 0], titleMark) => {
        const [was, is] = exists;
        if (!was && !is) return;
        const status = was && is ? "changed" : is ? "added" : "removed";
        const ids = [...new Set([...old.keys(), ...next.keys()])];
        const fields = ids.filter(id => status === "changed" ? compared(old.get(id)) !== compared(next.get(id))
            : holds(id, text(status === "added" ? next.get(id) : old.get(id))))
            .map(id => {
                // A field has its own status: inside a changed behaviour a
                // tier can be added or removed while the behaviour stays. A
                // side a field is absent from is null, never a word that
                // could read as a value.
                const was = status !== "added" && old.has(id), is = status !== "removed" && next.has(id);
                const entry = {label: labelOf(id, next.get(id), old.get(id)), status: was && is ? "changed" : is ? "added" : "removed",
                    before: was ? text(old.get(id)) : null, after: is ? text(next.get(id)) : null};
                // A field carries its colour and the marks of what it is about,
                // on the side that shows them.
                const side = (value, name) => value && typeof value === "object" ? value[name] : undefined;
                const extra = {labelMark: side(next.get(id), "labelMark") || side(old.get(id), "labelMark"),
                    beforeColour: status === "added" ? undefined : side(old.get(id), "colour"), afterColour: status === "removed" ? undefined : side(next.get(id), "colour"),
                    beforeMark: status === "added" ? undefined : side(old.get(id), "mark"), afterMark: status === "removed" ? undefined : side(next.get(id), "mark")};
                return {...entry, ...Object.fromEntries(Object.entries(extra).filter(([, value]) => value !== undefined))};
            });
        if (status === "changed" && !fields.length) return;
        items.push({area, unit, title, status, fields, place, ...(titleMark ? {titleMark} : {})});
    };
    const layerName = (value, i) => value.settings.names[i] || `Layer ${i}`;
    const macrosA = macroEditorView(before), macrosB = macroEditorView(after);
    const namesA = namesIn(a, macrosA), namesB = namesIn(b, macrosB);
    a.document.layers.forEach((keys, l) => keys.forEach((code, p) => {
        if (code === b.document.layers[l][p]) return;
        // The position's name on the board comes from the layout, which the
        // session knows: the item carries the matrix slot for it.
        const values = code => ({text: key(code), key: code});
        item("Layout", `layout:${l}:${p}`, layerName(b, l), new Map([["", values(code)]]), new Map([["", values(b.document.layers[l][p])]]),
            {kind: "key", layer: l, slot: p}, undefined, {kind: "layer", layer: l});
    }));
    a.settings.names.forEach((name, i) => item("Layers", `layerName:${i}`, `Layer ${i}`, new Map([["Name", name || `Layer ${i}`]]), new Map([["Name", b.settings.names[i] || `Layer ${i}`]]), {kind: "layers"}, undefined, {kind: "layer", layer: i}));
    const targets = new Map([...a.behaviors.rows, ...b.behaviors.rows].map(row => [JSON.stringify(row.target), row.target]));
    for (const [id, target] of targets) {
        const old = a.behaviors.rows.find(row => JSON.stringify(row.target) === id), next = b.behaviors.rows.find(row => JSON.stringify(row.target) === id);
        item("Behaviours", `behavior:${id}`, action(target, next ? namesB : namesA), behaviourFields(old, a.settings.values, namesA), behaviourFields(next, b.settings.values, namesB),
            {kind: "behaviour", target}, [Boolean(old), Boolean(next)]);
    }
    for (let i = 0; i < Math.max(a.combos.length, b.combos.length); i++) {
        item("Combos", `combo:${i}`, `Combo ${i + 1}`, comboFields(a.combos[i], namesA), comboFields(b.combos[i], namesB), {kind: "combo", index: i}, undefined,
            {kind: "combo", badge: `C${i + 1}`});
    }
    macrosA.viaMacros.forEach((slot, i) => {
        const fields = (macro) => new Map([["Steps", macro.payload || "empty"], ["Name", macro.name || "no name"]]);
        const old = macrosA.viaMacros[i], next = macrosB.viaMacros[i], has = (macro) => Boolean(macro.payload || macro.name);
        item("Macros", `macro:${i}`, `Macro ${i}${(next.name || old.name) ? ` · ${next.name || old.name}` : ""}`, fields(old), fields(next), {kind: "macro", index: i}, [has(old), has(next)]);
    });
    // Settings are saved a section at a time, so a section is one item.
    const sections = (snapshot, settings) => settingsEditorView(snapshot).sections.map(section => ({id: section.id, label: section.label,
        fields: new Map(section.fields.map(field => {
            let value = field.value;
            if (field.kind === "toggle") value = field.enabled ? "on" : "off";
            else if (field.kind === "layer") return [field.macro, {label: field.label, text: settings.names[Number(value.slice(6))] || value, key: field.value,
                labelMark: field.governs, mark: {kind: "layer", layer: Number(value.slice(6))}}];
            else if (field.choices) value = field.choices.find(choice => typeof choice === "object" && String(choice.value) === field.value)?.label || value;
            const ms = /ms\b/.test(field.hint || "") || /\(ms\)/.test(field.label);
            return [field.macro, {label: field.label.replace(/\s*\(ms\)$/, ""), text: ms && /^\d+$/.test(value) ? `${value} ms` : value, key: field.value, labelMark: field.governs}];
        }))}));
    const sectionsA = sections(before, a.settings);
    for (const section of sections(after, b.settings)) {
        const old = sectionsA.find(entry => entry.id === section.id);
        item("Settings", `settings:${section.id}`, section.label, old?.fields || new Map(), section.fields, {kind: "settings", section: section.id}, [true, true]);
    }
    const masks = after.options?.keymapMasks.reduce((mask, value) => mask | value, 0) || 0;
    item("Settings", "settings:otherKeyOptions", "Other key options", new Map([["Stored bits", `0x${(a.settings.values[24] & ~masks).toString(16)}`]]),
        new Map([["Stored bits", `0x${(b.settings.values[24] & ~masks).toString(16)}`]]), {kind: "settings"}, [true, true]);
    for (let id = 0; id < 8; id++) {
        const old = a.pdModes?.[id], next = b.pdModes?.[id];
        item("Pointing modes", `pd:${id}`, `Slot ${id + 1}${(next?.name || old?.name) ? ` · ${next?.kind ? next.name : old?.name}` : ""}`,
            pointingFields(old), pointingFields(next), {kind: "pointing", slot: id}, [Boolean(old?.kind), Boolean(next?.kind)], {kind: "pointing", slot: id});
    }
    const lightA = lightingRecords(a), lightB = lightingRecords(b);
    for (const [unit, record] of lightB) {
        const old = lightA.get(unit);
        item("Lighting", unit, record.title, old?.fields || new Map(), record.fields, {kind: "lighting", stage: record.stage, ...record.where}, [Boolean(old), true], record.titleMark);
    }
    for (const [unit, record] of lightA) if (!lightB.has(unit)) item("Lighting", unit, record.title, record.fields, new Map(), {kind: "lighting", stage: record.stage, ...record.where});
    // The items above describe the profile in words. If the stored bytes
    // differ and none of them caught it, the review still must not read as
    // empty: an apply always shows that something will change.
    if (!items.length && before.fingerprint !== after.fingerprint) {
        items.push({area: "Profile", unit: "profile", title: "Stored profile", status: "changed", fields: [{label: "", before: `fingerprint ${before.fingerprint}`,
            after: `fingerprint ${after.fingerprint} · a change this review cannot describe`}], place: null});
    }
    return items;
}
module.exports = {profileReview};
