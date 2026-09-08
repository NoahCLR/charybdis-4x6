"use strict";
const {decodeProfileBlob, encodeProfileBlob, PROFILE_DOMAIN_IDS} = require("../schema/profile-blob-v1");
const {decodeRgbDomainV1, encodeRgbDomainV1, RGB_LAYER_MODES, RGB_LOCALITIES, RGB_AUTOMOUSE_MODES, RGB_TAP_COMMIT_MODES, RGB_PD_MODE_IDS, RGB_KEY_SEMANTICS} = require("../schema/rgb-domain-v1");

const keycodes = require("../data/keycode-catalog");
const {semanticActionForExpression, resolveNativeQmkExpression} = require("../schema/compiled-profile-v1");
const {encodeComboDomainV1, decodeComboDomainV1} = require("../schema/combo-domain-v1");
const {actionName, knownActionAbi} = require("./device-profile-view");
const {BEHAVIOR_EDITS, editKeyBehaviors} = require("./key-behavior-edits");
const COMBO_EDITS = new Set(["addCombo", "saveCombo", "deleteCombo", "updateComboHoldTerm"]);

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
function color(value) {
    return {h: integer(value.h ?? value.hue, 255, "Hue"), s: integer(value.s ?? value.sat, 255, "Saturation"), v: integer(value.v ?? value.val, 255, "Brightness")};
}
function namedId(value, prefix) {
    const match = String(value).match(new RegExp(`^${prefix} (\\d+)$`));
    if (!match) throw invalid(`Choose a reported ${prefix.toLowerCase()}.`);
    return integer(match[1], 255, prefix);
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

function editDeviceProfile(bytes, message, context = {}) {
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
    const rgb = decodeRgbDomainV1(domain.payload);
    switch (message.type) {
        case "updateLayerColor": {
            const row = existing(rgb.layerColors, row => row.layerId === namedId(message.layer, "Layer"), "Layer");
            row.color = color(message); row.mode = enumValue(message.mode, RGB_LAYER_MODES, "layer policy"); break;
        }
        case "updatePdModeColor": {
            const id = enumValue(message.pointingMode, RGB_PD_MODE_IDS, "pointing mode");
            const row = existing(rgb.pdModeColors, row => row.pdModeId === id, "Pointing mode");
            row.color = color(message); row.locality = enumValue(message.locality, RGB_LOCALITIES, "locality"); break;
        }
        case "updateAutomouseFade": rgb.automouseFade = {mode: enumValue(message.mode, RGB_AUTOMOUSE_MODES, "fade policy"), endColor: color(message)}; break;
        case "updateComboFeedback": rgb.comboFeedback = {color: color(message), locality: enumValue(message.locality, RGB_LOCALITIES, "locality")}; break;
        case "updateKeyBehaviorFeedback": {
            const feedback = message.config;
            rgb.keyFeedback = {tapBranchColors: feedback.tapBranchColors.map(color), tapCommittedColor: color(feedback.tapCommittedColor), holdActiveColor: color(feedback.holdActiveColor), longHoldActiveColor: color(feedback.longHoldActiveColor), tapCommitMode: enumValue(feedback.tapCommitMode, RGB_TAP_COMMIT_MODES, "tap policy"), locality: enumValue(feedback.locality, RGB_LOCALITIES, "locality")}; break;
        }
        case "updateRgbStages": rgb.stageEnableMask = integer(message.stageEnableMask, 31, "RGB stage mask"); break;
        case "saveRgbReusableLedGroup": {
            const group = message.group;
            const leds = group.ledIndices.map(value => integer(value, 57, "LED index"));
            if (group.originalName) existing(rgb.groups, row => row.id === namedId(group.originalName, "Group"), "LED group").leds = leds;
            else rgb.groups.push({id: rgb.groups.length, leds});
            break;
        }
        case "deleteRgbReusableLedGroup": {
            const id = namedId(message.name, "Group");
            existing(rgb.groups, row => row.id === id, "LED group");
            const references = [rgb.layerGroupRows, rgb.pdModeGroupRows, rgb.comboGroupRows, rgb.keyGroupRows].flat();
            if (references.some(row => row.groupId === id)) throw invalid("Remove this group's RGB assignments before deleting it.");
            rgb.groups = rgb.groups.filter(row => row.id !== id);
            break;
        }
        case "addRgbLedGroup": {
            const group = message.group;
            let groupId;
            if (group.ledGroupName) groupId = existing(rgb.groups, row => row.id === namedId(group.ledGroupName, "Group"), "LED group").id;
            else {
                groupId = rgb.groups.length;
                rgb.groups.push({id: groupId, leds: group.ledIndices.map(value => integer(value, 57, "LED index"))});
            }
            const row = {color: color(group), groupId};
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
    domain.payload = encodeRgbDomainV1(rgb);
    return encodeProfileBlob(profile);
}

function editCombos(bytes, message, context) {
    if (!(context.capabilities?.supportedDomainMask & 4)) throw invalid("This firmware can read combos but cannot save them. Flash the updated firmware pair first.");
    const read = context.combos;
    if (read?.state !== "read") throw invalid("Read the keyboard's combos before saving.");
    if (read.noTimer || read.customTrigger || read.customRelease || read.customRepress) throw invalid("This firmware has custom combo hooks or disabled timing that the profile editor cannot replace.");
    const profile = decodeProfileBlob(bytes);
    let domain = profile.domains.find(row => row.id === PROFILE_DOMAIN_IDS.COMBOS);
    const nativeAction = operand => ({kind: 1, operand});
    const rows = domain ? decodeComboDomainV1(domain.payload) : read.rows.map(row => ({...row, output: nativeAction(row.output), inputs: row.inputs.map(nativeAction)}));
    const expression = value => {
        const name = String(value).trim();
        if (/^MO\(/.test(name)) return semanticActionForExpression(name, {});
        const native = keycodes.encode(name);
        if (native !== undefined) return nativeAction(native);
        if (!knownActionAbi(context.capabilities.actionAbiDigest)) throw invalid("Named custom actions require a matching keyboard action vocabulary.");
        return semanticActionForExpression(name, {layers: Array.from({length: read.layerReferences.length}, (_, id) => ({name: `Layer ${id}`}))});
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
    const payload = encodeComboDomainV1(rows);
    if (domain) domain.payload = payload;
    else profile.domains.push({id: PROFILE_DOMAIN_IDS.COMBOS, version: 1, payload});
    return encodeProfileBlob(profile);
}

function assertEffectiveCombos(bytes, read) {
    const domain = decodeProfileBlob(bytes).domains.find(row => row.id === PROFILE_DOMAIN_IDS.COMBOS);
    if (!domain) return;
    if (read?.state !== "read") throw invalid("The profile was saved, but the running combos could not be verified. Read from keyboard before retrying.");
    const native = action => action.kind === 1 ? action.operand : resolveNativeQmkExpression(actionName(action), {});
    const expected = decodeComboDomainV1(domain.payload).map(row => ({...row, inputs: row.inputs.map(native), output: native(row.output)}));
    if (expected.length !== read.rows.length || expected.some((row, index) => { const actual = read.rows[index]; return ["id", "output", "termMs", "holdTermMs", "mustHold", "mustTap", "ordered"].some(key => row[key] !== actual[key]) || JSON.stringify(row.inputs) !== JSON.stringify(actual.inputs); })) throw invalid("The saved combo profile does not match the running combo table. Flash the current firmware pair and read from keyboard again.");
}

module.exports = {RGB_EDITS, COMBO_EDITS, editDeviceProfile, assertEffectiveCombos};
