"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");

const {decodeProfileBlob, encodeProfileBlob} = require("../../core/schema/profile-blob-v1");
const {decodeKeyBehaviorDomain, encodeKeyBehaviorDomain} = require("../../core/schema/key-behavior-domain-v1");
const {decodeRgbDomainV1, encodeRgbDomainV1} = require("../../core/schema/rgb-domain-v1");
const {
    buildCanonicalStudioProfileV1,
    semanticActionForExpression,
} = require("../../core/schema/compiled-profile-v1");

const fixturePath = path.resolve(__dirname, "../../../../tests/fixtures/compiled_profile_v1.fixture");
const fixture = new Map(fs.readFileSync(fixturePath, "utf8")
    .split(/\r?\n/)
    .filter((line) => line && !line.startsWith("#"))
    .map((line) => {
        const separator = line.indexOf("=");
        return [line.slice(0, separator), line.slice(separator + 1)];
    }));

test("real compiled defaults remain a canonical cross-language Profile Blob v1 fixture", () => {
    const bytes = Buffer.from(fixture.get("profile.full.hex"), "hex");
    const decoded = decodeProfileBlob(bytes);
    const codecOptions = {
        compiledStageMask: 0x1f,
        logicalLayerCount: 5,
        maximumBrightness: 200,
        tapBranchColorCount: 4,
        supportedPdModeIds: [0, 1, 2, 3, 4, 5],
    };
    const rgb = decodeRgbDomainV1(decoded.domains[0].payload, codecOptions);
    const behaviors = decodeKeyBehaviorDomain(decoded.domains[1].payload);

    assert.equal(bytes.length, Number(fixture.get("profile.byte_length")));
    assert.equal(decoded.crc32, Number.parseInt(fixture.get("profile.crc32"), 16));
    assert.equal(decoded.digest, Number.parseInt(fixture.get("profile.fnv1a32"), 16));
    assert.equal(decoded.domains[0].payload.length, Number(fixture.get("profile.rgb_payload_length")));
    assert.equal(decoded.domains[1].payload.length, Number(fixture.get("profile.behavior_payload_length")));
    assert.equal(rgb.layerColors.length, 5);
    assert.equal(rgb.pdModeColors.length, 6);
    assert.equal(behaviors.rowCount, Number(fixture.get("profile.behavior_rows")));
    assert.equal(behaviors.populatedStepCount, Number(fixture.get("profile.populated_behavior_steps")));
    assert.deepEqual(encodeRgbDomainV1(rgb, codecOptions), decoded.domains[0].payload);
    assert.deepEqual(encodeKeyBehaviorDomain({rows: behaviors.rows}), decoded.domains[1].payload);
    assert.deepEqual(encodeProfileBlob({domains: decoded.domains}), bytes);
});

function minimalStudioModel() {
    return {
        layers: [{name: "_BASE"}],
        configDefaults: [],
        rgb: {
            ledGroups: [],
            layerColors: [{layer: "_BASE", color: {h: 0, s: 0, v: 0}, mode: "ALL_KEYS"}],
            layerLedGroups: [],
            pdModeColors: [],
            pdModeLedGroups: [],
            comboFeedbackLedGroups: [],
            keyBehaviorFeedbackLedGroups: [],
        },
        keyBehaviors: [{
            keycode: "CUSTOM_ONE",
            tapHoldTerm: "180",
            steps: [{
                tapCount: 0,
                tap: {helper: "TAP_SENDS", action: "KC_A"},
                hold: {helper: "PRESS_AND_HOLD_UNTIL_RELEASE", action: "MO(_BASE)"},
            }],
        }],
        qmkKeycodeValues: {KC_A: 4},
        customKeycodes: ["CUSTOM_ONE"],
    };
}

const milestoneCapabilities = {
    maxLogicalLayers: 8,
    maxBehaviorRows: 64,
    maxTapStepsPerBehavior: 5,
    maxPopulatedBehaviorSteps: 128,
    hardcodedMacroSlots: 16,
    viaMacroSlots: 64,
    maxProfilePayload: 4064,
    physicalLedCount: 58,
};

test("Studio source model compiles RGB and behaviors into one canonical milestone blob", () => {
    const compiled = buildCanonicalStudioProfileV1(minimalStudioModel(), {capabilities: milestoneCapabilities});
    const decoded = decodeProfileBlob(compiled.blob);
    const behaviors = decodeKeyBehaviorDomain(decoded.domains[1].payload);

    assert.equal(compiled.domainMask, 3);
    assert.deepEqual(decoded.domains.map((domain) => domain.id), [0x10, 0x20]);
    assert.equal(behaviors.rows.length, 1);
    assert.equal(behaviors.rows[0].tapHoldTerm, 180);
    assert.deepEqual(behaviors.rows[0].steps[0].tap, {kind: 1, flags: 0, operand: 4});
    assert.deepEqual(behaviors.rows[0].steps[0].hold.action, {kind: 2, flags: 0, operand: 0});
    assert.deepEqual(encodeProfileBlob({domains: decoded.domains}), compiled.blob);
});

test("semantic compiler maps stable actions and rejects source-only helpers", () => {
    const model = minimalStudioModel();
    assert.deepEqual(semanticActionForExpression("VIA_MACRO_12", model), {kind: 6, operand: 12});
    assert.deepEqual(semanticActionForExpression("LOCK_LAYER(_BASE)", model), {kind: 3, operand: 0});
    assert.throws(
        () => semanticActionForExpression("SOME_LOCAL_C_FUNCTION(KC_A)", model),
        (error) => error?.code === "UNSUPPORTED_ACTION"
    );
});
