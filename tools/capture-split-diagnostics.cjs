#!/usr/bin/env node
'use strict';
// Engineering tool, separate from Charybdis Live. Requires its node-hid install.
const {createRequire} = require('node:module');
const path = require('node:path');

function decode(page, response) {
    const data = Buffer.from(response);
    if (data.length !== 32 || data[0] !== 7 || data[1] !== 0 || data[2] !== 10 || data[3] !== 1 || data[4] !== page || data[5] !== 0 || data[6] !== 25 || data[7] !== 1) {
        throw new Error(`Invalid/unavailable diagnostic page ${page}: ${data.toString('hex')}`);
    }
    if (page === 0) return {transactionCount: data[8], armed: !!data[9], frozen: !!data[10], durationUs: data.readUInt32LE(11), activityId: data[15]};
    if (data[8] !== page - 1) throw new Error('Transaction page identity mismatch');
    return {id: data[8], attempts: data.readUInt32LE(9), failures: data.readUInt32LE(13), attemptedBytes: data.readUInt32LE(17), totalUs: data.readUInt32LE(21), maxUs: data.readUInt32LE(25)};
}

// Pointing-cadence recorder (value 0x03, NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS
// builds): rolling one-second windows kept since boot, no arming.
function decodeCadence(page, response) {
    const data = Buffer.from(response);
    if (data.length !== 32 || data[0] !== 7 || data[1] !== 0 || data[2] !== 3 || data[3] !== 1 || data[4] !== page || data[5] !== 0 || data[6] !== 25) {
        throw new Error(`Invalid/unavailable cadence page ${page}: ${data.toString('hex')}`);
    }
    const payload = data.subarray(7);
    if (page === 0) {
        if (payload[0] !== 1) throw new Error(`Unknown cadence format ${payload[0]}`);
        const bucketUpperUs = [];
        for (let b = 0; b < 5; ++b) bucketUpperUs.push(payload.readUInt16LE(11 + b * 2));
        return {pages: payload[1], completed: payload[2], sequence: payload.readUInt32LE(3), windowUs: payload.readUInt32LE(7), bucketUpperUs, started: !!payload[21]};
    }
    const index = payload[4];
    if (index === 0xff) return {sequence: payload.readUInt32LE(0), index: null};
    if (index !== page - 1) throw new Error('Cadence page identity mismatch');
    const histogram = [];
    for (let b = 0; b < 6; ++b) histogram.push(payload.readUInt16LE(13 + b * 2));
    return {sequence: payload.readUInt32LE(0), index, maxPointingGapUs: payload.readUInt32LE(5), matrixScans: payload.readUInt16LE(9), pointingPolls: payload.readUInt16LE(11), histogram};
}

// The windows inside a capture that ended just before this read: the last
// complete ones, leaving out the window that may straddle the capture start.
function summarizeCadence(metadata, windows, captureMs) {
    const inside = windows.slice(-Math.max(1, Math.floor(captureMs / 1000) - 1));
    const polls = inside.map(w => w.pointingPolls);
    const histogram = [0, 0, 0, 0, 0, 0];
    for (const w of inside) w.histogram.forEach((count, b) => { histogram[b] += count; });
    const labels = metadata.bucketUpperUs.map(us => `<${us}us`).concat([`>=${metadata.bucketUpperUs[metadata.bucketUpperUs.length - 1]}us`]);
    return {
        windows: inside.length,
        pointingPollsPerSecond: {mean: polls.reduce((a, b) => a + b, 0) / inside.length, min: Math.min(...polls), max: Math.max(...polls)},
        matrixScansPerSecond: inside.reduce((a, w) => a + w.matrixScans, 0) / inside.length,
        maxPointingGapUs: Math.max(...inside.map(w => w.maxPointingGapUs)),
        gapHistogram: Object.fromEntries(labels.map((label, b) => [label, histogram[b]])),
    };
}

// What the pointing poll rate would be if split transactions took no time:
// polls / (1 - share of the capture spent in them). Measured transaction spans
// only, so it leaves out userspace packet building; USB polling caps real
// reports at 1000/s. An estimate for comparing builds, not a measurement.
function estimateCeiling(metadata, transactions, cadence) {
    if (!cadence || cadence.unavailable || !metadata.durationUs) return null;
    const splitUs = transactions.reduce((sum, t) => sum + t.totalUs, 0);
    const splitShare = splitUs / metadata.durationUs;
    if (splitShare >= 1) return null;
    return {splitShare, pointingPollsPerSecond: cadence.pointingPollsPerSecond.mean / (1 - splitShare), usbCapPerSecond: 1000};
}

async function main(argv = process.argv.slice(2)) {
    if (argv.includes('--help')) {
        console.log('Usage: node tools/capture-split-diagnostics.cjs [--path HID_PATH]\nClose Charybdis Live/VIA, use a diagnostic build, then move the ball during the ten-second capture. JSON goes to stdout. No profile writes.');
        return;
    }
    if (argv.length && !(argv.length === 2 && argv[0] === '--path')) throw new Error('Expected --path HID_PATH or --help');
    const requireLive = createRequire(path.resolve(__dirname, 'charybdis-live/package.json'));
    const hid = requireLive('node-hid');
    const devices = hid.devices().filter(d => d.vendorId === 0xa8f8 && d.productId === 0x1833 && d.usagePage === 0xff60 && d.usage === 0x61);
    const device = argv.length ? devices.find(d => d.path === argv[1]) : devices.length === 1 ? devices[0] : null;
    if (!device) throw new Error(`Found ${devices.length} matching interfaces; select exactly one with --path. ${devices.map(d => d.path).join(', ')}`);
    const handle = new hid.HID(device.path);
    function exchange(command, page, value = 10) {
        const report = Buffer.alloc(32);
        report[0] = command; report[2] = value; report[3] = 1; report[4] = page;
        handle.write([0, ...report]);
        const response = Buffer.from(handle.readTimeout(2000));
        if (response.length !== 32 || !response.subarray(0, 5).equals(report.subarray(0, 5)) || response[5] !== 0) throw new Error(`Diagnostic request rejected/timed out: ${response.toString('hex')}`);
        return response;
    }
    // Windows roll every second; retry if one rolled while its pages were read.
    function readCadence() {
        for (let attempt = 0; attempt < 3; ++attempt) {
            const metadata = decodeCadence(0, exchange(7, 0, 3));
            const windows = [];
            let consistent = true;
            for (let p = 1; p <= metadata.completed; ++p) {
                const w = decodeCadence(p, exchange(7, p, 3));
                if (w.sequence !== metadata.sequence) { consistent = false; break; }
                if (w.index !== null) windows.push(w);
            }
            if (consistent && windows.length) return summarizeCadence(metadata, windows, 10500);
        }
        throw new Error('Cadence windows kept rolling during readback');
    }
    try {
        exchange(8, 0);
        console.error('Capture armed. Move the trackball for ten seconds; no diagnostic requests will be sent during capture.');
        await new Promise(resolve => setTimeout(resolve, 10500));
        const metadata = decode(0, exchange(7, 0));
        if (!metadata.frozen || metadata.armed) throw new Error('Capture has not frozen');
        const transactions = [];
        for (let p = 1; p <= metadata.transactionCount; ++p) {
            const entry = decode(p, exchange(7, p));
            if (entry.attempts) transactions.push(entry);
        }
        let cadence = null;
        try {
            cadence = readCadence();
        } catch (e) {
            cadence = {unavailable: `${e.message} (build with NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS=yes)`};
        }
        const ceiling = estimateCeiling(metadata, transactions, cadence);
        console.log(JSON.stringify({metadata, transactions, cadence, ceiling, note: 'attemptedBytes includes nominal complete frames for failed attempts; it is not a physical byte count. Times are measured transaction spans, not CPU utilization. Pointing polls are pointing-task runs, an upper bound on USB mouse reports. The ceiling is an estimate: polls with split transaction time removed.'}, null, 2));
    } finally { handle.close(); }
}
if (require.main === module) main().catch(e => { console.error(e.message); process.exitCode = 1; });
module.exports = {decode, decodeCadence, summarizeCadence, estimateCeiling};
