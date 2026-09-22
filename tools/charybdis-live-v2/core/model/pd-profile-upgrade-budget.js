"use strict";

const {validateSnapshot} = require("./portable-profile");
const {encodePdDomain} = require("../schema/pd-mode-domain-v1");
const {PROFILE_BLOB_V1} = require("../schema/profile-blob-v1");

// Planned geometry, never a substitute for connected-device capabilities.
// Deployed firmware remains at 16 KiB / two 4 KiB slots until versioned restore
// and resource gates pass. Keep the complete lower 8 KiB VIA bank unchanged.
const PD_UPGRADE_STORAGE = Object.freeze({
    viaEnd: 0x1fff, slotAStart: 0x2000, slotBStart: 0x3400,
    slotBytes: 5120, headerBytes: 32, payloadBytes: 5088,
    logicalBytes: 18432, backingBytes: 36864,
});

// Size preflight only: does not invent missing legacy settings, translate an ABI,
// create a new-format document, or authorize a write. Callers supply the slots.
function measurePdProfileUpgrade(document, slots) {
    const legacy = validateSnapshot(document);
    if (legacy.document.layers.length !== 8 || legacy.rgb.pdModeColors.length !== 6) {
        throw Object.assign(new Error("PD size preflight requires the eight-layer, six-mode profile baseline."), {code: "UNSUPPORTED_PD_BASELINE"});
    }
    const pdDomainBytes = encodePdDomain(slots).length;
    const domainEnvelopeBytes = PROFILE_BLOB_V1.DOMAIN_HEADER_SIZE;
    const additionalRgbBytes = 2 * 5; // Two ID/HSV/locality rows; group rows stay intact.
    const requiredBytes = legacy.profile.length + pdDomainBytes + domainEnvelopeBytes + additionalRgbBytes;
    const capacityBytes = PROFILE_BLOB_V1.MAX_SIZE;
    return {
        legacyBytes: legacy.profile.length, pdDomainBytes, domainEnvelopeBytes, additionalRgbBytes,
        requiredBytes, capacityBytes, remainingBytes: capacityBytes - requiredBytes, fits: requiredBytes <= capacityBytes,
        plannedStorage: {
            capacityBytes: PD_UPGRADE_STORAGE.payloadBytes,
            remainingBytes: PD_UPGRADE_STORAGE.payloadBytes - requiredBytes,
            fits: requiredBytes <= PD_UPGRADE_STORAGE.payloadBytes,
        },
    };
}

module.exports = {PD_UPGRADE_STORAGE, measurePdProfileUpgrade};
