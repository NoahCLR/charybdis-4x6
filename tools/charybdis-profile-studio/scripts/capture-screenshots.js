#!/usr/bin/env node
"use strict";

const childProcess = require("child_process");
const fs = require("fs");
const fsp = require("fs/promises");
const net = require("net");
const os = require("os");
const path = require("path");
const vm = require("vm");

const extensionRoot = path.resolve(__dirname, "..");
const repoRoot = path.resolve(extensionRoot, "..", "..");
const extensionPath = path.join(extensionRoot, "extension.js");

const defaultViews = [
    {id: "layout", label: "Layout", fileName: "studio-layout-tab.png"},
    {id: "macros", label: "Macros", fileName: "studio-macros-tab.png"},
    {id: "rgb", label: "RGB", fileName: "studio-rgb-tab.png"},
    {id: "defaults", label: "Defaults", fileName: "studio-defaults-tab.png"},
];

async function main() {
    const options = parseArgs(process.argv.slice(2));
    if (options.help) {
        printHelp();
        return;
    }
    if (typeof WebSocket !== "function") {
        throw new Error("This script needs a Node.js runtime with global WebSocket support.");
    }

    const outputDir = path.resolve(repoRoot, options.outputDir);
    await fsp.mkdir(outputDir, {recursive: true});

    const chromePath = findChrome(options.chromePath);
    const browser = await startChrome(chromePath);
    const tempDir = await fsp.mkdtemp(path.join(os.tmpdir(), "profile-studio-screenshots-"));

    try {
        const {getStudioHtml, buildModel} = loadStudioInternals();
        const model = await buildModel(repoRoot);
        if (model.diagnostics && model.diagnostics.length) {
            console.warn("Profile Studio parser diagnostics:");
            for (const diagnostic of model.diagnostics) {
                console.warn("  " + diagnostic);
            }
        }

        const browserCdp = new CdpClient((await getJson(browser.versionUrl)).webSocketDebuggerUrl);
        await browserCdp.ready;

        const results = [];
        for (const view of defaultViews) {
            const harnessPath = await writeHarness({
                tempDir,
                viewId: view.id,
                getStudioHtml,
                model,
            });
            const outputPath = path.join(outputDir, view.fileName);
            results.push(await captureHarness({
                browser,
                browserCdp,
                harnessPath,
                outputPath,
                viewId: view.id,
                label: view.label,
                width: options.width,
                viewportHeight: options.viewportHeight,
            }));
        }

        browserCdp.close();
        for (const result of results) {
            console.log(`${result.label}: ${result.width} x ${result.height} -> ${path.relative(repoRoot, result.outputPath)}`);
        }
    } finally {
        await stopChrome(browser);
        if (!options.keepHarness) {
            await fsp.rm(tempDir, {recursive: true, force: true});
        } else {
            console.log(`Kept screenshot harness files in ${tempDir}`);
        }
    }
}

function parseArgs(argv) {
    const options = {
        chromePath: process.env.CHROME_BIN || "",
        outputDir: process.env.PROFILE_STUDIO_SCREENSHOT_DIR || "docs/media/profile-studio",
        width: numberFromEnv("PROFILE_STUDIO_SCREENSHOT_WIDTH", 1920),
        viewportHeight: numberFromEnv("PROFILE_STUDIO_SCREENSHOT_VIEWPORT_HEIGHT", 900),
        keepHarness: false,
        help: false,
    };

    for (let index = 0; index < argv.length; index += 1) {
        const arg = argv[index];
        if (arg === "--help" || arg === "-h") {
            options.help = true;
        } else if (arg === "--chrome") {
            options.chromePath = requireValue(argv, ++index, arg);
        } else if (arg === "--out") {
            options.outputDir = requireValue(argv, ++index, arg);
        } else if (arg === "--width") {
            options.width = positiveInteger(requireValue(argv, ++index, arg), arg);
        } else if (arg === "--viewport-height") {
            options.viewportHeight = positiveInteger(requireValue(argv, ++index, arg), arg);
        } else if (arg === "--keep-harness") {
            options.keepHarness = true;
        } else {
            throw new Error(`Unknown argument: ${arg}`);
        }
    }

    options.width = positiveInteger(options.width, "width");
    options.viewportHeight = positiveInteger(options.viewportHeight, "viewport height");
    return options;
}

function printHelp() {
    console.log(`Usage: npm run screenshots -- [options]

Options:
  --chrome <path>           Chrome/Chromium executable. Defaults to CHROME_BIN or common app paths.
  --out <dir>               Output directory relative to repo root. Default: docs/media/profile-studio
  --width <px>              Capture viewport width. Default: 1920
  --viewport-height <px>    Initial viewport height before full-page capture. Default: 900
  --keep-harness            Keep generated temporary harness HTML files.
`);
}

function numberFromEnv(name, fallback) {
    return process.env[name] ? Number(process.env[name]) : fallback;
}

function requireValue(argv, index, arg) {
    if (index >= argv.length || argv[index].startsWith("--")) {
        throw new Error(`${arg} requires a value`);
    }
    return argv[index];
}

function positiveInteger(value, label) {
    const parsed = Number(value);
    if (!Number.isInteger(parsed) || parsed <= 0) {
        throw new Error(`${label} must be a positive integer`);
    }
    return parsed;
}

function findChrome(explicitPath) {
    const candidates = [
        explicitPath,
        "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
        "/Applications/Chromium.app/Contents/MacOS/Chromium",
        "/Applications/Brave Browser.app/Contents/MacOS/Brave Browser",
        "google-chrome",
        "chromium",
        "chromium-browser",
        "chrome",
    ].filter(Boolean);

    for (const candidate of candidates) {
        const resolved = resolveExecutable(candidate);
        if (resolved) {
            return resolved;
        }
    }

    throw new Error("Could not find Chrome/Chromium. Set CHROME_BIN or pass --chrome <path>.");
}

function resolveExecutable(candidate) {
    if (candidate.includes(path.sep)) {
        return fs.existsSync(candidate) ? candidate : "";
    }

    const pathEntries = String(process.env.PATH || "").split(path.delimiter);
    for (const entry of pathEntries) {
        const fullPath = path.join(entry, candidate);
        if (fs.existsSync(fullPath)) {
            return fullPath;
        }
    }
    return "";
}

async function startChrome(chromePath) {
    const port = await freePort();
    const userDataDir = await fsp.mkdtemp(path.join(os.tmpdir(), "profile-studio-chrome-"));
    const args = [
        "--headless=new",
        "--disable-background-networking",
        "--disable-component-update",
        "--disable-default-apps",
        "--disable-extensions",
        "--disable-gpu",
        "--disable-sync",
        "--metrics-recording-only",
        "--no-default-browser-check",
        "--no-first-run",
        `--remote-debugging-port=${port}`,
        `--user-data-dir=${userDataDir}`,
        "about:blank",
    ];
    const child = childProcess.spawn(chromePath, args, {
        stdio: ["ignore", "pipe", "pipe"],
    });
    let output = "";
    const rememberOutput = (chunk) => {
        output = (output + chunk.toString()).slice(-4000);
    };
    child.stdout.on("data", rememberOutput);
    child.stderr.on("data", rememberOutput);

    const versionUrl = `http://127.0.0.1:${port}/json/version`;
    try {
        await waitForDevtools(versionUrl, () => output);
    } catch (error) {
        child.kill();
        await fsp.rm(userDataDir, {recursive: true, force: true});
        throw error;
    }

    return {child, userDataDir, versionUrl, port};
}

async function stopChrome(browser) {
    if (!browser) {
        return;
    }
    if (browser.child && !browser.child.killed) {
        browser.child.kill();
        await new Promise((resolve) => browser.child.once("exit", resolve));
    }
    if (browser.userDataDir) {
        await fsp.rm(browser.userDataDir, {recursive: true, force: true});
    }
}

function freePort() {
    return new Promise((resolve, reject) => {
        const server = net.createServer();
        server.once("error", reject);
        server.listen(0, "127.0.0.1", () => {
            const address = server.address();
            const port = typeof address === "object" && address ? address.port : 0;
            server.close(() => resolve(port));
        });
    });
}

async function waitForDevtools(versionUrl, output) {
    const started = Date.now();
    let lastError;
    while (Date.now() - started < 10000) {
        try {
            await getJson(versionUrl);
            return;
        } catch (error) {
            lastError = error;
            await delay(100);
        }
    }
    throw new Error(`Chrome DevTools did not become ready: ${lastError && lastError.message}\n${output()}`);
}

function loadStudioInternals() {
    const source = fs.readFileSync(extensionPath, "utf8");
    const fakeVscode = {
        workspace: {
            workspaceFolders: [
                {uri: {fsPath: repoRoot}},
                {uri: {fsPath: path.resolve(repoRoot, "..", "bastardkb-qmk")}},
            ],
            openTextDocument() {
                return Promise.resolve({});
            },
        },
        window: {
            activeTextEditor: undefined,
            createStatusBarItem() {
                return {show() {}, dispose() {}};
            },
            createWebviewPanel() {
                return {webview: {onDidReceiveMessage() {}, postMessage() {}}};
            },
            showErrorMessage() {},
            showTextDocument() {
                return Promise.resolve();
            },
        },
        commands: {
            registerCommand() {
                return {dispose() {}};
            },
        },
        StatusBarAlignment: {Left: 1},
        ViewColumn: {One: 1, Beside: 2},
    };
    const module = {exports: {}};
    const context = {
        Buffer,
        console,
        module,
        exports: module.exports,
        process,
        setTimeout,
        clearTimeout,
        __dirname: extensionRoot,
        __filename: extensionPath,
        require(name) {
            if (name === "vscode") {
                return fakeVscode;
            }
            return require(name);
        },
    };

    vm.runInNewContext(
        `${source}\nmodule.exports.__screenshots = { getStudioHtml, buildModel };`,
        context,
        {filename: extensionPath}
    );
    return module.exports.__screenshots;
}

async function writeHarness({tempDir, viewId, getStudioHtml, model}) {
    let html = getStudioHtml({});
    html = html.replace(/\s*<meta http-equiv="Content-Security-Policy"[^>]*>/, "");
    html = html.replace(/let activeView = "[^"]+";/, `let activeView = "${viewId}";`);
    html = html.replace(
        "</style>",
        "\n        /* screenshot harness overrides */\n        header { position: static !important; }\n    </style>"
    );

    const serializedModel = JSON.stringify(model).replace(/<\/script/gi, "<\\/script");
    const bootstrap = [
        "<script>",
        `window.__studioModel = ${serializedModel};`,
        "window.acquireVsCodeApi = function () {",
        "    return {",
        "        postMessage(message) {",
        "            window.__lastStudioMessage = message;",
        "            if (message && message.type === 'ready') {",
        "                setTimeout(function () {",
        "                    window.postMessage({type: 'model', model: window.__studioModel}, '*');",
        "                }, 0);",
        "            }",
        "        }",
        "    };",
        "};",
        "</script>",
    ].join("\n");
    html = html.replace("<script nonce=", `${bootstrap}\n<script nonce=`);

    const harnessPath = path.join(tempDir, `studio-${viewId}-harness.html`);
    await fsp.writeFile(harnessPath, html, "utf8");
    return harnessPath;
}

async function captureHarness({browser, browserCdp, harnessPath, outputPath, viewId, label, width, viewportHeight}) {
    const target = await getJson(
        `http://127.0.0.1:${browser.port}/json/new?${encodeURIComponent("file://" + harnessPath)}`,
        {method: "PUT"}
    );
    const page = new CdpClient(target.webSocketDebuggerUrl);
    await page.ready;

    try {
        await page.send("Page.enable");
        await page.send("Runtime.enable");
        await page.send("Emulation.setDeviceMetricsOverride", {
            width,
            height: viewportHeight,
            deviceScaleFactor: 1,
            mobile: false,
        });
        await delay(400);
        await waitForRendered(page, viewId);

        const pageWidth = Math.max(width, await evaluateNumber(page, pageWidthExpression()));
        const pageHeight = await evaluateNumber(page, pageHeightExpression());
        await page.send("Emulation.setDeviceMetricsOverride", {
            width: pageWidth,
            height: pageHeight,
            deviceScaleFactor: 1,
            mobile: false,
        });
        await delay(250);
        await waitForRendered(page, viewId);

        const screenshot = await page.send("Page.captureScreenshot", {
            format: "png",
            fromSurface: true,
            captureBeyondViewport: true,
            clip: {x: 0, y: 0, width: pageWidth, height: pageHeight, scale: 1},
        });
        await fsp.writeFile(outputPath, Buffer.from(screenshot.data, "base64"));
        return {label, width: pageWidth, height: pageHeight, outputPath};
    } finally {
        page.close();
        await browserCdp.send("Target.closeTarget", {targetId: target.id}).catch(() => undefined);
    }
}

function pageWidthExpression() {
    return [
        "document.documentElement.scrollWidth",
        "document.body.scrollWidth",
        "document.documentElement.clientWidth",
        "document.body.clientWidth",
    ].join(", ");
}

function pageHeightExpression() {
    return [
        "document.documentElement.scrollHeight",
        "document.body.scrollHeight",
        "document.documentElement.offsetHeight",
        "document.body.offsetHeight",
    ].join(", ");
}

async function waitForRendered(page, viewId) {
    const expression = `(() => {
        const selected = document.querySelector(${JSON.stringify(`button[data-view="${viewId}"][aria-selected="true"]`)});
        const main = document.querySelector("main");
        return Boolean(selected && main && main.innerText.trim().length > 0);
    })()`;
    const started = Date.now();
    while (Date.now() - started < 10000) {
        const result = await page.send("Runtime.evaluate", {expression, returnByValue: true});
        if (result.result && result.result.value) {
            return;
        }
        await delay(100);
    }
    throw new Error(`Timed out waiting for ${viewId} to render`);
}

async function evaluateNumber(page, expressionBody) {
    const expression = `Math.ceil(Math.max(${expressionBody}))`;
    const result = await page.send("Runtime.evaluate", {expression, returnByValue: true});
    const value = result.result && result.result.value;
    if (!Number.isFinite(value)) {
        throw new Error(`Expected numeric result from ${expression}: ${JSON.stringify(result)}`);
    }
    return value;
}

async function getJson(url, options = {}) {
    const response = await fetch(url, options);
    if (!response.ok) {
        throw new Error(`${options.method || "GET"} ${url} failed with ${response.status}`);
    }
    return response.json();
}

function delay(ms) {
    return new Promise((resolve) => setTimeout(resolve, ms));
}

class CdpClient {
    constructor(url) {
        this.nextId = 1;
        this.pending = new Map();
        this.listeners = new Map();
        this.ready = new Promise((resolve, reject) => {
            this.ws = new WebSocket(url);
            this.ws.addEventListener("open", resolve, {once: true});
            this.ws.addEventListener("error", reject, {once: true});
            this.ws.addEventListener("message", (event) => this.onMessage(event));
        });
    }

    onMessage(event) {
        const message = JSON.parse(event.data);
        if (message.id && this.pending.has(message.id)) {
            const pending = this.pending.get(message.id);
            this.pending.delete(message.id);
            if (message.error) {
                pending.reject(new Error(message.error.message || JSON.stringify(message.error)));
            } else {
                pending.resolve(message.result || {});
            }
            return;
        }

        const listeners = this.listeners.get(message.method) || [];
        for (const listener of listeners) {
            listener(message.params || {});
        }
    }

    async send(method, params = {}) {
        await this.ready;
        const id = this.nextId++;
        const promise = new Promise((resolve, reject) => {
            this.pending.set(id, {resolve, reject});
        });
        this.ws.send(JSON.stringify({id, method, params}));
        return promise;
    }

    close() {
        if (this.ws) {
            this.ws.close();
        }
    }
}

main().catch((error) => {
    console.error(error && error.stack ? error.stack : error);
    process.exitCode = 1;
});
