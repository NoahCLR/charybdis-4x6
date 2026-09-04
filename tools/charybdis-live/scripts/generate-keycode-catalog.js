#!/usr/bin/env node
"use strict";

// Generates the vendored keycode catalog from QMK's own constant data.
//
// The live app must not depend on a firmware workspace, so this runs once
// against a QMK checkout and writes a checked-in JSON file. QMK version drift
// then shows up as a diff rather than as silent behaviour change.
//
// This captures the numeric keycode, which Profile Studio's parser discards:
// Studio only ever needed names, while the live app must also render what it
// reads back from a device, where a keycode arrives as a bare uint16.
//
//   node scripts/generate-keycode-catalog.js [--qmk <path>] [--check]

const fs = require("node:fs");
const path = require("node:path");

const KEYCODE_DATA_RELATIVE_PATH = path.join("data", "constants", "keycodes");
const OUTPUT_RELATIVE_PATH = path.join("live-link", "keycode-catalog.json");
const CATALOG_FORMAT = "charybdis-keycode-catalog-v1";

function main(argv) {
    const flags = parseArguments(argv);
    const appRoot = path.resolve(__dirname, "..");
    const qmkRoot = flags.qmk ? path.resolve(flags.qmk) : findQmkRoot(appRoot);
    if (!qmkRoot) {
        throw new Error(
            "Could not find a QMK checkout. Pass --qmk <path> to a tree containing " +
            KEYCODE_DATA_RELATIVE_PATH
        );
    }

    const catalog = buildCatalog(qmkRoot);
    const outputPath = path.join(appRoot, OUTPUT_RELATIVE_PATH);
    const serialized = `${JSON.stringify(catalog, null, 2)}\n`;

    if (flags.check) {
        const current = fs.existsSync(outputPath) ? fs.readFileSync(outputPath, "utf8") : "";
        if (current !== serialized) {
            throw new Error(
                `${OUTPUT_RELATIVE_PATH} is out of date with ${qmkRoot}. ` +
                "Re-run without --check to regenerate."
            );
        }
        process.stdout.write(`keycode catalog is current: ${catalog.entries.length} keycodes\n`);
        return;
    }

    fs.writeFileSync(outputPath, serialized);
    process.stdout.write(
        `wrote ${OUTPUT_RELATIVE_PATH}: ${catalog.entries.length} keycodes from QMK ${catalog.qmkVersion}\n`
    );
}

function buildCatalog(qmkRoot) {
    const keycodeDir = path.join(qmkRoot, KEYCODE_DATA_RELATIVE_PATH);
    const files = listKeycodeDataFiles(keycodeDir);
    if (!files.length) {
        throw new Error(`No keycode data found in ${keycodeDir}`);
    }

    // Later spec revisions supersede earlier ones for the same numeric value,
    // and the file list is sorted, so a plain overwrite keeps the newest.
    const byValue = new Map();
    for (const file of files) {
        const text = fs.readFileSync(file, "utf8");
        for (const entry of parseKeycodeEntries(text)) {
            if (shouldSkip(entry)) {
                continue;
            }
            byValue.set(entry.value, entry);
        }
    }

    const entries = Array.from(byValue.values()).sort((left, right) => left.value - right.value);
    return {
        format: CATALOG_FORMAT,
        qmkVersion: readQmkVersion(qmkRoot),
        source: path.relative(qmkRoot, keycodeDir),
        generatedFrom: files.map((file) => path.basename(file)),
        entries,
    };
}

// Entries live under "keycodes" and "aliases" maps whose own keys are the
// numeric code, e.g. "0x0004": {"key": "KC_A", ...}.
function parseKeycodeEntries(text) {
    const entries = [];
    for (const section of ["keycodes", "aliases"]) {
        const body = findSectionBody(text, section);
        if (!body) {
            continue;
        }
        const pattern = /"(0[xX][0-9a-fA-F]+)"\s*:\s*\{/g;
        let match;
        while ((match = pattern.exec(body)) !== null) {
            const open = body.indexOf("{", match.index + match[0].length - 1);
            const close = findMatching(body, open, "{", "}");
            if (close === -1) {
                break;
            }
            const record = body.slice(open + 1, close);
            const name = stringField(record, "key");
            if (name) {
                entries.push({
                    value: Number.parseInt(match[1], 16),
                    name,
                    label: stringField(record, "label") || name,
                    group: stringField(record, "group") || "other",
                    aliases: stringListField(record, "aliases").filter((alias) => !alias.startsWith("!")),
                });
            }
            pattern.lastIndex = close + 1;
        }
    }
    return entries;
}

function shouldSkip(entry) {
    if (!entry.name || entry.name === "SAFE_RANGE") {
        return true;
    }
    // Range markers are boundaries, not keycodes a user can select.
    return entry.name.endsWith("_MIN") || entry.name.endsWith("_MAX");
}

function findSectionBody(text, sectionName) {
    const marker = new RegExp(`"${sectionName}"\\s*:\\s*\\{`);
    const match = marker.exec(text);
    if (!match) {
        return "";
    }
    const open = text.indexOf("{", match.index + match[0].length - 1);
    const close = findMatching(text, open, "{", "}");
    return close === -1 ? "" : text.slice(open + 1, close);
}

function findMatching(text, start, openChar, closeChar) {
    let depth = 0;
    let inString = false;
    for (let index = start; index < text.length; index += 1) {
        const character = text[index];
        if (inString) {
            if (character === "\\") {
                index += 1;
            } else if (character === '"') {
                inString = false;
            }
            continue;
        }
        if (character === '"') {
            inString = true;
        } else if (character === openChar) {
            depth += 1;
        } else if (character === closeChar) {
            depth -= 1;
            if (depth === 0) {
                return index;
            }
        }
    }
    return -1;
}

function stringField(body, field) {
    const match = body.match(new RegExp(`"${field}"\\s*:\\s*"((?:\\\\.|[^"\\\\])*)"`));
    return match ? decodeString(match[1]) : "";
}

function stringListField(body, field) {
    const match = body.match(new RegExp(`"${field}"\\s*:\\s*\\[([\\s\\S]*?)\\]`));
    if (!match) {
        return [];
    }
    return Array.from(match[1].matchAll(/"((?:\\.|[^"\\])*)"/g)).map((item) => decodeString(item[1]));
}

function decodeString(value) {
    try {
        return JSON.parse(`"${value}"`);
    } catch {
        return value;
    }
}

function listKeycodeDataFiles(keycodeDir) {
    if (!fs.existsSync(keycodeDir)) {
        return [];
    }
    const files = fs
        .readdirSync(keycodeDir, {withFileTypes: true})
        .filter((entry) => entry.isFile() && entry.name.endsWith(".hjson"))
        .map((entry) => path.join(keycodeDir, entry.name));

    const extrasDir = path.join(keycodeDir, "extras");
    if (fs.existsSync(extrasDir)) {
        files.push(
            ...fs
                .readdirSync(extrasDir, {withFileTypes: true})
                .filter((entry) => entry.isFile() && /^keycodes_us_\d+\.\d+\.\d+\.hjson$/.test(entry.name))
                .map((entry) => path.join(extrasDir, entry.name))
        );
    }
    return files.sort();
}

// The point of the stamp is to make drift visible, so prefer the identity of
// the actual checkout over a file QMK does not always ship.
function readQmkVersion(qmkRoot) {
    const versionFile = path.join(qmkRoot, "version.txt");
    if (fs.existsSync(versionFile)) {
        return fs.readFileSync(versionFile, "utf8").trim();
    }
    try {
        return require("node:child_process")
            .execFileSync("git", ["describe", "--tags", "--always", "--dirty"], {
                cwd: qmkRoot,
                encoding: "utf8",
                stdio: ["ignore", "pipe", "ignore"],
            })
            .trim();
    } catch {
        return "unknown";
    }
}

function findQmkRoot(appRoot) {
    const candidates = [
        path.resolve(appRoot, "..", "..", "..", "bastardkb-qmk"),
        path.resolve(appRoot, "..", "..", "..", "qmk_firmware"),
    ];
    return candidates.find((candidate) =>
        fs.existsSync(path.join(candidate, KEYCODE_DATA_RELATIVE_PATH))
    );
}

function parseArguments(argv) {
    const flags = {qmk: "", check: false};
    for (let index = 0; index < argv.length; index += 1) {
        if (argv[index] === "--check") {
            flags.check = true;
        } else if (argv[index] === "--qmk") {
            flags.qmk = argv[index + 1];
            index += 1;
            if (!flags.qmk) {
                throw new Error("--qmk requires a path");
            }
        } else {
            throw new Error(`Unknown argument: ${argv[index]}`);
        }
    }
    return flags;
}

module.exports = {buildCatalog, parseKeycodeEntries, CATALOG_FORMAT};

if (require.main === module) {
    try {
        main(process.argv.slice(2));
    } catch (error) {
        process.stderr.write(`generate-keycode-catalog failed: ${error.message}\n`);
        process.exitCode = 1;
    }
}
