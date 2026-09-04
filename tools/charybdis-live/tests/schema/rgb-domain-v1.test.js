"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");

const {
    RGB_DOMAIN_V1,
    RGB_STAGE_BITS,
    canonicalizeStudioRgbDomainV1,
    createRgbDomainV1,
    createStudioRgbDomainV1,
    decodeRgbDomainV1,
    encodeRgbDomainV1,
    encodeStudioRgbDomainV1,
} = require("../../core/schema/rgb-domain-v1");
const {
    PROFILE_DOMAIN_IDS,
    crc32,
    decodeProfileBlob,
    encodeProfileBlob,
    fnv1a32,
} = require("../../core/schema/profile-blob-v1");

function fixture() {
    const semantic = JSON.parse(fs.readFileSync(path.join(__dirname, "../fixtures/rgb-domain-v1.json"), "utf8"));
    const vectors = Object.fromEntries(fs.readFileSync(path.resolve(__dirname, "../../../../tests/fixtures/rgb_domain_v1.fixture"), "utf8")
        .split(/\r?\n/u)
        .filter((line) => line && !line.startsWith("#"))
        .map((line) => {
            const separator = line.indexOf("=");
            return [line.slice(0, separator), line.slice(separator + 1)];
        }));
    return {
        ...semantic,
        codecOptions: {
            compiledStageMask: Number(vectors["codec.compiled_stage_mask"]),
            logicalLayerCount: Number(vectors["codec.logical_layer_count"]),
            maximumBrightness: Number(vectors["codec.maximum_brightness"]),
            tapBranchColorCount: Number(vectors["codec.tap_branch_color_count"]),
            supportedPdModeIds: Array.from({length: 6}, (unused, id) => id)
                .filter((id) => (Number(vectors["codec.supported_pd_mode_mask"]) & (1 << id)) !== 0),
        },
        payloadHex: vectors["payload.hex"],
        blobHex: vectors["blob.hex"],
        fnv1a32: vectors["blob.fnv1a32"],
        crc32: vectors["blob.crc32"],
    };
}

function clone(value) {
    return JSON.parse(JSON.stringify(value));
}

function assertCode(code, callback) {
    assert.throws(callback, (error) => error?.code === code, `expected RGB protocol error ${code}`);
}

function remapGroupIds(profile, mapping) {
    for (const group of profile.groups) group.id = mapping[group.id];
    for (const table of [profile.layerGroupRows, profile.pdModeGroupRows, profile.comboGroupRows, profile.keyGroupRows]) {
        for (const row of table) row.groupId = mapping[row.groupId];
    }
}

test("representative RGB payload and whole-profile blob match the shared C/JavaScript golden vectors", () => {
    const golden = fixture();
    const payload = encodeRgbDomainV1(golden.profile, golden.codecOptions);
    assert.equal(payload.toString("hex"), golden.payloadHex);
    assert.deepEqual(decodeRgbDomainV1(payload, golden.codecOptions), golden.profile);

    const blob = encodeProfileBlob({domains: [createRgbDomainV1(golden.profile, golden.codecOptions)]});
    assert.equal(blob.toString("hex"), golden.blobHex);
    assert.equal(fnv1a32(blob), Number.parseInt(golden.fnv1a32, 16));
    assert.equal(crc32(blob), Number.parseInt(golden.crc32, 16));
    const decoded = decodeProfileBlob(blob);
    assert.equal(decoded.domains.length, 1);
    assert.equal(decoded.domains[0].id, PROFILE_DOMAIN_IDS.RGB);
    assert.equal(decoded.domains[0].version, RGB_DOMAIN_V1.DOMAIN_VERSION);
    assert.deepEqual(decodeRgbDomainV1(decoded.domains[0].payload, golden.codecOptions), golden.profile);
});

test("Profile Studio parsed RGB model canonicalizes to the same exact domain and blob", () => {
    const golden = fixture();
    const canonical = canonicalizeStudioRgbDomainV1(golden.studioModel);
    assert.deepEqual(canonical.profile, golden.profile);
    assert.equal(encodeStudioRgbDomainV1(golden.studioModel).toString("hex"), golden.payloadHex);
    const blob = encodeProfileBlob({domains: [createStudioRgbDomainV1(golden.studioModel)]});
    assert.equal(blob.toString("hex"), golden.blobHex);
});

test("identity tables and LED-group names canonicalize while renderer row order stays authored", () => {
    const golden = fixture();
    const expected = encodeRgbDomainV1(golden.profile, golden.codecOptions);

    const direct = clone(golden.profile);
    direct.groups.reverse();
    direct.layerColors.reverse();
    direct.pdModeColors.reverse();
    remapGroupIds(direct, {0: 2, 1: 1, 2: 0});
    assert.deepEqual(encodeRgbDomainV1(direct, golden.codecOptions), expected);

    const studio = clone(golden.studioModel);
    studio.rgb.ledGroups.reverse();
    studio.rgb.ledGroups[0].ledIndices.reverse();
    studio.rgb.layerColors.reverse();
    studio.rgb.pdModeColors.reverse();
    studio.rgb.layerLedGroups[0].ledIndices.reverse();
    assert.deepEqual(encodeStudioRgbDomainV1(studio), expected);

    const reorderedRows = clone(golden.profile);
    reorderedRows.layerGroupRows.reverse();
    assert.notDeepEqual(encodeRgbDomainV1(reorderedRows, golden.codecOptions), expected);
});

test("compiled-source adapter omits reusable groups that no renderer row references", () => {
    const golden = fixture();
    const expected = encodeStudioRgbDomainV1(golden.studioModel, {includeUnusedGroups: false});
    const withAnotherUnusedGroup = clone(golden.studioModel);
    withAnotherUnusedGroup.rgb.ledGroups.push({name: "RGB_LED_GROUP_UNUSED_TOO", ledIndices: [56]});
    assert.deepEqual(
        encodeStudioRgbDomainV1(withAnotherUnusedGroup, {includeUnusedGroups: false}),
        expected
    );
});

test("decoder rejects noncanonical headers, geometry, dictionary order, and bitmap bits", () => {
    const golden = fixture();
    const valid = encodeRgbDomainV1(golden.profile, golden.codecOptions);
    for (const offset of [1, 14, 15]) {
        const reserved = Buffer.from(valid);
        reserved[offset] = 1;
        assertCode("RESERVED_BITS", () => decodeRgbDomainV1(reserved, golden.codecOptions));
    }
    const stageBit = Buffer.from(valid);
    stageBit[2] |= 0x20;
    assertCode("RESERVED_BITS", () => decodeRgbDomainV1(stageBit, golden.codecOptions));
    for (const [offset, value] of [[12, 57], [13, 7]]) {
        const geometry = Buffer.from(valid);
        geometry[offset] = value;
        assertCode("INCOMPATIBLE_GEOMETRY", () => decodeRgbDomainV1(geometry, golden.codecOptions));
    }

    const reservedLedBits = Buffer.from(valid);
    reservedLedBits[24] = 0x04;
    assertCode("RESERVED_BITS", () => decodeRgbDomainV1(reservedLedBits, golden.codecOptions));
    const duplicateBitmap = Buffer.from(valid);
    valid.copy(duplicateBitmap, 26, 17, 25);
    assertCode("DUPLICATE_BITMAP", () => decodeRgbDomainV1(duplicateBitmap, golden.codecOptions));
    const noncanonicalDictionary = Buffer.from(valid);
    valid.copy(noncanonicalDictionary, 17, 26, 34);
    valid.copy(noncanonicalDictionary, 26, 17, 25);
    assertCode("NONCANONICAL", () => decodeRgbDomainV1(noncanonicalDictionary, golden.codecOptions));
    const noncanonicalId = Buffer.from(valid);
    noncanonicalId[16] = 1;
    assertCode("NONCANONICAL_ORDER", () => decodeRgbDomainV1(noncanonicalId, golden.codecOptions));
});

test("strict decoder rejects invalid references, selectors, enums, brightness, and PD ids", () => {
    const golden = fixture();
    const valid = encodeRgbDomainV1(golden.profile, golden.codecOptions);
    const badReference = Buffer.from(valid);
    badReference[62] = 3;
    assertCode("INVALID_REFERENCE", () => decodeRgbDomainV1(badReference, golden.codecOptions));
    const badLayerSelector = Buffer.from(valid);
    badLayerSelector[58] = 3;
    assertCode("INVALID_SELECTOR", () => decodeRgbDomainV1(badLayerSelector, golden.codecOptions));
    const badLayerMode = Buffer.from(valid);
    badLayerMode[47] = 2;
    assertCode("INVALID_ENUM", () => decodeRgbDomainV1(badLayerMode, golden.codecOptions));
    const tooBright = Buffer.from(valid);
    tooBright[46] = 201;
    assertCode("BRIGHTNESS_EXCEEDED", () => decodeRgbDomainV1(tooBright, golden.codecOptions));
    const badPdId = Buffer.from(valid);
    badPdId[72] = 6;
    assertCode("INVALID_ID", () => decodeRgbDomainV1(badPdId, golden.codecOptions));
});

test("complete RGB surfaces and negotiated feature inclusion are enforced", () => {
    const golden = fixture();
    const missingLayer = clone(golden.profile);
    missingLayer.layerColors.pop();
    assertCode("INCOMPLETE_SURFACE", () => encodeRgbDomainV1(missingLayer, golden.codecOptions));
    const missingPdMode = clone(golden.profile);
    missingPdMode.pdModeColors.pop();
    assertCode("INCOMPLETE_SURFACE", () => encodeRgbDomainV1(missingPdMode, golden.codecOptions));
    const missingTapColor = clone(golden.profile);
    missingTapColor.keyFeedback.tapBranchColors.pop();
    assertCode("INCOMPLETE_SURFACE", () => encodeRgbDomainV1(missingTapColor, golden.codecOptions));

    const unsupportedEnable = clone(golden.profile);
    assertCode("UNSUPPORTED_STAGE", () => encodeRgbDomainV1(unsupportedEnable, {
        ...golden.codecOptions,
        compiledStageMask: 30,
    }));
    const unsupportedData = clone(golden.profile);
    unsupportedData.stageEnableMask &= ~RGB_STAGE_BITS.AUTOMOUSE;
    assertCode("UNSUPPORTED_STAGE_DATA", () => encodeRgbDomainV1(unsupportedData, {
        ...golden.codecOptions,
        compiledStageMask: 29,
    }));

    const layerOnly = clone(golden.profile);
    layerOnly.stageEnableMask = RGB_STAGE_BITS.LAYER;
    layerOnly.automouseFade = {mode: 0, endColor: {h: 0, s: 0, v: 0}};
    layerOnly.pdModeColors = [];
    layerOnly.pdModeGroupRows = [];
    layerOnly.comboFeedback = {color: {h: 0, s: 0, v: 0}, locality: 0};
    layerOnly.comboGroupRows = [];
    layerOnly.keyFeedback = {
        tapBranchColors: [],
        tapCommittedColor: {h: 0, s: 0, v: 0},
        holdActiveColor: {h: 0, s: 0, v: 0},
        longHoldActiveColor: {h: 0, s: 0, v: 0},
        tapCommitMode: 0,
        locality: 0,
    };
    layerOnly.keyGroupRows = [];
    const layerOptions = {...golden.codecOptions, compiledStageMask: RGB_STAGE_BITS.LAYER};
    assert.deepEqual(decodeRgbDomainV1(encodeRgbDomainV1(layerOnly, layerOptions), layerOptions), layerOnly);
});

test("fixed v1 group, row, tap-color, LED, and payload boundaries are enforced", () => {
    const golden = fixture();
    const maximum = clone(golden.profile);
    maximum.groups = Array.from({length: RGB_DOMAIN_V1.MAX_GROUPS}, (_, id) => ({id, leds: id === 0 ? Array.from({length: 58}, (unused, led) => led) : [id - 1]}));
    maximum.layerGroupRows = Array.from({length: RGB_DOMAIN_V1.MAX_STAGE_GROUP_ROWS}, () => ({selector: 255, color: {h: 0, s: 0, v: 0}, groupId: 0}));
    maximum.pdModeGroupRows = [];
    maximum.comboGroupRows = [];
    maximum.keyGroupRows = [];
    const maximumBytes = encodeRgbDomainV1(maximum, golden.codecOptions);
    const maximumDecoded = decodeRgbDomainV1(maximumBytes, golden.codecOptions);
    assert.equal(maximumDecoded.groups.length, RGB_DOMAIN_V1.MAX_GROUPS);
    assert.equal(maximumDecoded.layerGroupRows.length, RGB_DOMAIN_V1.MAX_STAGE_GROUP_ROWS);
    assert.deepEqual(maximumDecoded.groups.find((group) => group.leds.length === 58).leds, Array.from({length: 58}, (unused, led) => led));

    const tooManyGroups = clone(golden.profile);
    tooManyGroups.groups = Array.from({length: RGB_DOMAIN_V1.MAX_GROUPS + 1}, (_, id) => ({id, leds: [id]}));
    assertCode("CAPACITY_EXCEEDED", () => encodeRgbDomainV1(tooManyGroups, golden.codecOptions));
    const tooManyRows = clone(golden.profile);
    tooManyRows.layerGroupRows = Array.from({length: RGB_DOMAIN_V1.MAX_STAGE_GROUP_ROWS + 1}, () => ({selector: 255, color: {h: 0, s: 0, v: 0}, groupId: 0}));
    assertCode("CAPACITY_EXCEEDED", () => encodeRgbDomainV1(tooManyRows, golden.codecOptions));
    const tooManyTapColors = clone(golden.profile);
    tooManyTapColors.keyFeedback.tapBranchColors.push({h: 0, s: 0, v: 0});
    assertCode("CAPACITY_EXCEEDED", () => encodeRgbDomainV1(tooManyTapColors, golden.codecOptions));
    const badLed = clone(golden.profile);
    badLed.groups[0].leds = [58];
    assertCode("INVALID_LED", () => encodeRgbDomainV1(badLed, golden.codecOptions));
    const duplicateGroup = clone(golden.profile);
    duplicateGroup.groups[1].leds = [];
    assertCode("DUPLICATE_BITMAP", () => encodeRgbDomainV1(duplicateGroup, golden.codecOptions));
});

test("decoder rejects header count overflow, truncation, and trailing bytes", () => {
    const golden = fixture();
    const valid = encodeRgbDomainV1(golden.profile, golden.codecOptions);
    const groupOverflow = Buffer.from(valid);
    groupOverflow[4] = RGB_DOMAIN_V1.MAX_GROUPS + 1;
    assertCode("CAPACITY_EXCEEDED", () => decodeRgbDomainV1(groupOverflow, golden.codecOptions));
    const rowOverflow = Buffer.from(valid);
    rowOverflow[6] = RGB_DOMAIN_V1.MAX_STAGE_GROUP_ROWS;
    rowOverflow[8] = 1;
    rowOverflow[9] = 0;
    rowOverflow[11] = 0;
    assertCode("CAPACITY_EXCEEDED", () => decodeRgbDomainV1(rowOverflow, golden.codecOptions));
    assertCode("TRUNCATED", () => decodeRgbDomainV1(valid.subarray(0, valid.length - 1), golden.codecOptions));
    assertCode("TRAILING_BYTES", () => decodeRgbDomainV1(Buffer.concat([valid, Buffer.from([0])]), golden.codecOptions));
    assertCode("TRUNCATED", () => decodeRgbDomainV1(Buffer.alloc(RGB_DOMAIN_V1.HEADER_SIZE - 1), golden.codecOptions));
});

test("Studio adapter rejects ambiguous identities, expressions, and feature capabilities", () => {
    const golden = fixture();
    const duplicateLayer = clone(golden.studioModel);
    duplicateLayer.layers[1].name = duplicateLayer.layers[0].name;
    assertCode("DUPLICATE_ID", () => canonicalizeStudioRgbDomainV1(duplicateLayer));
    const duplicateGroup = clone(golden.studioModel);
    duplicateGroup.rgb.ledGroups[1].name = duplicateGroup.rgb.ledGroups[0].name;
    assertCode("DUPLICATE_ID", () => canonicalizeStudioRgbDomainV1(duplicateGroup));
    const unknownEnum = clone(golden.studioModel);
    unknownEnum.rgb.layerColors[0].mode = "FUTURE_LAYER_MODE";
    assertCode("INVALID_ENUM", () => canonicalizeStudioRgbDomainV1(unknownEnum));
    const unresolved = clone(golden.studioModel);
    unresolved.rgb.layerColors[0].color.h = "UNKNOWN_HUE";
    assertCode("UNRESOLVED_EXPRESSION", () => canonicalizeStudioRgbDomainV1(unresolved));
    const cyclic = clone(golden.studioModel);
    cyclic.rgb.layerColors[0].color.h = "COLOR_A";
    assertCode("UNRESOLVED_EXPRESSION", () => canonicalizeStudioRgbDomainV1(cyclic, {constants: {COLOR_A: "COLOR_B", COLOR_B: "COLOR_A"}}));
    assert.throws(() => canonicalizeStudioRgbDomainV1(golden.studioModel, {supportedPdModeIds: [6]}), /registry/);
});
