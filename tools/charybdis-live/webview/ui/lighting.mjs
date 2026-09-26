// Lighting: the same shape as Keys. The board is the constant, with the layer
// tabs on it, and each stage owns a full-width surface in the workbench, whose
// tabs are the stages in the order the firmware paints them.

import {css, isOff, label as hsvLabel} from "../lib/colour.mjs";
import {el, esc} from "../lib/dom.mjs";
import {LED_INDEX, TRACKBALL_LED} from "../view/geometry.mjs";
import {PD_MODE_IDS, baseColour, feedbackColours, layerColourRow, pdColourRow, stageEnabled, stageIdle, stageInEffect} from "../view/lighting.mjs";
import {branchName, slotCalled, stageOrder, vocabulary, word} from "../view/vocabulary.mjs";
import {canEdit as canEditArea, currentLayer, getModel, layerName, layers, post, render, state, writable, heldLayers, previewing, showLayer} from "../store.mjs";
import * as edits from "../view/edits.mjs";
import {board} from "./board.mjs";
import {colourEditor} from "./colour-editor.mjs";
import {keepInView, layerBar} from "./layerbar.mjs";
import {topbar, unavailable} from "./shell.mjs";
import {draftDot, draftMarks} from "../view/review.mjs";
import {marked} from "./marks.mjs";
import {sectionsIn} from "./settings.mjs";
import {shareHold} from "../view/share.mjs";

// The key-feedback colour rows, named as their feedback owners are.
const SEMANTIC_ROWS = [
    {id: "tapCommittedColor", owner: "KEY_FEEDBACK_GROUP_TAP_COMMITTED"},
    {id: "holdActiveColor", owner: "KEY_FEEDBACK_GROUP_HOLD_ACTIVE"},
    {id: "longHoldActiveColor", owner: "KEY_FEEDBACK_GROUP_LONG_HOLD_ACTIVE"},
];

const stageBit = (model, id) => (model?.rgb?.stages || []).find((stage) => stage.id === id)?.bit;

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
        `<button class="btn ghost" data-act="read" ${model?.device?.health?.busy ? "disabled" : ""}>Read lighting</button>`,
    )}</div>`);
    main.querySelector('[data-act="read"]').addEventListener("click", () => post({type: "refresh"}));

    const content = el(`<div class="content"><div class="pad keys-pad"></div></div>`);
    const pad = content.firstElementChild;
    // The board, what it is showing and the layer tabs under them are one
    // card, as on Keys; on LED groups the board is the LED selector instead.
    const card = el(`<div class="board-card"></div>`);
    const preview = state.stage === "pd" && state.pdPreview && stageEnabled(model, "pd")
        ? pdColourRow(model, state.pdSlot) : null;
    card.appendChild(board(model, layer, {
        mode: onGroups ? "leds" : "light",
        faces: !onGroups,
        held: onGroups ? [] : heldLayers(),
        picks: state.ledPicks,
        trackball: state.trackball,
        pdActive: preview ? {mode: preview.pointingMode, color: preview.color, locality: preview.locality, triggerIndex: undefined} : null,
        onKey: onGroups ? (index) => {
            state.ledPicks = state.ledPicks.includes(index)
                ? state.ledPicks.filter((value) => value !== index) : [...state.ledPicks, index];
            render();
        } : undefined,
        onTrackball: onGroups ? () => { state.trackball = !state.trackball; render(); } : undefined,
    }));
    card.appendChild(el(`<div class="board-foot">
        <b>${onGroups ? "LED selector" : "Preview"}</b>
        <span class="board-foot-text">${onGroups
            ? `click physical LEDs to build a group · ${state.ledPicks.length} selected${state.trackball ? " + trackball" : ""}`
            : `${esc(layerName(layer))}${previewing() && !onGroups
                ? ` with ${esc([...heldLayers()].reverse().map((at) => layerName(layers()[at])).join(", "))} on`
                : ""} over the base effect${preview
                ? ` · ${esc(slotName(model, state.pdSlot))} held, painting ${esc(localityLabel(preview.locality).toLowerCase())}`
                : ""}`}</span>
        ${onGroups ? `<span class="right">
            <button class="btn tiny ghost" data-act="clearleds">Clear selection</button>
            <button class="btn tiny ${state.trackball ? "primary" : "ghost"}" data-act="trackball"
                data-tip="Add or remove the trackball LED, index ${TRACKBALL_LED}, from this selection.">Trackball LED</button></span>`
            : ""}
    </div>`));
    card.querySelector('[data-act="clearleds"]')?.addEventListener("click", () => { state.ledPicks = []; render(); });
    card.querySelector('[data-act="trackball"]')?.addEventListener("click", () => { state.trackball = !state.trackball; render(); });
    card.append(layerBar());
    pad.appendChild(card);

    const marks = draftMarks(model?.draft?.changes);
    // The stage tabs are the paint order: numbered as the firmware paints
    // them, each in the colour it paints now, a stage that is off drawn off.
    // LED groups is no stage; every stage can draw on them.
    const stages = stageOrder(model);
    const stageTab = (stage, index) => {
        const on = stageInEffect(model, stage.id), colour = stageColour(model, stage.id), lit = on && colour && !isOff(colour);
        const idle = stageIdle(model, stage.id);
        return `<button role="tab" class="${on ? "" : "off"}" data-ltab="${stage.id}" aria-selected="${state.stage === stage.id}"
            data-tip="Painted ${index + 1} of ${stages.length}${on ? "" : ` · ${esc(idle || "this stage is off")}`}">
            <span class="n">${index + 1}</span><i class="swatch ${lit ? "" : "swatch-off"}" style="${lit ? `background:${css(colour)}` : ""}"></i>
            ${esc(stage.label)}${marks.lighting.has(stage.id) ? draftDot() : ""}</button>`;
    };
    const bench = el(`<div class="card bench">
        <div class="bench-tabs"><div class="bench-tablist" role="tablist" aria-label="Lighting stages, in paint order">
            ${stages.map(stageTab).join(`<span class="tab-step" aria-hidden="true">›</span>`)}
            <span class="tab-sep" aria-hidden="true"></span>
            <button role="tab" data-ltab="groups" aria-selected="${onGroups}">LED groups${marks.lighting.has("groups") ? draftDot() : ""}</button></div>
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
    keepInView(bench.querySelector(".bench-tablist"));
    pad.appendChild(bench);

    main.appendChild(content);
    return main;
}

const slotName = slotCalled;
const localityLabel = (value) => word(vocabulary(getModel()).localities, value);

function stageSwitch(model, id, bit) {
    const on = stageEnabled(model, id);
    const node = el(`<label class="sw" data-tip="Turn this whole stage off without losing the colours it stores.">
        <input type="checkbox" ${on ? "checked" : ""} ${writable() ? "" : "disabled"}><span class="track"></span>
        <span class="txt">${on ? "Stage on" : "Stage off"}</span></label>`);
    node.querySelector("input").addEventListener("change", (event) => {
        const mask = model.rgb.stageEnableMask ?? 0;
        post(edits.rgbStages(mask, bit, event.target.checked));
    });
    return node;
}

// The colour a stage paints now, for its tab: the layer shown, the pointing
// mode picked, the hold colour for key feedback.
function stageColour(model, id) {
    return {
        base: () => baseColour(model),
        layers: () => layerColourRow(model, currentLayer()?.index)?.color,
        auto: () => model.rgb.automouseFade?.end_color,
        pd: () => pdColourRow(model, state.pdSlot)?.color,
        combo: () => model.rgb.comboFeedback?.color,
        key: () => feedbackColours(model).hold,
    }[id]?.();
}

// How long the auto-mouse colour holds before it fades, as a share of the
// timeout. The keyboard stores milliseconds and keeps them with its global
// policy, so this is a settings section, saved whole; a share cannot outlast
// the timeout, and a new timeout on Mouse carries it along.
function fadeTiming(model, idle) {
    const section = sectionsIn(model, "Lighting").find((entry) => entry.stage === "auto");
    const node = el(`<section><div class="sect-h"><h4>Timing${section && draftMarks(model?.draft?.changes).settings.has(section.id) ? draftDot() : ""}</h4></div></section>`);
    if (!section) {
        node.append(el(`<p class="note">${esc(unavailable(model) || "Choose Read keyboard to load the auto-mouse timing.")}</p>`));
        return node;
    }
    const field = section.fields[0];
    const editable = canEditArea("settings") && !field.readOnly && !idle;
    const whole = Number(field.whole);
    // The slider's track is the timeout: solid while the colour holds, fading
    // after. Dragging previews here and commits once on release, since every
    // commit redraws this control and is a step in the draft's history.
    const control = el(`<label class="field share-field"><span>${esc(field.label)}<b class="mono" data-readout></b></span>
        <input type="range" class="share-range" min="0" max="${esc(field.max)}" step="1" value="${esc(field.value)}" data-macro="${esc(field.macro)}"
            aria-valuetext="" ${editable ? "" : "disabled"} data-tip="${esc(field.hint)}"></label>`);
    const range = control.querySelector("input");
    const note = el(`<p class="note"></p>`);
    const preview = () => {
        const percent = Number(range.value), hold = shareHold(field, percent);
        range.style.setProperty("--share", `${percent}%`);
        range.setAttribute("aria-valuetext", `${percent}%, ${hold} ms`);
        control.querySelector("[data-readout]").textContent = `${percent}%`;
        note.textContent = `Of the ${whole} ms timeout, the colour holds for ${hold} ms, then fades over the last ${whole - hold} ms. The timeout is set on Mouse; changing it keeps this share.`;
    };
    preview();
    range.addEventListener("input", preview);
    const open = el(`<button class="btn tiny" style="justify-self:start">Open Mouse → Auto-mouse</button>`);
    open.addEventListener("click", () => { state.screen = "mouse"; state.reveal = '.settings-group[data-section="autoMouse"]'; render(); });
    range.addEventListener("change", () => post(edits.settingsSection(section, () => range.value, model.settingsEditing?.identity)));
    const box = el(`<div class="stack" style="gap:12px"></div>`);
    box.append(control, note);
    if (!idle) box.append(open);
    if (!editable && !idle) box.append(el(`<div class="unavailable">${esc(unavailable(model) || "This firmware reports the fade timing but cannot save it.")}</div>`));
    node.append(box);
    return node;
}

/* ── the stage surfaces ────────────────────────────────────────────────── */
function stageBody(body) {
    const model = getModel();
    const canEdit = writable();
    const marks = draftMarks(model?.draft?.changes);
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
                <span class="t"><span class="swatch-lg ${lit ? "" : "swatch-off"}" style="width:11px;height:11px;border-radius:3px;display:inline-block;vertical-align:-1px;margin-right:7px;${lit ? `background:${css(row.color)}` : ""}"></span>${esc(layerName(layer))}${marks.lightingLayers.has(layer.index) ? draftDot() : ""}</span>
                <span class="m mono">${esc(row ? hsvLabel(row.color) : "not reported")} · ${esc(word(vocabulary(model).paintModes, row?.mode).toLowerCase())}</span></button>`);
            item.addEventListener("click", () => { showLayer(index); render(); });
            list.append(item);
        });
        const layer = currentLayer();
        const row = layerColourRow(model, layer?.index);
        const main = el(`<div class="tab-grid two" style="gap:22px"></div>`);
        const colour = section(`${layerName(layer)} colour`);
        colour.append(colourControl({
            colour: row?.color, canEdit, title: `${layerName(layer)} layer`,
            offNote: "No colour is stored for this layer, so the base effect shows through wherever it would paint.",
            onChange: (next) => post(edits.layerColour(row.layer, row.mode, next)),
        }));
        const where = section("Where it paints");
        const mode = el(`<label class="field"><span>Which keys light up</span>
            <select class="input" ${canEdit ? "" : "disabled"}>
                ${vocabulary(model).paintModes.map(([id, text]) => `<option value="${id}" ${(row?.mode || "KEYS_MAPPED_ON_THIS_LAYER_ONLY") === id ? "selected" : ""}>${esc(text)}</option>`).join("")}
            </select></label>`);
        mode.querySelector("select").addEventListener("change", (event) =>
            post(edits.layerColour(row.layer, event.target.value, row.color)));
        where.append(stack(mode, el(`<p class="note">Pass-through leaves whatever is underneath visible; transparent keys keep their ▽ on the board either way. The board above is already showing this.</p>`)));
        const overrides = section("LED overrides on this layer", `<span class="right"><button class="btn tiny ghost" data-act="groups">Edit groups</button></span>`);
        overrides.className = "span";
        overrides.append(groupRowsTable(model.rgb.layerLedGroups, "layer"));
        overrides.querySelector('[data-act="groups"]').addEventListener("click", () => { state.stage = "groups"; render(); });
        main.append(colour, where, overrides);
        node.append(list, main);
    }

    if (state.stage === "auto") {
        const fade = model.rgb.automouseFade || {};
        node.className = "tab-grid three";
        // With auto-mouse off the fade never runs, so its controls are shown
        // disabled rather than pretending to matter. Nothing is cleared: the
        // stage switch and these values come back with auto-mouse.
        const idle = stageIdle(model, "auto");
        const editFade = canEdit && !idle;
        if (idle) {
            node.classList.add("idle");
            const notice = el(`<div class="unavailable span"><span>${esc(idle)} A pointer layer turned on by hand shows its own layer lighting. The fade settings below are kept for when auto-mouse is back on.</span>
                <button class="btn tiny" style="justify-self:start">Open Mouse → Auto-mouse</button></div>`);
            notice.querySelector("button").addEventListener("click", () => { state.screen = "mouse"; state.reveal = '.settings-group[data-section="autoMouse"]'; render(); });
            node.append(notice);
        }
        const policy = section("Fade");
        const select = el(`<label class="field"><span>Fade mode</span><select class="input" ${editFade ? "" : "disabled"}>
            ${vocabulary(model).fadeModes.map(([id, text]) => `<option value="${id}" ${fade.mode === id ? "selected" : ""}>${text}</option>`).join("")}</select></label>`);
        select.querySelector("select").addEventListener("change", (event) =>
            post(edits.automouseFade(event.target.value, fade.end_color)));
        policy.append(stack(select, el(`<p class="note">Where the fade lands once the auto-mouse layer drops out. The board shows the destination, not the animation.</p>`)));
        const endColour = section("End colour");
        const unused = fade.mode === "FOLLOW_REAL_DESTINATION";
        endColour.append(unused
            ? el(`<div class="row" style="gap:10px;opacity:.5"><span class="swatch-lg ${isOff(fade.end_color) ? "swatch-off" : ""}" style="width:34px;height:34px;${isOff(fade.end_color) ? "" : `background:${css(fade.end_color)}`}"></span>
                <div><div style="font-size:12.5px">Unused in this mode</div><div class="note mono">${esc(hsvLabel(fade.end_color))}</div></div></div>`)
            : colourControl({colour: fade.end_color, canEdit: editFade, title: "Fade destination",
                onChange: (next) => post(edits.automouseFade(fade.mode, next))}));
        if (unused) endColour.append(el(`<p class="note" style="margin-top:10px">Follow-the-real-destination lands on whatever the board would show once the auto-mouse layer drops out, so the end colour is not read. It stays disabled rather than pretending to matter.</p>`));
        node.append(policy, endColour, fadeTiming(model, idle));
    }

    if (state.stage === "pd") {
        node.className = "tab-split";
        const list = el(`<aside class="rowlist"><div class="rowlist-h">Pointing modes</div></aside>`);
        (model.pdModes || []).forEach((slot) => {
            const row = pdColourRow(model, slot.id);
            const lit = row && !isOff(row.color);
            const item = el(`<button class="rowitem ${slot.id === state.pdSlot ? "on" : ""} ${slot.kind ? "" : "quiet"}" data-slot="${slot.id}">
                <span class="t"><span class="swatch-lg ${lit ? "" : "swatch-off"}" style="width:11px;height:11px;border-radius:3px;display:inline-block;vertical-align:-1px;margin-right:7px;${lit ? `background:${css(row.color)}` : ""}"></span>${esc(slot.displayName || `Slot ${slot.id + 1}`)}${marks.lightingSlots.has(slot.id) ? draftDot() : ""}</span>
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
            onChange: (next) => post(edits.pdModeColour(PD_MODE_IDS[state.pdSlot], row.locality, next)),
        }));
        const where = section("Where it paints");
        const locality = el(`<label class="field"><span>Locality</span>
            <select class="input" ${canEdit ? "" : "disabled"}
                data-tip="The overlay is not drawn on the key that binds the mode; it paints this region while the mode runs.">
            ${vocabulary(model).localities.map(([id, text]) => `<option value="${id}" ${row?.locality === id ? "selected" : ""}>${text}</option>`).join("")}</select></label>`);
        locality.querySelector("select").addEventListener("change", (event) =>
            post(edits.pdModeColour(PD_MODE_IDS[state.pdSlot], event.target.value, row.color)));
        const previewSwitch = el(`<label class="sw"><input type="checkbox" ${state.pdPreview ? "checked" : ""}><span class="track"></span>
            <span class="txt">Preview it active on the board</span></label>`);
        previewSwitch.querySelector("input").addEventListener("change", (event) => { state.pdPreview = event.target.checked; render(); });
        where.append(stack(locality, previewSwitch));
        const note = section("What this stage does");
        note.className = "span";
        note.append(el(`<p class="note" style="max-width:96ch">A pointing-mode colour is an overlay that exists only while the mode is held or toggled. It replaces whatever the layer paints inside its locality for as long as the mode runs — it is never painted on the key that binds it.</p>`));
        note.append(groupRowsTable(model.rgb.pdModeLedGroups, "pdMode"));
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
            onChange: (next) => post(edits.comboFeedback(combo.locality, next)),
        }));
        const where = section("Where it paints");
        const locality = el(`<label class="field"><span>Locality</span><select class="input" ${canEdit ? "" : "disabled"}>
            ${vocabulary(model).localities.map(([id, text]) => `<option value="${id}" ${combo.locality === id ? "selected" : ""}>${text}</option>`).join("")}</select></label>`);
        locality.querySelector("select").addEventListener("change", (event) =>
            post(edits.comboFeedback(event.target.value, combo.color)));
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
            ...SEMANTIC_ROWS.map((row) => ({...row, label: word(vocabulary(model).feedbackOwners, row.owner), colour: feedback[row.id]})),
            ...(feedback.tapBranchColors || []).map((colour, index) => ({id: `branch:${index}`, label: branchName(model, index + 2), colour})),
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
            onChange: (next) => post(edits.keyFeedback(model.rgb.keyBehaviorFeedback, current.id, next)),
        }));
        const policy = section("Policy");
        const commit = el(`<label class="field"><span>Tap commit</span><select class="input" ${canEdit ? "" : "disabled"}>
            ${vocabulary(model).tapCommit.map(([id, text]) => `<option value="${id}" ${feedback.tapCommitMode === id ? "selected" : ""}>${text}</option>`).join("")}</select></label>`);
        commit.querySelector("select").addEventListener("change", (event) =>
            post(edits.keyFeedback(model.rgb.keyBehaviorFeedback, null, null, {tapCommitMode: event.target.value})));
        const locality = el(`<label class="field"><span>Where</span><select class="input" ${canEdit ? "" : "disabled"}>
            ${vocabulary(model).localities.map(([id, text]) => `<option value="${id}" ${feedback.locality === id ? "selected" : ""}>${text}</option>`).join("")}</select></label>`);
        locality.querySelector("select").addEventListener("change", (event) =>
            post(edits.keyFeedback(model.rgb.keyBehaviorFeedback, null, null, {locality: event.target.value})));
        policy.append(stack(commit, locality, el(`<p class="note">The flash interval lives in Settings → Lighting feedback, because the keyboard stores it with its timing.</p>`)));
        const onBoard = section("On the board");
        onBoard.className = "span";
        const colours = feedbackColours(model);
        const dot = (colour) => `<i class="fbdot" style="${stageEnabled(model, "key") && !isOff(colour) ? `background:${css(colour)}` : "background:none;border-style:dashed"}"></i>`;
        onBoard.append(el(`<div class="row" style="gap:12px;flex-wrap:wrap;margin-bottom:10px">
            <span class="chip">${dot(colours.tap)} tap branch</span>
            <span class="chip">${dot(colours.hold)} hold branch</span>
            <span class="chip">${dot(colours.long)} long hold branch</span>
            ${(colours.branches || []).map((colour, index) => `<span class="chip">${dot(colour)} ${esc(branchName(model, index + 2).toLowerCase())}</span>`).join("")}</div>`));
        onBoard.append(el(`<p class="note">These are the colours a key wears on the board: a behaviour's tap, hold and long-hold dots, and the branch numbers in the behaviour grid.${stageEnabled(model, "key") ? "" : " The stage is off, so every one of them is drawn hollow."}</p>`));
        onBoard.append(groupRowsTable(model.rgb.keyBehaviorFeedbackLedGroups, "keyBehavior"));
        main.append(colour, policy, onBoard);
        node.append(list, main);
    }

    if (state.stage === "groups") {
        node.className = "tab-split builder";
        node.append(groupBuilder(model, canEdit), groupTables(model, canEdit));
    }

    body.replaceChildren(node);
}

// A row's owner by the name its dropdown offers, with the owner's mark —
// the layer's colour, the pointing mode's light, the feedback tier's dot —
// as the review shows the same row.
function ownerCell(model, target, owner) {
    const name = rowOwners(model, target).find(([value]) => value === owner)?.[1] ?? owner ?? "—";
    const layer = target === "layer" && /^Layer (\d+)$/.exec(owner || "");
    const slot = target === "pdMode" ? (model.rgb.pdModeColors || []).findIndex((row) => row.pointingMode === owner) : -1;
    const tier = target === "keyBehavior" && {KEY_FEEDBACK_GROUP_TAP_COMMITTED: "tap", KEY_FEEDBACK_GROUP_HOLD_ACTIVE: "hold", KEY_FEEDBACK_GROUP_LONG_HOLD_ACTIVE: "long"}[owner];
    const markOf = layer ? {kind: "layer", layer: Number(layer[1])} : slot >= 0 ? {kind: "pointing", slot} : tier ? {kind: "tier", tier} : null;
    return marked(model, markOf, name);
}

function groupRowsTable(rows, target) {
    const model = getModel();
    if (!rows?.length) return el(`<p class="note" style="margin-top:8px">No LED override rows in this table. Rows override the stage colour on the LEDs they name.</p>`);
    const node = el(`<table class="t" style="margin-top:8px"><thead><tr><th>Owner</th><th>LEDs</th><th>Colour</th><th></th></tr></thead>
        <tbody>${rows.map((row, index) => `<tr>
            <td>${target === "combo" ? "All combos" : ownerCell(model, target, row.owner)}</td>
            <td class="mono">${esc(row.ledGroup)} · ${row.ledIndices.length} LED${row.ledIndices.length === 1 ? "" : "s"}</td>
            <td><span class="swatch-lg ${isOff(row.color) ? "swatch-off" : ""}" style="width:13px;height:13px;border-radius:4px;display:inline-block;vertical-align:-2px;${isOff(row.color) ? "" : `background:${css(row.color)}`}"></span>
                <code class="dim" style="margin-left:6px">${esc(hsvLabel(row.color))}</code>${isOff(row.color) ? ` <span class="note">inherits the stage colour</span>` : ""}</td>
            <td style="text-align:right"><button class="btn tiny ghost" data-remove="${index}" ${writable() ? "" : "disabled"}>Remove</button></td></tr>`).join("")}</tbody></table>`);
    node.querySelectorAll("[data-remove]").forEach((button) => button.addEventListener("click", () =>
        post(edits.deleteLedRow(target, Number(button.dataset.remove)))));
    return node;
}

// Who a row belongs to depends on its table: a layer, a pointing mode, a key
// feedback state, or — for combo feedback — nobody, since every combo shares
// one colour. The values are the names the host's RGB enums use.
function rowOwners(model, target) {
    if (target === "layer") return [...layers().map((layer) => [`Layer ${layer.index}`, layerName(layer)]), ["RGB_LAYER_GROUP_ALL", "All layers"]];
    if (target === "pdMode") return [...(model.rgb.pdModeColors || []).map((row, index) =>
        [row.pointingMode, slotCalled(model, index)]), ["RGB_PD_MODE_GROUP_ALL", "All pointing modes"]];
    if (target === "keyBehavior") return vocabulary(model).feedbackOwners;
    return [];
}

function groupBuilder(model, canEdit) {
    const groups = model.rgb.ledGroups || [];
    const draft = state.ledRow;
    const owners = rowOwners(model, draft.target);
    if (!owners.some(([value]) => value === draft.owner)) draft.owner = owners[0]?.[0] ?? "";
    if (draft.source && !groups.some((group) => group.name === draft.source)) draft.source = "";
    const option = (value, text, current) => `<option value="${esc(value)}" ${value === current ? "selected" : ""}>${esc(text)}</option>`;
    const node = el(`<aside class="stack" style="gap:12px">
        <div class="sect-h"><h4>New row from the selection</h4><span class="right tag">${state.ledPicks.length} LEDs</span></div>
        <label class="field"><span>Table</span><select class="input" data-target ${canEdit ? "" : "disabled"}>
            ${[["layer", "Layer LED groups"], ["pdMode", "Pointing-mode LED groups"], ["combo", "Combo feedback LED groups"], ["keyBehavior", "Key feedback LED groups"]]
                .map(([value, text]) => option(value, text, draft.target)).join("")}</select></label>
        ${owners.length ? `<label class="field"><span>Owner</span><select class="input" data-owner ${canEdit ? "" : "disabled"}>
            ${owners.map(([value, text]) => option(value, text, draft.owner)).join("")}</select></label>` : ""}
        <label class="field"><span>LEDs</span><select class="input" data-source ${canEdit ? "" : "disabled"}>
            ${option("", "Inline selection from the board", draft.source)}
            ${groups.map((group) => option(group.name, `${group.name} · ${group.ledIndices.length} LEDs`, draft.source)).join("")}</select></label>
    </aside>`);
    node.querySelector("[data-target]").addEventListener("change", (event) => { draft.target = event.target.value; draft.owner = ""; render(); });
    node.querySelector("[data-owner]")?.addEventListener("change", (event) => { draft.owner = event.target.value; });
    node.querySelector("[data-source]").addEventListener("change", (event) => { draft.source = event.target.value; });
    node.append(colourEditor({
        colour: state.rowColour || {h: "0", s: "0", v: "0"}, canEdit, title: "Row colour",
        maximumBrightness: model.rgb.maximumBrightness,
        offNote: "A row stored as HSV(0, 0, 0) inherits its stage colour instead of painting its own.",
        onChange: (next) => { state.rowColour = {h: String(next.h), s: String(next.s), v: String(next.v)}; render(); },
    }));
    const actions = el(`<div class="row" style="gap:8px">
        <button class="btn primary" data-act="keep" ${canEdit && (draft.source || state.ledPicks.length) ? "" : "disabled"}
            data-tip="Pick LEDs on the board, or choose a saved group, to give the row something to paint.">Keep row in draft</button>
        <button class="btn ghost" data-act="save" ${canEdit ? "" : "disabled"}
            data-tip="Store this selection as a named group other rows can point at.">Save selection as a group</button></div>`);
    actions.querySelector('[data-act="keep"]').addEventListener("click", () => {
        post(edits.ledRow(draft, ledIndices(), state.rowColour));
    });
    actions.querySelector('[data-act="save"]').addEventListener("click", () =>
        post(edits.saveLedGroup(ledIndices())));
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
        post(edits.deleteLedGroup(button.dataset.delete))));
    for (const [title, rows, target] of [
        ["Rows in the layer table", model.rgb.layerLedGroups, "layer"],
        ["Rows in the pointing-mode table", model.rgb.pdModeLedGroups, "pdMode"],
        ["Rows in the combo table", model.rgb.comboFeedbackLedGroups, "combo"],
        ["Rows in the key feedback table", model.rgb.keyBehaviorFeedbackLedGroups, "keyBehavior"],
    ]) {
        const box = el(`<div><div class="sect-h"><h4>${title}</h4><span class="right tag">${rows?.length || 0} rows</span></div></div>`);
        box.append(groupRowsTable(rows, target));
        node.append(box);
    }
    return node;
}
