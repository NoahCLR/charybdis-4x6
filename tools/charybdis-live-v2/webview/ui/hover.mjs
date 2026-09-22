// The board's hover card: everything a key reaches, without clicking and
// without leaving the layer you are reading.

import {css, isOff} from "../lib/colour.mjs";
import {el, esc} from "../lib/dom.mjs";
import {actionLabel, behaviourFor, behaviourTiers, bindingsForSlot, combosForKey, keyFace, keyMeaning, macroKeycodes, pointingSlotFor} from "../view/keyface.mjs";
import {feedbackColours, pdColourRow, stageEnabled} from "../view/lighting.mjs";
import {getModel, positionAt} from "../store.mjs";

const tip = el(`<div class="tip" hidden></div>`);
const card = el(`<div class="hovercard" hidden></div>`);
let timer = 0;

export function mountHover(root) {
    document.body.append(tip, card);
    root.addEventListener("mouseover", (event) => {
        const key = event.target.closest?.("[data-key]");
        const hinted = event.target.closest?.("[data-tip]");
        clearTimeout(timer);
        if (key) {
            const rect = key.getBoundingClientRect();
            timer = setTimeout(() => { card.innerHTML = keyCard(Number(key.dataset.key)); place(card, rect, 14, true); }, 140);
            tip.hidden = true;
            return;
        }
        card.hidden = true;
        if (hinted) {
            const rect = hinted.getBoundingClientRect();
            timer = setTimeout(() => { tip.textContent = hinted.dataset.tip; place(tip, rect, 8); }, 260);
            return;
        }
        tip.hidden = true;
    }, true);
    root.addEventListener("mouseleave", hideHover, true);
    addEventListener("scroll", hideHover, true);
}

export function hideHover() { clearTimeout(timer); tip.hidden = true; card.hidden = true; }

function place(node, rect, gap = 10, beside = false) {
    node.hidden = false;
    const box = node.getBoundingClientRect();
    let left, top;
    if (beside) {
        const right = rect.right + gap;
        left = right + box.width < innerWidth - 10 ? right : rect.left - gap - box.width;
        top = rect.top + rect.height / 2 - box.height / 2;
    } else {
        left = rect.left + rect.width / 2 - box.width / 2;
        top = rect.top - box.height - gap;
        if (top < 10) top = rect.bottom + gap;
    }
    node.style.left = `${Math.max(10, Math.min(left, innerWidth - box.width - 10))}px`;
    node.style.top = `${Math.max(10, Math.min(top, innerHeight - box.height - 10))}px`;
}

const dot = (colour, lit) => `<i class="fbdot" style="${lit && !isOff(colour) ? `background:${css(colour)}` : "background:none;border-style:dashed"}"></i>`;

function keyCard(index) {
    const model = getModel();
    const layer = (model?.layers || [])[state()] || model?.layers?.[0];
    const position = positionAt(layer, index);
    if (!position) return "";
    const face = keyFace(position);
    const behaviour = behaviourFor(model, keyMeaning(position));
    const combos = combosForKey(model, position);
    const macros = macroKeycodes(keyMeaning(position));
    const slot = pointingSlotFor(model, keyMeaning(position));
    const lit = stageEnabled(model, "key");
    const colours = feedbackColours(model);
    const sections = [];

    if (behaviour) {
        const branches = (behaviour.steps || []).map((step) => {
            const rows = [["tap", "Tap", step.tap], ["hold", "Hold", step.hold], ["long", "Long hold", step.longHold]]
                .filter(([, , branch]) => branch)
                .map(([kind, name, branch]) => `<div class="hc-act"><span class="hc-stage">${dot(colours[kind], lit)}${name}</span>
                    <span><span class="hc-target">${esc(branch.action)}</span>
                    <span class="hc-life">${esc(helperText(kind, branch.helper))}</span></span></div>`).join("");
            const branchColour = colours.branches[step.tapCount - 1];
            const tint = lit && branchColour && !isOff(branchColour) ? `border-color:${css(branchColour)};color:${css(branchColour)}` : "";
            return `<div class="hc-branch"><span class="hc-n" style="${tint}">${step.tapCount + 1}×</span><div>${rows}</div></div>`;
        }).join("");
        sections.push(`<div class="hc-sect"><div class="hc-h">Key behaviour</div>
            <div class="hc-sub">${esc(actionLabel(model, behaviour.keycode))} · ${behaviour.steps.length} branch${behaviour.steps.length === 1 ? "" : "es"} · tap/hold ${timing(behaviour.tapHoldTerm)}${behaviour.keepsAutoMouseAnchored ? " · keeps auto-mouse anchored" : ""}</div>
            ${branches}
            <div class="hc-sub" style="margin:7px 0 0">${lit
                ? "The dot beside each tier is the colour the keyboard flashes on this key when that tier resolves."
                : "Key feedback is switched off, so none of these flash on the keyboard."}</div></div>`);
    }
    if (macros.length) {
        const slots = [...(model?.viaMacros || []), ...(model?.hardcodedMacros || [])];
        sections.push(`<div class="hc-sect"><div class="hc-h">Macro payload</div>${macros.map((keycode) => {
            const macro = slots.find((row) => row.keycode === keycode);
            return `<div class="hc-sub">${esc(keycode)}${macro ? "" : " · not reported"}</div>`
                + (macro?.payload ? `<div class="hc-step"><span class="k">payload</span><span class="v">${esc(macro.payload.slice(0, 64))}</span></div>` : "");
        }).join("")}</div>`);
    }
    if (slot) {
        const row = pdColourRow(model, slot.id);
        sections.push(`<div class="hc-sect"><div class="hc-h">Pointing mode</div>
            <div class="hc-flow">${row && !isOff(row.color) ? `<span class="swatch-lg" style="width:12px;height:12px;border-radius:4px;background:${css(row.color)}"></span>` : ""}
            <span>${esc(slot.name || `Slot ${slot.id + 1}`)}</span>
            <span class="hc-life">${slot.kind === 2 ? "scrolling" : slot.kind ? "directional" : "empty slot"}</span></div>
            <div class="hc-sub" style="margin:6px 0 0">${slot.kind
                ? `Its colour paints ${esc(localityText(row?.locality))} while the mode runs — not this key.`
                : "This slot is empty, so the key does nothing until it is configured. The keyboard keeps the keycode either way."}</div></div>`);
    }
    if (combos.length) {
        sections.push(`<div class="hc-sect"><div class="hc-h">Combos</div>${combos.map((combo) => `
            <div class="hc-flow"><span class="hc-chip">${esc(combo.badge || "C")}</span>
            ${(combo.inputDisplays || combo.inputs || []).map((input) => `<span class="hc-chip">${esc(input)}</span>`).join('<span class="hc-life">+</span>')}
            <span class="hc-life">→</span><span class="hc-chip out">${esc(combo.outputDisplay || combo.output)}</span></div>`).join("")}</div>`);
    }
    if (!behaviour && !combos.length && !macros.length && !slot) {
        sections.unshift(`<div class="hc-sect"><div class="hc-empty">No key behaviour, macro, combo or pointing mode on this key.</div></div>`);
    }
    return `<div class="hc-head"><div class="hc-title"><span class="t">${esc(face.main || "Unmapped")}</span>
        <span class="hc-pill">index ${index}</span></div>
        <code class="hc-code">${esc(position.keycode)}</code></div>${sections.join("")}`;
}

const helperText = (kind, helper) => kind === "tap" ? "tap sends" : ({
    PRESS_AND_HOLD_UNTIL_RELEASE: "held until release",
    TAP_AT_HOLD_THRESHOLD: "tap at hold threshold",
    TAP_ON_RELEASE_AFTER_HOLD: "tap on release after hold",
    REPEAT_WHILE_HELD: "repeat while held",
}[helper] || String(helper || "").toLowerCase().replace(/_/g, " "));

const timing = (value) => Number(value) ? `${value} ms` : "keyboard default";

const localityText = (locality) => ({
    RGB_BOTH_HALVES: "both halves", RGB_LEFT_HALF: "the left half", RGB_RIGHT_HALF: "the right half",
    RGB_KEY_HALF: "the half holding the trigger key", RGB_KEYS_ONLY: "the trigger key only",
}[locality] || "its locality");

// The card is drawn for whichever layer the screen is showing.
let layerIndex = () => 0;
export const bindLayerIndex = (fn) => { layerIndex = fn; };
const state = () => layerIndex();
