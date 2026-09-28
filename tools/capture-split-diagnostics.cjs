#!/usr/bin/env node
'use strict';
// Engineering tool. Install its own dependencies with npm ci --prefix tools.

// Format 2 adds each transaction's CRC failures: writes the slave dropped and
// reported, and reads the master rejected (zero without the frame CRC).
function decode(page, response) {
    const data = Buffer.from(response);
    if (data.length !== 32 || data[0] !== 7 || data[1] !== 0 || data[2] !== 10 || data[3] !== 1 || data[4] !== page || data[5] !== 0 || data[6] !== 25 || (data[7] !== 1 && data[7] !== 2)) {
        throw new Error(`Invalid/unavailable diagnostic page ${page}: ${data.toString('hex')}`);
    }
    if (page === 0) return {transactionCount: data[8], armed: !!data[9], frozen: !!data[10], durationUs: data.readUInt32LE(11), activityId: data[15]};
    if (data[8] !== page - 1) throw new Error('Transaction page identity mismatch');
    const entry = {id: data[8], attempts: data.readUInt32LE(9), failures: data.readUInt32LE(13), attemptedBytes: data.readUInt32LE(17), totalUs: data.readUInt32LE(21), maxUs: data.readUInt32LE(25)};
    if (data[7] === 2) entry.crcFailures = data.readUInt16LE(29);
    return entry;
}

// Pointing-cadence recorder (value 0x03, NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS
// builds): rolling one-second windows kept since boot, no arming. It is read
// with VIA's id_custom_get_value (0x08), unlike the split recorder above,
// which reads with 7 and arms with 8. Format 2 adds per-window loop stage
// times after the window pages; format 1 firmware has none. Wire layout:
// users/noah/lib/state/diagnostics/runtime_diag.h.
const VIA_CUSTOM_GET = 8;
// Firmware stage order (noah_runtime_diag_stage_t, less IDLE).
const STAGE_NAMES = ['matrixScan', 'durableIo', 'keyRuntime', 'splitSync', 'qmkTasks', 'processRecord', 'rgbRender', 'sensorRead', 'pointingTask', 'pointingReport', 'outsideKeyboardTask'];
function decodeCadence(page, response, metadata = null) {
    const data = Buffer.from(response);
    if (data.length !== 32 || data[0] !== VIA_CUSTOM_GET || data[1] !== 0 || data[2] !== 3 || data[3] !== 1 || data[4] !== page || data[5] !== 0 || data[6] !== 25) {
        throw new Error(`Invalid/unavailable cadence page ${page}: ${data.toString('hex')}`);
    }
    const payload = data.subarray(7);
    if (page === 0) {
        const format = payload[0];
        if (format !== 1 && format !== 2) throw new Error(`Unknown cadence format ${format}`);
        const bucketUpperUs = [];
        for (let b = 0; b < 5; ++b) bucketUpperUs.push(payload.readUInt16LE(11 + b * 2));
        const stageCount = format === 2 ? payload[22] : 0;
        const stagesPerPage = format === 2 ? payload[23] : 0;
        const stagePagesPerWindow = stageCount ? Math.ceil(stageCount / stagesPerPage) : 0;
        return {format, pages: payload[1], windowCount: (payload[1] - 1) / (1 + stagePagesPerWindow), completed: payload[2], sequence: payload.readUInt32LE(3), windowUs: payload.readUInt32LE(7), bucketUpperUs, started: !!payload[21], stageCount, stagesPerPage, stagePagesPerWindow, stageUnitUs: format === 2 ? 2 ** payload[24] : 0};
    }
    const index = payload[4];
    if (index === 0xff) return {sequence: payload.readUInt32LE(0), index: null};
    if (metadata && page > metadata.windowCount) {
        const stagePage = page - 1 - metadata.windowCount;
        if (index !== Math.floor(stagePage / metadata.stagePagesPerWindow)) throw new Error('Cadence stage page identity mismatch');
        const first = (stagePage % metadata.stagePagesPerWindow) * metadata.stagesPerPage;
        const stages = [];
        for (let slot = 0; slot < metadata.stagesPerPage && first + slot < metadata.stageCount; ++slot) {
            stages.push({stage: first + slot, totalUs: payload.readUInt16LE(5 + slot * 4) * metadata.stageUnitUs, maxLoopUs: payload.readUInt16LE(7 + slot * 4)});
        }
        return {sequence: payload.readUInt32LE(0), index, stages};
    }
    if (index !== page - 1) throw new Error('Cadence page identity mismatch');
    const histogram = [];
    for (let b = 0; b < 6; ++b) histogram.push(payload.readUInt16LE(13 + b * 2));
    return {sequence: payload.readUInt32LE(0), index, maxPointingGapUs: payload.readUInt32LE(5), matrixScans: payload.readUInt16LE(9), pointingPolls: payload.readUInt16LE(11), histogram};
}

// Windows are numbered from boot: the one at index i of a read at sequence S
// is S - completed + i. A capture from sequence `before` to `after` fully
// contains windows before + 1 .. after - 1; window `before` was already
// running when it started.
function captureWindowIds(before, after) {
    const ids = [];
    for (let id = before + 1; id < after; ++id) ids.push(id);
    return ids;
}

// Per stage over the capture's windows: share of wall time, mean per matrix
// scan (one per loop), and the longest single loop. Stage times are exclusive,
// so the shares sum to about 1; `coverage` reports that sum.
function summarizeStages(metadata, windows) {
    if (!metadata.stageCount) return null;
    const spanUs = windows.length * metadata.windowUs;
    const scans = windows.reduce((a, w) => a + w.matrixScans, 0);
    const stages = {};
    let coverage = 0;
    for (let stage = 0; stage < metadata.stageCount; ++stage) {
        const entries = windows.map(w => w.stages[stage]);
        const totalUs = entries.reduce((a, e) => a + e.totalUs, 0);
        coverage += totalUs / spanUs;
        stages[STAGE_NAMES[stage] || `stage${stage}`] = {share: totalUs / spanUs, meanPerScanUs: scans ? totalUs / scans : null, maxLoopUs: Math.max(...entries.map(e => e.maxLoopUs))};
    }
    return {stages, coverage};
}

// The windows inside the capture, as selected by captureWindowIds.
function summarizeCadence(metadata, inside) {
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
        ...summarizeStages(metadata, inside),
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
        console.log('Usage: node tools/capture-split-diagnostics.cjs [--path HID_PATH]\nClose Charybdis Ark/VIA, use a diagnostic build, then move the ball during the ten-second capture. JSON goes to stdout. No profile writes.');
        return;
    }
    if (argv.length && !(argv.length === 2 && argv[0] === '--path')) throw new Error('Expected --path HID_PATH or --help');
    const hid = require('node-hid');
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
    const readCadenceMetadata = () => decodeCadence(0, exchange(VIA_CUSTOM_GET, 0, 3));
    // Windows roll every second, shifting indices; retry if one rolled while
    // its pages were read. The ids stay fixed.
    function readCadence(ids) {
        for (let attempt = 0; attempt < 3; ++attempt) {
            const metadata = readCadenceMetadata();
            const read = page => {
                const entry = decodeCadence(page, exchange(VIA_CUSTOM_GET, page, 3), metadata);
                if (entry.sequence !== metadata.sequence) throw new Error('rolled');
                if (entry.index === null) throw new Error('Cadence window missing');
                return entry;
            };
            try {
                const windows = ids.map(id => {
                    const index = id - (metadata.sequence - metadata.completed);
                    if (index < 0 || index >= metadata.completed) throw new Error(`Cadence window ${id} is no longer kept`);
                    const window = read(1 + index);
                    window.stages = [];
                    for (let k = 0; k < metadata.stagePagesPerWindow; ++k) window.stages.push(...read(1 + metadata.windowCount + index * metadata.stagePagesPerWindow + k).stages);
                    return window;
                });
                return summarizeCadence(metadata, windows);
            } catch (e) {
                if (e.message !== 'rolled') throw e;
            }
        }
        throw new Error('Cadence windows kept rolling during readback');
    }
    try {
        let cadenceBefore = null;
        try {
            cadenceBefore = readCadenceMetadata();
        } catch (e) {
            cadenceBefore = {unavailable: e.message};
        }
        exchange(8, 0);
        console.error('Capture armed. Move the trackball for ten seconds; no diagnostic requests will be sent during capture.');
        await new Promise(resolve => setTimeout(resolve, 10500));
        const cadenceAfter = cadenceBefore.unavailable ? null : readCadenceMetadata();
        const metadata = decode(0, exchange(7, 0));
        if (!metadata.frozen || metadata.armed) throw new Error('Capture has not frozen');
        const transactions = [];
        for (let p = 1; p <= metadata.transactionCount; ++p) {
            const entry = decode(p, exchange(7, p));
            if (entry.attempts) transactions.push(entry);
        }
        let cadence = null;
        try {
            if (cadenceBefore.unavailable) throw new Error(cadenceBefore.unavailable);
            const ids = captureWindowIds(cadenceBefore.sequence, cadenceAfter.sequence);
            if (!ids.length) throw new Error('No complete cadence window inside the capture');
            cadence = readCadence(ids);
        } catch (e) {
            cadence = {unavailable: `${e.message} (build with NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS=yes)`};
        }
        const ceiling = estimateCeiling(metadata, transactions, cadence);
        console.log(JSON.stringify({metadata, transactions, cadence, ceiling, note: 'attemptedBytes includes nominal complete frames for failed attempts; it is not a physical byte count. Times are measured transaction spans, not CPU utilization. Pointing polls are pointing-task runs, an upper bound on USB mouse reports. The ceiling is an estimate: polls with split transaction time removed. Stage times are exclusive wall time per loop stage; matrixScan includes the split transactions above.'}, null, 2));
    } finally { handle.close(); }
}
if (require.main === module) main().catch(e => { console.error(e.message); process.exitCode = 1; });
module.exports = {decode, decodeCadence, captureWindowIds, summarizeStages, summarizeCadence, estimateCeiling};
