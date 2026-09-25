// One item of a comparison, drawn one way wherever a draft is compared: the
// review (the draft against the keyboard) and the draft history (each step
// against the step before it).

import {esc} from "../lib/dom.mjs";
import {css, isOff} from "../lib/colour.mjs";
import {FIELDS_SHOWN} from "../view/review.mjs";
import {mark, marked} from "./marks.mjs";

// A colour is shown as the keyboard would light it, beside its value; an
// off colour is drawn off, as it is everywhere else.
const swatch = (colour) => colour
    ? `<i class="rv-swatch ${isOff(colour) ? "swatch-off" : ""}" style="${isOff(colour) ? "" : `background:${css(colour)}`}"></i>` : "";
// Everything with a colour of its own carries its mark, drawn as every
// editor draws it (ui/marks.mjs): a tier's dot, a branch badge, a layer's
// or pointing mode's light, a combo badge, a stage's on/off dot.
const value = (model, text, klass, colour, marker) => text === null ? ""
    : `<span class="rv-val">${swatch(colour)}${mark(model, marker)}<span class="${klass}">${esc(text)}</span></span>`;
// A behaviour tier is labelled as the grid heads it: the branch badge,
// the tier's dot, then the tier's name.
const TIER_NAMES = {tap: "tap", hold: "hold", long: "long hold"};
// Within an item, marked and unmarked labels share one mark slot, so the
// words start at one edge and the tier dots line up in a column.
const fieldLabel = (model, entry, slot) => {
    const text = entry.labelMark?.kind === "tier" && entry.labelMark.branch ? TIER_NAMES[entry.labelMark.tier] : entry.label;
    return text ? marked(model, entry.labelMark, text, {slot}) : "";
};
// Every field is one row of the same three columns — what, before, after —
// so the two sides line up down the whole sheet. A side that has nothing
// (an added thing before, a removed one after) says so with a dash in its
// own column.
const nothing = `<span class="rv-none">—</span>`;
// Each field's own status sits in a narrow column before it, as a diff
// marks its lines: + a field added, − one removed, nothing for one
// changed. So a tier dropped from a behaviour that stays reads as removed,
// and a behaviour removed whole reads − on every line.
const SIGNS = {added: "+", removed: "−"};
const field = (model, slot) => (entry) => `<span class="rv-sign ${esc(entry.status || "")}" aria-label="${esc(entry.status || "")}">${SIGNS[entry.status] || ""}</span>
    <span class="rv-k">${fieldLabel(model, entry, slot)}</span>
    <span class="rv-v">${entry.before === null ? nothing : value(model, entry.before, "del", entry.beforeColour, entry.beforeMark)}</span>
    <span class="rv-v">${entry.after === null ? nothing : value(model, entry.after, "ins", entry.afterColour, entry.afterMark)}</span>`;
const fields = (model, item) => {
    const head = item.fields.slice(0, FIELDS_SHOWN), rest = item.fields.slice(FIELDS_SHOWN);
    const row = field(model, item.fields.some((entry) => entry.labelMark));
    return `<div class="rv-fields">${head.map(row).join("")}${rest.length
        ? `<details class="rv-more"><summary>Show all ${item.fields.length}</summary><div class="rv-fields">${rest.map(row).join("")}</div></details>` : ""}</div>`;
};

// An item is one row of the grid: a status gutter, the title, the fields,
// and two action slots that are always in the same place — Show, then
// Discard at the edge — whether or not an item has them.
export const reviewItem = (model, entry, {show = "", discard = "", titleSlot = false} = {}) => `<div class="rv-item">
    <span class="rv-gutter"><span class="rv-status ${esc(entry.status)}">${esc(entry.status)}</span></span>
    <div class="rv-title">${titleSlot ? `<span class="mk"><span class="mk-slot title">${mark(model, entry.titleMark)}</span><span class="t">${esc(entry.title)}</span></span>` : `<span class="t">${esc(entry.title)}</span>`}
        ${entry.note ? titleSlot ? `<span class="mk"><span class="mk-slot title"></span><span class="rv-note">${esc(entry.note)}</span></span>` : `<span class="rv-note">${esc(entry.note)}</span>` : ""}</div>
    ${fields(model, entry)}
    <span class="rv-act">${show}</span>
    <span class="rv-act">${discard}</span></div>`;

// The grid's column heads, named for what the two sides are.
export const reviewColumns = (what, before, after) => `<div class="rv-cols"><span></span><span>${esc(what)}</span>
    <div class="rv-fields"><span></span><span></span><span>${esc(before)}</span><span>${esc(after)}</span></div><span></span><span></span></div>`;
