"use strict";
const {encodePdDomain, decodePdDomain} = require("../schema/pd-mode-domain-v1");
const {decodeProfileBlob, encodeProfileBlob, PROFILE_DOMAIN_IDS} = require("../schema/profile-blob-v1");
const {decodeRgbDomainV1, encodeRgbDomainV1, RGB_LAYER_MODES, RGB_LOCALITIES, RGB_AUTOMOUSE_MODES, RGB_TAP_COMMIT_MODES, RGB_PD_MODE_IDS, RGB_KEY_SEMANTICS} = require("../schema/rgb-domain-v1");

const keycodes = require("../data/keycode-catalog");
const {semanticActionForExpression, resolveNativeQmkExpression} = require("../schema/compiled-profile-v1");
const {encodeComboDomainV1, decodeComboDomainV1} = require("../schema/combo-domain-v1");
const {actionLimitsFor, actionName, keycodeAction, knownActionAbi, layerRef, nativeCode} = require("../schema/actions");
const {BEHAVIOR_EDITS, editKeyBehaviors} = require("./key-behavior-edits");
const COMBO_EDITS = new Set(["addCombo", "saveCombo", "deleteCombo", "updateComboHoldTerm"]);

const PD_EDITS = new Set(["savePdMode", "clearPdMode", "duplicatePdMode"]);
const RGB_EDITS = new Set(["updateLayerColor", "updatePdModeColor", "updateAutomouseFade", "updateComboFeedback", "updateKeyBehaviorFeedback", "updateRgbStages", "saveRgbReusableLedGroup", "deleteRgbReusableLedGroup", "addRgbLedGroup", "deleteRgbLedGroup"]);
const invalid = message => Object.assign(new Error(message), {code: "INVALID_PROFILE_EDIT"});
function integer(value, max, label) {
    if (typeof value === "string" && !/^\d+$/.test(value.trim())) throw invalid(`${label} must be a whole number.`);
    if (typeof value !== "string" && typeof value !== "number") throw invalid(`${label} must be a whole number.`);
    const number = Number(value);
    if (!Number.isInteger(number) || number < 0 || number > max) throw invalid(`${label} must be between 0 and ${max}.`);
    return number;
}
function enumValue(value, values, label) {
    if (!Object.hasOwn(values, value)) throw invalid(`Unknown ${label}: ${value}.`);
    return values[value];
}
function color(value, maximumBrightness = 255) {
    return {h: integer(value.h ?? value.hue, 255, "Hue"), s: integer(value.s ?? value.sat, 255, "Saturation"), v: integer(value.v ?? value.val, maximumBrightness, "Brightness")};
}
function namedId(value, prefix) {
    const match = String(value).match(new RegExp(`^${prefix} (\\d+)$`));
    if (!match) throw invalid(`Choose a reported ${prefix.toLowerCase()}.`);
    return integer(match[1], 255, prefix);
}
// A group is its set of LEDs, so two groups never hold the same set: a row
// or a saved selection that matches an existing group reuses it.
const ledSet = leds => [...new Set(leds)].sort((left, right) => left - right).join(",");
const sameLeds = (left, right) => ledSet(left) === ledSet(right);
function groupFor(rgb, leds) {
    const found = rgb.groups.find(row => sameLeds(row.leds, leds));
    if (found) return found.id;
    rgb.groups.push({id: rgb.groups.length, leds});
    return rgb.groups.length - 1;
}
function groupRows(rgb, target) {
    const table = {layer: "layerGroupRows", pdMode: "pdModeGroupRows", combo: "comboGroupRows", keyBehavior: "keyGroupRows"}[target];
    if (!table) throw invalid("Unknown LED group target.");
    return rgb[table];
}
function existing(rows, predicate, label) {
    const row = rows.find(predicate);
    if (!row) throw invalid(`${label} is absent from the keyboard read.`);
    return row;
}

// Every catalog name and alias by its keycode value, built once: a pointing
// mode's shortcuts resolve through it on every save.
let keycodeValueCache;
const keycodeValues = () => {
    if (!keycodeValueCache) {
        keycodeValueCache = {};
        for (const entry of keycodes.entries()) for (const name of [entry.name, ...entry.aliases]) keycodeValueCache[name] = entry.value;
    }
    return keycodeValueCache;
};

function editDeviceProfile(bytes, message, context = {}) {
    if (PD_EDITS.has(message.type)) {
        if (!(context.capabilities?.supportedDomainMask & 16)) throw invalid("PD editing needs firmware with eight configurable slots.");
        const profile = decodeProfileBlob(bytes);
        if (profile.schema.major !== 2) throw invalid("Upgrade this profile before editing modes.");
        const domain = existing(profile.domains, row => row.id === 80, "Pointing modes");
        const slots = decodePdDomain(domain.payload), id = integer(message.slot, 7, "Slot");
        if (message.type === "clearPdMode") slots[id] = {id, kind: 0, name: ""};
        else if (message.type === "duplicatePdMode") {
            const source = integer(message.source, 7, "Source slot");
            if (slots[id].kind || !slots[source].kind || id === source) throw invalid("Choose a configured source and an empty destination slot.");
            slots[id] = {...slots[source], id};
        } else slots[id] = {...structuredClone(message.config), id};
        const keyValues = keycodeValues();
        const convertTap = tap => {
            if (!tap || typeof tap.keycode !== "string") return;
            const native = resolveNativeQmkExpression(tap.keycode, {qmkKeycodeValues: keyValues});
            if (native === undefined) throw invalid(`Unknown PD shortcut: ${tap.keycode}`);
            tap.keycode = native;
        };
        Object.values(slots[id].directions || {}).forEach(convertTap);
        Object.values(slots[id].diagonals || {}).forEach(convertTap);
        slots[id].buttons?.forEach(button => convertTap(button.tap));
        domain.payload = encodePdDomain(slots);
        return encodeProfileBlob(profile);
    }
    if (COMBO_EDITS.has(message.type)) return editCombos(bytes, message, context);
    if (BEHAVIOR_EDITS.has(message.type)) {
        const profile = decodeProfileBlob(bytes);
        const domain = existing(profile.domains, row => row.id === PROFILE_DOMAIN_IDS.KEY_BEHAVIORS, "Key behaviours");
        domain.payload = editKeyBehaviors(domain.payload, message, context.capabilities);
        return encodeProfileBlob(profile);
    }
    if (!RGB_EDITS.has(message.type)) throw invalid("Unsupported profile edit.");
    const profile = decodeProfileBlob(bytes);
    const domain = existing(profile.domains, row => row.id === PROFILE_DOMAIN_IDS.RGB, "RGB");
    const maximumBrightness = context.maximumBrightness ?? 255;
    const rgbOptions = {maximumBrightness};
    const rgb = decodeRgbDomainV1(domain.payload, rgbOptions);
    const editColor = (value) => color(value, maximumBrightness);
    switch (message.type) {
        case "updateLayerColor": {
            const row = existing(rgb.layerColors, row => row.layerId === namedId(message.layer, "Layer"), "Layer");
            row.color = editColor(message); row.mode = enumValue(message.mode, RGB_LAYER_MODES, "layer policy"); break;
        }
        case "updatePdModeColor": {
            const id = enumValue(message.pointingMode, RGB_PD_MODE_IDS, "pointing mode");
            const row = existing(rgb.pdModeColors, row => row.pdModeId === id, "Pointing mode");
            row.color = editColor(message); row.locality = enumValue(message.locality, RGB_LOCALITIES, "locality"); break;
        }
        case "updateAutomouseFade": rgb.automouseFade = {mode: enumValue(message.mode, RGB_AUTOMOUSE_MODES, "fade policy"), endColor: editColor(message)}; break;
        case "updateComboFeedback": rgb.comboFeedback = {color: editColor(message), locality: enumValue(message.locality, RGB_LOCALITIES, "locality")}; break;
        case "updateKeyBehaviorFeedback": {
            const feedback = message.config;
            rgb.keyFeedback = {tapBranchColors: feedback.tapBranchColors.map(editColor), tapCommittedColor: editColor(feedback.tapCommittedColor), holdActiveColor: editColor(feedback.holdActiveColor), longHoldActiveColor: editColor(feedback.longHoldActiveColor), tapCommitMode: enumValue(feedback.tapCommitMode, RGB_TAP_COMMIT_MODES, "tap policy"), locality: enumValue(feedback.locality, RGB_LOCALITIES, "locality")}; break;
        }
        case "updateRgbStages": rgb.stageEnableMask = integer(message.stageEnableMask, 31, "RGB stage mask"); break;
        case "saveRgbReusableLedGroup": {
            const group = message.group;
            const leds = group.ledIndices.map(value => integer(value, 57, "LED index"));
            if (group.originalName) {
                const target = existing(rgb.groups, row => row.id === namedId(group.originalName, "Group"), "LED group");
                const twin = rgb.groups.find(row => row !== target && sameLeds(row.leds, leds));
                if (twin) throw invalid(`Group ${twin.id} already holds exactly these LEDs.`);
                target.leds = leds;
            } else groupFor(rgb, leds);
            break;
        }
        case "deleteRgbReusableLedGroup": {
            const id = namedId(message.name, "Group");
            existing(rgb.groups, row => row.id === id, "LED group");
            const references = [rgb.layerGroupRows, rgb.pdModeGroupRows, rgb.comboGroupRows, rgb.keyGroupRows].flat();
            if (references.some(row => row.groupId === id)) throw invalid("Remove this group's RGB assignments before deleting it.");
            // Group ids are positions: consecutive from zero. Removing one
            // renumbers the ones after it, and every row follows its group.
            rgb.groups = rgb.groups.filter(row => row.id !== id);
            const renumbered = new Map(rgb.groups.map((row, index) => [row.id, index]));
            rgb.groups.forEach((row, index) => {row.id = index;});
            for (const row of references) row.groupId = renumbered.get(row.groupId);
            break;
        }
        case "addRgbLedGroup": {
            const group = message.group;
            let groupId;
            if (group.ledGroupName) groupId = existing(rgb.groups, row => row.id === namedId(group.ledGroupName, "Group"), "LED group").id;
            else groupId = groupFor(rgb, group.ledIndices.map(value => integer(value, 57, "LED index")));
            const row = {color: editColor(group), groupId};
            if (group.target === "layer") row.selector = group.owner === "RGB_LAYER_GROUP_ALL" ? 255 : namedId(group.owner, "Layer");
            if (group.target === "pdMode") row.selector = group.owner === "RGB_PD_MODE_GROUP_ALL" ? 255 : enumValue(group.owner, RGB_PD_MODE_IDS, "pointing mode");
            if (group.target === "keyBehavior") row.semantic = enumValue(group.owner, RGB_KEY_SEMANTICS, "key feedback type");
            groupRows(rgb, group.target).push(row); break;
        }
        case "deleteRgbLedGroup": {
            const rows = groupRows(rgb, message.target);
            const index = integer(message.index, rows.length - 1, "Assignment index");
            rows.splice(index, 1); break;
        }
    }
    domain.payload = encodeRgbDomainV1(rgb, rgbOptions);
    return encodeProfileBlob(profile);
}

function editCombos(bytes, message, context) {
    if (!(context.capabilities?.supportedDomainMask & 4)) throw invalid("This firmware can read combos but cannot save them. Flash the updated firmware pair first.");
    const read = context.combos;
    if (read?.state !== "read") throw invalid("Read the keyboard's combos before saving.");
    if (read.noTimer || read.customTrigger || read.customRelease || read.customRepress) throw invalid("This firmware has custom combo hooks or disabled timing that the profile editor cannot replace.");
    const profile = decodeProfileBlob(bytes);
    let domain = profile.domains.find(row => row.id === PROFILE_DOMAIN_IDS.COMBOS);
    const nativeAction = keycodeAction;
    const actionOptions = actionLimitsFor(profile.schema.major);
    const rows = domain ? decodeComboDomainV1(domain.payload, actionOptions) : read.rows.map(row => ({...row, output: nativeAction(row.output), inputs: row.inputs.map(nativeAction)}));
    const expression = value => {
        const name = String(value).trim();
        if (/^MO\(/.test(name)) return semanticActionForExpression(name, {});
        const native = keycodes.encode(name);
        if (native !== undefined) return nativeAction(native);
        if (!knownActionAbi(context.capabilities.actionAbiDigest)) throw invalid("Named custom actions require a matching keyboard action vocabulary.");
        return semanticActionForExpression(name, {layers: Array.from({length: read.layerReferences.length}, (_, id) => ({name: layerRef(id)}))});
    };
    if (message.type === "updateComboHoldTerm") {
        const holdTermMs = integer(message.holdTermMs, 65535, "Hold threshold");
        rows.forEach(row => {row.holdTermMs = holdTermMs;});
    } else if (message.type === "deleteCombo") {
        rows.splice(integer(message.id, rows.length - 1, "Combo index"), 1);
    } else {
        if (!Array.isArray(message.inputs)) throw invalid("Choose two to four combo input keys.");
        const row = {inputs: message.inputs.map(expression), output: expression(message.output), termMs: integer(message.termMs, 65535, "Combo window"), holdTermMs: rows[0]?.holdTermMs ?? integer(message.holdTermMs, 65535, "Hold threshold"), mustHold: message.mustHold === true, mustTap: message.mustTap === true, ordered: message.ordered === true};
        if (message.type === "saveCombo") rows[integer(message.id, rows.length - 1, "Combo index")] = row;
        else rows.push(row);
    }
    const payload = encodeComboDomainV1(rows, actionOptions);
    if (domain) domain.payload = payload;
    else profile.domains.push({id: PROFILE_DOMAIN_IDS.COMBOS, version: 1, payload});
    return encodeProfileBlob(profile);
}

function assertEffectiveCombos(bytes, read) {
    const profile = decodeProfileBlob(bytes);
    const domain = profile.domains.find(row => row.id === PROFILE_DOMAIN_IDS.COMBOS);
    if (!domain) return;
    if (read?.state !== "read") throw invalid("The profile was saved, but the running combos could not be verified. Read from keyboard before retrying.");
    const native = nativeCode;
    const expected = decodeComboDomainV1(domain.payload, actionLimitsFor(profile.schema.major)).map(row => ({...row, inputs: row.inputs.map(native), output: native(row.output)}));
    if (expected.length !== read.rows.length || expected.some((row, index) => { const actual = read.rows[index]; return ["id", "output", "termMs", "holdTermMs", "mustHold", "mustTap", "ordered"].some(key => row[key] !== actual[key]) || JSON.stringify(row.inputs) !== JSON.stringify(actual.inputs); })) throw invalid("The saved combo profile does not match the running combo table. Flash the current firmware pair and read from keyboard again.");
}

module.exports = {PD_EDITS, RGB_EDITS, COMBO_EDITS, editDeviceProfile, assertEffectiveCombos};
