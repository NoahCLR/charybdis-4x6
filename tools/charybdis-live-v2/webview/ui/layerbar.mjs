// The layer chips. Which layer you are looking at is a property of the
// keyboard, not of a screen, so Keys and Lighting share one control.

import {css, isOff, label as hsvLabel} from "../lib/colour.mjs";
import {el, esc} from "../lib/dom.mjs";
import {getModel, layerName, layers, render, state} from "../store.mjs";
import {layerColourRow, stageEnabled} from "../view/lighting.mjs";
import {draftMarks} from "../view/review.mjs";

// A layer's swatch and how it is lit, the same wherever a layer is named: a
// layer whose colour is off, or whose stage is off, shows as unlit.
export function layerSwatch(model, layer) {
    const row = layerColourRow(model, layer.index);
    const lit = stageEnabled(model, "layers") && row && !isOff(row.color);
    return {
        html: `<span class="swatch ${lit ? "" : "swatch-off"}" style="${lit ? `background:${css(row.color)}` : ""}"></span>`,
        tip: `${layerName(layer)} · ${lit ? hsvLabel(row.color) : "no layer colour"}`,
    };
}

export function layerBar(trailing = "") {
    const model = getModel();
    const drafted = draftMarks(model?.draft?.changes).layers;
    const chips = layers().map((layer, index) => {
        const swatch = layerSwatch(model, layer);
        return `<button class="layer-chip" data-layer="${index}" aria-pressed="${state.layer === index}"
            data-tip="${esc(swatch.tip)}${drafted.has(layer.index) ? " · changed in your draft" : ""}">${swatch.html}
            <span>${esc(layerName(layer))}</span><span class="idx">${layer.index}</span>${drafted.has(layer.index) ? '<i class="draft-dot"></i>' : ""}</button>`;
    }).join("");
    const node = el(`<div class="layerbar-wrap"><div class="layerbar">${chips}${trailing}</div></div>`);
    node.querySelectorAll("[data-layer]").forEach((button) => button.addEventListener("click", () => {
        state.layer = Number(button.dataset.layer);
        render();
    }));
    return node;
}
