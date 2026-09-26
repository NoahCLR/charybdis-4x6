// The layer stack: what each layer is called, and which one wins.
//
// It hangs off the layer tabs rather than the workbench below, because it is
// not a property of the selected key like the tabs there are — it is the tabs
// themselves, so the control sits after the last layer and opens upward over
// the board, since the tabs sit at the board's foot.
//
// The order is the host's to hold. Moving a layer rewrites every key, behaviour
// and setting that refers to it, so the edit is staged there and arrives back
// as `portable.layers`. "Keys follow their layers" (on by default) decides
// whether layer keys are renumbered with the move or keep their numbers.

import {el, esc} from "../lib/dom.mjs";
import {getModel, layers, post, render, state, canEdit as canEditArea} from "../store.mjs";

let dismiss = null;   // the outside-click listener for the open panel

// Adds the trigger after the layer tabs and, while it is open, the panel
// above them. Both sit on the tabs' wrapper, not inside the strip, which
// scrolls sideways and would hide the one and clip the other.
export function attachLayersControl(bar) {
    const model = getModel();
    const portable = model?.portable || {};
    const busy = Boolean(portable.busy);
    const canEdit = canEditArea("layers");

    const trigger = el(`<button class="layer-edit" aria-haspopup="dialog" aria-expanded="${Boolean(state.layersOpen)}"
        data-tip="Rename layers and change their order: a higher layer wins over the ones under it.">
        <svg viewBox="0 0 16 16" aria-hidden="true"><path d="M5 13V3M2.5 5.5 5 3l2.5 2.5M11 3v10M8.5 10.5 11 13l2.5-2.5"/></svg>
        <span>Rename &amp; Reorder</span></button>`);
    trigger.addEventListener("click", () => {
        state.layersOpen = !state.layersOpen;
        if (!state.layersOpen) state.layersAsked = false;
        render();
    });
    bar.append(trigger);
    if (!state.layersOpen) {
        closeDismiss();
        return;
    }

    const panel = el(`<div class="layerpanel" role="dialog" aria-label="Layers"></div>`);
    if (!portable.layers) {
        panel.append(el(`<p class="note" style="padding:4px 2px">${esc(canEdit ? "Reading the layer stack…" : portable.legacy
            ? "This keyboard runs the five-layer firmware, which has no editable layer stack."
            : "Connect a keyboard with complete-profile firmware to rename or reorder its layers.")}</p>`));
        // One request per opening: the stack is read from the keyboard, or
        // lifted from the draft when one is open.
        if (canEdit && !state.layersAsked) {
            state.layersAsked = true;
            post({type: "managePortableLayers"});
        }
    } else {
        panel.append(editor(model, portable, busy));
    }
    bar.append(panel);
    // The row is measured once it is on screen; render attaches it afterwards.
    requestAnimationFrame(() => placePanel(bar, panel));
    watchOutside(panel, trigger);
}

// The panel opens upward over the board, above the tabs. The room is what the
// scrolling area shows above the tabs, not the window. When the panel is
// taller than that, the area scrolls back up (bringing the tabs down toward
// the draft bar at the window's foot) and whatever still does not fit scrolls
// in the panel's list, under its heading and above its buttons.
function placePanel(bar, panel) {
    if (!panel.isConnected) return;
    const margin = 12;
    const rowOf = () => (bar.querySelector(".layer-tabs") || bar).getBoundingClientRect();
    let scroller = bar.parentElement;
    while (scroller && !/(auto|scroll)/.test(getComputedStyle(scroller).overflowY)) scroller = scroller.parentElement;
    const viewOf = () => scroller ? scroller.getBoundingClientRect() : {top: 0, bottom: window.innerHeight};
    const floor = () => {
        const draftBar = document.querySelector(".commit")?.getBoundingClientRect();
        return draftBar && draftBar.height ? Math.min(viewOf().bottom, draftBar.top) : viewOf().bottom;
    };
    const roomAbove = () => rowOf().top - viewOf().top - margin;
    const shortfall = panel.scrollHeight - roomAbove();
    if (shortfall > 0 && scroller) scroller.scrollTop -= Math.max(0, Math.min(shortfall, floor() - rowOf().bottom - margin));
    panel.style.maxHeight = `${Math.max(160, Math.floor(roomAbove()))}px`;
}

function editor(model, portable, busy) {
    // Priority reads top-down, so the highest layer comes first; the device
    // model counts the other way.
    const names = [...portable.layers.names];
    const order = [...portable.layers.order].reverse();
    const follow = portable.layers.keysFollow !== false;
    const node = el(`<div class="layerpanel-body" style="gap:10px">
        <div class="sect-h"><h4>Layers</h4><span class="note">higher layers win · base stays underneath</span></div>
        <div class="list"></div>
        <div class="stack layerpanel-actions" style="gap:10px">
            <div class="stack" style="gap:4px">
                <label class="sw"><input type="checkbox" id="layerKeysFollow" data-act="follow" ${follow ? "checked" : ""} ${busy ? "disabled" : ""}>
                    <span class="track"></span><span class="txt">Keys follow their layers</span></label>
                <span class="note">${follow
                    ? "Layer keys (MO, LT, TG, TO, TT, OSL…) are renumbered with the move, so each still reaches the same layer."
                    : "Layer keys keep their numbers: a key set to MO(1) reaches whatever layer is now 1. Names, colours and the pointer and sniping settings still move with their layer."}</span>
            </div>
            <div class="row" style="gap:8px;align-items:center">
                <button class="btn primary" data-act="save" ${busy ? "disabled" : ""}>Keep layers in draft</button>
                <button class="btn ghost" data-act="cancel" ${busy ? "disabled" : ""}>Discard</button>
            </div>
        </div></div>`);

    const list = node.querySelector(".list");
    order.forEach((layerId) => {
        const position = portable.layers.order.indexOf(layerId);
        const layer = layers().find((entry) => entry.index === layerId);
        const mapped = (layer?.positions || []).filter((key) =>
            key.keycode && !["KC_TRANSPARENT", "KC_TRNS", "KC_NO"].includes(key.keycode)).length;
        const row = el(`<div class="list-row" style="grid-template-columns:24px minmax(0,1fr) 92px auto auto">
            <span class="note mono">${layerId}</span>
            <input class="input" value="${esc(names[layerId])}" data-name="${layerId}" maxlength="23" ${busy ? "disabled" : ""}
                aria-label="${layerId ? `Name for layer ${layerId}` : "Base layer name"}">
            <span class="note ${mapped ? "" : "dim"}">${mapped ? `${mapped} key${mapped === 1 ? "" : "s"}` : "nothing mapped"}</span>
            ${layerId
                ? `<button class="btn tiny ghost" data-move="1" data-id="${layerId}" ${position === 7 || busy ? "disabled" : ""}>Up</button>
                   <button class="btn tiny ghost" data-move="-1" data-id="${layerId}" ${position === 1 || busy ? "disabled" : ""}>Down</button>`
                : `<span class="tag" style="grid-column:span 2">base</span>`}</div>`);
        list.append(row);
    });

    const currentNames = () => {
        node.querySelectorAll("[data-name]").forEach((input) => { names[Number(input.dataset.name)] = input.value.trim(); });
        return [...names];
    };
    node.querySelector('[data-act="follow"]').addEventListener("change", (event) => post({
        type: "editPortableLayer", keysFollow: event.target.checked, names: currentNames(),
    }));
    node.querySelectorAll("[data-move]").forEach((button) => button.addEventListener("click", () => post({
        type: "editPortableLayer", id: Number(button.dataset.id), direction: Number(button.dataset.move), names: currentNames(),
    })));
    // Both close the panel here rather than waiting for the host's answer: the
    // row underneath shows the result, and a panel left open over it would only
    // hide what it changed.
    node.querySelector('[data-act="save"]').addEventListener("click", () => {
        post({type: "savePortableLayers", names: currentNames()});
        closeLayers();
        render();
    });
    node.querySelector('[data-act="cancel"]').addEventListener("click", () => {
        post({type: "cancelPortableReview"});
        closeLayers();
        render();
    });
    return node;
}

export function closeLayers() {
    closeDismiss();
    state.layersOpen = false;
    state.layersAsked = false;
}

function closeDismiss() {
    if (!dismiss) return;
    document.removeEventListener("pointerdown", dismiss, true);
    dismiss = null;
}

function watchOutside(panel, trigger) {
    closeDismiss();
    dismiss = (event) => {
        if (panel.contains(event.target) || trigger.contains(event.target)) return;
        closeLayers();
        render();
    };
    document.addEventListener("pointerdown", dismiss, true);
}
