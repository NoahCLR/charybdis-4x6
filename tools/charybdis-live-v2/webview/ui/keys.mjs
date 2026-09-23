// Keys: the board stays on screen and the workbench below it switches between
// the key, its behaviour, its combos, and what this layer reaches.

import {css, isOff, label as hsvLabel} from "../lib/colour.mjs";
import {el, esc} from "../lib/dom.mjs";
import {LED_INDEX} from "../view/geometry.mjs";
import {actionLabel, behaviourFor, behaviourListeningTo, canonicalKeycode, behaviourGridSteps, behaviourGroups, behaviourTiers, comboEditInputs, comboGroups, combosForKey, keyFace, keyMeaning, macroReach, pointingReach, pointingSlotFor, pointingVariant, reachKeys} from "../view/keyface.mjs";
import {feedbackColours, layerColourRow, pdColourRow, stageEnabled} from "../view/lighting.mjs";
import {currentLayer, getModel, layerName, layers, positionAt, post, render, selectedPosition, state, writable} from "../store.mjs";
import {board} from "./board.mjs";
import {layerBar} from "./layerbar.mjs";
import {attachLayersControl} from "./layers.mjs";
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
const DEFAULT_REPEAT_HZ = "20";

export function screenKeys() {
    const model = getModel();
    const layer = currentLayer();
    const main = el(`<div class="main">${topbar(
        "Keys",
        "The keyboard stays on screen. Pick a key on it, then work in the tab you need — the key itself, its behaviour, the combos it belongs to, and the macros and pointing modes this layer reaches.",
    )}</div>`);

    const blocked = unavailable(model);
    if (!layer) {
        const content = el(`<div class="content"><div class="pad"><div class="screen-stub">
            <h3>Nothing read yet</h3><p class="note">${esc(blocked || "Choose Read keyboard to load this keyboard's layers.")}</p></div></div></div>`);
        main.appendChild(content);
        return main;
    }

    const bar = layerBar();
    attachLayersControl(bar);
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
    if (state.placement) stage.appendChild(placementBar());
    else if (state.comboPicking) stage.appendChild(pickBar());
    stage.appendChild(board(model, layer, {
        selected: state.selected,
        reach: reachHighlight(model),
        picking: state.comboPicking || Boolean(state.placement),
        inputs: state.comboPicking || (state.comboOpen && state.tab === "combos") ? state.comboInputs : [],
        onKey: (index) => {
            if (state.placement) {
                const placement = state.placement;
                state.placement = null;
                state.selected = index;
                if (writable()) post({type: "updateLayoutKeys", layer: layer.name, changes: [{layoutIndex: index, keycode: placement.keycode}]});
            } else if (state.comboPicking) {
                state.comboInputs = state.comboInputs.includes(index)
                    ? state.comboInputs.filter((value) => value !== index) : [...state.comboInputs, index];
            } else {
                state.selected = index;
                const behaviour = behaviourFor(model, keyMeaning(positionAt(layer, index)));
                if (state.tab === "behaviours" && behaviour) { state.behaviourRow = behaviour.keycode; state.cell = null; }
            }
            render();
        },
        onOpen: writable() ? (index) => pickKeycodeFor(index) : undefined,
        onSwap: writable() ? (from, to) => swapKeys(from, to) : undefined,
    }));
    stage.appendChild(legend(model));
    pad.appendChild(stage);
    const workbench = el(`<div class="workbench-stack"></div>`);
    workbench.append(bar, bench());
    pad.appendChild(workbench);
    main.appendChild(content);
    return main;
}

function placementBar() {
    const placement = state.placement;
    const node = el(`<div class="pickbar">
        <span><b>Placing ${esc(placement?.label || placement?.keycode || "keycode")}</b> — choose a layer, then click its destination key</span>
        <code class="n">${esc(placement?.keycode || "")}</code>
        <span class="right" style="margin-left:auto"><button class="btn tiny ghost" data-act="cancel">Cancel</button></span></div>`);
    node.querySelector('[data-act="cancel"]').addEventListener("click", () => { state.placement = null; render(); });
    return node;
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
        <span class="legend-item dim">${writable() ? "double-click to pick a keycode" : "select a key to read it"}${writable() ? " · drag one key onto another to swap · ⌘C and ⌘V copy between keys · delete makes a key transparent" : " · ⌘C copies a key"}</span>
        ${state.keyClipboard ? `<span class="legend-item">copied <code class="n">${esc(state.keyClipboard.keycode)}</code></span>` : ""}
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
    node.querySelector('[data-act="clear"]').addEventListener("click", () => { state.comboInputs = []; state.comboExtraInputs = []; render(); });
    node.querySelector('[data-act="done"]').addEventListener("click", () => { state.comboPicking = false; render(); });
    return node;
}

// Every tab that answers "what does this layer reach" draws its groups the
// same way: a counted header, the first open and the rest a click away. The
// group a tab does not have simply has no rows.
const GROUP_TITLES = {
    here: "On this layer",
    branches: "Through a behaviour on this layer",
    through: "Through a transparent key",
    belowBranches: "Through a behaviour under a transparent key",
    elsewhere: "Unreachable from this layer",
};

// The order every reach list reads in: the two this layer holds itself, then
// the two it only reaches under a transparent key, then the rest of the board.
// A tab lists the groups it has in any order and they come out in this one, so
// the headers do not move between tabs.
const GROUP_ORDER = ["here", "branches", "through", "belowBranches", "elsewhere"];
const inGroupOrder = (groups) =>
    GROUP_ORDER.map((id) => groups.find((group) => group.id === id)).filter(Boolean);

function groupHeader(tab, id, count) {
    const open = state.groups[tab][id];
    return `<button class="group-h" data-group-tab="${tab}" data-group="${id}" aria-expanded="${open}">
        <span class="chev">${open ? "▾" : "▸"}</span><span class="ttl">${esc(GROUP_TITLES[id])}</span>
        <span class="n">${count}</span></button>`;
}

const groupOpen = (tab, id) => state.groups[tab][id];

const attachGroupToggles = (node) => node.querySelectorAll("[data-group-tab]").forEach((button) =>
    button.addEventListener("click", () => {
        const bag = state.groups[button.dataset.groupTab];
        bag[button.dataset.group] = !bag[button.dataset.group];
        render();
    }));

// The keys the board rings: the ones that reach whatever row is picked in the
// open tab. A behaviour is found on the keys carrying it; anything a branch
// sends is found on the same keys, which is the whole point of saying so.
function reachHighlight(model) {
    const stack = layers(), at = state.layer;
    const found = (groups, picked) => {
        const [group, ...rest] = String(picked).split(":");
        const name = rest.join(":");
        return ({here: groups.onKeys, branches: groups.fromBranches,
            through: groups.throughKeys, belowBranches: groups.fromBranchesBelow}[group] || [])
            .find((entry) => String(entry.name) === name);
    };
    if (state.tab === "behaviours") {
        return state.behaviourRow ? reachKeys(stack, at, {behaviours: [{keycode: state.behaviourRow}]}) : [];
    }
    const picked = state.reachRow[state.tab];
    if (!picked) return [];
    if (state.tab === "combos") {
        const groups = comboGroups(model, stack, at);
        const entry = [groups.onKeys, groups.throughKeys, groups.elsewhere].flat()
            .find((row) => picked.endsWith(`:${row.combo.id}`));
        return reachKeys(stack, at, entry);
    }
    if (state.tab === "macros") return reachKeys(stack, at, found(macroReach(model, stack, at), picked));
    if (state.tab === "pointing") return reachKeys(stack, at, found(pointingReach(model, stack, at), picked));
    return [];
}

// Rows across the tabs pick themselves the same way, and picking the same row
// again lets go of it.
const attachReachRows = (node, tab) => node.querySelectorAll("[data-reach]").forEach((row) =>
    row.addEventListener("click", (event) => {
        if (event.target.closest("button")) return;
        const picked = row.dataset.reach;
        state.reachRow[tab] = state.reachRow[tab] === picked ? null : picked;
        render();
    }));

// One thing can be listed under several routes, so a pick names the route it
// was made from — otherwise picking it in one group would ring the keys of all
// of them.
const reachAttrs = (tab, group, name) => {
    const picked = `${group}:${name}`;
    return ` data-reach="${esc(picked)}"${state.reachRow[tab] === picked ? ' data-picked="true"' : ""}`;
};

// A tab's own number is what this layer stores: a key here, or a behaviour
// mapped here firing it from a branch — the two groups it leads with. What the
// stack lets it reach is in the tab, not in the count.
const storedCount = (reach) =>
    new Set([...reach.onKeys, ...reach.fromBranches].map((entry) => entry.name)).size;

function bench() {
    const model = getModel();
    const layer = currentLayer();
    const counts = {
        key: String(state.selected),
        behaviours: String(behaviourGroups(model, layers(), state.layer).here.length),
        combos: String(comboGroups(model, layers(), state.layer).onKeys.length),
        macros: String(storedCount(macroReach(model, layers(), state.layer))),
        pointing: String(storedCount(pointingReach(model, layers(), state.layer))),
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
    const behaviour = behaviourFor(model, keyMeaning(position));
    const combos = combosForKey(model, position);
    const slot = pointingSlotFor(model, keyMeaning(position));
    right.innerHTML = `<span class="note">index ${position?.layoutIndex ?? "—"} · row ${position?.row ?? "—"} · col ${position?.column ?? "—"} · LED ${LED_INDEX[position?.layoutIndex] ?? "—"}</span>`;

    const node = el(`<div class="tab-grid three">
        <section>
            <div class="sect-h"><h4>This position on ${esc(layerName(layer))}</h4></div>
            <div class="field"><span>Keycode</span>
                <div class="input-row"><input class="input mono" id="keycodeField" value="${esc(keyMeaning(position))}" ${writable() ? "" : "disabled"}>
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
            <p class="note" style="margin-top:8px">Higher layers win, and only among the layers held at the time. A transparent key is answered by the highest layer below that is also held — ${esc(layerName(layers()[0]))} always is, so a layer in between answers only when you hold it too.</p>
        </section>
        <section>
            <div class="sect-h"><h4>What this key reaches</h4></div>
            <div class="stack" style="gap:8px">
                <button class="reach ${behaviour ? "" : "empty"}" data-goto="behaviours">
                    <span class="rl">Behaviour</span>
                    <span class="rv">${behaviour ? `${esc(actionLabel(model, behaviour.keycode))} · ${behaviour.steps.length} branch${behaviour.steps.length === 1 ? "" : "es"}` : "none on this key"}</span>
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
    node.querySelector("#keycodeField")?.addEventListener("change", (event) => {
        const written = event.target.value.trim();
        if (!written || written === keyMeaning(position)) return;
        post({type: "updateLayoutKeys", layer: layer.name, changes: [{layoutIndex: position.layoutIndex, keycode: written}]});
    });
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
    const {here, through, elsewhere} = behaviourGroups(model, layers(), state.layer);
    if (!state.behaviourRow || !behaviourFor(model, state.behaviourRow)) {
        state.behaviourRow = behaviourFor(model, keyMeaning(selectedPosition()))?.keycode
            || here[0]?.keycode || through[0]?.row.keycode || elsewhere[0]?.keycode || null;
    }
    const behaviour = behaviourFor(model, state.behaviourRow);

    right.replaceChildren();
    const selectedCode = keyMeaning(selectedPosition());
    if (writable() && selectedCode && !behaviourFor(model, selectedCode)) {
        const add = el(`<button class="btn tiny" data-tip="Give the selected key a behaviour: taps, holds, long holds and repeated-tap branches.">+ Behaviour on ${esc(keyFace(selectedPosition()).main || selectedCode)}</button>`);
        add.addEventListener("click", () => {
            state.behaviourRow = selectedCode;
            state.cell = null;
            post({
                type: "addBehavior", expectedBase: model.profileIdentity,
                behavior: {keycode: selectedCode, steps: [{tapCount: 0, tap: {helper: "TAP_SENDS", action: selectedCode}}]},
            });
        });
        right.appendChild(add);
    }

    const item = (row, note) => `<button class="rowitem ${row.keycode === state.behaviourRow ? "on" : ""} ${note ? "quiet" : ""}" data-row="${esc(row.keycode)}">
        <span class="t">${esc(actionLabel(model, row.keycode))}${actionLabel(model, row.keycode) === row.keycode ? ""
            : ` <code class="dim">${esc(row.keycode)}</code>`}</span>
        <span class="m">${behaviourTiers(row).map((tier) => tierDot(model, tier.kind)).join("")} ${row.steps.length} branch${row.steps.length === 1 ? "" : "es"}${note ? ` · ${esc(note)}` : ""}</span></button>`;

    // A selection made anywhere else — the board, the Key tab, a fresh
    // behaviour — can land in a closed group, so the group holding it opens
    // once when the selection moves there. Closing it again then sticks.
    const groups = [
        {id: "here", rows: here.map((row) => ({row, note: ""})),
            empty: "No key behaviour is placed on this layer."},
        {id: "through", rows: through.map((entry) => ({row: entry.row, note: `on ${sourceLabel(entry)}`})),
            empty: "No transparent key falls through to a behaviour."},
        {id: "elsewhere", rows: elsewhere.map((row) => ({row, note: "not on this layer"})),
            empty: "Every behaviour on the board is reached from this layer."},
    ];
    if (state.behaviourRow !== state.behaviourRowShown) {
        const holding = groups.find((group) => group.rows.some((entry) => entry.row.keycode === state.behaviourRow));
        if (holding) state.groups.behaviours[holding.id] = true;
        state.behaviourRowShown = state.behaviourRow;
    }

    const section = (group) => `<div class="rowgroup">
        ${groupHeader("behaviours", group.id, group.rows.length)}
        ${groupOpen("behaviours", group.id)
            ? (group.rows.length ? group.rows.map((entry) => item(entry.row, entry.note)).join("")
                : `<p class="note" style="padding:10px 12px">${esc(group.empty)}</p>`)
            : ""}</div>`;

    const node = el(`<div class="tab-split">
        <aside class="rowlist">
            ${inGroupOrder(groups).map(section).join("")}
        </aside>
        <div class="beh-main"></div>
    </div>`);
    attachGroupToggles(node);
    node.querySelectorAll("[data-row]").forEach((button) => button.addEventListener("click", () => {
        state.behaviourRow = button.dataset.row;
        state.behaviourRowShown = button.dataset.row;
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

// An empty timing field falls back to the keyboard's own default, which it
// reports and Settings · Key Timing edits — so the note names both.
function timingDefaultsNote(model) {
    const defaults = model?.behaviorTimingDefaults || {};
    const values = [["tap / hold", defaults.tapHoldTerm], ["long hold", defaults.longerHoldTerm], ["repeated taps", defaults.multiTapTerm]]
        .filter(([, value]) => String(value ?? "").trim() !== "")
        .map(([name, value]) => `${name} ${value} ms`);
    return values.length
        ? `Timing left empty uses the keyboard default from Settings · Key Timing: ${values.join(", ")}.`
        : "Timing left empty uses the keyboard default from Settings · Key Timing.";
}

function behaviourEditor(behaviour) {
    const model = getModel();
    const steps = behaviourGridSteps(behaviour, model?.behaviorEditing?.maxTapStepsPerBehavior);
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
                <div class="row" style="gap:9px"><h3 style="font-size:15px">${esc(actionLabel(model, behaviour.keycode))}</h3>
                    ${actionLabel(model, behaviour.keycode) === behaviour.keycode ? "" : `<code class="dim">${esc(behaviour.keycode)}</code>`}</div>
                <p class="note" style="margin-top:3px">${esc(timingDefaultsNote(model))}</p>
            </div>
            <div class="right row" style="gap:8px;margin-left:auto">
                <button class="btn ghost" data-act="rekey" ${canEdit ? "" : "disabled"}
                    data-tip="Pick the key this behaviour listens to. Everything it does moves with it.">Change key…</button>
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
    node.querySelector('[data-act="rekey"]')?.addEventListener("click", () => pickBehaviourKey(behaviour));
    if (state.retarget?.from === behaviour.keycode) node.append(retargetPrompt(behaviour));
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
            ${kind === "tap" ? "<span></span>" : `<label class="field" data-repeat-field ${branch?.helper === "REPEAT_WHILE_HELD" ? "" : "hidden"}><span>Repeat rate · Hz</span>
                <input class="input mono" type="number" min="1" max="100" step="1" data-repeat value="${esc(Number(branch?.repeatHz) > 0 ? branch.repeatHz : DEFAULT_REPEAT_HZ)}" ${canEdit ? "" : "disabled"}></label>`}
            <div class="row" style="gap:8px;align-items:end">
                ${branch ? `<button class="btn ghost" data-act="clear" ${canEdit ? "" : "disabled"}>Remove tier</button>` : ""}
                <span class="note">Changes are kept in the draft automatically.</span>
            </div>
        </div>
    </div>`);
    const stage = (action = node.querySelector("[data-action]").value.trim()) => {
        const helper = kind === "tap" ? "TAP_SENDS" : node.querySelector("[data-helper]").value;
        // The rate only means something for "repeat while held", and there it
        // must be 1–100 Hz; the field starts at a valid rate the first time
        // the helper is chosen, rather than posting 0.
        const repeatHz = helper === "REPEAT_WHILE_HELD" ? (node.querySelector("[data-repeat]")?.value.trim() || DEFAULT_REPEAT_HZ) : "0";
        saveBehaviour(document, behaviour, {
            tapCount: step.tapCount,
            kind,
            branch: action ? {helper, action, repeatHz} : null,
        });
    };
    node.querySelector('[data-act="close"]').addEventListener("click", () => { state.cell = null; render(); });
    node.querySelector('[data-act="pick"]')?.addEventListener("click", () => openPicker({
        title: `${kind === "tap" ? "Tap" : kind === "hold" ? "Hold" : "Long hold"} action`,
        context: `${behaviour.keycode} · ${step.tapCount + 1}× branch`,
        seed: branch?.action ? [branch.action] : [],
        onPick: (expression) => {
            state.picker = null;
            stage(expression);
            render();
        },
    }));
    node.querySelector("[data-helper]")?.addEventListener("change", (event) => {
        const field = node.querySelector("[data-repeat-field]");
        if (field) field.hidden = event.target.value !== "REPEAT_WHILE_HELD";
    });
    node.querySelectorAll("[data-action], [data-helper], [data-repeat]").forEach((field) =>
        field.addEventListener("change", () => stage()));
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
    const sourceSteps = new Map((behaviour.steps || []).map((step) => [step.tapCount, step]));
    if (change && !sourceSteps.has(change.tapCount)) sourceSteps.set(change.tapCount, {tapCount: change.tapCount});
    const steps = [...sourceSteps.values()].sort((left, right) => left.tapCount - right.tapCount).map((step) => {
        const next = {tapCount: step.tapCount};
        for (const [kind, field] of Object.entries(TIER_FIELDS)) {
            const branch = change && change.tapCount === step.tapCount && change.kind === kind ? change.branch : step[field];
            if (!branch) continue;
            next[field] = kind === "tap"
                ? {helper: "TAP_SENDS", action: branch.action}
                : {helper: branch.helper, action: branch.action, repeatHz: branch.repeatHz || "0"};
        }
        return next;
    }).filter((step) => step.tap || step.hold || step.longHold);
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

// QMK keeps one hold threshold for every combo, and falls back to the tapping
// term when nothing is stored — so a keyboard with no combos yet still has an
// answer, and it is the device's own, not a number this app invented.
const comboHoldTerm = (model, written) => String(written || model?.combos?.[0]?.holdTermMs
    || model?.behaviorTimingDefaults?.tappingTerm || "").trim();

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
        state.comboInputs = []; state.comboInputCodes = {}; state.comboExtraInputs = [];
        if (!state.comboOpen) state.comboPicking = false;
        render();
    });
    right.appendChild(toggle);

    const layer = currentLayer();
    const node = el(`<div class="tab-split wide">
        <div>
            <div class="row" style="gap:16px;padding:0 0 12px">
                <label class="field" style="width:180px"><span>Hold threshold · all combos</span>
                    <input class="input mono" id="comboHoldTerm" value="${esc(combos[0]?.holdTermMs ?? "")}"
                    placeholder="${esc(model?.behaviorTimingDefaults?.tappingTerm ?? "")}" ${canEdit ? "" : "disabled"}
                    data-tip="Shared by every combo, exactly as QMK does it. Empty means the keyboard's tapping term."></label>
                <span class="note" style="margin:18px 0 0 auto">${combos.length} combo${combos.length === 1 ? "" : "s"} read from the keyboard${readback.enabled === false ? " · combos are disabled on the keyboard" : ""}</span>
            </div>
            <div id="comboTable"></div>
        </div>
        <div id="comboSide"></div>
    </div>`);

    // One threshold for every combo, the way QMK stores it, so it is posted on
    // its own rather than riding along with whichever row is saved next.
    node.querySelector("#comboHoldTerm")?.addEventListener("change", (event) => {
        const written = event.target.value.trim();
        if (written === String(combos[0]?.holdTermMs ?? "")) return;
        post({type: "updateComboHoldTerm", holdTermMs: comboHoldTerm(model, written), expectedBase: model.profileIdentity});
    });
    const groupsOf = comboGroups(model, layers(), state.layer);
    const row = (group, entry, reachedBy) => {
        const combo = entry.combo;
        const requires = [combo.mustHold ? "hold" : "", combo.mustTap ? "tap only" : "", combo.ordered ? "in order" : ""].filter(Boolean).join(" · ") || "—";
        return `<tr${reachAttrs("combos", group, combo.id)}><td class="mono">${esc(combo.badge || "")}</td>
            <td>${(combo.inputDisplays || combo.inputs || []).map((input) => `<span class="tok">${esc(input)}</span>`).join(" + ")}</td>
            <td class="mono">${esc(combo.outputDisplay || combo.output)}</td>
            <td class="mono">${esc(combo.termMs ?? "")} ms</td>
            <td class="muted">${esc(requires)}</td>
            <td class="muted">${reachedBy}</td>
            <td style="text-align:right"><button class="btn tiny ghost" data-edit="${esc(String(combo.id))}" ${canEdit ? "" : "disabled"}>Edit</button></td></tr>`;
    };
    const comboGroupRows = [
        {id: "here", rows: groupsOf.onKeys.map((entry) => row("here", entry, keysReach(entry.keys))),
            empty: readback.state === "read" ? "No combo has all of its inputs on this layer." : "Combos have not been read from this keyboard."},
        {id: "through", rows: groupsOf.throughKeys.map((entry) => row("through", entry, keysReach(entry.keys))),
            empty: "No combo is completed by keys falling through."},
        {id: "elsewhere", rows: groupsOf.elsewhere.map((entry) => row("elsewhere", entry,
            entry.inputs ? `${entry.covered} of ${entry.inputs} inputs, never at once` : "no inputs")),
            empty: "Every combo on the board fires from this layer."},
    ];
    const comboTable = reachTable("combos", comboGroupRows, [
        ["Combo", "7%"], ["Inputs", "31%"], ["Sends", "13%"], ["Window", "8%"],
        ["Requires", "12%"], ["Reached by", "21%"], ["", "8%"]]);
    attachReachRows(comboTable, "combos");
    node.querySelector("#comboTable").replaceWith(comboTable);

    node.querySelectorAll("[data-edit]").forEach((button) => button.addEventListener("click", () => {
        const combo = combos.find((row) => String(row.id) === button.dataset.edit);
        state.comboEditId = combo?.id ?? null;
        state.comboOpen = true;
        state.comboOutput = combo?.output || "";
        const inputs = comboEditInputs(model, layers(), state.layer, combo);
        state.comboInputs = inputs.positions;
        state.comboInputCodes = inputs.codes;
        state.comboExtraInputs = inputs.extras;
        render();
    }));
    const side = node.querySelector("#comboSide");
    side.appendChild(state.comboOpen ? comboBuilder(layer, canEdit, () => comboHoldTerm(model, node.querySelector("#comboHoldTerm")?.value)) : el(`<div class="empty-card">
        <p class="note">Pick <b>New combo</b> to build one: choose what it sends, then click its input keys straight on the board.</p></div>`));
    body.replaceChildren(node);
}

function comboBuilder(layer, canEdit, holdTerm) {
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
                    ${inputs.length ? inputs.map((position) => `<span class="chip"><span class="mono">${esc(state.comboInputCodes[position.layoutIndex]
                        ? actionLabel(model, state.comboInputCodes[position.layoutIndex]) : keyFace(position).main || keyMeaning(position))}</span>
                        <button data-remove="${position.layoutIndex}" style="color:var(--text-3)">✕</button></span>`).join("")
                        : (state.comboExtraInputs.length ? "" : `<span class="note">no inputs yet</span>`)}
                    ${state.comboExtraInputs.map((input) => `<span class="chip" data-tip="Not reachable from this layer, so it is not on the board. It stays an input unless you remove it.">
                        <span class="mono">${esc(actionLabel(model, input))}</span>
                        <button data-remove-extra="${esc(input)}" style="color:var(--text-3)">✕</button></span>`).join("")}
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
    node.querySelectorAll("[data-remove-extra]").forEach((button) => button.addEventListener("click", () => {
        state.comboExtraInputs = state.comboExtraInputs.filter((input) => input !== button.dataset.removeExtra);
        render();
    }));
    node.querySelector('[data-act="pickboard"]')?.addEventListener("click", () => { state.comboPicking = !state.comboPicking; render(); });
    node.querySelector('[data-act="pickout"]')?.addEventListener("click", () => openPicker({
        title: "Combo output", context: editing ? original?.badge : "new combo",
        seed: state.comboOutput ? [state.comboOutput] : [],
        onPick: (expression) => { state.comboOutput = expression; state.picker = null; render(); },
    }));
    node.querySelector('[data-act="cancel"]').addEventListener("click", () => {
        state.comboOpen = false; state.comboPicking = false; state.comboEditId = null;
        state.comboInputs = []; state.comboInputCodes = {}; state.comboExtraInputs = []; render();
    });
    node.querySelector('[data-act="delete"]')?.addEventListener("click", () => post({type: "deleteCombo", id: state.comboEditId}));
    node.querySelector('[data-act="keep"]')?.addEventListener("click", () => {
        const payload = {
            output: node.querySelector("[data-output]").value.trim(),
            inputs: [...inputs.map((position) => state.comboInputCodes[position.layoutIndex] ?? position.keycode), ...state.comboExtraInputs],
            termMs: node.querySelector("[data-term]").value,
            holdTermMs: holdTerm(),
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
    const slots = [...(model?.viaMacros || []), ...(model?.hardcodedMacros || [])];
    const reach = macroReach(model, layers(), state.layer);
    right.replaceChildren();
    const open = el(`<button class="btn tiny ghost">Open the Macros view</button>`);
    open.addEventListener("click", () => { state.screen = "macros"; render(); });
    right.appendChild(open);

    const row = (group, keycode, reachedBy) => {
        const slot = slots.find((entry) => entry.keycode === keycode);
        return `<tr${reachAttrs("macros", group, keycode)}>
            <td>${esc(actionLabel(model, keycode))} <code class="dim">${esc(keycode)}</code></td>
            <td class="mono">${esc(slot?.payload || "—")}</td>
            <td class="muted">${reachedBy}</td>
            <td style="text-align:right">${slot ? `<button class="btn tiny ghost" data-editmacro="${esc(keycode)}">Edit</button>` : ""}</td></tr>`;
    };
    const groups = [
        {id: "here", rows: reach.onKeys.map((entry) => row("here", entry.name, reachLabel(model, entry))),
            empty: "No macro keycode is placed on this layer."},
        {id: "through", rows: reach.throughKeys.map((entry) => row("through", entry.name, reachLabel(model, entry))),
            empty: "No transparent key falls through to a macro key."},
        {id: "branches", rows: reach.fromBranches.map((entry) => row("branches", entry.name, reachLabel(model, entry))),
            empty: "No behaviour mapped on this layer sends a macro."},
        {id: "belowBranches", rows: reach.fromBranchesBelow.map((entry) => row("belowBranches", entry.name, reachLabel(model, entry))),
            empty: "No behaviour under a transparent key sends a macro."},
        {id: "elsewhere", rows: reach.elsewhere.map((keycode) => row("elsewhere", keycode, "not reached from this layer")),
            empty: "Every stored macro is reached from this layer."},
    ];

    const node = reachTable("macros", groups, [["Slot", "24%"], ["Payload", "44%"], ["Reached by", "24%"], ["", "8%"]]);
    attachReachRows(node, "macros");
    node.querySelectorAll("[data-editmacro]").forEach((button) => button.addEventListener("click", () => {
        const keycode = button.dataset.editmacro;
        state.macroBank = (model?.hardcodedMacros || []).some((entry) => entry.keycode === keycode) ? "user" : "via";
        state.macroSlot = keycode;
        state.screen = "macros";
        render();
    }));
    body.replaceChildren(node);
}

// Where a reach came from, in the words of the thing that reaches it. A
// transparent key can be answered by more than one layer, so each is named —
// the default layer always answers, any other only while it is held too.
const sourceLabel = (entry) =>
    esc(layerName(entry.layer)) + (entry.whileHeld ? " when held" : "");

const keysReach = (keys) => {
    const indexes = [...new Set(keys.map((entry) => entry.position.layoutIndex))];
    const sources = [...new Set(keys.filter((entry) => entry.fellThrough).map(sourceLabel))];
    return `index ${indexes.join(", ")}${sources.length ? ` · on ${sources.join(", ")}` : ""}`;
};
// A behaviour that only answers through a transparent key still fires its
// branches, so it belongs here — named with the layer that holds it, because
// that is the part this layer does not store.
// A branch names both what it sends and the behaviour it comes from — but only
// when those differ from what the row is already titled with. A macro row is
// its own macro, so repeating it would be noise; a pointing mode is a slot, and
// which of its two keycodes a branch sends is the whole point.
const branchReach = (model, entry, showSends = true) => entry.behaviours.map((row) => {
    const from = `from ${esc(actionLabel(model, row.keycode))}`
        + (row.layer ? ` · on ${sourceLabel(row)}` : "");
    return showSends && row.action && String(entry.name) !== String(row.action)
        ? `sends ${esc(actionLabel(model, row.action))} · ${from}` : from;
}).join(", ");

// Every way this layer reaches one thing, not only the way it was grouped by.
// A macro can sit on a key here *and* be fired from a behaviour's branch; the
// board rings both sets of keys, so the row has to name both or the two
// disagree about the same entry.
const reachLabel = (model, entry) => [
    entry.keys?.length ? keysReach(entry.keys) : "",
    entry.behaviours?.length ? branchReach(model, entry) : "",
].filter(Boolean).join(" · ");

// A grouped table: the column header once, then a counted header row per
// group. One table keeps the columns lined up across the groups being
// compared, which is the point of showing them together. Each column is given
// its width, so unfolding a group never re-sizes the ones already on screen.
function reachTable(tab, groups, columns) {
    const section = (group) => `<tbody class="rowgroup">
        <tr><td colspan="${columns.length}" class="t-group">${groupHeader(tab, group.id, group.rows.length)}</td></tr>
        ${groupOpen(tab, group.id)
            ? (group.rows.length ? group.rows.join("")
                : `<tr><td colspan="${columns.length}"><p class="note">${esc(group.empty)}</p></td></tr>`)
            : ""}</tbody>`;
    const node = el(`<div style="padding:2px 0"><table class="t fixed">
        <colgroup>${columns.map(([, width]) => `<col style="width:${width}">`).join("")}</colgroup>
        <thead><tr>${columns.map(([name]) => `<th>${esc(name)}</th>`).join("")}</tr></thead>
        ${inGroupOrder(groups).map(section).join("")}
    </table></div>`);
    attachGroupToggles(node);
    return node;
}

function tabPointing(body, right) {
    const model = getModel();
    const reach = pointingReach(model, layers(), state.layer);
    right.replaceChildren();
    const open = el(`<button class="btn tiny ghost">Open the Pointing modes view</button>`);
    open.addEventListener("click", () => { state.screen = "pointing"; render(); });
    right.appendChild(open);

    const card = (group, slotId, reachedBy, variant) => {
        const slot = (model?.pdModes || []).find((entry) => entry.id === Number(slotId));
        if (!slot) return "";
        const row = pdColourRow(model, slot.id);
        const lit = slot.kind && row && !isOff(row.color);
        return `<div class="pd-card"${reachAttrs("pointing", group, slot.id)}>
            <div class="row" style="gap:9px">
                <span class="swatch-lg ${lit ? "" : "swatch-off"}" style="width:16px;height:16px;border-radius:5px;${lit ? `background:${css(row.color)}` : ""}"></span>
                <b>${esc(slot.name || `Slot ${slot.id + 1}`)}${variant ? ` · ${esc(variant)}` : ""}</b>
                <span class="tag">slot ${slot.id + 1}</span>
                <button class="btn tiny ghost" data-editpd="${slot.id}" style="margin-left:auto">Edit</button></div>
            <div class="note">${slot.kind
                ? `${slot.kind === 2 ? "Scrolling" : "Directional"}${slot.dpi ? ` · ${slot.dpi} DPI` : " · normal pointer speed"}`
                : "Empty · the keyboard refuses to activate it, so these keys do nothing yet"}</div>
            <div class="note">${reachedBy}</div>
            ${slot.kind ? `<div class="note">its colour paints ${esc(row?.locality || "its locality").toLowerCase().replace(/rgb_/, "").replace(/_/g, " ")} while the mode runs — not this key</div>` : ""}
        </div>`;
    };
    // Holding a mode and toggling it on are the same mode reached two ways, so
    // the card is named for the one its route uses. A route that uses both
    // keeps them in the line beneath, where they can be told apart per key.
    const reachCard = (group) => (entry) => {
        const variants = [...new Set([
            ...entry.keys.map((key) => pointingVariant(model, keyMeaning(key.position))),
            ...(entry.behaviours || []).map((row) => pointingVariant(model, row.action)),
        ])];
        const only = variants.length === 1 ? variants[0] : "";
        return card(group, entry.name, [
            entry.keys.length ? keysReach(entry.keys) : "",
            entry.behaviours?.length ? branchReach(model, entry, !only) : "",
        ].filter(Boolean).join(" · "), only);
    };

    const groups = [
        {id: "here", cards: reach.onKeys.map(reachCard("here")),
            empty: "No pointing mode is placed on this layer."},
        {id: "through", cards: reach.throughKeys.map(reachCard("through")),
            empty: "No transparent key falls through to a pointing-mode key."},
        {id: "branches", cards: reach.fromBranches.map(reachCard("branches")),
            empty: "No behaviour mapped on this layer sends a pointing mode."},
        {id: "belowBranches", cards: reach.fromBranchesBelow.map(reachCard("belowBranches")),
            empty: "No behaviour under a transparent key sends a pointing mode."},
        {id: "elsewhere", cards: reach.elsewhere.map((slotId) => card("elsewhere", slotId, "not reached from this layer")),
            empty: "Every configured mode is reached from this layer."},
    ];
    const section = (group) => `<div class="reach-group">
        ${groupHeader("pointing", group.id, group.cards.length)}
        ${groupOpen("pointing", group.id)
            ? (group.cards.length ? `<div class="pd-reach">${group.cards.join("")}</div>`
                : `<p class="note" style="padding:10px 2px">${esc(group.empty)}</p>`)
            : ""}</div>`;

    const node = el(`<div class="stack" style="gap:10px;padding:2px 0">
        ${inGroupOrder(groups).map(section).join("")}</div>`);
    attachGroupToggles(node);
    attachReachRows(node, "pointing");
    node.querySelectorAll("[data-editpd]").forEach((button) => button.addEventListener("click", () => {
        state.pdSlot = Number(button.dataset.editpd);
        state.pdKind = null;
        state.screen = "pointing";
        render();
    }));
    body.replaceChildren(node);
}

/* ── key edits ─────────────────────────────────────────────────────────── */
function pickKeycodeFor(layoutIndex) {
    if (!writable()) return;
    const layer = currentLayer();
    const position = positionAt(layer, layoutIndex);
    state.selected = layoutIndex;
    openPicker({
        title: `Keycode on ${layerName(layer)}`,
        context: `index ${layoutIndex}`,
        seed: position ? [keyMeaning(position)] : [],
        onPick: (expression) => {
            state.picker = null;
            post({type: "updateLayoutKeys", layer: layer.name, changes: [{layoutIndex, keycode: expression}]});
        },
    });
}

// ⌘C copies the selected key's keycode and ⌘V stores it on the selected key,
// on any layer; Delete or Backspace makes it transparent. Text fields keep
// their own copy, paste and delete.
export function keysShortcut(event) {
    if (state.screen !== "keys" || state.overlay || state.picker || state.retarget || state.recording || state.comboPicking) return false;
    const key = event.key.toLowerCase();
    const clear = (key === "delete" || key === "backspace") && !(event.metaKey || event.ctrlKey || event.altKey || event.shiftKey);
    const clipboard = (key === "c" || key === "v") && (event.metaKey || event.ctrlKey) && !event.altKey && !event.shiftKey;
    if (!clear && !clipboard) return false;
    if (event.target.closest?.("input, textarea, select, [contenteditable]")) return false;
    if (key === "c" && String(getSelection?.() || "")) return false;
    const layer = currentLayer();
    const position = positionAt(layer, state.selected);
    if (!position) return false;

    event.preventDefault();
    const store = (keycode) => {
        if (writable() && keycode !== position.keycode) {
            post({type: "updateLayoutKeys", layer: layer.name, changes: [{layoutIndex: position.layoutIndex, keycode}]});
        }
    };
    if (clear) {
        if (keyFace(position).kind !== "transparent") store("KC_TRANSPARENT");
    } else if (key === "c") {
        state.keyClipboard = {keycode: position.keycode, label: keyFace(position).main};
        navigator.clipboard?.writeText(position.keycode).catch(() => {});
        render();
    } else if (state.keyClipboard) {
        store(state.keyClipboard.keycode);
    }
    return true;
}

// A behaviour belongs to the key it listens to, so changing that key moves the
// whole row. A key that already has a row is never replaced silently: the
// person chooses to overwrite it, swap the two, or leave both alone.
function pickBehaviourKey(behaviour) {
    openPicker({
        title: "Key this behaviour listens to",
        context: actionLabel(getModel(), behaviour.keycode),
        seed: [behaviour.keycode],
        onPick: (expression) => {
            state.picker = null;
            const model = getModel();
            const to = canonicalKeycode(model, expression);
            if (!to || to === canonicalKeycode(model, behaviour.keycode)) { render(); return; }
            const existing = behaviourListeningTo(model, expression);
            if (existing) { state.retarget = {from: behaviour.keycode, to: expression, existing: existing.keycode}; render(); return; }
            retargetBehaviour(behaviour.keycode, expression);
        },
    });
}

function retargetBehaviour(from, to, conflict) {
    const model = getModel();
    state.retarget = null;
    state.behaviourRow = conflict === "swap" || conflict === "overwrite"
        ? behaviourListeningTo(model, to).keycode : canonicalKeycode(model, to);
    state.behaviourRowShown = null;
    state.cell = null;
    post({type: "retargetBehavior", keycode: from, target: to, expectedBase: model.profileIdentity, ...(conflict ? {conflict} : {})});
    render();
}

function retargetPrompt(behaviour) {
    const model = getModel();
    const {to, existing} = state.retarget;
    const name = (keycode) => `<b>${esc(actionLabel(model, keycode))}</b> <code class="dim">${esc(keycode)}</code>`;
    const node = el(`<div class="scrim"><div class="sheet" role="alertdialog" aria-label="Key already has a behaviour" style="width:min(520px,100%)">
        <div class="sheet-h"><h2>${esc(actionLabel(model, existing))} already has a behaviour</h2></div>
        <div class="sheet-b" style="padding:16px 18px"><p style="font-size:13px;line-height:1.55">
            You are moving the behaviour on ${name(behaviour.keycode)} to ${name(existing)}, which has its own.</p>
            <ul class="note" style="margin:10px 0 0 18px;line-height:1.7">
                <li><b>Overwrite</b> replaces it; ${esc(actionLabel(model, behaviour.keycode))} then sends its plain keycode.</li>
                <li><b>Swap</b> gives each key the other's behaviour.</li></ul></div>
        <div class="sheet-f"><span class="note">Both are one step in the draft, so ⌘Z takes them back.</span>
            <span class="right"><button class="btn" data-act="cancel">Cancel</button>
                <button class="btn" data-act="swap">Swap</button>
                <button class="btn primary" data-act="overwrite">Overwrite</button></span></div>
    </div></div>`);
    node.addEventListener("click", (event) => {
        const act = event.target === node ? "cancel" : event.target.closest("[data-act]")?.dataset.act;
        if (act === "cancel") { state.retarget = null; render(); }
        if (act === "swap" || act === "overwrite") retargetBehaviour(behaviour.keycode, to, act);
    });
    queueMicrotask(() => node.querySelector('[data-act="cancel"]')?.focus());
    return node;
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
