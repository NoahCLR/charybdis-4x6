// The layer stack: what each layer is called, and which one wins.
//
// It hangs off the layer row rather than the workbench below, because it is not
// a property of the selected key like the tabs there are — it is the row itself,
// so the control sits after the last layer and drops open over the board.
//
// The order is the host's to hold. Moving a layer rewrites every key, behaviour
// and setting that refers to it, so the edit is staged there and arrives back
// as `portable.layers`.

import {el, esc} from "../lib/dom.mjs";
import {getModel, layers, post, render, state} from "../store.mjs";

let dismiss = null;   // the outside-click listener for the open panel

// Adds the trigger to the layer row and, while it is open, the panel beneath
// it. The panel is placed on the row's wrapper, not inside the row, which
// scrolls sideways and would clip it.
export function attachLayersControl(bar) {
    const model = getModel();
    const portable = model?.portable || {};
    const busy = Boolean(portable.busy);
    const canEdit = Boolean(portable.available && portable.eightLayers) && !busy;

    const trigger = el(`<button class="layer-edit" aria-expanded="${Boolean(state.layersOpen)}"
        data-tip="Rename layers and change which one wins.">Edit layers</button>`);
    trigger.addEventListener("click", () => {
        state.layersOpen = !state.layersOpen;
        if (!state.layersOpen) state.layersAsked = false;
        render();
    });
    bar.querySelector(".layerbar").append(trigger);
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
    watchOutside(panel, trigger);
}

function editor(model, portable, busy) {
    // Priority reads top-down, so the highest layer comes first; the device
    // model counts the other way.
    const names = [...portable.layers.names];
    const order = [...portable.layers.order].reverse();
    const node = el(`<div class="stack" style="gap:10px">
        <div class="sect-h"><h4>Layers</h4><span class="note">higher layers win · base stays underneath</span></div>
        <div class="list"></div>
        <div class="row" style="gap:8px;align-items:center">
            <button class="btn primary" data-act="save" ${busy ? "disabled" : ""}>Keep layers in draft</button>
            <button class="btn ghost" data-act="cancel" ${busy ? "disabled" : ""}>Discard</button>
            <span class="note" style="margin-left:auto">Moving a layer rewrites everything that refers to it.</span>
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
