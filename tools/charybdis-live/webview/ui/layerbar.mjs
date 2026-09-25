// The layer tabs, and the swatch a layer is shown with wherever it is named.

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

// The layer tabs. Which layer you are looking at is a property of the
// keyboard, not of a screen, so Keys and Lighting share one control, and it
// sits on the board it chooses for: the picked layer's tab opens into it.
export function layerBar(trailing = "") {
    const model = getModel();
    const drafted = draftMarks(model?.draft?.changes).layers;
    const tabs = layers().map((layer, index) => {
        const swatch = layerSwatch(model, layer);
        return `<button class="layer-tab" role="tab" data-layer="${index}" aria-selected="${state.layer === index}"
            data-tip="${esc(swatch.tip)}${drafted.has(layer.index) ? " · changed in your draft" : ""}">${swatch.html}
            <span>${esc(layerName(layer))}</span><span class="idx">${layer.index}</span>${drafted.has(layer.index) ? '<i class="draft-dot"></i>' : ""}</button>`;
    }).join("");
    const node = el(`<div class="layerbar-wrap"><div class="layer-tabs" role="tablist" aria-label="Layers">${tabs}${trailing}</div></div>`);
    node.querySelectorAll("[data-layer]").forEach((button) => button.addEventListener("click", () => {
        state.layer = Number(button.dataset.layer);
        render();
    }));
    keepInView(node.querySelector(".layer-tabs"));
    return node;
}

// A strip of tabs too long for its width scrolls sideways; every render
// builds it afresh, so it brings its picked tab back into view once drawn,
// and fades the edge that has more tabs beyond it.
export function keepInView(strip) {
    if (!strip) return;
    const edges = () => {
        strip.classList.toggle("more-left", strip.scrollLeft > 1);
        strip.classList.toggle("more-right", strip.scrollLeft + strip.clientWidth < strip.scrollWidth - 1);
    };
    strip.addEventListener("scroll", edges, {passive: true});
    requestAnimationFrame(() => {
        const tab = strip.isConnected && strip.querySelector('[aria-selected="true"]');
        if (!tab) return;
        const left = tab.offsetLeft - 28, right = tab.offsetLeft + tab.offsetWidth + 28;
        if (left < strip.scrollLeft) strip.scrollLeft = left;
        else if (right > strip.scrollLeft + strip.clientWidth) strip.scrollLeft = right - strip.clientWidth;
        edges();
    });
}
