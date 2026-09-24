// One mark per thing that has a colour of its own, drawn the same everywhere.
//
// A behaviour tier is the dot the keyboard flashes when it resolves, a tap
// count is its branch badge, a layer is its layer colour, a pointing mode is
// the light its slot paints, a combo is its badge in the combo colour, a
// lighting stage is its on/off dot. The grid, the hover card, Settings and the
// draft review all draw these from here, so a colour means one thing on every
// surface — and a stage that is off draws its marks off.

import {css, isOff} from "../lib/colour.mjs";
import {esc} from "../lib/dom.mjs";
import {feedbackColours, stageEnabled} from "../view/lighting.mjs";
import {layerSwatch} from "./layerbar.mjs";
import {slotLight} from "./pointing.mjs";

// A branch's badge as the grid heads its column: from 2× on, tinted with the
// tap-branch colour the keyboard shows while that branch is pending.
export function branchBadge(model, count) {
    const colour = feedbackColours(model).branches[count - 2];
    const lit = count > 1 && stageEnabled(model, "key") && colour && !isOff(colour);
    return `<span class="bn" style="${lit ? `border-color:${css(colour)};color:${css(colour)}` : ""}">${count}×</span>`;
}

export function tierDot(model, kind) {
    const colour = feedbackColours(model)[kind];
    const lit = stageEnabled(model, "key") && !isOff(colour);
    return `<i class="fbdot" style="${lit ? `background:${css(colour)}` : "background:none;border-style:dashed"}"></i>`;
}

export function comboBadge(model, badge = "C") {
    const colour = model?.rgb?.comboFeedback?.color;
    const lit = stageEnabled(model, "combo") && colour && !isOff(colour);
    return `<i class="mk-badge" style="${lit ? `border-color:${css(colour)}` : ""}">${esc(badge)}</i>`;
}

// A mark as the host describes it: {kind: "tier", tier, branch?} · {kind:
// "branch", count} · {kind: "layer", layer} · {kind: "pointing", slot} ·
// {kind: "combo", badge?} · {kind: "stage", on}. Anything else draws nothing.
export function mark(model, value) {
    switch (value?.kind) {
        case "tier": return `${value.branch ? branchBadge(model, value.branch) : ""}${tierDot(model, value.tier)}`;
        case "branch": return branchBadge(model, value.count);
        case "layer": return layerSwatch(model, {index: value.layer}).html.replace('class="swatch', 'class="mk-swatch swatch');
        case "pointing": return slotLight(model, {id: value.slot}).swatch("mk-swatch");
        case "combo": return comboBadge(model, value.badge);
        case "stage": return `<i class="stagedot ${value.on ? "on" : ""}"></i>`;
        default: return "";
    }
}

// A label with its mark in front, as a label reads everywhere. Where marked
// and unmarked labels share a column, `slot` gives every one of them the same
// mark slot, so their words start at one edge and the dots line up.
export const marked = (model, value, text, {slot = false} = {}) => {
    const shown = mark(model, value);
    if (slot) return `<span class="mk"><span class="mk-slot">${shown}</span><span>${esc(text)}</span></span>`;
    return shown ? `<span class="mk">${shown}<span>${esc(text)}</span></span>` : esc(text);
};
