// Custom keys: the keyboard's 64 named keys that do only what their behaviour
// says. A key here is its name; what it does is its behaviour, edited on the
// Keys screen like any other. Place one on a layer or send it from a combo.

import {el, esc} from "../lib/dom.mjs";
import {customKeyPlacements} from "../view/keyface.mjs";
import {setReachGroupOpen} from "../view/reach-groups.mjs";
import {canEdit as canEditArea, getModel, layerName, layers, post, render, state, writable} from "../store.mjs";
import * as edits from "../view/edits.mjs";
import {layerSwatch} from "./layerbar.mjs";
import {draftDot, draftMarks} from "../view/review.mjs";
import {topbar, unavailable} from "./shell.mjs";

const ROUTE_WORDS = {here: "key", combos: "combo"};

export function screenCustomKeys() {
    const model = getModel();
    const keys = model?.customKeys || [];
    const key = keys.find((row) => row.keycode === state.customKey) || keys[0];
    const canEdit = canEditArea("customKeys");

    const main = el(`<div class="main">${topbar(
        "Custom keys",
        "Named keys that do only what their behaviour says. Place one on a key or send it from a combo; a behaviour cannot send one.",
        keys.length ? `<input class="input" id="customKeySearch" type="search" placeholder="Search custom keys by name" style="width:220px" value="${esc(state.customKeySearch || "")}">` : "",
    )}</div>`);
    main.querySelector("#customKeySearch")?.addEventListener("input", (event) => { state.customKeySearch = event.target.value; render(); });

    const content = el(`<div class="content"><div class="pad" style="display:grid;grid-template-columns:minmax(0,340px) minmax(0,1fr);gap:20px;align-items:start"></div></div>`);
    const pad = content.firstElementChild;
    if (!keys.length) {
        pad.style.display = "block";
        pad.appendChild(el(`<div class="screen-stub"><h3>No custom keys read</h3>
            <p class="note">${esc(unavailable(model) || "This firmware has no custom keys, or its profile has not been read.")}</p></div>`));
        main.appendChild(content);
        return main;
    }

    const named = keys.filter((row) => row.name).length;
    const grid = el(`<div class="card"><div class="card-h"><h3>Keys</h3>
        <span class="right tag">${named} of ${keys.length} named</span></div>
        <div class="card-b"><div class="macro-grid"></div></div></div>`);
    const cells = grid.querySelector(".macro-grid");
    const query = (state.customKeySearch || "").trim().toLowerCase();
    const matches = (row) => !query || row.name.toLowerCase().includes(query) || String(row.slot) === query;
    if (!keys.some(matches)) cells.replaceWith(el(`<p class="note">No custom key is named “${esc(query)}”.</p>`));
    const changed = draftMarks(model?.draft?.changes).customKeys;
    for (const row of keys) {
        if (!matches(row)) continue;
        const tip = `Custom key ${row.slot}${row.name ? ` · ${row.name}` : ""} · ${row.hasBehavior ? "has a behaviour" : "no behaviour: does nothing yet"}`;
        const cell = el(`<button class="mslot ${row.name || row.hasBehavior ? "filled" : ""}" data-key="${esc(row.keycode)}"
            aria-current="${row.keycode === key?.keycode}" data-tip="${esc(tip)}">
            <span class="n">K${row.slot}</span>${changed.has(row.keycode) ? draftDot("Changed in your draft", "corner") : ""}
            <span class="v">${row.name ? esc(row.name.length > 13 ? `${row.name.slice(0, 12)}…` : row.name) : "—"}</span></button>`);
        cell.addEventListener("click", () => { state.customKey = row.keycode; render(); });
        cells.append(cell);
    }
    pad.appendChild(grid);
    pad.appendChild(key ? editor(model, key, canEdit) : el(`<div class="empty-card"><p class="note">Pick a key.</p></div>`));
    main.appendChild(content);
    return main;
}

function editor(model, key, canEdit) {
    const space = model?.customKeyNameSpace || {perName: 20};
    const card = el(`<div class="card">
        <div class="card-h"><h3>${esc(key.name || `Custom key ${key.slot}`)}</h3>
            <span class="tag" data-tip="${esc(key.keycode)}">Key ${esc(key.slot)}</span>
            <span class="right row" style="gap:8px">
                <button class="btn tiny ghost" data-act="behaviour" ${writable() ? "" : "disabled"}>${key.hasBehavior ? "Edit behaviour…" : "Add a behaviour…"}</button>
                <button class="btn tiny ghost" data-act="place" ${writable() ? "" : "disabled"}>Place on a key…</button></span></div>
        <div class="card-b stack">
            <label class="field"><span>Name</span>
                <input class="input" data-name maxlength="${esc(space.perName)}" value="${esc(key.name)}" placeholder="Custom key ${esc(key.slot)}" ${canEdit ? "" : "disabled"}
                    data-tip="Up to ${esc(space.perName)} plain characters: letters, digits, spaces and punctuation.">
                <span class="note" data-name-count>${esc(key.name.length)} / ${esc(space.perName)} characters</span></label>
            <div class="field"><span>What it does</span>
                <span class="note">${key.hasBehavior ? "Its behaviour: open it to see and change what each tap and hold sends." : "Nothing yet. Add a behaviour to give it taps and holds."}</span></div>
            ${placedOn(model, key)}
        </div></div>`);
    const nameInput = card.querySelector("[data-name]");
    nameInput.addEventListener("input", () => {
        card.querySelector("[data-name-count]").textContent = `${nameInput.value.length} / ${space.perName} characters`;
    });
    nameInput.addEventListener("change", () => post(edits.customKeyNameMessage(key.keycode, nameInput.value, model?.customKeyEditing?.identity)));
    card.querySelector('[data-act="place"]').addEventListener("click", () => {
        state.placement = {keycode: key.keycode, label: key.name || key.keycode};
        state.screen = "keys"; state.tab = "key"; render();
    });
    card.querySelector('[data-act="behaviour"]').addEventListener("click", () => {
        Object.assign(state, {screen: "keys", tab: "behaviours", behaviourRow: key.keycode});
        if (!key.hasBehavior) post(edits.addBehaviour(key.keycode, model.profileIdentity));
        render();
    });
    card.querySelectorAll("[data-goto-layer]").forEach((button) => button.addEventListener("click", () =>
        showOnLayer(key.keycode, Number(button.dataset.gotoLayer))));
    return card;
}

// The layers where a key carries this custom key or a combo sends it, each in
// its layer colour; a click shows that layer with the key picked.
function placedOn(model, key) {
    const chips = customKeyPlacements(model, layers(), key.keycode).map(({layer, at, routes}) => {
        const swatch = layerSwatch(model, layer);
        const how = routes.map((route) => `${ROUTE_WORDS[route.group] || route.group}${route.group === "here" && route.keys.length > 1 ? ` ×${route.keys.length}` : ""}`).join(" · ");
        return `<button class="layer-chip link" data-goto-layer="${at}"
            data-tip="${esc(swatch.tip)} · reached by ${esc(how)} · show on the board">${swatch.html}
            <span>${esc(layerName(layer))}</span><span class="idx">${esc(how)}</span></button>`;
    }).join("");
    return `<div class="field"><span>On layers</span>
        ${chips ? `<div class="row" style="gap:6px;flex-wrap:wrap">${chips}</div>`
            : `<span class="note">No key or combo sends this custom key. Place it on a key to use it.</span>`}</div>`;
}

function showOnLayer(keycode, at) {
    const placement = customKeyPlacements(getModel(), layers(), keycode).find((entry) => entry.at === at);
    const route = placement?.routes[0];
    if (!route) return;
    Object.assign(state, {screen: "keys", tab: "key", layer: at, placement: null});
    if (route.keys.length) state.selected = route.keys[0];
    state.reachGroups = setReachGroupOpen(state.reachGroups, route.group, true);
    render();
}
