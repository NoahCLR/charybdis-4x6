// One colour control, used by every stage that stores a colour.
//
// It never mutates anything: it reports the HSV the person chose, and the
// screen posts the edit the keyboard understands. What is drawn is whatever
// came back in the model, so the board and this control cannot disagree.

import {css, hsv, isOff, label as hsvLabel, rgb} from "../lib/colour.mjs";
import {el, esc} from "../lib/dom.mjs";

export function colourEditor({colour, title, canEdit = true, onChange, offNote}) {
    const [h, s, v] = hsv(colour);
    const node = el(`<div class="stack" style="gap:12px"></div>`);
    const commit = (next) => onChange?.({
        h: Math.max(0, Math.min(255, Math.round(next[0]))),
        s: Math.max(0, Math.min(255, Math.round(next[1]))),
        v: Math.max(0, Math.min(255, Math.round(next[2]))),
    });

    if (isOff(colour)) {
        node.append(el(`<div class="row" style="gap:10px"><span class="swatch-lg swatch-off" style="width:34px;height:34px"></span>
            <div><div style="font-size:12.5px">${esc(title)}</div><div class="note mono">HSV(0, 0, 0)</div></div></div>`));
        node.append(el(`<div class="callout">${esc(offNote || "No colour is stored, so this stage leaves whatever is underneath visible.")}</div>`));
        if (canEdit) {
            const give = el(`<button class="btn" style="justify-self:start">Give it a colour</button>`);
            give.addEventListener("click", () => commit([140, 255, 200]));
            node.append(give);
        }
        return node;
    }

    const head = el(`<div class="row" style="gap:10px">
        <span class="swatch-lg" style="width:34px;height:34px;background:${css(colour)}"></span>
        <div><div style="font-size:12.5px">${esc(title)}</div>
            <div class="note mono">${esc(hsvLabel(colour))} · rgb(${rgb(colour).join(", ")})</div></div>
        ${canEdit ? `<button class="btn tiny ghost" data-off style="margin-left:auto"
            data-tip="Store HSV(0, 0, 0) so this stage stops painting and lets what is underneath show.">Turn off</button>` : ""}</div>`);
    node.append(head);
    head.querySelector("[data-off]")?.addEventListener("click", () => commit([0, 0, 0]));

    const field = el(`<div class="sv-field" style="background:linear-gradient(to top, #000, transparent), linear-gradient(to right, #fff, ${css({h: String(h), s: "255", v: "255"})})">
        <span class="thumb" style="left:${(s / 255) * 100}%;top:${100 - (v / 255) * 100}%"></span></div>`);
    if (canEdit) field.addEventListener("pointerdown", (event) => {
        const rect = field.getBoundingClientRect();
        commit([h, ((event.clientX - rect.left) / rect.width) * 255, (1 - (event.clientY - rect.top) / rect.height) * 255]);
    });
    node.append(field);

    const ramp = el(`<div class="swatch-pick"><span class="label" style="width:34px">Hue</span>
        <span class="hue-ramp"><span class="thumb" style="left:${(h / 255) * 100}%"></span></span></div>`);
    if (canEdit) ramp.querySelector(".hue-ramp").addEventListener("pointerdown", (event) => {
        const rect = event.currentTarget.getBoundingClientRect();
        commit([((event.clientX - rect.left) / rect.width) * 255, s, v]);
    });
    node.append(ramp);

    const fields = el(`<div class="grid3">
        ${[["H", h], ["S", s], ["V", v]].map(([name, value], index) =>
            `<label class="field"><span>${name}</span><input class="input mono" value="${value}" data-channel="${index}" ${canEdit ? "" : "disabled"}></label>`).join("")}</div>`);
    fields.querySelectorAll("[data-channel]").forEach((input) => input.addEventListener("change", () => {
        const next = [h, s, v];
        next[Number(input.dataset.channel)] = Number(input.value) || 0;
        commit(next);
    }));
    node.append(fields);
    return node;
}
