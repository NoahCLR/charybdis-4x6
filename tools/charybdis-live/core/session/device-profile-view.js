"use strict";

// Presentation of validated device domains. Names here describe wire IDs;
// configuration values and membership always come from the received domain.
const {actionName, knownActionAbi, layerRef} = require("../schema/actions");
const {VOCABULARY, branchName} = require("../model/vocabulary");
const keycodes = require("../data/keycode-catalog");
const {PROFILE_ACTION_KINDS: ACTION} = require("../schema/profile-blob-v1");
const {KEY_BEHAVIOR_HOLD_MODES} = require("../schema/key-behavior-domain-v1");
const {resolveNativeQmkExpression} = require("../schema/compiled-profile-v1");
const {
    RGB_AUTOMOUSE_MODES, RGB_DOMAIN_V1, RGB_KEY_SEMANTICS, RGB_LAYER_MODES,
    RGB_LOCALITIES, RGB_PD_MODE_IDS, RGB_STAGE_BITS, RGB_TAP_COMMIT_MODES,
} = require("../schema/rgb-domain-v1");

function enumName(values, value) {
    const entry = Object.entries(values).find(([, id]) => id === value);
    if (!entry) throw new Error(`Unsupported device enum value ${value}.`);
    return entry[0];
}

function behaviorRowsForView(domain) {
    const hold = (branch) => branch && ({
        helper: enumName(KEY_BEHAVIOR_HOLD_MODES, branch.mode),
        action: actionName(branch.action),
        repeatHz: String(branch.repeatHz),
    });
    return domain.rows.map((row) => ({
        keycode: actionName(row.target),
        tapHoldTerm: String(row.tapHoldTerm),
        longerHoldTerm: String(row.longerHoldTerm),
        multiTapTerm: String(row.multiTapTerm),
        keepsAutoMouseAnchored: row.keepsAutoMouseAnchored,
        steps: row.steps.map((step) => ({
            tapCount: step.tapIndex,
            tapCountName: branchName(step.tapIndex + 1),
            ...(step.tap ? {tap: {helper: "TAP_SENDS", action: actionName(step.tap)}} : {}),
            ...(step.hold ? {hold: hold(step.hold)} : {}),
            ...(step.longHold ? {longHold: hold(step.longHold)} : {}),
        })),
    }));
}

function behaviorAliasesForView(domain, capabilities) {
    if (!knownActionAbi(capabilities?.actionAbiDigest)) return {};
    const aliases = {};
    for (const row of domain.rows) {
        if (row.target.kind === ACTION.QMK_KEYCODE) continue;
        const name = actionName(row.target);
        const value = resolveNativeQmkExpression(name, {});
        if (value !== undefined) aliases[keycodes.resolve(value).name] = name;
    }
    return aliases;
}

const colorForView = (color) => ({h: String(color.h), s: String(color.s), v: String(color.v)});
const layerName = layerRef;

function rgbForView(domain) {
    const ledGroups = domain.groups.map((group) => ({
        name: `Group ${group.id}`,
        id: group.id,
        ledIndices: [...group.leds],
        expression: `LEDs ${group.leds.join(", ")}`,
        usageCount: 0,
        usages: [],
    }));
    const groupRows = (rows, target, ownerFor) => rows.map((row, index) => {
        const group = ledGroups.find((entry) => entry.id === row.groupId);
        if (!group) throw new Error(`Device LED group ${row.groupId} is missing.`);
        const owner = ownerFor ? ownerFor(row) : "";
        group.usageCount += 1;
        group.usages.push({target, owner});
        return {
            index, owner, color: colorForView(row.color), ledGroup: group.name,
            ledGroupKind: "reusable", ledIndices: [...group.ledIndices],
        };
    });
    const all = RGB_DOMAIN_V1.SELECTOR_ALL;
    return {
        stageEnableMask: domain.stageEnableMask,
        // Stages by id and bit, in the vocabulary's words; the interface finds
        // a stage by its id, never by what it is called.
        stages: VOCABULARY.stages.filter((stage) => stage.bit).map(({id, label, bit}) => ({id, label, bit, enabled: Boolean(domain.stageEnableMask & bit)})),
        ledGroups,
        layerColors: domain.layerColors.map((row) => ({
            layer: layerName(row.layerId), layerId: row.layerId,
            color: colorForView(row.color), mode: enumName(RGB_LAYER_MODES, row.mode),
        })),
        layerLedGroups: groupRows(domain.layerGroupRows, "layer", (row) => row.selector === all ? "RGB_LAYER_GROUP_ALL" : layerName(row.selector)),
        automouseFade: {mode: enumName(RGB_AUTOMOUSE_MODES, domain.automouseFade.mode), end_color: colorForView(domain.automouseFade.endColor)},
        pdModeColors: domain.pdModeColors.map((row) => ({
            pointingMode: enumName(RGB_PD_MODE_IDS, row.pdModeId), color: colorForView(row.color), locality: enumName(RGB_LOCALITIES, row.locality),
        })),
        pdModeLedGroups: groupRows(domain.pdModeGroupRows, "pdMode", (row) => row.selector === all ? "RGB_PD_MODE_GROUP_ALL" : enumName(RGB_PD_MODE_IDS, row.selector)),
        comboFeedback: {color: colorForView(domain.comboFeedback.color), locality: enumName(RGB_LOCALITIES, domain.comboFeedback.locality)},
        comboFeedbackLedGroups: groupRows(domain.comboGroupRows, "combo"),
        keyBehaviorFeedback: {
            tapBranchColors: domain.keyFeedback.tapBranchColors.map(colorForView),
            tapCommittedColor: colorForView(domain.keyFeedback.tapCommittedColor),
            holdActiveColor: colorForView(domain.keyFeedback.holdActiveColor),
            longHoldActiveColor: colorForView(domain.keyFeedback.longHoldActiveColor),
            tapCommitMode: enumName(RGB_TAP_COMMIT_MODES, domain.keyFeedback.tapCommitMode),
            locality: enumName(RGB_LOCALITIES, domain.keyFeedback.locality),
        },
        keyBehaviorFeedbackLedGroups: groupRows(domain.keyGroupRows, "keyBehavior", (row) => enumName(RGB_KEY_SEMANTICS, row.semantic)),
    };
}

function baseRgbForView(read, maximumBrightness) {
    if (read?.state !== "read") return {state: read?.state || "unread", message: read?.error?.message || "Base RGB has not been read from the keyboard."};
    const {effectId, hue, saturation, brightness, speed, readAt} = read;
    const enabled = effectId !== 0;
    const previewBrightness = Number.isInteger(maximumBrightness)
        ? Math.round(brightness * maximumBrightness / 255)
        : brightness;
    return {
        state: "read", enabled, effectId, hue, saturation, brightness, speed, readAt,
        effectName: !enabled ? "Off" : effectId === 1 ? "Solid colour" : `Effect ${effectId}`,
        brightnessPercent: Math.round(brightness * 100 / 255),
        // VIA reports brightness as 0..255 relative to the compiled limit,
        // while profile colours store their absolute HSV value. Put the base
        // effect back in that absolute domain before comparing or composing it
        // with layer colours. Keep `brightness` above verbatim for readback UI.
        previewColor: !enabled ? {h: "0", s: "0", v: "0"} : effectId === 1
            ? {h: String(hue), s: String(saturation), v: String(previewBrightness)} : undefined,
    };
}

function combosForView(read, labels) {
    if (read?.state !== "read") return [];
    const resolve = value => {
        const key = keycodes.resolve(value);
        return {name: key.name, label: labels[key.name] || key.label};
    };
    return read.rows.map(row => ({
        id: row.id, badge: `C${row.id + 1}`,
        inputs: row.inputs.map(value => resolve(value).name),
        inputDisplays: row.inputs.map(value => resolve(value).label),
        output: resolve(row.output).name,
        outputDisplay: row.output === 0 ? "Firmware callback" : resolve(row.output).label,
        termMs: row.termMs, holdTermMs: row.holdTermMs, mustHold: row.mustHold, mustTap: row.mustTap, ordered: row.ordered,
    }));
}

module.exports = {baseRgbForView, behaviorAliasesForView, behaviorRowsForView, combosForView, rgbForView};
