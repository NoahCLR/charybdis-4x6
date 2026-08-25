"use strict";

const {PROFILE_DOMAIN_IDS} = require("./profile-blob-v1");

const RGB_DOMAIN_V1 = Object.freeze({
    DOMAIN_ID: PROFILE_DOMAIN_IDS.RGB,
    DOMAIN_VERSION: 1,
    FORMAT_VERSION: 1,
    HEADER_SIZE: 16,
    PHYSICAL_LED_COUNT: 58,
    LED_BITMAP_SIZE: 8,
    MAX_GROUPS: 16,
    MAX_LOGICAL_LAYERS: 8,
    MAX_PD_MODES: 6,
    MAX_STAGE_GROUP_ROWS: 32,
    MAX_TAP_BRANCH_COLORS: 4,
    MAX_PAYLOAD_SIZE: 4052,
    SELECTOR_ALL: 0xff,
});

const RGB_STAGE_BITS = Object.freeze({
    LAYER: 1 << 0,
    AUTOMOUSE: 1 << 1,
    PD_MODE: 1 << 2,
    COMBO: 1 << 3,
    KEY_BEHAVIOR: 1 << 4,
});
const RGB_STAGE_MASK_ALL = Object.values(RGB_STAGE_BITS).reduce((mask, bit) => mask | bit, 0);

const RGB_LAYER_MODES = Object.freeze({
    ALL_KEYS: 0,
    KEYS_MAPPED_ON_THIS_LAYER_ONLY: 1,
});
const RGB_LOCALITIES = Object.freeze({
    RGB_BOTH_HALVES: 0,
    RGB_LEFT_HALF: 1,
    RGB_RIGHT_HALF: 2,
    RGB_KEY_HALF: 3,
    RGB_KEYS_ONLY: 4,
});
const RGB_AUTOMOUSE_MODES = Object.freeze({
    FOLLOW_REAL_DESTINATION: 0,
    END_COLOR_WHERE_BASE_EFFECT_WOULD_SHOW: 1,
    END_COLOR_ON_ALL_KEYS: 2,
});
const RGB_TAP_COMMIT_MODES = Object.freeze({
    KEY_FEEDBACK_TAP_COMMIT_OFF: 0,
    KEY_FEEDBACK_TAP_COMMIT_NON_BASE_TAPS: 1,
});
const RGB_KEY_SEMANTICS = Object.freeze({
    KEY_FEEDBACK_GROUP_TAP_BRANCH_PENDING: 0,
    KEY_FEEDBACK_GROUP_TAP_COMMITTED: 1,
    KEY_FEEDBACK_GROUP_HOLD_ACTIVE: 2,
    KEY_FEEDBACK_GROUP_LONG_HOLD_ACTIVE: 3,
    KEY_FEEDBACK_GROUP_ALL: RGB_DOMAIN_V1.SELECTOR_ALL,
});
const RGB_PD_MODE_IDS = Object.freeze({
    PD_MODE_DRAGSCROLL: 0,
    PD_MODE_VOLUME: 1,
    PD_MODE_BRIGHTNESS: 2,
    PD_MODE_ZOOM: 3,
    PD_MODE_ARROW: 4,
    PD_MODE_PINCH: 5,
});

const BLACK = Object.freeze({h: 0, s: 0, v: 0});

class RgbDomainProtocolError extends Error {
    constructor(code, message, details = {}) {
        super(message);
        this.name = "RgbDomainProtocolError";
        this.code = code;
        Object.assign(this, details);
    }
}

function encodeRgbDomainV1(profile, options = {}) {
    const limits = normalizeOptions(options);
    const value = normalizeProfile(profile, limits);
    const header = Buffer.alloc(RGB_DOMAIN_V1.HEADER_SIZE);
    header[0] = RGB_DOMAIN_V1.FORMAT_VERSION;
    header[1] = 0;
    header.writeUInt16LE(value.stageEnableMask, 2);
    header[4] = value.groups.length;
    header[5] = value.layerColors.length;
    header[6] = value.layerGroupRows.length;
    header[7] = value.pdModeColors.length;
    header[8] = value.pdModeGroupRows.length;
    header[9] = value.comboGroupRows.length;
    header[10] = value.keyFeedback.tapBranchColors.length;
    header[11] = value.keyGroupRows.length;
    header[12] = RGB_DOMAIN_V1.PHYSICAL_LED_COUNT;
    header[13] = RGB_DOMAIN_V1.LED_BITMAP_SIZE;

    const chunks = [header];
    for (const group of value.groups) {
        chunks.push(Buffer.from([group.id]), ledsToBitmap(group.leds));
    }
    for (const row of value.layerColors) {
        chunks.push(Buffer.from([row.layerId, row.color.h, row.color.s, row.color.v, row.mode]));
    }
    for (const row of value.layerGroupRows) {
        chunks.push(Buffer.from([row.selector, row.color.h, row.color.s, row.color.v, row.groupId]));
    }
    chunks.push(Buffer.from([
        value.automouseFade.mode,
        value.automouseFade.endColor.h,
        value.automouseFade.endColor.s,
        value.automouseFade.endColor.v,
    ]));
    for (const row of value.pdModeColors) {
        chunks.push(Buffer.from([row.pdModeId, row.color.h, row.color.s, row.color.v, row.locality]));
    }
    for (const row of value.pdModeGroupRows) {
        chunks.push(Buffer.from([row.selector, row.color.h, row.color.s, row.color.v, row.groupId]));
    }
    chunks.push(Buffer.from([
        value.comboFeedback.color.h,
        value.comboFeedback.color.s,
        value.comboFeedback.color.v,
        value.comboFeedback.locality,
    ]));
    for (const row of value.comboGroupRows) {
        chunks.push(Buffer.from([row.color.h, row.color.s, row.color.v, row.groupId]));
    }
    for (const color of value.keyFeedback.tapBranchColors) {
        chunks.push(Buffer.from([color.h, color.s, color.v]));
    }
    chunks.push(Buffer.from([
        value.keyFeedback.tapCommittedColor.h,
        value.keyFeedback.tapCommittedColor.s,
        value.keyFeedback.tapCommittedColor.v,
        value.keyFeedback.holdActiveColor.h,
        value.keyFeedback.holdActiveColor.s,
        value.keyFeedback.holdActiveColor.v,
        value.keyFeedback.longHoldActiveColor.h,
        value.keyFeedback.longHoldActiveColor.s,
        value.keyFeedback.longHoldActiveColor.v,
        value.keyFeedback.tapCommitMode,
        value.keyFeedback.locality,
    ]));
    for (const row of value.keyGroupRows) {
        chunks.push(Buffer.from([row.semantic, row.color.h, row.color.s, row.color.v, row.groupId]));
    }

    const payload = Buffer.concat(chunks);
    if (payload.length > RGB_DOMAIN_V1.MAX_PAYLOAD_SIZE) {
        throw rgbError("CAPACITY_EXCEEDED", `RGB domain payload is ${payload.length} bytes; maximum is ${RGB_DOMAIN_V1.MAX_PAYLOAD_SIZE}.`);
    }
    return payload;
}

function decodeRgbDomainV1(value, options = {}) {
    const bytes = copyBytes(value, "RGB domain payload");
    const limits = normalizeOptions(options);
    if (bytes.length < RGB_DOMAIN_V1.HEADER_SIZE) {
        throw rgbError("TRUNCATED", `RGB domain needs a ${RGB_DOMAIN_V1.HEADER_SIZE}-byte header.`);
    }
    if (bytes.length > RGB_DOMAIN_V1.MAX_PAYLOAD_SIZE) {
        throw rgbError("CAPACITY_EXCEEDED", `RGB domain payload is ${bytes.length} bytes; maximum is ${RGB_DOMAIN_V1.MAX_PAYLOAD_SIZE}.`);
    }
    if (bytes[0] !== RGB_DOMAIN_V1.FORMAT_VERSION) {
        throw rgbError("INVALID_VERSION", `RGB payload format ${bytes[0]} is not supported.`);
    }
    if (bytes[1] !== 0 || bytes[14] !== 0 || bytes[15] !== 0) {
        throw rgbError("RESERVED_BITS", "RGB payload header reserved bytes must be zero.");
    }
    if (bytes[12] !== RGB_DOMAIN_V1.PHYSICAL_LED_COUNT || bytes[13] !== RGB_DOMAIN_V1.LED_BITMAP_SIZE) {
        throw rgbError("INCOMPATIBLE_GEOMETRY", `RGB payload geometry must be ${RGB_DOMAIN_V1.PHYSICAL_LED_COUNT} LEDs and ${RGB_DOMAIN_V1.LED_BITMAP_SIZE} bitmap bytes.`);
    }

    const counts = {
        groups: bytes[4],
        layerColors: bytes[5],
        layerGroups: bytes[6],
        pdColors: bytes[7],
        pdGroups: bytes[8],
        comboGroups: bytes[9],
        branchColors: bytes[10],
        keyGroups: bytes[11],
    };
    assertHeaderCounts(counts, limits);
    const reader = new Reader(bytes, RGB_DOMAIN_V1.HEADER_SIZE);
    const groups = [];
    const bitmapKeys = new Set();
    for (let index = 0; index < counts.groups; index += 1) {
        const id = reader.u8("groups", index, "id");
        const bitmap = reader.bytes(RGB_DOMAIN_V1.LED_BITMAP_SIZE, "groups", index, "bitmap");
        if (id !== index) {
            throw rgbError("NONCANONICAL_ORDER", `RGB group id ${id} appears at index ${index}; ids must be consecutive from zero.`, {table: "groups", row: index, field: "id"});
        }
        assertBitmap(bitmap, index);
        const key = bitmap.toString("hex");
        if (bitmapKeys.has(key)) {
            throw rgbError("DUPLICATE_BITMAP", `RGB group ${index} duplicates an earlier LED bitmap.`, {table: "groups", row: index, field: "bitmap"});
        }
        bitmapKeys.add(key);
        groups.push({id, leds: bitmapToLeds(bitmap)});
    }

    const layerColors = [];
    for (let index = 0; index < counts.layerColors; index += 1) {
        const layerId = reader.u8("layerColors", index, "layerId");
        if (layerId !== index) {
            throw rgbError("NONCANONICAL_ORDER", `Layer color id ${layerId} appears at index ${index}; complete layer colors must be consecutive from zero.`, {table: "layerColors", row: index, field: "layerId"});
        }
        layerColors.push({layerId, color: reader.hsv(limits, "layerColors", index, "color"), mode: reader.enumValue(1, "layerColors", index, "mode")});
    }
    const layerGroupRows = readGroupRows(reader, counts.layerGroups, counts.groups, (selector, row) => assertSelector(selector, counts.layerColors, "layerGroupRows", row), "layerGroupRows", limits);
    const automouseFade = {
        mode: reader.enumValue(2, "automouseFade", 0, "mode"),
        endColor: reader.hsv(limits, "automouseFade", 0, "endColor"),
    };

    const pdModeColors = [];
    let priorPdId = -1;
    for (let index = 0; index < counts.pdColors; index += 1) {
        const pdModeId = reader.u8("pdModeColors", index, "pdModeId");
        assertSupportedPdId(pdModeId, limits, "pdModeColors", index, "pdModeId");
        if (pdModeId <= priorPdId) {
            throw rgbError("NONCANONICAL_ORDER", "PD-mode color ids must be unique and strictly ascending.", {table: "pdModeColors", row: index, field: "pdModeId"});
        }
        pdModeColors.push({pdModeId, color: reader.hsv(limits, "pdModeColors", index, "color"), locality: reader.enumValue(4, "pdModeColors", index, "locality")});
        priorPdId = pdModeId;
    }
    const pdModeGroupRows = readGroupRows(reader, counts.pdGroups, counts.groups, (selector, row) => assertPdSelector(selector, limits, "pdModeGroupRows", row), "pdModeGroupRows", limits);
    const comboFeedback = {
        color: reader.hsv(limits, "comboFeedback", 0, "color"),
        locality: reader.enumValue(4, "comboFeedback", 0, "locality"),
    };
    const comboGroupRows = [];
    for (let index = 0; index < counts.comboGroups; index += 1) {
        comboGroupRows.push({
            color: reader.hsv(limits, "comboGroupRows", index, "color"),
            groupId: reader.groupId(counts.groups, "comboGroupRows", index),
        });
    }
    const tapBranchColors = [];
    for (let index = 0; index < counts.branchColors; index += 1) {
        tapBranchColors.push(reader.hsv(limits, "keyFeedback.tapBranchColors", index, "color"));
    }
    const keyFeedback = {
        tapBranchColors,
        tapCommittedColor: reader.hsv(limits, "keyFeedback", 0, "tapCommittedColor"),
        holdActiveColor: reader.hsv(limits, "keyFeedback", 0, "holdActiveColor"),
        longHoldActiveColor: reader.hsv(limits, "keyFeedback", 0, "longHoldActiveColor"),
        tapCommitMode: reader.enumValue(1, "keyFeedback", 0, "tapCommitMode"),
        locality: reader.enumValue(4, "keyFeedback", 0, "locality"),
    };
    const keyGroupRows = readGroupRows(reader, counts.keyGroups, counts.groups, (semantic, row) => assertKeySemantic(semantic, "keyGroupRows", row), "keyGroupRows", limits, "semantic");
    if (reader.offset !== bytes.length) {
        throw rgbError("TRAILING_BYTES", `RGB domain has ${bytes.length - reader.offset} trailing byte${bytes.length - reader.offset === 1 ? "" : "s"}.`);
    }

    const decoded = {
        formatVersion: bytes[0],
        stageEnableMask: bytes.readUInt16LE(2),
        groups,
        layerColors,
        layerGroupRows,
        automouseFade,
        pdModeColors,
        pdModeGroupRows,
        comboFeedback,
        comboGroupRows,
        keyFeedback,
        keyGroupRows,
    };
    const normalized = normalizeProfile(decoded, limits);
    if (!encodeRgbDomainV1(normalized, limits).equals(bytes)) {
        throw rgbError("NONCANONICAL", "RGB domain does not use its canonical representation.");
    }
    return normalized;
}

function createRgbDomainV1(profile, options = {}) {
    return {id: RGB_DOMAIN_V1.DOMAIN_ID, version: RGB_DOMAIN_V1.DOMAIN_VERSION, payload: encodeRgbDomainV1(profile, options)};
}

function canonicalizeStudioRgbDomainV1(model, options = {}) {
    if (!model || typeof model !== "object" || !model.rgb || typeof model.rgb !== "object") {
        throw new TypeError("Profile Studio model must contain an rgb object.");
    }
    const rgb = model.rgb;
    const layerIds = studioLayerIds(model, options);
    const constants = studioConstants(model, options);
    const maximumBrightness = options.maximumBrightness === undefined
        ? constantInteger(constants.RGB_MATRIX_MAXIMUM_BRIGHTNESS ?? 255, "RGB_MATRIX_MAXIMUM_BRIGHTNESS", constants)
        : assertU8(options.maximumBrightness, "maximumBrightness");
    const compiledStageMask = options.compiledStageMask === undefined
        ? inferStudioCompiledStageMask(model)
        : assertStageMask(options.compiledStageMask, RGB_STAGE_MASK_ALL, "compiledStageMask");
    const stageEnableMask = options.stageEnableMask === undefined
        ? compiledStageMask
        : assertStageMask(options.stageEnableMask, compiledStageMask, "stageEnableMask");
    const codecOptions = {
        compiledStageMask,
        logicalLayerCount: layerIds.size,
        maxLogicalLayers: options.maxLogicalLayers,
        maximumBrightness,
        tapBranchColorCount: options.tapBranchColorCount,
        supportedPdModeIds: options.supportedPdModeIds,
    };
    const groupContext = studioGroupContext(rgb);
    const profile = {
        stageEnableMask,
        groups: groupContext.groups,
        layerColors: (rgb.layerColors || []).map((row) => ({
            layerId: resolveNamedId(row.layer, layerIds, "layer color layer"),
            color: studioHsv(row.color, constants, "layer color"),
            mode: resolveNamedEnum(row.mode, RGB_LAYER_MODES, "layer color mode"),
        })),
        layerGroupRows: studioGroupRows(rgb.layerLedGroups, groupContext, constants, "layerGroupRows", (owner) => owner === "RGB_LAYER_GROUP_ALL" ? RGB_DOMAIN_V1.SELECTOR_ALL : resolveNamedId(owner, layerIds, "layer group selector")),
        automouseFade: rgb.automouseFade ? {
            mode: resolveNamedEnum(rgb.automouseFade.mode, RGB_AUTOMOUSE_MODES, "automouse fade mode"),
            endColor: studioHsv(rgb.automouseFade.end_color, constants, "automouse fade end color"),
        } : {mode: 0, endColor: {...BLACK}},
        pdModeColors: (rgb.pdModeColors || []).map((row) => ({
            pdModeId: resolveNamedEnum(row.pointingMode, RGB_PD_MODE_IDS, "PD-mode color id"),
            color: studioHsv(row.color, constants, "PD-mode color"),
            locality: resolveNamedEnum(row.locality, RGB_LOCALITIES, "PD-mode locality"),
        })),
        pdModeGroupRows: studioGroupRows(rgb.pdModeLedGroups, groupContext, constants, "pdModeGroupRows", (owner) => owner === "RGB_PD_MODE_GROUP_ALL" ? RGB_DOMAIN_V1.SELECTOR_ALL : resolveNamedEnum(owner, RGB_PD_MODE_IDS, "PD-mode group selector")),
        comboFeedback: rgb.comboFeedback ? {
            color: studioHsv(rgb.comboFeedback.color, constants, "combo feedback color"),
            locality: resolveNamedEnum(rgb.comboFeedback.locality, RGB_LOCALITIES, "combo feedback locality"),
        } : {color: {...BLACK}, locality: 0},
        comboGroupRows: studioGroupRows(rgb.comboFeedbackLedGroups, groupContext, constants, "comboGroupRows"),
        keyFeedback: studioKeyFeedback(rgb.keyBehaviorFeedback, constants),
        keyGroupRows: studioGroupRows(rgb.keyBehaviorFeedbackLedGroups, groupContext, constants, "keyGroupRows", (owner) => resolveNamedEnum(owner, RGB_KEY_SEMANTICS, "key-feedback group semantic"), "semantic"),
    };
    return {profile: normalizeProfile(profile, normalizeOptions(codecOptions)), codecOptions};
}

function encodeStudioRgbDomainV1(model, options = {}) {
    const canonical = canonicalizeStudioRgbDomainV1(model, options);
    return encodeRgbDomainV1(canonical.profile, canonical.codecOptions);
}

function createStudioRgbDomainV1(model, options = {}) {
    const canonical = canonicalizeStudioRgbDomainV1(model, options);
    return createRgbDomainV1(canonical.profile, canonical.codecOptions);
}

function normalizeProfile(profile, limits) {
    if (!profile || typeof profile !== "object") {
        throw new TypeError("RGB domain input must be an object.");
    }
    if (profile.formatVersion !== undefined && profile.formatVersion !== RGB_DOMAIN_V1.FORMAT_VERSION) {
        throw rgbError("INVALID_VERSION", `RGB payload format ${profile.formatVersion} is not supported.`);
    }
    const stageEnableMask = assertStageMask(profile.stageEnableMask, limits.compiledStageMask, "stageEnableMask");
    const groupState = normalizeGroups(requiredArray(profile.groups, "groups"));
    const groups = groupState.groups;
    const layerColors = requiredArray(profile.layerColors, "layerColors").map((row, index) => ({
        layerId: assertU8(row?.layerId, `layerColors[${index}].layerId`),
        color: normalizeHsv(row?.color, limits, "layerColors", index, "color"),
        mode: assertEnum(row?.mode, 1, "layerColors", index, "mode"),
    })).sort((left, right) => left.layerId - right.layerId);
    for (let index = 0; index < layerColors.length; index += 1) {
        if (layerColors[index].layerId !== index) {
            throw rgbError("INCOMPLETE_SURFACE", "Layer colors must contain each logical layer exactly once from id zero.", {table: "layerColors", row: index, field: "layerId"});
        }
    }
    if (layerColors.length > limits.maxLogicalLayers) {
        throw rgbError("CAPACITY_EXCEEDED", `Layer color count ${layerColors.length} exceeds the supported logical-layer ceiling.`, {table: "layerColors"});
    }
    if (limits.logicalLayerCount !== undefined && layerColors.length !== limits.logicalLayerCount) {
        throw rgbError("INCOMPLETE_SURFACE", `Layer colors must contain all ${limits.logicalLayerCount} logical layers.`, {table: "layerColors"});
    }
    const layerGroupRows = normalizeGroupRows(profile.layerGroupRows, groupState, limits, "layerGroupRows", (selector, row) => assertSelector(selector, layerColors.length, "layerGroupRows", row));
    const automouseFade = {
        mode: assertEnum(profile.automouseFade?.mode, 2, "automouseFade", 0, "mode"),
        endColor: normalizeHsv(profile.automouseFade?.endColor, limits, "automouseFade", 0, "endColor"),
    };
    const pdModeColors = requiredArray(profile.pdModeColors, "pdModeColors").map((row, index) => ({
        pdModeId: assertSupportedPdId(assertU8(row?.pdModeId, `pdModeColors[${index}].pdModeId`), limits, "pdModeColors", index, "pdModeId"),
        color: normalizeHsv(row?.color, limits, "pdModeColors", index, "color"),
        locality: assertEnum(row?.locality, 4, "pdModeColors", index, "locality"),
    })).sort((left, right) => left.pdModeId - right.pdModeId);
    assertUniqueIds(pdModeColors, "pdModeId", "pdModeColors");
    const pdModeGroupRows = normalizeGroupRows(profile.pdModeGroupRows, groupState, limits, "pdModeGroupRows", (selector, row) => assertPdSelector(selector, limits, "pdModeGroupRows", row));
    const comboFeedback = {
        color: normalizeHsv(profile.comboFeedback?.color, limits, "comboFeedback", 0, "color"),
        locality: assertEnum(profile.comboFeedback?.locality, 4, "comboFeedback", 0, "locality"),
    };
    const comboGroupRows = normalizeComboRows(profile.comboGroupRows, groupState, limits);
    const keyFeedback = normalizeKeyFeedback(profile.keyFeedback, limits);
    const keyGroupRows = normalizeGroupRows(profile.keyGroupRows, groupState, limits, "keyGroupRows", (semantic, row) => assertKeySemantic(semantic, "keyGroupRows", row), "semantic");
    const groupRowCount = layerGroupRows.length + pdModeGroupRows.length + comboGroupRows.length + keyGroupRows.length;
    if (groupRowCount > RGB_DOMAIN_V1.MAX_STAGE_GROUP_ROWS) {
        throw rgbError("CAPACITY_EXCEEDED", `RGB stage group rows total ${groupRowCount}; maximum is ${RGB_DOMAIN_V1.MAX_STAGE_GROUP_ROWS}.`);
    }
    const normalized = {
        formatVersion: RGB_DOMAIN_V1.FORMAT_VERSION,
        stageEnableMask,
        groups,
        layerColors,
        layerGroupRows,
        automouseFade,
        pdModeColors,
        pdModeGroupRows,
        comboFeedback,
        comboGroupRows,
        keyFeedback,
        keyGroupRows,
    };
    assertCompiledSurfaces(normalized, limits);
    return normalized;
}

function normalizeOptions(options = {}) {
    const maxLogicalLayers = options.maxLogicalLayers === undefined ? RGB_DOMAIN_V1.MAX_LOGICAL_LAYERS : assertU8(options.maxLogicalLayers, "maxLogicalLayers");
    if (maxLogicalLayers > RGB_DOMAIN_V1.MAX_LOGICAL_LAYERS) {
        throw new RangeError(`maxLogicalLayers cannot exceed ${RGB_DOMAIN_V1.MAX_LOGICAL_LAYERS}.`);
    }
    const logicalLayerCount = options.logicalLayerCount === undefined ? undefined : assertU8(options.logicalLayerCount, "logicalLayerCount");
    if (logicalLayerCount !== undefined && (logicalLayerCount === 0 || logicalLayerCount > maxLogicalLayers)) {
        throw new RangeError("logicalLayerCount must be between one and maxLogicalLayers.");
    }
    const supportedPdModeIds = options.supportedPdModeIds === undefined
        ? Object.values(RGB_PD_MODE_IDS)
        : Array.from(options.supportedPdModeIds, (value) => assertU8(value, "supportedPdModeIds entry"));
    supportedPdModeIds.sort((left, right) => left - right);
    if (supportedPdModeIds.length > RGB_DOMAIN_V1.MAX_PD_MODES || new Set(supportedPdModeIds).size !== supportedPdModeIds.length) {
        throw new RangeError(`supportedPdModeIds must contain at most ${RGB_DOMAIN_V1.MAX_PD_MODES} unique ids.`);
    }
    const knownPdModeIds = new Set(Object.values(RGB_PD_MODE_IDS));
    if (supportedPdModeIds.some((id) => !knownPdModeIds.has(id))) {
        throw new RangeError("supportedPdModeIds contains an id outside the Profile Wire v1 PD-mode registry.");
    }
    return {
        compiledStageMask: assertStageMask(options.compiledStageMask === undefined ? RGB_STAGE_MASK_ALL : options.compiledStageMask, RGB_STAGE_MASK_ALL, "compiledStageMask"),
        logicalLayerCount,
        maxLogicalLayers,
        maximumBrightness: options.maximumBrightness === undefined ? 0xff : assertU8(options.maximumBrightness, "maximumBrightness"),
        tapBranchColorCount: options.tapBranchColorCount === undefined ? RGB_DOMAIN_V1.MAX_TAP_BRANCH_COLORS : assertTapBranchColorCount(options.tapBranchColorCount),
        supportedPdModeIds,
        supportedPdModeSet: new Set(supportedPdModeIds),
    };
}

function normalizeGroups(input) {
    if (input.length > RGB_DOMAIN_V1.MAX_GROUPS) {
        throw rgbError("CAPACITY_EXCEEDED", `RGB group count ${input.length}; maximum is ${RGB_DOMAIN_V1.MAX_GROUPS}.`, {table: "groups"});
    }
    const groupsByInputId = input.map((group, index) => {
        const leds = normalizeLeds(group?.leds, "groups", index);
        return {
            inputId: assertU8(group?.id, `groups[${index}].id`),
            leds,
            bitmap: ledsToBitmap(leds),
        };
    }).sort((left, right) => left.inputId - right.inputId);
    const bitmapKeys = new Set();
    for (let index = 0; index < groupsByInputId.length; index += 1) {
        if (groupsByInputId[index].inputId !== index) {
            throw rgbError("NONCANONICAL_ID", "RGB group ids must be consecutive from zero.", {table: "groups", row: index, field: "id"});
        }
        const key = groupsByInputId[index].bitmap.toString("hex");
        if (bitmapKeys.has(key)) {
            throw rgbError("DUPLICATE_BITMAP", `RGB group ${index} duplicates an earlier LED bitmap.`, {table: "groups", row: index, field: "leds"});
        }
        bitmapKeys.add(key);
    }
    const canonical = [...groupsByInputId].sort((left, right) => Buffer.compare(left.bitmap, right.bitmap));
    const remappedIdByInputId = new Map();
    const groups = canonical.map((group, id) => {
        remappedIdByInputId.set(group.inputId, id);
        return {id, leds: group.leds};
    });
    return {groups, remappedIdByInputId};
}

function normalizeGroupRows(input, groupState, limits, table, selectorCheck, selectorField = "selector") {
    return requiredArray(input, table).map((row, index) => ({
        [selectorField]: selectorCheck(assertU8(row?.[selectorField], `${table}[${index}].${selectorField}`), index),
        color: normalizeHsv(row?.color, limits, table, index, "color"),
        groupId: remapGroupId(row?.groupId, groupState, table, index),
    }));
}

function normalizeComboRows(input, groupState, limits) {
    return requiredArray(input, "comboGroupRows").map((row, index) => ({
        color: normalizeHsv(row?.color, limits, "comboGroupRows", index, "color"),
        groupId: remapGroupId(row?.groupId, groupState, "comboGroupRows", index),
    }));
}

function normalizeKeyFeedback(value, limits) {
    const branches = requiredArray(value?.tapBranchColors, "keyFeedback.tapBranchColors");
    if (branches.length > RGB_DOMAIN_V1.MAX_TAP_BRANCH_COLORS) {
        throw rgbError("CAPACITY_EXCEEDED", `Key-feedback tap-branch color count ${branches.length}; maximum is ${RGB_DOMAIN_V1.MAX_TAP_BRANCH_COLORS}.`, {table: "keyFeedback.tapBranchColors"});
    }
    if ((limits.compiledStageMask & RGB_STAGE_BITS.KEY_BEHAVIOR) !== 0 && branches.length !== limits.tapBranchColorCount) {
        throw rgbError("INCOMPLETE_SURFACE", `Key-feedback tap-branch colors must contain exactly ${limits.tapBranchColorCount} compiled entries.`, {table: "keyFeedback.tapBranchColors"});
    }
    return {
        tapBranchColors: branches.map((color, index) => normalizeHsv(color, limits, "keyFeedback.tapBranchColors", index, "color")),
        tapCommittedColor: normalizeHsv(value?.tapCommittedColor, limits, "keyFeedback", 0, "tapCommittedColor"),
        holdActiveColor: normalizeHsv(value?.holdActiveColor, limits, "keyFeedback", 0, "holdActiveColor"),
        longHoldActiveColor: normalizeHsv(value?.longHoldActiveColor, limits, "keyFeedback", 0, "longHoldActiveColor"),
        tapCommitMode: assertEnum(value?.tapCommitMode, 1, "keyFeedback", 0, "tapCommitMode"),
        locality: assertEnum(value?.locality, 4, "keyFeedback", 0, "locality"),
    };
}

function assertCompiledSurfaces(profile, limits) {
    const compiled = limits.compiledStageMask;
    if ((compiled & RGB_STAGE_BITS.LAYER) !== 0) {
        if (!profile.layerColors.length) {
            throw rgbError("INCOMPLETE_SURFACE", "A compiled layer RGB stage requires at least one layer color.", {table: "layerColors"});
        }
    } else if (profile.layerColors.length || profile.layerGroupRows.length) {
        throw rgbError("UNSUPPORTED_STAGE_DATA", "Layer RGB data is present but the layer stage is not compiled.");
    }
    if ((compiled & RGB_STAGE_BITS.AUTOMOUSE) === 0 && (profile.automouseFade.mode !== 0 || !isBlack(profile.automouseFade.endColor))) {
        throw rgbError("UNSUPPORTED_STAGE_DATA", "Auto-mouse RGB data must be canonical zero when the stage is not compiled.");
    }
    if ((compiled & RGB_STAGE_BITS.PD_MODE) !== 0) {
        if (!sameNumbers(profile.pdModeColors.map((row) => row.pdModeId), limits.supportedPdModeIds)) {
            throw rgbError("INCOMPLETE_SURFACE", "PD-mode colors must contain each supported stable PD id exactly once.", {table: "pdModeColors"});
        }
    } else if (profile.pdModeColors.length || profile.pdModeGroupRows.length) {
        throw rgbError("UNSUPPORTED_STAGE_DATA", "PD-mode RGB data is present but the stage is not compiled.");
    }
    if ((compiled & RGB_STAGE_BITS.COMBO) === 0 && (profile.comboGroupRows.length || profile.comboFeedback.locality !== 0 || !isBlack(profile.comboFeedback.color))) {
        throw rgbError("UNSUPPORTED_STAGE_DATA", "Combo RGB data must be canonical zero when the stage is not compiled.");
    }
    if ((compiled & RGB_STAGE_BITS.KEY_BEHAVIOR) === 0 && (profile.keyGroupRows.length || profile.keyFeedback.tapBranchColors.length || profile.keyFeedback.tapCommitMode !== 0 || profile.keyFeedback.locality !== 0 || !isBlack(profile.keyFeedback.tapCommittedColor) || !isBlack(profile.keyFeedback.holdActiveColor) || !isBlack(profile.keyFeedback.longHoldActiveColor))) {
        throw rgbError("UNSUPPORTED_STAGE_DATA", "Key-feedback RGB data must be canonical zero when the stage is not compiled.");
    }
}

function assertHeaderCounts(counts, limits) {
    if (counts.groups > RGB_DOMAIN_V1.MAX_GROUPS || counts.layerColors > limits.maxLogicalLayers || counts.pdColors > RGB_DOMAIN_V1.MAX_PD_MODES || counts.branchColors > RGB_DOMAIN_V1.MAX_TAP_BRANCH_COLORS) {
        throw rgbError("CAPACITY_EXCEEDED", "RGB payload header count exceeds a fixed v1 capacity.");
    }
    if (counts.layerGroups + counts.pdGroups + counts.comboGroups + counts.keyGroups > RGB_DOMAIN_V1.MAX_STAGE_GROUP_ROWS) {
        throw rgbError("CAPACITY_EXCEEDED", `RGB stage group rows exceed ${RGB_DOMAIN_V1.MAX_STAGE_GROUP_ROWS}.`);
    }
}

function readGroupRows(reader, count, groupCount, selectorCheck, table, limits, selectorField = "selector") {
    const rows = [];
    for (let index = 0; index < count; index += 1) {
        const selector = selectorCheck(reader.u8(table, index, selectorField), index);
        rows.push({
            [selectorField]: selector,
            color: reader.hsv(limits, table, index, "color"),
            groupId: reader.groupId(groupCount, table, index),
        });
    }
    return rows;
}

class Reader {
    constructor(bytes, offset) {
        this.value = bytes;
        this.offset = offset;
    }

    ensure(length, table, row, field) {
        if (this.offset + length > this.value.length) {
            throw rgbError("TRUNCATED", `RGB ${table}[${row}].${field} is truncated.`, {table, row, field, offset: this.offset});
        }
    }

    u8(table, row, field) {
        this.ensure(1, table, row, field);
        return this.value[this.offset++];
    }

    bytes(length, table, row, field) {
        this.ensure(length, table, row, field);
        const result = Buffer.from(this.value.subarray(this.offset, this.offset + length));
        this.offset += length;
        return result;
    }

    hsv(limits, table, row, field) {
        const color = {h: this.u8(table, row, `${field}.h`), s: this.u8(table, row, `${field}.s`), v: this.u8(table, row, `${field}.v`)};
        return normalizeHsv(color, limits, table, row, field);
    }

    enumValue(maximum, table, row, field) {
        return assertEnum(this.u8(table, row, field), maximum, table, row, field);
    }

    groupId(groupCount, table, row) {
        return assertGroupId(this.u8(table, row, "groupId"), groupCount, table, row);
    }
}

function studioGroupContext(rgb) {
    const reusableByName = new Map();
    const sources = [];
    for (const [index, group] of requiredArray(rgb.ledGroups || [], "rgb.ledGroups").entries()) {
        const name = String(group?.name || "");
        if (!name || reusableByName.has(name)) {
            throw rgbError("DUPLICATE_ID", `Reusable RGB group name ${name || "<empty>"} is invalid or duplicated.`, {table: "rgb.ledGroups", row: index, field: "name"});
        }
        const leds = normalizeLeds(group.ledIndices, "rgb.ledGroups", index);
        reusableByName.set(name, leds);
        sources.push(leds);
    }
    const tables = [rgb.layerLedGroups, rgb.pdModeLedGroups, rgb.comboFeedbackLedGroups, rgb.keyBehaviorFeedbackLedGroups];
    for (const rows of tables) {
        for (const [index, row] of (rows || []).entries()) {
            sources.push(studioRowLeds(row, reusableByName, index));
        }
    }
    const unique = new Map();
    for (const leds of sources) {
        const bitmap = ledsToBitmap(leds);
        unique.set(bitmap.toString("hex"), {bitmap, leds: bitmapToLeds(bitmap)});
    }
    const ordered = Array.from(unique.values()).sort((left, right) => Buffer.compare(left.bitmap, right.bitmap));
    if (ordered.length > RGB_DOMAIN_V1.MAX_GROUPS) {
        throw rgbError("CAPACITY_EXCEEDED", `Studio RGB model uses ${ordered.length} unique LED groups; maximum is ${RGB_DOMAIN_V1.MAX_GROUPS}.`);
    }
    const groups = ordered.map((group, id) => ({id, leds: group.leds}));
    const idByBitmap = new Map(ordered.map((group, id) => [group.bitmap.toString("hex"), id]));
    return {groups, idByBitmap, reusableByName};
}

function studioGroupRows(rows, context, constants, table, ownerMapper, selectorField = "selector") {
    return (rows || []).map((row, index) => {
        const leds = studioRowLeds(row, context.reusableByName, index);
        const groupId = context.idByBitmap.get(ledsToBitmap(leds).toString("hex"));
        const result = {color: studioHsv(row?.color, constants, `${table}[${index}].color`), groupId};
        if (ownerMapper) {
            result[selectorField] = ownerMapper(String(row?.owner || ""));
        }
        return result;
    });
}

function studioRowLeds(row, reusableByName, index) {
    const expression = String(row?.ledGroup || "");
    if (reusableByName.has(expression)) {
        return reusableByName.get(expression);
    }
    if (!Array.isArray(row?.ledIndices)) {
        throw rgbError("INVALID_GROUP", `Studio LED group row ${index} has no parsed LED indices.`, {row: index, field: "ledIndices"});
    }
    if (expression && row?.ledGroupKind === "inline" && !/^RGB_LED_GROUP\s*\(/.test(expression)) {
        throw rgbError("INVALID_GROUP", `Studio LED group expression ${expression} is not a reusable or inline RGB group.`, {row: index, field: "ledGroup"});
    }
    return normalizeLeds(row.ledIndices, "Studio LED group rows", index);
}

function studioKeyFeedback(value, constants) {
    if (!value) {
        return {
            tapBranchColors: [],
            tapCommittedColor: {...BLACK},
            holdActiveColor: {...BLACK},
            longHoldActiveColor: {...BLACK},
            tapCommitMode: 0,
            locality: 0,
        };
    }
    return {
        tapBranchColors: (value.tapBranchColors || []).map((color, index) => studioHsv(color, constants, `key tap branch color ${index}`)),
        tapCommittedColor: studioHsv(value.tapCommittedColor, constants, "key tap committed color"),
        holdActiveColor: studioHsv(value.holdActiveColor, constants, "key hold color"),
        longHoldActiveColor: studioHsv(value.longHoldActiveColor, constants, "key long-hold color"),
        tapCommitMode: resolveNamedEnum(value.tapCommitMode, RGB_TAP_COMMIT_MODES, "key tap-commit mode"),
        locality: resolveNamedEnum(value.locality, RGB_LOCALITIES, "key-feedback locality"),
    };
}

function studioLayerIds(model, options) {
    if (options.layerIds) {
        return validatedLayerIds(Object.entries(options.layerIds).map(([name, id]) => [name, assertU8(id, `layerIds.${name}`)]));
    }
    return validatedLayerIds(requiredArray(model.layers, "model.layers").map((layer, index) => [String(layer?.name || ""), index]));
}

function validatedLayerIds(entries) {
    if (!entries.length || entries.length > RGB_DOMAIN_V1.MAX_LOGICAL_LAYERS) {
        throw rgbError("CAPACITY_EXCEEDED", `Studio layer identity table must contain 1..${RGB_DOMAIN_V1.MAX_LOGICAL_LAYERS} entries.`);
    }
    const names = new Set();
    const ids = new Set();
    for (const [name, id] of entries) {
        if (!name || names.has(name) || ids.has(id)) {
            throw rgbError("DUPLICATE_ID", "Studio layer names and ids must be nonempty and unique.");
        }
        names.add(name);
        ids.add(id);
    }
    for (let id = 0; id < entries.length; id += 1) {
        if (!ids.has(id)) throw rgbError("INCOMPLETE_SURFACE", "Studio layer ids must be consecutive from zero.");
    }
    return new Map(entries);
}

function studioConstants(model, options) {
    const values = {};
    for (const section of model.configDefaults || []) {
        for (const field of section?.fields || []) {
            if (field?.macro && field.value !== undefined) {
                values[field.macro] = field.value;
            }
        }
    }
    return {...values, ...(options.constants || {})};
}

function inferStudioCompiledStageMask(model) {
    const toggles = new Map();
    for (const section of model.configDefaults || []) {
        for (const field of section?.fields || []) {
            if (field?.macro) toggles.set(field.macro, field.enabled === true);
        }
    }
    const rgb = model.rgb || {};
    const enabled = (macro, fallback) => toggles.has(macro) ? toggles.get(macro) : fallback;
    let mask = (rgb.layerColors || []).length ? RGB_STAGE_BITS.LAYER : 0;
    if (enabled("RGB_AUTOMOUSE_GRADIENT_ENABLE", Boolean(rgb.automouseFade))) mask |= RGB_STAGE_BITS.AUTOMOUSE;
    if (enabled("RGB_PD_MODE_FEEDBACK_ENABLE", Boolean((rgb.pdModeColors || []).length))) mask |= RGB_STAGE_BITS.PD_MODE;
    if (enabled("RGB_COMBO_FEEDBACK_ENABLE", Boolean(rgb.comboFeedback))) mask |= RGB_STAGE_BITS.COMBO;
    if (enabled("RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE", Boolean(rgb.keyBehaviorFeedback))) mask |= RGB_STAGE_BITS.KEY_BEHAVIOR;
    return mask;
}

function studioHsv(value, constants, label) {
    if (!value || typeof value !== "object") {
        throw new TypeError(`${label} must be a parsed HSV object.`);
    }
    return {
        h: constantInteger(value.h, `${label}.h`, constants),
        s: constantInteger(value.s, `${label}.s`, constants),
        v: constantInteger(value.v, `${label}.v`, constants),
    };
}

function constantInteger(value, label, constants = {}, seen = new Set()) {
    if (typeof value === "number") return assertU8(value, label);
    const text = String(value ?? "").trim();
    if (/^(?:0[xX][0-9a-fA-F]+|[0-9]+)$/.test(text)) return assertU8(Number(text), label);
    if (Object.prototype.hasOwnProperty.call(constants, text)) {
        if (seen.has(text)) throw rgbError("UNRESOLVED_EXPRESSION", `${label} constant ${text} is cyclic.`, {field: label});
        seen.add(text);
        return constantInteger(constants[text], label, constants, seen);
    }
    throw rgbError("UNRESOLVED_EXPRESSION", `${label} expression ${text || "<empty>"} is not a known 8-bit constant.`, {field: label});
}

function resolveNamedId(value, mapping, label) {
    const name = String(value || "");
    if (!mapping.has(name)) throw rgbError("INVALID_ID", `${label} ${name || "<empty>"} is unknown.`, {field: label});
    return mapping.get(name);
}

function resolveNamedEnum(value, mapping, label) {
    const name = String(value || "");
    if (!Object.prototype.hasOwnProperty.call(mapping, name)) throw rgbError("INVALID_ENUM", `${label} ${name || "<empty>"} is unknown.`, {field: label});
    return mapping[name];
}

function normalizeHsv(value, limits, table, row, field) {
    if (!value || typeof value !== "object") throw new TypeError(`${table}[${row}].${field} must be an HSV object.`);
    const color = {h: assertU8(value.h, `${table}[${row}].${field}.h`), s: assertU8(value.s, `${table}[${row}].${field}.s`), v: assertU8(value.v, `${table}[${row}].${field}.v`)};
    if (color.v > limits.maximumBrightness) {
        throw rgbError("BRIGHTNESS_EXCEEDED", `${table}[${row}].${field}.v ${color.v} exceeds compiled maximum ${limits.maximumBrightness}.`, {table, row, field: `${field}.v`});
    }
    return color;
}

function normalizeLeds(value, table, row) {
    const leds = requiredArray(value, `${table}[${row}].leds`).map((led, index) => {
        const parsed = Number(led);
        if (!Number.isInteger(parsed) || parsed < 0 || parsed >= RGB_DOMAIN_V1.PHYSICAL_LED_COUNT) {
            throw rgbError("INVALID_LED", `${table}[${row}] LED ${led} at index ${index} is outside 0..${RGB_DOMAIN_V1.PHYSICAL_LED_COUNT - 1}.`, {table, row, field: "leds", index});
        }
        return parsed;
    });
    return Array.from(new Set(leds)).sort((left, right) => left - right);
}

function ledsToBitmap(value) {
    const leds = normalizeLeds(value, "LED bitmap", 0);
    const bitmap = Buffer.alloc(RGB_DOMAIN_V1.LED_BITMAP_SIZE);
    for (const led of leds) bitmap[led >> 3] |= 1 << (led & 7);
    return bitmap;
}

function bitmapToLeds(value) {
    const bitmap = copyBytes(value, "LED bitmap");
    if (bitmap.length !== RGB_DOMAIN_V1.LED_BITMAP_SIZE) throw new RangeError(`LED bitmap must be ${RGB_DOMAIN_V1.LED_BITMAP_SIZE} bytes.`);
    const leds = [];
    for (let led = 0; led < RGB_DOMAIN_V1.PHYSICAL_LED_COUNT; led += 1) {
        if ((bitmap[led >> 3] & (1 << (led & 7))) !== 0) leds.push(led);
    }
    return leds;
}

function assertBitmap(bitmap, row) {
    if ((bitmap[7] & 0xfc) !== 0) {
        throw rgbError("RESERVED_BITS", "RGB LED bitmap bits 58 through 63 must be zero.", {table: "groups", row, field: "bitmap"});
    }
}

function assertSelector(value, count, table, row) {
    if (value !== RGB_DOMAIN_V1.SELECTOR_ALL && value >= count) throw rgbError("INVALID_SELECTOR", `${table}[${row}] selector ${value} does not reference a layer.`, {table, row, field: "selector"});
    return value;
}

function assertPdSelector(value, limits, table, row) {
    if (value !== RGB_DOMAIN_V1.SELECTOR_ALL) assertSupportedPdId(value, limits, table, row, "selector");
    return value;
}

function assertKeySemantic(value, table, row) {
    if (value !== RGB_DOMAIN_V1.SELECTOR_ALL && value > RGB_KEY_SEMANTICS.KEY_FEEDBACK_GROUP_LONG_HOLD_ACTIVE) throw rgbError("INVALID_SELECTOR", `${table}[${row}] semantic ${value} is unknown.`, {table, row, field: "semantic"});
    return value;
}

function assertSupportedPdId(value, limits, table, row, field) {
    if (!limits.supportedPdModeSet.has(value)) throw rgbError("INVALID_ID", `${table}[${row}].${field} PD id ${value} is not supported.`, {table, row, field});
    return value;
}

function assertGroupId(value, count, table, row) {
    const groupId = assertU8(value, `${table}[${row}].groupId`);
    if (groupId >= count) throw rgbError("INVALID_REFERENCE", `${table}[${row}] references RGB group ${groupId}; dictionary size is ${count}.`, {table, row, field: "groupId"});
    return groupId;
}

function remapGroupId(value, groupState, table, row) {
    const inputId = assertU8(value, `${table}[${row}].groupId`);
    if (!groupState.remappedIdByInputId.has(inputId)) {
        throw rgbError("INVALID_REFERENCE", `${table}[${row}] references RGB group ${inputId}; dictionary size is ${groupState.groups.length}.`, {table, row, field: "groupId"});
    }
    return groupState.remappedIdByInputId.get(inputId);
}

function assertEnum(value, maximum, table, row, field) {
    const parsed = assertU8(value, `${table}[${row}].${field}`);
    if (parsed > maximum) throw rgbError("INVALID_ENUM", `${table}[${row}].${field} value ${parsed} exceeds ${maximum}.`, {table, row, field});
    return parsed;
}

function assertUniqueIds(rows, field, table) {
    for (let index = 1; index < rows.length; index += 1) {
        if (rows[index - 1][field] === rows[index][field]) throw rgbError("DUPLICATE_ID", `${table} contains duplicate ${field} ${rows[index][field]}.`, {table, row: index, field});
    }
}

function assertStageMask(value, allowed, label) {
    const mask = assertU16(value, label);
    if ((mask & ~RGB_STAGE_MASK_ALL) !== 0) throw rgbError("RESERVED_BITS", `${label} contains unknown RGB stage bits.`, {field: label});
    if ((mask & ~allowed) !== 0) throw rgbError("UNSUPPORTED_STAGE", `${label} enables a stage that is not compiled.`, {field: label});
    return mask;
}

function requiredArray(value, label) {
    if (!Array.isArray(value)) throw new TypeError(`${label} must be an array.`);
    return value;
}

function assertU8(value, label) {
    const number = Number(value);
    if (!Number.isInteger(number) || number < 0 || number > 0xff) throw new RangeError(`${label} must be an 8-bit integer.`);
    return number;
}

function assertU16(value, label) {
    const number = Number(value);
    if (!Number.isInteger(number) || number < 0 || number > 0xffff) throw new RangeError(`${label} must be a 16-bit integer.`);
    return number;
}

function assertTapBranchColorCount(value) {
    const count = assertU8(value, "tapBranchColorCount");
    if (count === 0 || count > RGB_DOMAIN_V1.MAX_TAP_BRANCH_COLORS) {
        throw new RangeError(`tapBranchColorCount must be between one and ${RGB_DOMAIN_V1.MAX_TAP_BRANCH_COLORS}.`);
    }
    return count;
}

function sameNumbers(left, right) {
    return left.length === right.length && left.every((value, index) => value === right[index]);
}

function isBlack(color) {
    return color.h === 0 && color.s === 0 && color.v === 0;
}

function copyBytes(value, label) {
    if (!(value instanceof Uint8Array)) throw new TypeError(`${label} must be a Buffer or Uint8Array.`);
    return Buffer.from(value.buffer, value.byteOffset, value.byteLength);
}

function rgbError(code, message, details = {}) {
    return new RgbDomainProtocolError(code, message, details);
}

module.exports = {
    RGB_AUTOMOUSE_MODES,
    RGB_DOMAIN_V1,
    RGB_KEY_SEMANTICS,
    RGB_LAYER_MODES,
    RGB_LOCALITIES,
    RGB_PD_MODE_IDS,
    RGB_STAGE_BITS,
    RGB_STAGE_MASK_ALL,
    RGB_TAP_COMMIT_MODES,
    RgbDomainProtocolError,
    bitmapToLeds,
    canonicalizeStudioRgbDomainV1,
    createRgbDomainV1,
    createStudioRgbDomainV1,
    decodeRgbDomainV1,
    encodeRgbDomainV1,
    encodeStudioRgbDomainV1,
    ledsToBitmap,
};
