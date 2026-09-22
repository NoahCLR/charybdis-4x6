// The layer chips. Which layer you are looking at is a property of the
// keyboard, not of a screen, so Keys and Lighting share one control.

import {css, isOff, label as hsvLabel} from "../lib/colour.mjs";
import {el, esc} from "../lib/dom.mjs";
import {getModel, layerName, layers, render, state} from "../store.mjs";
import {layerColourRow, stageEnabled} from "../view/lighting.mjs";

export function layerBar(trailing = "") {
    const model = getModel();
    const chips = layers().map((layer, index) => {
        const row = layerColourRow(model, layer.index);
        const lit = stageEnabled(model, "layers") && row && !isOff(row.color);
        return `<button class="layer-chip" data-layer="${index}" aria-pressed="${state.layer === index}"
            data-tip="${esc(layerName(layer))} · ${lit ? esc(hsvLabel(row.color)) : "no layer colour"}">
            <span class="swatch ${lit ? "" : "swatch-off"}" style="${lit ? `background:${css(row.color)}` : ""}"></span>
            <span>${esc(layerName(layer))}</span><span class="idx">${layer.index}</span></button>`;
    }).join("");
    const node = el(`<div><div class="layerbar">${chips}${trailing}</div></div>`);
    node.querySelectorAll("[data-layer]").forEach((button) => button.addEventListener("click", () => {
        state.layer = Number(button.dataset.layer);
        render();
    }));
    return node;
}
