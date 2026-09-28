#!/usr/bin/env node
'use strict';
// Engineering tool: prints markdown tables comparing capture files written by
// tools/capture-split-diagnostics.cjs, one column per file. Used for the
// records under measurements/pointing-cadence/. Reads files only.
const fs = require('node:fs');
const path = require('node:path');

const pct = value => `${(value * 100).toFixed(1)}%`;
const us = value => `${Math.round(value)} µs`;
const ms = value => (value < 1000 ? us(value) : `${(value / 1000).toFixed(1)} ms`);

function table(header, rows) {
    return [`| ${header.join(' | ')} |`, `| --- |${header.slice(1).map(() => ' ---: |').join('')}`, ...rows.map(row => `| ${row.join(' | ')} |`)].join('\n');
}

// One capture's figures. Split time is the transaction recorder's measured
// spans over its capture duration; the rest comes from the cadence windows.
function captureFigures(capture) {
    const cadence = capture.cadence;
    if (!cadence || cadence.unavailable) throw new Error(`capture has no cadence data${cadence && cadence.unavailable ? `: ${cadence.unavailable}` : ''}`);
    const durationUs = capture.metadata.durationUs;
    const histogram = Object.entries(cadence.gapHistogram);
    const gaps = histogram.reduce((sum, [, count]) => sum + count, 0);
    return {
        cadence,
        durationUs,
        splitShare: capture.transactions.reduce((sum, t) => sum + t.totalUs, 0) / durationUs,
        failures: capture.transactions.reduce((sum, t) => sum + t.failures, 0),
        histogram: histogram.map(([label, count]) => [label, gaps ? count / gaps : 0]),
        longGapsPerSecond: histogram[histogram.length - 1][1] / cadence.windows,
        transactions: new Map(capture.transactions.map(t => [t.id, t])),
    };
}

function summarizeCaptures(entries) {
    const names = entries.map(e => e.name);
    const figures = entries.map(e => captureFigures(e.capture));
    const sections = [];

    const rows = [
        ['Pointing polls/s', ...figures.map(f => `${Math.round(f.cadence.pointingPollsPerSecond.mean)} (${f.cadence.pointingPollsPerSecond.min}–${f.cadence.pointingPollsPerSecond.max})`)],
        ['Matrix scans/s', ...figures.map(f => `${Math.round(f.cadence.matrixScansPerSecond)}`)],
        ['Cadence windows', ...figures.map(f => `${f.cadence.windows}`)],
        ['Split transaction share', ...figures.map(f => pct(f.splitShare))],
        ['Failed transactions', ...figures.map(f => `${f.failures}`)],
        ['Longest poll gap', ...figures.map(f => ms(f.cadence.maxPointingGapUs))],
        ['Poll gaps ≥ 5 ms per second', ...figures.map(f => f.longGapsPerSecond.toFixed(1))],
    ];
    figures[0].histogram.forEach(([label], b) => rows.push([`Poll gaps ${label}`, ...figures.map(f => pct(f.histogram[b][1]))]));
    sections.push(table(['', ...names], rows));

    const staged = figures.filter(f => f.cadence.stages);
    if (staged.length) {
        const stageNames = Object.keys(staged[0].cadence.stages);
        const stageRows = stageNames.map(stage => [`\`${stage}\``, ...figures.map(f => {
            const s = f.cadence.stages && f.cadence.stages[stage];
            return s ? `${pct(s.share)} · ${us(s.meanPerScanUs)} · ${ms(s.maxLoopUs)}` : '—';
        })]);
        stageRows.push(['Coverage', ...figures.map(f => (f.cadence.stages ? f.cadence.coverage.toFixed(3) : '—'))]);
        sections.push('Stage: share of wall time · mean per loop · longest loop\n\n' + table(['Stage', ...names], stageRows));
    }

    const ids = [...new Set(figures.flatMap(f => [...f.transactions.keys()]))].sort((a, b) => a - b);
    const txRows = ids.map(id => [`${id}`, ...figures.map(f => {
        const t = f.transactions.get(id);
        return t ? `${(t.attempts / f.durationUs * 1e6).toFixed(1)}/s · ${us(t.totalUs / t.attempts)} · ${ms(t.maxUs)}${t.failures ? ` · ${t.failures} failed` : ''}` : '—';
    })]);
    sections.push('Split transaction: attempts/s · mean · longest\n\n' + table(['Id', ...names], txRows));
    return sections.join('\n\n') + '\n';
}

function main(argv = process.argv.slice(2)) {
    if (!argv.length || argv.includes('--help')) {
        console.log('Usage: node tools/summarize-cadence-captures.cjs CAPTURE.json...\nPrints markdown tables, one column per capture file.');
        return;
    }
    process.stdout.write(summarizeCaptures(argv.map(file => ({name: path.basename(file, '.json'), capture: JSON.parse(fs.readFileSync(file, 'utf8'))}))));
}
if (require.main === module) {
    try {
        main();
    } catch (e) {
        console.error(e.message);
        process.exitCode = 1;
    }
}
module.exports = {summarizeCaptures};
