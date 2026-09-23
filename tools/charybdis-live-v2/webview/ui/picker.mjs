// The keycode picker: a real keyboard first, then the sections this keyboard
// actually has — its layers, its pointing slots, its macro banks — and the
// vendored QMK catalogue behind a search.

import {el, esc} from "../lib/dom.mjs";
import {getModel, layerName, layers, post, render, state} from "../store.mjs";
import {PICKER_BOARD} from "../view/picker-board.mjs";
import {PICKER_MODIFIERS, pickerExpression} from "../view/edits.mjs";
import {entriesForPickerSection, pickerSections} from "../view/picker-sections.mjs";

const MODIFIERS = PICKER_MODIFIERS;

export function openPicker({title, context, seed = [], mode = "single", onPick}) {
    state.picker = {title, context, mode, section: "board", search: "", mods: [], keys: seed.slice(), layerTap: null, onPick};
    render();
}

// Layer keycodes are posted by index — MO(1), LOCK_LAYER(1), LT(1, KC_A) —
// the form the keyboard reports and the draft encodes; the name is display.
const layerTapName = (index) => layerName(layers()[Number(index)]) || `Layer ${index}`;

export const closePicker = () => { state.picker = null; render(); };

const expression = () => state.picker ? pickerExpression(state.picker) : "";

const chunk = (values, size) => values.reduce((rows, value, index) =>
    (index % size ? rows[rows.length - 1].push(value) : rows.push([value]), rows), []);

function keyButton(entry, picked) {
    return `<button class="pk ${picked ? "on" : ""}" data-pick="${esc(entry.value)}">
        <span class="l">${esc(entry.label || entry.value)}</span><span class="c">${esc(entry.value)}</span></button>`;
}

function sectionBody(model) {
    const picker = state.picker;
    const catalogue = model?.qmkKeycodes || [];
    const picked = (value) => picker.keys.includes(value);

    if (picker.search) {
        const query = picker.search.toLowerCase();
        const matches = catalogue.filter((entry) => entry.search?.includes(query) || entry.value.toLowerCase().includes(query)).slice(0, 64);
        if (!matches.length) return `<p class="note" style="padding:18px">Nothing in the keyboard's catalogue matches “${esc(picker.search)}”.</p>`;
        return `<div class="pk-body"><p class="note" style="margin-bottom:10px">${matches.length} matches</p>
            <div class="pk-rows">${chunk(matches, 8).map((row) => `<div class="pk-row">${row.map((entry) => keyButton(entry, picked(entry.value))).join("")}</div>`).join("")}</div></div>`;
    }

    const section = pickerSections().find((entry) => entry.id === picker.section) || pickerSections()[0];
    if (section.kind === "board") {
        const keys = PICKER_BOARD.keys.map((key) => {
            const on = picked(key.value);
            const shifted = key.labels[1] && key.labels[1].length <= 3;
            return `<g class="pkb-key ${on ? "on" : ""}" data-pick="${esc(key.value)}" tabindex="0" role="button" aria-label="${esc(key.value)}">
                <rect x="${key.x}" y="${key.y}" width="${key.w}" height="${key.h}" rx="8"></rect>
                ${shifted
                    ? `<text class="pkb-alt" x="${key.x + key.w / 2}" y="${key.y + key.h * 0.36}">${esc(key.labels[1])}</text>
                       <text class="pkb-main" x="${key.x + key.w / 2}" y="${key.y + key.h * 0.74}">${esc(key.labels[0])}</text>`
                    : `<text class="pkb-main ${key.labels[0].length > 4 ? "sm" : ""}" x="${key.x + key.w / 2}" y="${key.y + key.h / 2 + 5}">${esc(key.labels[0])}</text>`}
            </g>`;
        }).join("");
        return `<div class="pk-body"><div class="pkb"><svg viewBox="0 0 ${PICKER_BOARD.width} ${PICKER_BOARD.height}" xmlns="http://www.w3.org/2000/svg">${keys}</svg></div>
            <p class="note" style="margin-top:12px">Click a key. Modifiers above wrap it, so <code>Cmd</code> + <code>C</code> stores <code>G(KC_C)</code> — identical to <code>LGUI(KC_C)</code>.</p></div>`;
    }
    if (section.kind === "layers") {
        return `<div class="pk-body"><div class="pk-layers">${layers().map((layer) => `
            <div class="pk-layer">
                <span></span><span class="nm">${esc(layerName(layer))}</span>
                <button class="pk wide ${picked(`MO(${layer.index})`) ? "on" : ""}" data-pick="MO(${layer.index})"><span class="l">Hold</span><span class="c">MO(${layer.index})</span></button>
                <button class="pk wide ${picked(`LOCK_LAYER(${layer.index})`) ? "on" : ""}" data-pick="LOCK_LAYER(${layer.index})"><span class="l">Lock</span><span class="c">LOCK_LAYER(${layer.index})</span></button>
                <button class="pk wide ${picker.layerTap === String(layer.index) ? "on" : ""}" data-lt="${layer.index}"><span class="l">Tap-hold</span><span class="c">LT(${layer.index}, …)</span></button>
            </div>`).join("")}</div>
            <p class="note" style="margin-top:12px">${picker.layerTap
                ? `Tap-hold on <b>${esc(layerTapName(picker.layerTap))}</b> is armed — now pick the tap key from any section.`
                : "Hold reaches the layer while the key is down. Lock toggles it. Tap-hold asks for a tap key next."}</p></div>`;
    }
    if (section.kind === "macros") {
        const slots = [...(model?.viaMacros || []), ...(model?.hardcodedMacros || [])];
        if (!slots.length) return `<p class="note" style="padding:18px">This keyboard has not reported its macro banks.</p>`;
        return `<div class="pk-body"><div class="pk-rows">${chunk(slots, 8).map((row) => `<div class="pk-row">${row.map((slot) =>
            `<button class="pk ${picked(slot.keycode) ? "on" : ""}" data-pick="${esc(slot.keycode)}">
                <span class="l">${esc(slot.keycode.replace(/^VIA_MACRO_/, "M").replace(/^MACRO_/, "U"))}</span>
                <span class="c">${slot.empty ? "empty" : `${slot.bytes} B`}</span></button>`).join("")}</div>`).join("")}</div></div>`;
    }
    const entries = entriesForPickerSection(catalogue, section);
    if (!entries.length) return `<p class="note" style="padding:18px">The keyboard's catalogue has nothing in this section.</p>`;
    return `<div class="pk-body"><div class="pk-rows">${chunk(entries.slice(0, 160), 8).map((row) =>
        `<div class="pk-row">${row.map((entry) => keyButton(entry, picked(entry.value))).join("")}</div>`).join("")}</div></div>`;
}

export function pickerOverlay() {
    const picker = state.picker;
    if (!picker) return null;
    const model = getModel();
    const value = expression();
    const node = el(`<div class="scrim"><div class="sheet picker" role="dialog" aria-modal="true" aria-label="Pick a keycode" style="width:min(1180px,100%)">
        <div class="sheet-h">
            <div><h2>Pick a keycode</h2>
                <p class="note">${picker.mode === "list" ? "Choose one or more keys." : `For <b>${esc(picker.title)}</b> · ${esc(picker.context || "")}`}</p></div>
            <input class="input" id="pickerSearch" placeholder="Search every section" style="max-width:300px;margin-left:12px" value="${esc(picker.search)}">
            <span class="right" style="margin-left:auto"><button class="btn ghost" data-act="cancel">Cancel</button></span>
        </div>
        ${picker.mode === "list" ? "" : `<div class="pk-mods"><span class="label">Modifiers</span>
            ${MODIFIERS.map(([name, wrap]) => `<button class="pk-mod ${picker.mods.includes(name) ? "on" : ""}" data-mod="${name}"
                data-tip="Wraps the picked key as ${wrap}(key).">${name}</button>`).join("")}
            <span class="note" style="margin-left:auto">held together with the key</span></div>`}
        <div class="sheet-b" style="display:grid;grid-template-columns:186px minmax(0,1fr);align-items:start">
            <div class="picker-side">${pickerSections().map((section) =>
                `<button data-sec="${esc(section.id)}" aria-current="${picker.section === section.id && !picker.search}">${esc(section.label)}</button>`).join("")}</div>
            <div>${sectionBody(model)}</div>
        </div>
        <div class="sheet-f">
            <span class="label">Selected</span>
            ${picker.layerTap ? `<button class="chip" data-act="clearlt">LT ${esc(layerTapName(picker.layerTap))} ✕</button>` : ""}
            ${picker.keys.length ? picker.keys.map((key, index) => `<button class="chip" data-remove="${index}">${esc(key)} ✕</button>`).join("")
                : `<span class="note">nothing picked yet</span>`}
            <code class="mono" style="margin-left:10px;color:var(--text)">${esc(value || "—")}</code>
            <span class="right"><button class="btn ghost" data-act="clear">Clear</button>
                <button class="btn ghost" data-act="cancel">Cancel</button>
                <button class="btn primary" data-act="use" ${value ? "" : "disabled"}>Use keycode</button></span>
        </div>
    </div></div>`);

    node.addEventListener("click", (event) => {
        if (event.target === node || event.target.closest('[data-act="cancel"]')) return closePicker();
        const section = event.target.closest("[data-sec]");
        if (section) { picker.section = section.dataset.sec; picker.search = ""; return render(); }
        const modifier = event.target.closest("[data-mod]");
        if (modifier) {
            const name = modifier.dataset.mod;
            picker.mods = picker.mods.includes(name) ? picker.mods.filter((value2) => value2 !== name) : [...picker.mods, name];
            return render();
        }
        const layerTap = event.target.closest("[data-lt]");
        if (layerTap) { picker.layerTap = picker.layerTap === layerTap.dataset.lt ? null : layerTap.dataset.lt; return render(); }
        if (event.target.closest('[data-act="clearlt"]')) { picker.layerTap = null; return render(); }
        if (event.target.closest('[data-act="clear"]')) { picker.keys = []; picker.mods = []; picker.layerTap = null; return render(); }
        const remove = event.target.closest("[data-remove]");
        if (remove) { picker.keys.splice(Number(remove.dataset.remove), 1); return render(); }
        if (event.target.closest('[data-act="use"]')) {
            const chosen = expression();
            if (chosen) picker.onPick?.(chosen);
            return;
        }
        const pick = event.target.closest("[data-pick]");
        if (pick) {
            const chosen = pick.dataset.pick;
            picker.keys = picker.mode === "list"
                ? (picker.keys.includes(chosen) ? picker.keys.filter((key) => key !== chosen) : [...picker.keys, chosen])
                : [chosen];
            render();
        }
    });
    const search = node.querySelector("#pickerSearch");
    search.addEventListener("input", () => {
        picker.search = search.value;
        render();
        const again = document.querySelector("#pickerSearch");
        if (again) { again.focus(); again.setSelectionRange(again.value.length, again.value.length); }
    });
    return node;
}
