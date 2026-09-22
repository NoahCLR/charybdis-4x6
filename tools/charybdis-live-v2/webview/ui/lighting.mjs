// Lighting: the same shape as Keys. The board is the constant, the paint order
// sits under it, and each stage owns a full-width surface in the workbench.

import {css, hsv, isOff, label as hsvLabel} from "../lib/colour.mjs";
import {el, esc} from "../lib/dom.mjs";
import {LED_INDEX, TRACKBALL_LED} from "../view/geometry.mjs";
import {PD_MODE_IDS, STAGE_ORDER, baseColour, feedbackColours, layerColourRow, pdColourRow, stageEnabled} from "../view/lighting.mjs";
import {currentLayer, getModel, layerName, layers, post, render, state, writable} from "../store.mjs";
import {board} from "./board.mjs";
import {colourEditor} from "./colour-editor.mjs";
import {layerBar} from "./layerbar.mjs";
import {topbar, unavailable} from "./shell.mjs";

const TABS = [
    {id: "base", label: "Base effect"},
    {id: "layers", label: "Layer colours"},
    {id: "auto", label: "Auto-mouse fade"},
    {id: "pd", label: "Pointing modes"},
    {id: "combo", label: "Combo feedback"},
    {id: "key", label: "Key feedback"},
    {id: "groups", label: "LED groups"},
];

const LOCALITIES = [
    ["RGB_BOTH_HALVES", "Both halves"], ["RGB_LEFT_HALF", "Left half"], ["RGB_RIGHT_HALF", "Right half"],
    ["RGB_KEY_HALF", "The half holding the trigger key"], ["RGB_KEYS_ONLY", "Only the trigger key"],
];
const FADE_MODES = [
    ["FOLLOW_REAL_DESTINATION", "Follow the real destination"],
    ["END_COLOR_WHERE_BASE_EFFECT_WOULD_SHOW", "End colour where the base effect would show"],
    ["END_COLOR_ON_ALL_KEYS", "End colour on all keys"],
];
const TAP_COMMIT = [
    ["KEY_FEEDBACK_TAP_COMMIT_OFF", "Off"],
    ["KEY_FEEDBACK_TAP_COMMIT_NON_BASE_TAPS", "Non-base taps"],
];
const SEMANTIC_ROWS = [
    {id: "tapCommittedColor", label: "Tap committed"},
    {id: "holdActiveColor", label: "Hold active"},
    {id: "longHoldActiveColor", label: "Long hold active"},
];

const stageBit = (model, id) => (model?.rgb?.stages || [])
    .find((stage) => stage.label === STAGE_ORDER.find((entry) => entry.id === id)?.label)?.bit;

export function screenLighting() {
    const model = getModel();
    if (!model?.rgb?.stages) {
        return el(`<div class="main">${topbar("Lighting", "Nothing read yet.")}
            <div class="content"><div class="pad"><div class="screen-stub"><h3>No lighting read</h3>
            <p class="note">${esc(unavailable(model) || "Choose Read keyboard to load this keyboard's lighting.")}</p></div></div></div></div>`);
    }
    const onGroups = state.stage === "groups";
    const layer = currentLayer();
    const main = el(`<div class="main">${topbar(
        "Lighting",
        "The only colour in this app is colour the keyboard emits. Stages paint in order, and the board below is the result — your draft's result, before anything is applied.",
        `<button class="btn ghost" data-act="read">Read lighting</button>`,
    )}</div>`);
    main.querySelector('[data-act="read"]').addEventListener("click", () => post({type: "refresh"}));

    const content = el(`<div class="content"><div class="pad keys-pad"></div></div>`);
    const pad = content.firstElementChild;
    const stageWrap = el(`<div class="stack" style="gap:10px"></div>`);
    const preview = state.stage === "pd" && state.pdPreview && stageEnabled(model, "pd")
        ? pdColourRow(model, state.pdSlot) : null;
    stageWrap.appendChild(el(`<div class="stage-head">
        <h2>${onGroups ? "LED selector" : "Preview"}</h2>
        <span class="muted" style="font-size:12px">${onGroups
            ? `click physical LEDs to build a group · ${state.ledPicks.length} selected${state.trackball ? " + trackball" : ""}`
            : `${esc(layerName(layer))} over the base effect${preview
                ? ` · ${esc(slotName(model, state.pdSlot))} held, painting ${esc(localityLabel(preview.locality).toLowerCase())}`
                : ""}`}</span>
        ${onGroups ? `<span class="right row" style="gap:6px">
            <button class="btn tiny ghost" data-act="clearleds">Clear selection</button>
            <button class="btn tiny ${state.trackball ? "primary" : "ghost"}" data-act="trackball"
                data-tip="Add or remove the trackball LED, index ${TRACKBALL_LED}, from this selection.">Trackball LED</button></span>` : ""}
    </div>`));
    stageWrap.appendChild(board(model, layer, {
        mode: onGroups ? "leds" : "light",
        faces: !onGroups,
        picks: state.ledPicks,
        trackball: state.trackball,
        pdActive: preview ? {color: preview.color, locality: preview.locality, triggerIndex: undefined} : null,
        onKey: onGroups ? (index) => {
            state.ledPicks = state.ledPicks.includes(index)
                ? state.ledPicks.filter((value) => value !== index) : [...state.ledPicks, index];
            render();
        } : undefined,
        onTrackball: onGroups ? () => { state.trackball = !state.trackball; render(); } : undefined,
    }));
    stageWrap.appendChild(paintOrder(model));
    stageWrap.querySelector('[data-act="clearleds"]')?.addEventListener("click", () => { state.ledPicks = []; render(); });
    stageWrap.querySelector('[data-act="trackball"]')?.addEventListener("click", () => { state.trackball = !state.trackball; render(); });
    pad.appendChild(stageWrap);

    const bench = el(`<div class="card bench">
        <div class="bench-tabs" role="tablist">
            ${TABS.map((tab) => `<button role="tab" data-ltab="${tab.id}" aria-selected="${state.stage === tab.id}">
                ${STAGE_ORDER.some((stage) => stage.id === tab.id) ? `<i class="stagedot ${stageEnabled(model, tab.id) ? "on" : ""}"></i>` : ""}${tab.label}</button>`).join("")}
            <span class="bench-right" id="benchRight"></span>
        </div>
        <div class="bench-body" id="benchBody"></div>
    </div>`);
    bench.querySelectorAll("[data-ltab]").forEach((button) => button.addEventListener("click", () => {
        state.stage = button.dataset.ltab;
        render();
    }));
    const right = bench.querySelector("#benchRight");
    const bit = stageBit(model, state.stage);
    if (bit !== undefined) right.appendChild(stageSwitch(model, state.stage, bit));
    stageBody(bench.querySelector("#benchBody"));
    const bar = layerBar(`<span class="note" style="margin-left:10px">the board above shows this layer</span>`);
    const workbench = el(`<div class="workbench-stack"></div>`);
    workbench.append(bar, bench);
    pad.appendChild(workbench);

    main.appendChild(content);
    return main;
}

const slotName = (model, id) => (model?.pdModes || []).find((slot) => slot.id === id)?.name || `Slot ${id + 1}`;
const localityLabel = (value) => (LOCALITIES.find(([id]) => id === value) || [, value])[1];

function stageSwitch(model, id, bit) {
    const on = stageEnabled(model, id);
    const node = el(`<label class="sw" data-tip="Turn this whole stage off without losing the colours it stores.">
        <input type="checkbox" ${on ? "checked" : ""} ${writable() ? "" : "disabled"}><span class="track"></span>
        <span class="txt">${on ? "Stage on" : "Stage off"}</span></label>`);
    node.querySelector("input").addEventListener("change", (event) => {
        const mask = model.rgb.stageEnableMask ?? 0;
        post({type: "updateRgbStages", stageEnableMask: event.target.checked ? mask | bit : mask & ~bit});
    });
    return node;
}

// The order the firmware paints in, as a row under the board: it explains what
// you are looking at, and doubles as a way into each stage.
function paintOrder(model) {
    const row = el(`<div class="paintorder"><span class="note">painted in order</span></div>`);
    const swatchFor = {
        base: () => baseColour(model),
        layers: () => layerColourRow(model, currentLayer()?.index)?.color,
        auto: () => model.rgb.automouseFade?.end_color,
        pd: () => pdColourRow(model, state.pdSlot)?.color,
        combo: () => model.rgb.comboFeedback?.color,
        key: () => feedbackColours(model).hold,
    };
    STAGE_ORDER.forEach((stage, index) => {
        const on = stageEnabled(model, stage.id);
        const colour = swatchFor[stage.id]();
        const lit = on && colour && !isOff(colour);
        const chip = el(`<button class="pochip ${state.stage === stage.id ? "on" : ""} ${on ? "" : "off"}" data-po="${stage.id}">
            <span class="n">${index + 1}</span>
            <span class="swatch-lg ${lit ? "" : "swatch-off"}" style="width:11px;height:11px;border-radius:3px;${lit ? `background:${css(colour)}` : ""}"></span>
            <span>${stage.label}</span>${on ? "" : `<span class="note">off</span>`}</button>`);
        chip.addEventListener("click", () => { state.stage = stage.id; render(); });
        row.appendChild(chip);
    });
    return row;
}

/* ── the stage surfaces ────────────────────────────────────────────────── */
function stageBody(body) {
    const model = getModel();
    const canEdit = writable();
    const colourControl = (options) => colourEditor({maximumBrightness: model.rgb.maximumBrightness, ...options});
    const node = el(`<div></div>`);
    const stack = (...nodes) => { const box = el(`<div class="stack" style="gap:12px"></div>`); box.append(...nodes); return box; };
    const section = (title, extra = "") => el(`<section><div class="sect-h"><h4>${esc(title)}</h4>${extra}</div></section>`);

    if (state.stage === "base") {
        // The base effect is a VIA read, and the keyboard stores it with the
        // rest of its global policy — so it is edited there, not in the profile.
        const base = model.rgb.baseEffect || {};
        node.className = "tab-grid three";
        const colour = section("Colour");
        colour.append(base.previewColor
            ? colourControl({colour: base.previewColor, title: "Base effect", canEdit: false})
            : el(`<div class="callout">${esc(base.enabled === false ? "The base effect is switched off." : "This effect animates, so it has no single colour to show.")}</div>`));
        const facts = section("What the keyboard reports");
        facts.append(el(`<dl class="kv" style="grid-template-columns:150px 1fr">
            <dt>Effect</dt><dd>${esc(base.effectName || "—")}</dd>
            <dt>Hue</dt><dd>${esc(base.hue ?? "—")}</dd>
            <dt>Saturation</dt><dd>${esc(base.saturation ?? "—")}</dd>
            <dt>Brightness</dt><dd>${esc(base.brightness ?? "—")}${base.brightnessPercent !== undefined ? ` · ${base.brightnessPercent}%` : ""}</dd>
            <dt>Speed</dt><dd>${esc(base.speed ?? "—")}</dd></dl>`));
        const where = section("Where it is edited");
        const jump = el(`<button class="btn" style="justify-self:start">Open Settings → Base lighting</button>`);
        jump.addEventListener("click", () => { state.screen = "settings"; render(); });
        where.append(el(`<p class="note">The base effect is not part of the profile: the keyboard keeps it with its global policy, and both halves report it. Editing it there keeps one value in one place; this board paints whatever it says.</p>`), jump);
        node.append(colour, facts, where);
    }

    if (state.stage === "layers") {
        node.className = "tab-split";
        const list = el(`<aside class="rowlist"><div class="rowlist-h">Layers</div></aside>`);
        layers().forEach((layer, index) => {
            const row = layerColourRow(model, layer.index);
            const lit = row && !isOff(row.color);
            const item = el(`<button class="rowitem ${index === state.layer ? "on" : ""}" data-layer="${index}">
                <span class="t"><span class="swatch-lg ${lit ? "" : "swatch-off"}" style="width:11px;height:11px;border-radius:3px;display:inline-block;vertical-align:-1px;margin-right:7px;${lit ? `background:${css(row.color)}` : ""}"></span>${esc(layerName(layer))}</span>
                <span class="m mono">${esc(row ? hsvLabel(row.color) : "not reported")} · ${row?.mode === "ALL_KEYS" ? "all keys" : "mapped keys"}</span></button>`);
            item.addEventListener("click", () => { state.layer = index; render(); });
            list.append(item);
        });
        const layer = currentLayer();
        const row = layerColourRow(model, layer?.index);
        const main = el(`<div class="tab-grid two" style="gap:22px"></div>`);
        const colour = section(`${layerName(layer)} colour`);
        colour.append(colourControl({
            colour: row?.color, canEdit, title: `${layerName(layer)} layer`,
            offNote: "No colour is stored for this layer, so the base effect shows through wherever it would paint.",
            onChange: (next) => post({type: "updateLayerColor", layer: row.layer, mode: row.mode, ...next}),
        }));
        const where = section("Where it paints");
        const mode = el(`<label class="field"><span>Which keys light up</span>
            <select class="input" ${canEdit ? "" : "disabled"}>
                <option value="KEYS_MAPPED_ON_THIS_LAYER_ONLY" ${row?.mode !== "ALL_KEYS" ? "selected" : ""}>Keys mapped on this layer only</option>
                <option value="ALL_KEYS" ${row?.mode === "ALL_KEYS" ? "selected" : ""}>All keys</option>
            </select></label>`);
        mode.querySelector("select").addEventListener("change", (event) =>
            post({type: "updateLayerColor", layer: row.layer, mode: event.target.value, ...hsvPayload(row.color)}));
        where.append(stack(mode, el(`<p class="note">Pass-through leaves whatever is underneath visible; transparent keys keep their ▽ on the board either way. The board above is already showing this.</p>`)));
        const overrides = section("LED overrides on this layer", `<span class="right"><button class="btn tiny ghost" data-act="groups">Edit groups</button></span>`);
        overrides.className = "span";
        overrides.append(groupRowsTable(model.rgb.layerLedGroups, "layer", (owner) => owner));
        overrides.querySelector('[data-act="groups"]').addEventListener("click", () => { state.stage = "groups"; render(); });
        main.append(colour, where, overrides);
        node.append(list, main);
    }

    if (state.stage === "auto") {
        const fade = model.rgb.automouseFade || {};
        node.className = "tab-grid three";
        const policy = section("Fade");
        const select = el(`<label class="field"><span>Fade mode</span><select class="input" ${canEdit ? "" : "disabled"}>
            ${FADE_MODES.map(([id, text]) => `<option value="${id}" ${fade.mode === id ? "selected" : ""}>${text}</option>`).join("")}</select></label>`);
        select.querySelector("select").addEventListener("change", (event) =>
            post({type: "updateAutomouseFade", mode: event.target.value, ...hsvPayload(fade.end_color)}));
        policy.append(stack(select, el(`<p class="note">Auto-mouse lighting fades toward this destination over the remaining timeout. The timings live in Settings → Auto-mouse.</p>`)));
        const endColour = section("End colour");
        const unused = fade.mode === "FOLLOW_REAL_DESTINATION";
        endColour.append(unused
            ? el(`<div class="row" style="gap:10px;opacity:.5"><span class="swatch-lg ${isOff(fade.end_color) ? "swatch-off" : ""}" style="width:34px;height:34px;${isOff(fade.end_color) ? "" : `background:${css(fade.end_color)}`}"></span>
                <div><div style="font-size:12.5px">Unused in this mode</div><div class="note mono">${esc(hsvLabel(fade.end_color))}</div></div></div>`)
            : colourControl({colour: fade.end_color, canEdit, title: "Fade destination",
                onChange: (next) => post({type: "updateAutomouseFade", mode: fade.mode, ...next})}));
        if (unused) endColour.append(el(`<p class="note" style="margin-top:10px">Follow-the-real-destination lands on whatever the board would show once the auto-mouse layer drops out, so the end colour is not read. It stays disabled rather than pretending to matter.</p>`));
        const note = section("What this stage does");
        note.append(el(`<p class="note">When trackball movement raises the auto-mouse layer, its lighting fades back toward the destination over the remaining timeout. The board shows the destination, not the animation.</p>`));
        node.append(policy, endColour, note);
    }

    if (state.stage === "pd") {
        node.className = "tab-split";
        const list = el(`<aside class="rowlist"><div class="rowlist-h">Pointing modes</div></aside>`);
        (model.pdModes || []).forEach((slot) => {
            const row = pdColourRow(model, slot.id);
            const lit = row && !isOff(row.color);
            const item = el(`<button class="rowitem ${slot.id === state.pdSlot ? "on" : ""} ${slot.kind ? "" : "quiet"}" data-slot="${slot.id}">
                <span class="t"><span class="swatch-lg ${lit ? "" : "swatch-off"}" style="width:11px;height:11px;border-radius:3px;display:inline-block;vertical-align:-1px;margin-right:7px;${lit ? `background:${css(row.color)}` : ""}"></span>${esc(slot.name || `Slot ${slot.id + 1}`)}</span>
                <span class="m mono">${esc(row ? hsvLabel(row.color) : "not reported")} · ${esc(localityLabel(row?.locality).toLowerCase())}</span></button>`);
            item.addEventListener("click", () => { state.pdSlot = slot.id; render(); });
            list.append(item);
        });
        const row = pdColourRow(model, state.pdSlot);
        const main = el(`<div class="tab-grid two" style="gap:22px"></div>`);
        const colour = section(`${slotName(model, state.pdSlot)} colour`);
        colour.append(colourControl({
            colour: row?.color, canEdit, title: `${slotName(model, state.pdSlot)} while active`,
            offNote: "No colour is stored, so this mode paints nothing while it runs.",
            onChange: (next) => post({type: "updatePdModeColor", pointingMode: PD_MODE_IDS[state.pdSlot], locality: row.locality, ...next}),
        }));
        const where = section("Where it paints");
        const locality = el(`<label class="field"><span>Locality</span>
            <select class="input" ${canEdit ? "" : "disabled"}
                data-tip="The overlay is not drawn on the key that binds the mode; it paints this region while the mode runs.">
            ${LOCALITIES.map(([id, text]) => `<option value="${id}" ${row?.locality === id ? "selected" : ""}>${text}</option>`).join("")}</select></label>`);
        locality.querySelector("select").addEventListener("change", (event) =>
            post({type: "updatePdModeColor", pointingMode: PD_MODE_IDS[state.pdSlot], locality: event.target.value, ...hsvPayload(row.color)}));
        const previewSwitch = el(`<label class="sw"><input type="checkbox" ${state.pdPreview ? "checked" : ""}><span class="track"></span>
            <span class="txt">Preview it active on the board</span></label>`);
        previewSwitch.querySelector("input").addEventListener("change", (event) => { state.pdPreview = event.target.checked; render(); });
        where.append(stack(locality, previewSwitch));
        const note = section("What this stage does");
        note.className = "span";
        note.append(el(`<p class="note" style="max-width:96ch">A pointing-mode colour is an overlay that exists only while the mode is held or toggled. It replaces whatever the layer paints inside its locality for as long as the mode runs — it is never painted on the key that binds it.</p>`));
        note.append(groupRowsTable(model.rgb.pdModeLedGroups, "pdMode", (owner) => owner));
        main.append(colour, where, note);
        node.append(list, main);
    }

    if (state.stage === "combo") {
        const combo = model.rgb.comboFeedback || {};
        node.className = "tab-grid three";
        const colour = section("Colour");
        colour.append(colourControl({
            colour: combo.color, canEdit, title: "Combo feedback",
            offNote: "No colour is stored, so combo keys are not repainted while their inputs are held.",
            onChange: (next) => post({type: "updateComboFeedback", locality: combo.locality, ...next}),
        }));
        const where = section("Where it paints");
        const locality = el(`<label class="field"><span>Locality</span><select class="input" ${canEdit ? "" : "disabled"}>
            ${LOCALITIES.map(([id, text]) => `<option value="${id}" ${combo.locality === id ? "selected" : ""}>${text}</option>`).join("")}</select></label>`);
        locality.querySelector("select").addEventListener("change", (event) =>
            post({type: "updateComboFeedback", locality: event.target.value, ...hsvPayload(combo.color)}));
        where.append(stack(locality));
        const onBoard = section("On the board");
        const lit = stageEnabled(model, "combo") && !isOff(combo.color);
        onBoard.append(el(`<div class="row" style="gap:8px;flex-wrap:wrap;margin-bottom:10px">${(model.combos || []).slice(0, 8).map((row) =>
            `<span class="chip"><i class="lbadge" style="${lit ? `border-color:${css(combo.color)}` : ""}">${esc(row.badge || "C")}</i>
            ${esc((row.inputDisplays || row.inputs || []).join(" + "))}</span>`).join("") || `<span class="note">No combos are stored.</span>`}</div>`));
        onBoard.append(el(`<p class="note">While a combo's inputs are held, its keys wear this colour. The badges on the key faces take their outline from it, so an off stage shows plain badges.</p>`));
        node.append(colour, where, onBoard);
    }

    if (state.stage === "key") {
        const feedback = model.rgb.keyBehaviorFeedback || {};
        const rows = [
            ...SEMANTIC_ROWS.map((row) => ({...row, colour: feedback[row.id]})),
            ...(feedback.tapBranchColors || []).map((colour, index) => ({id: `branch:${index}`, label: `Tap count ${index + 2}`, colour})),
        ];
        if (!rows.some((row) => row.id === state.feedbackRow)) state.feedbackRow = rows[0]?.id;
        const current = rows.find((row) => row.id === state.feedbackRow) || rows[0];
        node.className = "tab-split";
        const list = el(`<aside class="rowlist"><div class="rowlist-h">Semantics</div></aside>`);
        rows.forEach((row) => {
            const lit = !isOff(row.colour);
            const item = el(`<button class="rowitem ${row.id === current?.id ? "on" : ""}" data-row="${esc(row.id)}">
                <span class="t"><span class="swatch-lg ${lit ? "" : "swatch-off"}" style="width:11px;height:11px;border-radius:3px;display:inline-block;vertical-align:-1px;margin-right:7px;${lit ? `background:${css(row.colour)}` : ""}"></span>${esc(row.label)}</span>
                <span class="m mono">${esc(hsvLabel(row.colour))}</span></button>`);
            item.addEventListener("click", () => { state.feedbackRow = row.id; render(); });
            list.append(item);
        });
        const main = el(`<div class="tab-grid two" style="gap:22px"></div>`);
        const colour = section(current?.label || "Feedback");
        colour.append(colourControl({
            colour: current?.colour, canEdit, title: current?.label || "",
            offNote: "No colour is stored for this semantic, so the keyboard flashes nothing for it.",
            onChange: (next) => post(feedbackMessage(model, current.id, next)),
        }));
        const policy = section("Policy");
        const commit = el(`<label class="field"><span>Tap commit</span><select class="input" ${canEdit ? "" : "disabled"}>
            ${TAP_COMMIT.map(([id, text]) => `<option value="${id}" ${feedback.tapCommitMode === id ? "selected" : ""}>${text}</option>`).join("")}</select></label>`);
        commit.querySelector("select").addEventListener("change", (event) =>
            post(feedbackMessage(model, null, null, {tapCommitMode: event.target.value})));
        const locality = el(`<label class="field"><span>Where</span><select class="input" ${canEdit ? "" : "disabled"}>
            ${LOCALITIES.map(([id, text]) => `<option value="${id}" ${feedback.locality === id ? "selected" : ""}>${text}</option>`).join("")}</select></label>`);
        locality.querySelector("select").addEventListener("change", (event) =>
            post(feedbackMessage(model, null, null, {locality: event.target.value})));
        policy.append(stack(commit, locality, el(`<p class="note">The flash interval lives in Settings → Lighting feedback, because the keyboard stores it with its timing.</p>`)));
        const onBoard = section("On the board");
        onBoard.className = "span";
        const colours = feedbackColours(model);
        const dot = (colour) => `<i class="fbdot" style="${stageEnabled(model, "key") && !isOff(colour) ? `background:${css(colour)}` : "background:none;border-style:dashed"}"></i>`;
        onBoard.append(el(`<div class="row" style="gap:12px;flex-wrap:wrap;margin-bottom:10px">
            <span class="chip">${dot(colours.tap)} tap branch</span>
            <span class="chip">${dot(colours.hold)} hold branch</span>
            <span class="chip">${dot(colours.long)} long hold branch</span>
            ${(colours.branches || []).map((colour, index) => `<span class="chip">${dot(colour)} tap count ${index + 2}</span>`).join("")}</div>`));
        onBoard.append(el(`<p class="note">These are the colours a key wears on the board: a behaviour's tap, hold and long-hold dots, and the branch numbers in the behaviour grid.${stageEnabled(model, "key") ? "" : " The stage is off, so every one of them is drawn hollow."}</p>`));
        onBoard.append(groupRowsTable(model.rgb.keyBehaviorFeedbackLedGroups, "keyBehavior", (owner) => owner));
        main.append(colour, policy, onBoard);
        node.append(list, main);
    }

    if (state.stage === "groups") {
        node.className = "tab-split builder";
        node.append(groupBuilder(model, canEdit), groupTables(model, canEdit));
    }

    body.replaceChildren(node);
}

const hsvPayload = (colour) => {
    const [h, s, v] = hsv(colour);
    return {h, s, v};
};

// Key feedback is stored as one record, so any change posts the whole thing
// with the one field replaced.
function feedbackMessage(model, rowId, next, overrides = {}) {
    const feedback = model.rgb.keyBehaviorFeedback || {};
    const config = {
        tapBranchColors: (feedback.tapBranchColors || []).map(hsvPayload),
        tapCommittedColor: hsvPayload(feedback.tapCommittedColor),
        holdActiveColor: hsvPayload(feedback.holdActiveColor),
        longHoldActiveColor: hsvPayload(feedback.longHoldActiveColor),
        tapCommitMode: feedback.tapCommitMode,
        locality: feedback.locality,
        ...overrides,
    };
    if (rowId && next) {
        const branch = /^branch:(\d+)$/.exec(rowId);
        if (branch) config.tapBranchColors[Number(branch[1])] = next;
        else config[rowId] = next;
    }
    return {type: "updateKeyBehaviorFeedback", config};
}

function groupRowsTable(rows, target, ownerLabel) {
    if (!rows?.length) return el(`<p class="note" style="margin-top:8px">No LED override rows in this table. Rows override the stage colour on the LEDs they name.</p>`);
    const node = el(`<table class="t" style="margin-top:8px"><thead><tr><th>Owner</th><th>LEDs</th><th>Colour</th><th></th></tr></thead>
        <tbody>${rows.map((row, index) => `<tr>
            <td>${esc(ownerLabel(row.owner) || "—")}</td>
            <td class="mono">${esc(row.ledGroup)} · ${row.ledIndices.length} LED${row.ledIndices.length === 1 ? "" : "s"}</td>
            <td><span class="swatch-lg ${isOff(row.color) ? "swatch-off" : ""}" style="width:13px;height:13px;border-radius:4px;display:inline-block;vertical-align:-2px;${isOff(row.color) ? "" : `background:${css(row.color)}`}"></span>
                <code class="dim" style="margin-left:6px">${esc(hsvLabel(row.color))}</code>${isOff(row.color) ? ` <span class="note">inherits the stage colour</span>` : ""}</td>
            <td style="text-align:right"><button class="btn tiny ghost" data-remove="${index}" ${writable() ? "" : "disabled"}>Remove</button></td></tr>`).join("")}</tbody></table>`);
    node.querySelectorAll("[data-remove]").forEach((button) => button.addEventListener("click", () =>
        post({type: "deleteRgbLedGroup", target, index: Number(button.dataset.remove)})));
    return node;
}

function groupBuilder(model, canEdit) {
    const groups = model.rgb.ledGroups || [];
    const node = el(`<aside class="stack" style="gap:12px">
        <div class="sect-h"><h4>New row from the selection</h4><span class="right tag">${state.ledPicks.length} LEDs</span></div>
        <label class="field"><span>Table</span><select class="input" data-target ${canEdit ? "" : "disabled"}>
            <option value="layer">Layer LED groups</option>
            <option value="pdMode">Pointing-mode LED groups</option>
            <option value="combo">Combo feedback LED groups</option>
            <option value="keyBehavior">Key feedback LED groups</option></select></label>
        <label class="field"><span>Owner</span><select class="input" data-owner ${canEdit ? "" : "disabled"}>
            ${layers().map((layer) => `<option value="Layer ${layer.index}">${esc(layerName(layer))}</option>`).join("")}
            <option value="RGB_LAYER_GROUP_ALL">All layers</option></select></label>
        <label class="field"><span>LEDs</span><select class="input" data-source ${canEdit ? "" : "disabled"}>
            <option value="">Inline selection from the board</option>
            ${groups.map((group) => `<option value="${esc(group.name)}">${esc(group.name)} · ${group.ledIndices.length} LEDs</option>`).join("")}</select></label>
    </aside>`);
    node.append(colourEditor({
        colour: state.rowColour || {h: "0", s: "0", v: "0"}, canEdit, title: "Row colour",
        maximumBrightness: model.rgb.maximumBrightness,
        offNote: "A row stored as HSV(0, 0, 0) inherits its stage colour instead of painting its own.",
        onChange: (next) => { state.rowColour = {h: String(next.h), s: String(next.s), v: String(next.v)}; render(); },
    }));
    const actions = el(`<div class="row" style="gap:8px">
        <button class="btn primary" data-act="keep" ${canEdit ? "" : "disabled"}>Keep row in draft</button>
        <button class="btn ghost" data-act="save" ${canEdit ? "" : "disabled"}
            data-tip="Store this selection as a named group other rows can point at.">Save selection as a group</button></div>`);
    actions.querySelector('[data-act="keep"]').addEventListener("click", () => {
        const target = node.querySelector("[data-target]").value;
        const owner = node.querySelector("[data-owner]").value;
        const source = node.querySelector("[data-source]").value;
        post({type: "addRgbLedGroup", group: {
            target, owner,
            ...(source ? {ledGroupName: source} : {ledIndices: ledIndices()}),
            ...hsvPayload(state.rowColour),
        }});
    });
    actions.querySelector('[data-act="save"]').addEventListener("click", () =>
        post({type: "saveRgbReusableLedGroup", group: {ledIndices: ledIndices()}}));
    node.append(actions);
    node.append(el(`<p class="note">Rows are applied in device order, so a later row wins on the LEDs it shares.</p>`));
    return node;
}

const ledIndices = () => {
    const indices = state.ledPicks.map((index) => LED_INDEX[index]).filter((value) => value !== undefined);
    return state.trackball ? [...indices, TRACKBALL_LED] : indices;
};

function groupTables(model, canEdit) {
    const groups = model.rgb.ledGroups || [];
    const node = el(`<div class="stack" style="gap:16px"></div>`);
    node.append(el(`<div><div class="sect-h"><h4>Reusable groups</h4><span class="right note">one named set, used by many rows</span></div>
        ${groups.length ? `<table class="t"><thead><tr><th>Group</th><th>LEDs</th><th>Used by</th><th></th></tr></thead><tbody>
            ${groups.map((group) => `<tr><td>${esc(group.name)}</td>
                <td class="mono">${esc(group.ledIndices.join(", "))} · ${group.ledIndices.length} LEDs</td>
                <td class="muted">${group.usageCount ? esc(group.usages.map((usage) => `${usage.target}${usage.owner ? ` · ${usage.owner}` : ""}`).join(" · ")) : "not used yet"}</td>
                <td style="text-align:right"><button class="btn tiny ghost" data-delete="${esc(group.name)}"
                    ${canEdit && !group.usageCount ? "" : "disabled"}
                    data-tip="${group.usageCount ? "Rows still refer to this group, so it cannot be deleted." : "Delete this group."}">Remove</button></td></tr>`).join("")}
            </tbody></table>` : `<p class="note">This keyboard has no reusable LED groups yet.</p>`}</div>`));
    node.querySelectorAll("[data-delete]").forEach((button) => button.addEventListener("click", () =>
        post({type: "deleteRgbReusableLedGroup", name: button.dataset.delete})));
    for (const [title, rows, target] of [
        ["Rows in the layer table", model.rgb.layerLedGroups, "layer"],
        ["Rows in the pointing-mode table", model.rgb.pdModeLedGroups, "pdMode"],
        ["Rows in the combo table", model.rgb.comboFeedbackLedGroups, "combo"],
        ["Rows in the key feedback table", model.rgb.keyBehaviorFeedbackLedGroups, "keyBehavior"],
    ]) {
        const box = el(`<div><div class="sect-h"><h4>${title}</h4><span class="right tag">${rows?.length || 0} rows</span></div></div>`);
        box.append(groupRowsTable(rows, target, (owner) => owner));
        node.append(box);
    }
    return node;
}
