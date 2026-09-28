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
    function exchange(command, page) {
        const report = Buffer.alloc(32);
        report[0] = command; report[2] = 10; report[3] = 1; report[4] = page;
        handle.write([0, ...report]);
        const response = Buffer.from(handle.readTimeout(2000));
        if (response.length !== 32 || !response.subarray(0, 5).equals(report.subarray(0, 5)) || response[5] !== 0) throw new Error(`Diagnostic request rejected/timed out: ${response.toString('hex')}`);
        return response;
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
        console.log(JSON.stringify({metadata, transactions, note: 'attemptedBytes includes nominal complete frames for failed attempts; it is not a physical byte count. Times are measured transaction spans, not CPU utilization.'}, null, 2));
    } finally { handle.close(); }
}
if (require.main === module) main().catch(e => { console.error(e.message); process.exitCode = 1; });
module.exports = {decode};
