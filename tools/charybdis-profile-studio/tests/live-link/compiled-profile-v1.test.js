"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");

const {decodeProfileBlob, encodeProfileBlob} = require("../../live-link/profile-blob-v1");
const {decodeKeyBehaviorDomain, encodeKeyBehaviorDomain} = require("../../live-link/key-behavior-domain-v1");
const {decodeRgbDomainV1, encodeRgbDomainV1} = require("../../live-link/rgb-domain-v1");

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
