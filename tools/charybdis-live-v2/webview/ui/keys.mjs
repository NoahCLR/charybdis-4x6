// Keys: the board stays on screen and the workbench below it switches between
// the key, its behaviour, its combos, and what this layer reaches.

import {css, isOff, label as hsvLabel} from "../lib/colour.mjs";
import {el, esc} from "../lib/dom.mjs";
import {LED_INDEX} from "../view/geometry.mjs";
import {behaviourFor, behaviourTiers, combosForKey, keyFace, macroKeycodes, pointingSlotFor} from "../view/keyface.mjs";
import {feedbackColours, layerColourRow, pdColourRow, stageEnabled} from "../view/lighting.mjs";
import {currentLayer, getModel, layerName, layers, positionAt, post, render, selectedPosition, state, writable} from "../store.mjs";
import {board} from "./board.mjs";
import {layerBar} from "./layerbar.mjs";
import {openPicker} from "./picker.mjs";
import {topbar, unavailable} from "./shell.mjs";

const TABS = [
    {id: "key", label: "Key"},
    {id: "behaviours", label: "Behaviours"},
    {id: "combos", label: "Combos"},
    {id: "macros", label: "Macros"},
    {id: "pointing", label: "Pointing modes"},
];

const HOLD_HELPERS = [
    ["PRESS_AND_HOLD_UNTIL_RELEASE", "held until release"],
    ["TAP_AT_HOLD_THRESHOLD", "tap at hold threshold"],
    ["TAP_ON_RELEASE_AFTER_HOLD", "tap on release after hold"],
    ["REPEAT_WHILE_HELD", "repeat while held"],
];
const helperLabel = (kind, helper) => kind === "tap" ? "tap sends"
    : (HOLD_HELPERS.find(([name]) => name === helper) || [, String(helper || "").toLowerCase().replace(/_/g, " ")])[1];

const TIER_FIELDS = {tap: "tap", hold: "hold", long: "longHold"};

export function screenKeys() {
    const model = getModel();
    const layer = currentLayer();
    const main = el(`<div class="main">${topbar(
        "Keys",
        "The keyboard stays on screen. Pick a key on it, then work in the tab you need — the key itself, its behaviour, the combos it belongs to, and the macros and pointing modes this layer reaches.",
        `<button class="btn ghost" data-act="lighting" data-tip="Open the lighting stages that paint this board.">Lighting…</button>`,
    )}</div>`);
    main.querySelector('[data-act="lighting"]').addEventListener("click", () => { state.screen = "lighting"; render(); });

    const blocked = unavailable(model);
    if (!layer) {
        const content = el(`<div class="content"><div class="pad"><div class="screen-stub">
            <h3>Nothing read yet</h3><p class="note">${esc(blocked || "Choose Read keyboard to load this keyboard's layers.")}</p></div></div></div>`);
        main.appendChild(content);
        return main;
    }

    main.appendChild(layerBar());
    const content = el(`<div class="content"><div class="pad keys-pad"></div></div>`);
    const pad = content.firstElementChild;

    const stage = el(`<div class="stack" style="gap:10px"></div>`);
    const row = layerColourRow(model, layer.index);
    const lit = stageEnabled(model, "layers") && row && !isOff(row.color);
    stage.appendChild(el(`<div class="stage-head">
        <h2>${esc(layerName(layer))}</h2>
        <span class="muted" style="font-size:12px">${lit
            ? `lit ${esc(hsvLabel(row.color))} on ${row.mode === "ALL_KEYS" ? "every key" : "keys mapped here"}`
            : "no layer colour · the base effect shows through"}</span></div>`));
    if (state.comboPicking) stage.appendChild(pickBar());
    stage.appendChild(board(model, layer, {
        selected: state.selected,
        picking: state.comboPicking,
        inputs: state.comboPicking ? state.comboInputs : [],
        onKey: (index) => {
            if (state.comboPicking) {
                state.comboInputs = state.comboInputs.includes(index)
                    ? state.comboInputs.filter((value) => value !== index) : [...state.comboInputs, index];
            } else {
                state.selected = index;
                const behaviour = behaviourFor(model, positionAt(layer, index)?.keycode);
                if (state.tab === "behaviours" && behaviour) { state.behaviourRow = behaviour.keycode; state.cell = null; }
            }
            render();
        },
        onOpen: (index) => pickKeycodeFor(index),
        onSwap: writable() ? (from, to) => swapKeys(from, to) : undefined,
    }));
    stage.appendChild(legend(model));
    pad.appendChild(stage);
    pad.appendChild(bench());
    main.appendChild(content);
    return main;
}

function legend(model) {
    const colours = feedbackColours(model);
    const on = stageEnabled(model, "key");
    const dot = (colour) => `<i class="ldot" style="${on && !isOff(colour) ? `background:${css(colour)}` : "background:none;border-style:dashed"}"></i>`;
    return el(`<div class="board-legend">
        <span class="legend-item">the board shows the light this layer paints</span>
        <span class="legend-item">${dot(colours.tap)} tap branch</span>
        <span class="legend-item">${dot(colours.hold)} hold branch</span>
        <span class="legend-item">${dot(colours.long)} long hold branch</span>
        <span class="legend-item"><i class="lbadge">C1</i> combo input</span>
        <span class="legend-item"><span class="m" style="border-style:dashed"></span> transparent · falls through</span>
        <span class="legend-item dim">double-click to pick a keycode${writable() ? " · drag one key onto another to swap" : ""}</span>
    </div>`);
}

function pickBar() {
    const layer = currentLayer();
    const names = state.comboInputs.map((index) => keyFace(positionAt(layer, index)).main || "—");
    const node = el(`<div class="pickbar">
        <span><b>Picking combo inputs</b> — click keys on the board</span>
        <span class="n">${esc(names.join(" + ") || "none yet")}</span>
        <span class="right" style="margin-left:auto;display:flex;gap:6px">
            <button class="btn tiny ghost" data-act="clear">Clear</button>
            <button class="btn tiny" data-act="done">Done</button></span></div>`);
    node.querySelector('[data-act="clear"]').addEventListener("click", () => { state.comboInputs = []; render(); });
    node.querySelector('[data-act="done"]').addEventListener("click", () => { state.comboPicking = false; render(); });
    return node;
}

function bench() {
    const model = getModel();
    const layer = currentLayer();
    const codes = (layer?.positions || []).map((position) => position.keycode);
    const counts = {
        key: String(state.selected),
        behaviours: String(new Set(codes.filter((code) => behaviourFor(model, code))).size),
        combos: String((model?.combos || []).length),
        macros: String(new Set(codes.flatMap(macroKeycodes)).size),
        pointing: String(new Set(codes.filter((code) => pointingSlotFor(model, code))).size),
    };
    const node = el(`<div class="card bench">
        <div class="bench-tabs" role="tablist">
            ${TABS.map((tab) => `<button role="tab" data-tab="${tab.id}" aria-selected="${state.tab === tab.id}">
                ${tab.label} <span class="c">${esc(counts[tab.id])}</span></button>`).join("")}
            <span class="bench-right" id="benchRight"></span>
        </div>
        <div class="bench-body" id="benchBody"></div>
    </div>`);
    node.querySelectorAll("[data-tab]").forEach((button) => button.addEventListener("click", () => {
        state.tab = button.dataset.tab;
        state.cell = null;
        render();
    }));
    const body = node.querySelector("#benchBody"), right = node.querySelector("#benchRight");
    ({key: tabKey, behaviours: tabBehaviours, combos: tabCombos, macros: tabMacros, pointing: tabPointing}[state.tab] || tabKey)(body, right);
    return node;
}

/* ── the selected key ──────────────────────────────────────────────────── */
function tabKey(body, right) {
    const model = getModel();
    const layer = currentLayer();
    const position = selectedPosition();
    const face = keyFace(position);
    const behaviour = behaviourFor(model, position?.keycode);
    const combos = combosForKey(model, position);
    const slot = pointingSlotFor(model, position?.keycode);
    right.innerHTML = `<span class="note">index ${position?.layoutIndex ?? "—"} · row ${position?.row ?? "—"} · col ${position?.column ?? "—"} · LED ${LED_INDEX[position?.layoutIndex] ?? "—"}</span>`;

    const node = el(`<div class="tab-grid three">
        <section>
            <div class="sect-h"><h4>This position on ${esc(layerName(layer))}</h4></div>
            <div class="field"><span>Keycode</span>
                <div class="input-row"><input class="input mono" id="keycodeField" value="${esc(position?.keycode || "")}" ${writable() ? "" : "disabled"}>
                <button class="btn" data-act="pick" ${writable() ? "" : "disabled"}>Pick…</button></div></div>
            <dl class="kv" style="margin-top:12px">
                <dt>Resolves to</dt><dd>${esc(face.main || "—")}${face.sub ? ` · ${esc(face.sub)}` : ""}</dd>
                <dt>Stored</dt><dd>${esc(position?.keycode || "—")}</dd>
                <dt>Matrix</dt><dd>row ${position?.row ?? "—"} · col ${position?.column ?? "—"}</dd>
                <dt>LED index</dt><dd>${LED_INDEX[position?.layoutIndex] ?? "—"}</dd>
            </dl>
            ${writable() ? "" : `<div class="unavailable" style="margin-top:12px">${esc(unavailable(model))}</div>`}
        </section>
        <section>
            <div class="sect-h"><h4>This position across the stack</h4></div>
            <div class="stacklist">${[...layers()].reverse().map((other) => {
                const there = positionAt(other, position?.layoutIndex);
                const otherFace = keyFace(there);
                const through = otherFace.kind === "transparent";
                const row = layerColourRow(model, other.index);
                const lit = stageEnabled(model, "layers") && row && !isOff(row.color);
                return `<button class="stackrow ${other.index === layer.index ? "on" : ""}" data-golayer="${layers().indexOf(other)}">
                    <span class="swatch-lg ${lit ? "" : "swatch-off"}" style="width:12px;height:12px;border-radius:4px;${lit ? `background:${css(row.color)}` : ""}"></span>
                    <span class="nm">${esc(layerName(other))}</span>
                    <span class="val ${through ? "dim" : ""}">${through ? "falls through" : esc(otherFace.main || "nothing")}</span>
                    <code class="dim">${esc(there?.keycode || "")}</code></button>`;
            }).join("")}</div>
            <p class="note" style="margin-top:8px">Higher layers win. A transparent key lets the layer underneath answer.</p>
        </section>
        <section>
            <div class="sect-h"><h4>What this key reaches</h4></div>
            <div class="stack" style="gap:8px">
                <button class="reach ${behaviour ? "" : "empty"}" data-goto="behaviours">
                    <span class="rl">Behaviour</span>
                    <span class="rv">${behaviour ? `${esc(behaviour.keycode)} · ${behaviour.steps.length} branches` : "none on this key"}</span>
                    <span class="ra">${behaviour ? "Edit" : "Add"}</span></button>
                <button class="reach ${combos.length ? "" : "empty"}" data-goto="combos">
                    <span class="rl">Combos</span>
                    <span class="rv">${combos.length ? esc(combos.map((combo) => `${combo.badge} → ${combo.outputDisplay || combo.output}`).join(" · ")) : "not part of a combo"}</span>
                    <span class="ra">${combos.length ? "Edit" : "New"}</span></button>
                ${slot ? `<button class="reach" data-goto="pointing"><span class="rl">Pointing</span>
                    <span class="rv">${esc(slot.name || `Slot ${slot.id + 1}`)}</span><span class="ra">Edit</span></button>` : ""}
            </div>
        </section>
    </div>`);
    node.querySelector('[data-act="pick"]')?.addEventListener("click", () => pickKeycodeFor(position.layoutIndex));
    node.querySelectorAll("[data-golayer]").forEach((button) => button.addEventListener("click", () => {
        state.layer = Number(button.dataset.golayer);
        render();
    }));
    node.querySelectorAll("[data-goto]").forEach((button) => button.addEventListener("click", () => {
        state.tab = button.dataset.goto;
        if (state.tab === "behaviours" && behaviour) state.behaviourRow = behaviour.keycode;
        if (state.tab === "combos" && !combos.length) { state.comboOpen = true; state.comboEditId = null; }
        render();
    }));
    body.replaceChildren(node);
}

/* ── behaviours: tap count × tier, drawn as the grid it is ─────────────── */
function tabBehaviours(body, right) {
    const model = getModel();
    const layer = currentLayer();
    const onLayer = [...new Set((layer?.positions || []).map((position) => position.keycode))]
        .map((code) => behaviourFor(model, code)).filter(Boolean);
    const elsewhere = (model?.keyBehaviors || []).filter((row) => !onLayer.includes(row));
    if (!state.behaviourRow || !behaviourFor(model, state.behaviourRow)) {
        state.behaviourRow = behaviourFor(model, selectedPosition()?.keycode)?.keycode || onLayer[0]?.keycode || elsewhere[0]?.keycode || null;
    }
    const behaviour = behaviourFor(model, state.behaviourRow);

    right.replaceChildren();
    const selectedCode = selectedPosition()?.keycode || "";
    if (writable() && selectedCode && !behaviourFor(model, selectedCode)) {
        const add = el(`<button class="btn tiny" data-tip="Give the selected key a behaviour: taps, holds, long holds and repeated-tap branches.">+ Behaviour on ${esc(keyFace(selectedPosition()).main || selectedCode)}</button>`);
        add.addEventListener("click", () => post({
            type: "addBehavior", expectedBase: model.profileIdentity,
            behavior: {keycode: selectedCode, steps: [{tapCount: 0, tap: {helper: "TAP_SENDS", action: selectedCode}}]},
        }));
        right.appendChild(add);
    }

    const item = (row, quiet) => `<button class="rowitem ${row.keycode === state.behaviourRow ? "on" : ""} ${quiet ? "quiet" : ""}" data-row="${esc(row.keycode)}">
        <span class="t">${esc(row.keycode)}</span>
        <span class="m">${behaviourTiers(row).map((tier) => tierDot(model, tier.kind)).join("")} ${row.steps.length} branch${row.steps.length === 1 ? "" : "es"}${quiet ? " · not on this layer" : ""}</span></button>`;

    const node = el(`<div class="tab-split">
        <aside class="rowlist">
            <div class="rowlist-h">On this layer</div>
            ${onLayer.length ? onLayer.map((row) => item(row, false)).join("") : `<p class="note" style="padding:10px 12px">No key behaviour is placed on this layer.</p>`}
            ${elsewhere.length ? `<div class="rowlist-h" style="border-top:1px solid var(--line);border-bottom:0">Elsewhere</div>${elsewhere.map((row) => item(row, true)).join("")}` : ""}
        </aside>
        <div class="beh-main"></div>
    </div>`);
    node.querySelectorAll("[data-row]").forEach((button) => button.addEventListener("click", () => {
        state.behaviourRow = button.dataset.row;
        state.cell = null;
        render();
    }));
    const main = node.querySelector(".beh-main");
    if (behaviour) main.appendChild(behaviourEditor(behaviour));
    else main.appendChild(el(`<p class="note" style="padding:16px">No behaviour selected. Pick a key on the board and add one.</p>`));
    body.replaceChildren(node);
}

function tierDot(model, kind) {
    const colour = feedbackColours(model)[kind];
    const lit = stageEnabled(model, "key") && !isOff(colour);
    return `<i class="fbdot" style="${lit ? `background:${css(colour)}` : "background:none;border-style:dashed"}"></i>`;
}

function behaviourEditor(behaviour) {
    const model = getModel();
    const steps = [...(behaviour.steps || [])].sort((a, b) => a.tapCount - b.tapCount);
    const colours = feedbackColours(model);
    const canEdit = writable();

    const cellFor = (step, kind) => {
        const branch = step[TIER_FIELDS[kind]];
        const id = `${step.tapCount}-${kind}`;
        const open = state.cell === id;
        if (!branch) return `<button class="bcell empty ${open ? "on" : ""}" data-cell="${id}"><span class="plus">+</span></button>`;
        return `<button class="bcell ${open ? "on" : ""}" data-cell="${id}">
            <span class="bk">${esc(branch.action)}</span>
            <span class="bl">${esc(helperLabel(kind, branch.helper))}</span></button>`;
    };
    const branchTint = (tapCount) => {
        const colour = colours.branches[tapCount - 1];
        return stageEnabled(model, "key") && colour && !isOff(colour) ? `border-color:${css(colour)};color:${css(colour)}` : "";
    };

    const node = el(`<div>
        <div class="beh-head">
            <div>
                <div class="row" style="gap:9px"><h3 style="font-size:15px">${esc(behaviour.keycode)}</h3></div>
                <p class="note" style="margin-top:3px">Timing left empty uses the keyboard default, whose duration the firmware does not report.</p>
            </div>
            <div class="right row" style="gap:8px;margin-left:auto">
                <button class="btn ghost" data-act="remove" ${canEdit ? "" : "disabled"}
                    data-tip="Remove this behaviour from the draft. Its keys then send their plain keycode.">Remove behaviour</button>
            </div>
        </div>
        <div class="beh-timing">
            <label class="field"><span>Tap / hold</span><input class="input mono" data-term="tapHoldTerm" value="${esc(zeroBlank(behaviour.tapHoldTerm))}" placeholder="default" ${canEdit ? "" : "disabled"}></label>
            <label class="field"><span>Long hold</span><input class="input mono" data-term="longerHoldTerm" value="${esc(zeroBlank(behaviour.longerHoldTerm))}" placeholder="default" ${canEdit ? "" : "disabled"}></label>
            <label class="field"><span>Repeated taps</span><input class="input mono" data-term="multiTapTerm" value="${esc(zeroBlank(behaviour.multiTapTerm))}" placeholder="default" ${canEdit ? "" : "disabled"}></label>
            <label class="sw" data-tip="Treat this row as a mouse gesture, so pressing it keeps the pointer layer up instead of letting auto-mouse reset.">
                <input type="checkbox" data-anchor ${behaviour.keepsAutoMouseAnchored ? "checked" : ""} ${canEdit ? "" : "disabled"}>
                <span class="track"></span><span class="txt">Keeps auto-mouse anchored</span></label>
        </div>
        <div class="bgrid" style="grid-template-columns:86px repeat(${steps.length}, minmax(150px, 1fr))">
            <span></span>
            ${steps.map((step) => `<div class="bhead"><span class="bn" style="${branchTint(step.tapCount)}">${step.tapCount + 1}×</span>
                <span>${esc(step.tapCountName || `${step.tapCount + 1} taps`)}</span></div>`).join("")}
            ${[["tap", "Tap"], ["hold", "Hold"], ["long", "Long hold"]].map(([kind, name]) => `
                <div class="btier">${tierDot(model, kind)}${name}</div>
                ${steps.map((step) => cellFor(step, kind)).join("")}`).join("")}
        </div>
        <div id="cellEditor"></div>
        <p class="note" style="margin-top:12px">Every cell is one action: what it sends, and how it runs once its threshold passes. Empty cells are dropped when the profile is applied.</p>
    </div>`);

    node.querySelectorAll("[data-cell]").forEach((button) => button.addEventListener("click", () => {
        state.cell = state.cell === button.dataset.cell ? null : button.dataset.cell;
        render();
    }));
    if (state.cell) {
        const [tapCount, kind] = state.cell.split("-");
        const step = steps.find((row) => String(row.tapCount) === tapCount);
        if (step) node.querySelector("#cellEditor").appendChild(cellEditor(behaviour, step, kind));
    }
    node.querySelector('[data-act="remove"]')?.addEventListener("click", () =>
        post({type: "deleteBehavior", keycode: behaviour.keycode, expectedBase: model.profileIdentity}));
    const commit = () => saveBehaviour(node, behaviour);
    node.querySelectorAll("[data-term]").forEach((input) => input.addEventListener("change", commit));
    node.querySelector("[data-anchor]")?.addEventListener("change", commit);
    return node;
}

function cellEditor(behaviour, step, kind) {
    const branch = step[TIER_FIELDS[kind]];
    const canEdit = writable();
    const node = el(`<div class="cell-editor">
        <div class="ce-head"><span class="tag">${step.tapCount + 1}× branch</span><h4>${kind === "long" ? "Long hold" : kind[0].toUpperCase() + kind.slice(1)}</h4>
            <span class="note">${kind === "tap" ? "A tap tier fires on release, so it has no helper." : "Runs once this row's threshold passes."}</span>
            <span class="right"><button class="btn tiny ghost" data-act="close">Done</button></span></div>
        <div class="ce-body">
            <label class="field"><span>Sends</span>
                <div class="input-row"><input class="input mono" data-action value="${esc(branch?.action || "")}" placeholder="nothing" ${canEdit ? "" : "disabled"}>
                <button class="btn" data-act="pick" ${canEdit ? "" : "disabled"}>Pick…</button></div></label>
            ${kind === "tap" ? "<span></span>" : `<label class="field"><span>How it runs</span>
                <select class="input" data-helper ${canEdit ? "" : "disabled"}>
                    ${HOLD_HELPERS.map(([value, text]) => `<option value="${value}" ${branch?.helper === value ? "selected" : ""}>${text}</option>`).join("")}
                </select></label>`}
            ${branch?.helper === "REPEAT_WHILE_HELD" ? `<label class="field"><span>Repeat rate</span>
                <input class="input mono" data-repeat value="${esc(branch.repeatHz || "")}" ${canEdit ? "" : "disabled"}></label>` : "<span></span>"}
            <div class="row" style="gap:8px;align-items:end">
                ${branch ? `<button class="btn ghost" data-act="clear" ${canEdit ? "" : "disabled"}>Remove tier</button>` : ""}
                <button class="btn primary" data-act="keep" ${canEdit ? "" : "disabled"}>Keep in draft</button>
            </div>
        </div>
    </div>`);
    node.querySelector('[data-act="close"]').addEventListener("click", () => { state.cell = null; render(); });
    node.querySelector('[data-act="pick"]')?.addEventListener("click", () => openPicker({
        title: `${kind === "tap" ? "Tap" : kind === "hold" ? "Hold" : "Long hold"} action`,
        context: `${behaviour.keycode} · ${step.tapCount + 1}× branch`,
        seed: branch?.action ? [branch.action] : [],
        onPick: (expression) => {
            node.querySelector("[data-action]").value = expression;
            state.picker = null;
            render();
        },
    }));
    node.querySelector('[data-act="keep"]')?.addEventListener("click", () => {
        const action = node.querySelector("[data-action]").value.trim();
        const helper = kind === "tap" ? "TAP_SENDS" : node.querySelector("[data-helper]").value;
        const repeatHz = node.querySelector("[data-repeat]")?.value || "0";
        saveBehaviour(document, behaviour, {tapCount: step.tapCount, kind, branch: action ? {helper, action, repeatHz} : null});
    });
    node.querySelector('[data-act="clear"]')?.addEventListener("click", () =>
        saveBehaviour(document, behaviour, {tapCount: step.tapCount, kind, branch: null}));
    return node;
}

// One behaviour row, posted whole: the form is the row, and the host applies
// it to the draft.
function saveBehaviour(root, behaviour, change) {
    const model = getModel();
    const read = (name, fallback) => {
        const field = root.querySelector?.(`[data-term="${name}"]`);
        return field ? field.value.trim() || "0" : String(fallback ?? "0");
    };
    const anchorField = root.querySelector?.("[data-anchor]");
    const steps = (behaviour.steps || []).map((step) => {
        const next = {tapCount: step.tapCount};
        for (const [kind, field] of Object.entries(TIER_FIELDS)) {
            const branch = change && change.tapCount === step.tapCount && change.kind === kind ? change.branch : step[field];
            if (!branch) continue;
            next[field] = kind === "tap"
                ? {helper: "TAP_SENDS", action: branch.action}
                : {helper: branch.helper, action: branch.action, repeatHz: branch.repeatHz || "0"};
        }
        return next;
    });
    post({
        type: "saveBehavior",
        expectedBase: model.profileIdentity,
        behavior: {
            keycode: behaviour.keycode,
            tapHoldTerm: read("tapHoldTerm", behaviour.tapHoldTerm),
            longerHoldTerm: read("longerHoldTerm", behaviour.longerHoldTerm),
            multiTapTerm: read("multiTapTerm", behaviour.multiTapTerm),
            keepsAutoMouseAnchored: anchorField ? anchorField.checked : behaviour.keepsAutoMouseAnchored,
            steps,
        },
    });
}

const zeroBlank = (value) => Number(value) ? String(value) : "";

/* ── combos ────────────────────────────────────────────────────────────── */
function tabCombos(body, right) {
    const model = getModel();
    const combos = model?.combos || [];
    const readback = model?.comboReadback || {};
    const canEdit = writable() && readback.writable !== false;

    right.replaceChildren();
    const toggle = el(`<button class="btn tiny" ${canEdit ? "" : "disabled"}>${state.comboOpen ? "Close builder" : "New combo"}</button>`);
    toggle.addEventListener("click", () => {
        state.comboOpen = !state.comboOpen;
        state.comboEditId = null;
        if (!state.comboOpen) state.comboPicking = false;
        render();
    });
    right.appendChild(toggle);

    const layer = currentLayer();
    const node = el(`<div class="tab-split wide">
        <div>
            <div class="row" style="gap:16px;padding:0 0 12px">
                <label class="field" style="width:180px"><span>Hold threshold · all combos</span>
                    <input class="input mono" value="${esc(combos[0]?.holdTermMs ?? "")}" ${canEdit ? "" : "disabled"}
                    data-tip="Shared by every combo, exactly as QMK does it."></label>
                <span class="note" style="margin:18px 0 0 auto">${combos.length} combo${combos.length === 1 ? "" : "s"} read from the keyboard${readback.enabled === false ? " · combos are disabled on the keyboard" : ""}</span>
            </div>
            ${combos.length ? `<table class="t"><thead><tr><th>Combo</th><th>Inputs</th><th>Sends</th><th>Window</th><th>Requires</th><th>On this layer</th><th></th></tr></thead>
                <tbody>${combos.map((combo) => {
                    const here = (combo.inputPositions || []).length > 0;
                    const requires = [combo.mustHold ? "hold" : "", combo.mustTap ? "tap only" : "", combo.ordered ? "in order" : ""].filter(Boolean).join(" · ") || "—";
                    return `<tr><td class="mono">${esc(combo.badge || "")}</td>
                        <td>${(combo.inputDisplays || combo.inputs || []).map((input) => `<span class="tok">${esc(input)}</span>`).join(" + ")}</td>
                        <td class="mono">${esc(combo.outputDisplay || combo.output)}</td>
                        <td class="mono">${esc(combo.termMs ?? "")} ms</td>
                        <td class="muted">${esc(requires)}</td>
                        <td class="${here ? "" : "dim"}">${here ? "reachable" : "inputs not on this layer"}</td>
                        <td style="text-align:right"><button class="btn tiny ghost" data-edit="${esc(String(combo.id))}" ${canEdit ? "" : "disabled"}>Edit</button></td></tr>`;
                }).join("")}</tbody></table>`
                : `<p class="note">${esc(readback.state === "read" ? "This keyboard has no combos stored." : "Combos have not been read from this keyboard.")}</p>`}
        </div>
        <div id="comboSide"></div>
    </div>`);

    node.querySelectorAll("[data-edit]").forEach((button) => button.addEventListener("click", () => {
        const combo = combos.find((row) => String(row.id) === button.dataset.edit);
        state.comboEditId = combo?.id ?? null;
        state.comboOpen = true;
        state.comboOutput = combo?.output || "";
        state.comboInputs = (combo?.inputPositions || []).slice();
        render();
    }));
    const side = node.querySelector("#comboSide");
    side.appendChild(state.comboOpen ? comboBuilder(layer, canEdit) : el(`<div class="empty-card">
        <p class="note">Pick <b>New combo</b> to build one: choose what it sends, then click its input keys straight on the board.</p></div>`));
    body.replaceChildren(node);
}

function comboBuilder(layer, canEdit) {
    const model = getModel();
    const inputs = state.comboInputs.map((index) => positionAt(layer, index)).filter(Boolean);
    const editing = state.comboEditId !== null;
    const original = (model?.combos || []).find((combo) => combo.id === state.comboEditId);
    const node = el(`<div class="card" style="background:var(--surface-2)">
        <div class="card-h" style="padding:11px 13px"><h3>${editing ? `Edit ${esc(original?.badge || "combo")}` : "New combo"}</h3>
            <span class="right">${editing ? `<button class="btn tiny ghost" data-act="delete" ${canEdit ? "" : "disabled"}>Delete</button>` : ""}</span></div>
        <div class="card-b" style="padding:13px;display:grid;gap:11px">
            <div class="field"><span>Sends</span>
                <div class="input-row"><input class="input mono" data-output value="${esc(state.comboOutput)}" ${canEdit ? "" : "disabled"}>
                <button class="btn" data-act="pickout" ${canEdit ? "" : "disabled"}>Pick…</button></div></div>
            <div class="field"><span>Inputs</span>
                <div class="row" style="gap:6px;flex-wrap:wrap">
                    ${inputs.length ? inputs.map((position) => `<span class="chip"><span class="mono">${esc(keyFace(position).main || position.keycode)}</span>
                        <button data-remove="${position.layoutIndex}" style="color:var(--text-3)">✕</button></span>`).join("")
                        : `<span class="note">no inputs yet</span>`}
                </div>
                <div class="row" style="gap:6px;margin-top:4px">
                    <button class="btn tiny ${state.comboPicking ? "primary" : ""}" data-act="pickboard" ${canEdit ? "" : "disabled"}
                        data-tip="Switch the board into input-picking mode; click keys to add or remove them.">${state.comboPicking ? "Picking on board…" : "Pick on board"}</button>
                </div>
            </div>
            <label class="field"><span>Combo window</span>
                <input class="input mono" data-term value="${esc(original?.termMs ?? "")}" placeholder="ms" ${canEdit ? "" : "disabled"}></label>
            <div class="row" style="gap:14px;flex-wrap:wrap">
                <label class="sw"><input type="checkbox" data-musthold ${original?.mustHold ? "checked" : ""} ${canEdit ? "" : "disabled"}><span class="track"></span><span class="txt">Require hold</span></label>
                <label class="sw"><input type="checkbox" data-musttap ${original?.mustTap ? "checked" : ""} ${canEdit ? "" : "disabled"}><span class="track"></span><span class="txt">Tap only</span></label>
                <label class="sw"><input type="checkbox" data-ordered ${original?.ordered ? "checked" : ""} ${canEdit ? "" : "disabled"}><span class="track"></span><span class="txt">In order</span></label>
            </div>
            <div class="row" style="gap:8px">
                <button class="btn primary" data-act="keep" ${canEdit ? "" : "disabled"}>Keep combo in draft</button>
                <button class="btn ghost" data-act="cancel">Cancel</button>
            </div>
            <p class="note">A combo needs its output and at least two inputs, so this form keeps its own state until you keep it.</p>
        </div></div>`);

    node.querySelectorAll("[data-remove]").forEach((button) => button.addEventListener("click", () => {
        state.comboInputs = state.comboInputs.filter((index) => index !== Number(button.dataset.remove));
        render();
    }));
    node.querySelector('[data-act="pickboard"]')?.addEventListener("click", () => { state.comboPicking = !state.comboPicking; render(); });
    node.querySelector('[data-act="pickout"]')?.addEventListener("click", () => openPicker({
        title: "Combo output", context: editing ? original?.badge : "new combo",
        seed: state.comboOutput ? [state.comboOutput] : [],
        onPick: (expression) => { state.comboOutput = expression; state.picker = null; render(); },
    }));
    node.querySelector('[data-act="cancel"]').addEventListener("click", () => {
        state.comboOpen = false; state.comboPicking = false; state.comboEditId = null; render();
    });
    node.querySelector('[data-act="delete"]')?.addEventListener("click", () => post({type: "deleteCombo", id: state.comboEditId}));
    node.querySelector('[data-act="keep"]')?.addEventListener("click", () => {
        const payload = {
            output: node.querySelector("[data-output]").value.trim(),
            inputs: inputs.map((position) => position.keycode),
            termMs: node.querySelector("[data-term]").value,
            holdTermMs: model?.combos?.[0]?.holdTermMs,
            mustHold: node.querySelector("[data-musthold]").checked,
            mustTap: node.querySelector("[data-musttap]").checked,
            ordered: node.querySelector("[data-ordered]").checked,
        };
        post(editing ? {type: "saveCombo", id: state.comboEditId, ...payload} : {type: "addCombo", ...payload});
    });
    return node;
}

/* ── what this layer reaches ───────────────────────────────────────────── */
function tabMacros(body, right) {
    const model = getModel();
    const layer = currentLayer();
    const slots = [...(model?.viaMacros || []), ...(model?.hardcodedMacros || [])];
    const used = [...new Set((layer?.positions || []).flatMap((position) => macroKeycodes(position.keycode)))];
    right.replaceChildren();
    const open = el(`<button class="btn tiny ghost">Open the Macros view</button>`);
    open.addEventListener("click", () => { state.screen = "macros"; render(); });
    right.appendChild(open);
    body.replaceChildren(el(`<div style="padding:2px 0">${used.length
        ? `<table class="t"><thead><tr><th>Slot</th><th>Payload</th><th>Bytes</th><th>Keys on this layer</th></tr></thead><tbody>
            ${used.map((keycode) => {
                const slot = slots.find((row) => row.keycode === keycode);
                const keys = (layer.positions || []).filter((position) => position.keycode === keycode);
                return `<tr><td class="mono">${esc(keycode)}</td>
                    <td class="mono">${esc(slot?.payload || "—")}</td>
                    <td class="mono">${esc(slot?.bytes ?? "—")}</td>
                    <td class="muted">index ${keys.map((position) => position.layoutIndex).join(", ")}</td></tr>`;
            }).join("")}</tbody></table>`
        : `<p class="note" style="padding:14px 0">No macro keycode is placed on this layer.</p>`}</div>`));
}

function tabPointing(body, right) {
    const model = getModel();
    const layer = currentLayer();
    const reached = [...new Set((layer?.positions || []).map((position) => pointingSlotFor(model, position.keycode)).filter(Boolean))];
    right.replaceChildren();
    const open = el(`<button class="btn tiny ghost">Open the Pointing modes view</button>`);
    open.addEventListener("click", () => { state.screen = "pointing"; render(); });
    right.appendChild(open);
    body.replaceChildren(el(`<div style="padding:2px 0">${reached.length
        ? `<div class="pd-reach">${reached.map((slot) => {
            const row = pdColourRow(model, slot.id);
            const keys = (layer.positions || []).filter((position) => pointingSlotFor(model, position.keycode)?.id === slot.id);
            return `<div class="pd-card">
                <div class="row" style="gap:9px">
                    ${slot.kind && row && !isOff(row.color) ? `<span class="swatch-lg" style="width:16px;height:16px;border-radius:5px;background:${css(row.color)}"></span>` : `<span class="swatch-lg swatch-off" style="width:16px;height:16px;border-radius:5px"></span>`}
                    <b>${esc(slot.name || `Slot ${slot.id + 1}`)}</b><span class="tag">slot ${slot.id + 1}</span></div>
                <div class="note">${slot.kind
                    ? `${slot.kind === 2 ? "Scrolling" : "Directional"}${slot.dpi ? ` · ${slot.dpi} DPI` : " · normal pointer speed"}`
                    : "Empty · the keyboard refuses to activate it, so these keys do nothing yet"}</div>
                <div class="note">on ${keys.map((position) => esc(keyFace(position).main || position.keycode)).join(", ")}</div>
                ${slot.kind ? `<div class="note">its colour paints ${esc(row?.locality || "its locality").toLowerCase().replace(/rgb_/, "").replace(/_/g, " ")} while the mode runs — not this key</div>` : ""}
            </div>`;
        }).join("")}</div>`
        : `<p class="note" style="padding:14px 0">No pointing mode is placed on this layer.</p>`}</div>`));
}

/* ── key edits ─────────────────────────────────────────────────────────── */
function pickKeycodeFor(layoutIndex) {
    const layer = currentLayer();
    const position = positionAt(layer, layoutIndex);
    state.selected = layoutIndex;
    openPicker({
        title: `Keycode on ${layerName(layer)}`,
        context: `index ${layoutIndex}`,
        seed: position?.keycode ? [position.keycode] : [],
        onPick: (expression) => {
            state.picker = null;
            post({type: "updateLayoutKeys", layer: layer.name, changes: [{layoutIndex, keycode: expression}]});
        },
    });
}

function swapKeys(from, to) {
    const layer = currentLayer();
    const a = positionAt(layer, from), b = positionAt(layer, to);
    if (!a || !b) return;
    state.selected = to;
    post({type: "updateLayoutKeys", layer: layer.name, changes: [
        {layoutIndex: from, keycode: b.keycode},
        {layoutIndex: to, keycode: a.keycode},
    ]});
}
