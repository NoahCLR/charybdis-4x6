import {element, facts, hex, panel} from "../dom.js";

// Everything here was read from the keyboard. Nothing is derived from the
// repository, which is the point of the app.
export function renderIdentity(snapshot) {
    const caps = snapshot.capabilities;
    const via = snapshot.viaIdentity;
    if (!caps) {
        return panel("Identity", [element("p", {class: "muted"}, "No capability page read yet.")]);
    }

    const body = [
        element("h3", {}, "Firmware"),
        facts([
            ["VIA protocol", via?.protocolVersion !== undefined ? hex(via.protocolVersion, 4) : undefined],
            ["VIA firmware", via?.firmwareVersion !== undefined ? hex(via.firmwareVersion) : undefined],
            ["Profile Wire", `${caps.protocol?.major}.${caps.protocol?.minor}`],
            ["Profile schema", `${caps.schema?.major}.${caps.schema?.minor}`],
            ["Firmware version", hex(caps.firmwareVersion)],
            ["Action ABI digest", hex(caps.actionAbiDigest)],
            ["Compiled default digest", hex(caps.compiledDefaultDigest)],
        ]),
        element("h3", {}, "Capacities"),
        facts([
            ["Layers compiled", caps.compiledLayerCount],
            ["Max logical layers", caps.maxLogicalLayers],
            ["Max behaviour rows", caps.maxBehaviorRows],
            ["Max tap steps per behaviour", caps.maxTapStepsPerBehavior],
            ["Max combos", caps.maxCombos],
            ["Max keys per combo", caps.maxKeysPerCombo],
            ["Physical LEDs", caps.physicalLedCount],
            ["Reusable RGB groups", caps.maxReusableRgbGroups],
            ["Hardcoded macro slots", caps.hardcodedMacroSlots],
            ["VIA macro slots", caps.viaMacroSlots],
            ["VIA macro bytes", caps.viaMacroBytes],
        ]),
        element("h3", {}, "Storage"),
        facts([
            ["Max profile payload", caps.maxProfilePayload !== undefined ? `${caps.maxProfilePayload} B` : undefined],
            ["Profile slot payload", caps.profileSlotPayload !== undefined ? `${caps.profileSlotPayload} B` : undefined],
            ["Profile slot size", caps.profileSlotSize !== undefined ? `${caps.profileSlotSize} B` : undefined],
            ["Candidate chunk max", caps.candidateChunkMax !== undefined ? `${caps.candidateChunkMax} B` : undefined],
            ["Report size", caps.reportSize !== undefined ? `${caps.reportSize} B` : undefined],
            ["Supported domains", hex(caps.supportedDomainMask, 2)],
            ["Feature flags", hex(caps.featureFlags, 4)],
        ]),
    ];
    return panel("Identity", body);
}
