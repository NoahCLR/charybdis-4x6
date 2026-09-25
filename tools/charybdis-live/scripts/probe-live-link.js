#!/usr/bin/env node
"use strict";

const {
    CHARYBDIS_PRODUCT_ID,
    CHARYBDIS_VENDOR_ID,
    NodeHidDeviceAdapter,
    QMK_RAW_HID_USAGE,
    QMK_RAW_HID_USAGE_PAGE,
} = require("../core/transport/node-hid-adapter");

const TARGET = Object.freeze({
    vendorId: CHARYBDIS_VENDOR_ID,
    productId: CHARYBDIS_PRODUCT_ID,
    usagePage: QMK_RAW_HID_USAGE_PAGE,
    usage: QMK_RAW_HID_USAGE,
});

async function main(options = {}) {
    const argv = options.argv || process.argv.slice(2);
    const stdout = options.stdout || process.stdout;
    const adapter = options.adapter || new NodeHidDeviceAdapter();
    const flags = parseArguments(argv);

    if (flags.help) {
        stdout.write(usage());
        return {target: TARGET, devices: []};
    }

    const devices = await adapter.listDevices();
    const result = {target: TARGET, count: devices.length, devices};
    if (flags.json) {
        stdout.write(`${JSON.stringify(result, null, 2)}\n`);
    } else {
        writeHumanResult(stdout, result);
    }
    return result;
}

function parseArguments(argv) {
    const flags = {help: false, json: false};
    for (const argument of argv) {
        if (argument === "--help" || argument === "-h") {
            flags.help = true;
        } else if (argument === "--json") {
            flags.json = true;
        } else {
            throw new TypeError(`Unknown argument: ${argument}`);
        }
    }
    return flags;
}

function writeHumanResult(stream, result) {
    stream.write(
        "Charybdis Raw HID target: " +
        `VID 0x${hex(TARGET.vendorId, 4)}, PID 0x${hex(TARGET.productId, 4)}, ` +
        `usage page 0x${hex(TARGET.usagePage, 4)}, usage 0x${hex(TARGET.usage, 2)}\n`
    );
    stream.write("Read-only enumeration: this command does not open or write to a device.\n");
    if (!result.devices.length) {
        stream.write("No matching interfaces found.\n");
        return;
    }
    result.devices.forEach((device, index) => {
        const label = [device.manufacturer, device.product].filter(Boolean).join(" ") || "Charybdis";
        stream.write(`${index + 1}. ${label}\n`);
        stream.write(`   path: ${device.path}\n`);
        if (device.serialNumber) {
            stream.write(`   serial: ${device.serialNumber}\n`);
        }
    });
}

function usage() {
    return [
        "Usage: npm run probe:live-link -- [--json]",
        "",
        "Lists matching Charybdis QMK Raw HID interfaces without opening or writing to them.",
        "",
    ].join("\n");
}

function hex(value, width) {
    return value.toString(16).toUpperCase().padStart(width, "0");
}

if (require.main === module) {
    main().catch((error) => {
        const code = error?.code ? ` [${error.code}]` : "";
        process.stderr.write(`Live Link probe failed${code}: ${error.message}\n`);
        process.exitCode = 1;
    });
}

module.exports = {
    TARGET,
    main,
    parseArguments,
    usage,
};
