// The board's hover card: everything a key reaches, without clicking and
// without leaving the layer you are reading.

import {el, esc} from "../lib/dom.mjs";
import {actionLabel, behaviourFor, behaviourTiers, combosAt, keyFace, keyMeaning, macroKeycodes, pointingSlotFor, resolvedPositions} from "../view/keyface.mjs";
import {pdColourRow, stageEnabled} from "../view/lighting.mjs";
import {getModel, heldLayers, layerName, positionAt} from "../store.mjs";
import {branchBadge, mark, sends, sendsKind, tierDot} from "./marks.mjs";
import {helperWord, tierName, vocabulary, word} from "../view/vocabulary.mjs";

const tip = el(`<div class="tip" hidden></div>`);
const card = el(`<div class="hovercard" hidden></div>`);
let timer = 0;

export function mountHover(root) {
    document.body.append(tip, card);
    root.addEventListener("mouseover", (event) => {
        const key = event.target.closest?.("[data-key]");
        const hinted = event.target.closest?.("[data-tip]");
        clearTimeout(timer);
        // A key being dragged to a swap is not a key being read.
        if (document.querySelector(".kc.dragging")) { hideHover(); return; }
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


function keyCard(index) {
    const model = getModel();
    const stack = model?.layers || [];
    const layer = stack[state()] || stack[0];
    const position = positionAt(layer, index);
    if (!position) return "";
    const face = keyFace(position);
    const own = reachSections(model, layer, position, index);
    if (!own.length) own.push(`<div class="hc-sect"><div class="hc-empty">No key behaviour, macro, combo or pointing mode on this key.</div></div>`);
    return `<div class="hc-head"><div class="hc-title"><span class="t">${esc(face.main || "Unmapped")}</span>
        <span class="hc-pill">index ${index}</span></div>
        <code class="hc-code">${esc(position.keycode)}</code></div>${own.join("")}${seenThrough(model, stack, index)}`;
}

// In a layer preview, a key transparent on the viewed layer is answered by the
// highest layer below that is on. The card still reads the viewed layer's own
// key first, then what the answering key does, as the keyboard would run it.
function seenThrough(model, stack, index) {
    const held = heldLayers();
    if (!held.length) return "";
    const answer = resolvedPositions(stack, state(), held).find((entry) => entry.position.layoutIndex === index);
    if (!answer?.fellThrough) return "";
    const from = answer.layer, position = answer.position;
    const sections = reachSections(model, from, position, index);
    return `<div class="hc-through"><div class="hc-sect">
            <div class="hc-h">Seen through from ${esc(layerName(from))}</div>
            <div class="hc-title"><span class="t">${esc(keyFace(position).main || "Unmapped")}</span></div>
            <code class="hc-code">${esc(position.keycode)}</code></div>
        ${sections.length ? sections.join("") : `<div class="hc-sect"><div class="hc-empty">No key behaviour, macro, combo or pointing mode on that key.</div></div>`}</div>`;
}

// What one stored key reaches: its behaviour, macros, pointing mode and the
// combos on its layer at this position. Empty when it reaches none of them.
function reachSections(model, layer, position, index) {
    const behaviour = behaviourFor(model, keyMeaning(position));
    const combos = combosAt(model, model?.layers || [], layer?.index ?? 0, index);
    const macros = macroKeycodes(keyMeaning(position));
    const slot = pointingSlotFor(model, keyMeaning(position));
    const lit = stageEnabled(model, "key");
    const sections = [];
    if (behaviour) {
        const branches = (behaviour.steps || []).map((step) => {
            const rows = [["tap", tierName(model, "tap"), step.tap], ["hold", tierName(model, "hold"), step.hold], ["long", tierName(model, "long"), step.longHold]]
                .filter(([, , branch]) => branch)
                .map(([kind, name, branch]) => `<div class="hc-act"><span class="hc-stage">${tierDot(model, kind)}${name}</span>
                    <span>${sends(model, branch.action) || `<span class="hc-target">${esc(branch.action)}</span>`}
                    <span class="hc-life${sendsKind(model, branch.action) ? " hc-under" : ""}">${esc(described(sendsKind(model, branch.action), helperText(kind, branch.helper)))}</span></span></div>`).join("");
            return `<div class="hc-branch">${branchBadge(model, step.tapCount + 1).replace('class="bn"', 'class="bn hc-n"')}<div>${rows}</div></div>`;
        }).join("");
        sections.push(`<div class="hc-sect"><div class="hc-h">Key behaviour</div>
            <div class="hc-sub">${esc(actionLabel(model, behaviour.keycode))} · ${behaviour.steps.length} branch${behaviour.steps.length === 1 ? "" : "es"} · tap/hold ${timing(behaviour.tapHoldTerm)}${behaviour.keepsAutoMouseAnchored ? " · keeps auto-mouse anchored" : ""}</div>
            ${branches}
            <div class="hc-sub" style="margin:7px 0 0">${lit
                ? "The dot beside each tier is the colour the keyboard flashes on this key when that tier resolves."
                : "Key feedback is switched off, so none of these flash on the keyboard."}</div></div>`);
    }
    if (macros.length) {
        const slots = model?.viaMacros || [];
        sections.push(`<div class="hc-sect"><div class="hc-h">Macro payload</div>${macros.map((keycode) => {
            const macro = slots.find((row) => row.keycode === keycode);
            return `<div class="hc-sub">${esc(keycode)}${macro ? "" : " · not reported"}</div>`
                + (macro?.payload ? `<div class="hc-step"><span class="k">payload</span><span class="v">${esc(macro.payload.slice(0, 64))}</span></div>` : "");
        }).join("")}</div>`);
    }
    if (slot) {
        const row = pdColourRow(model, slot.id);
        sections.push(`<div class="hc-sect"><div class="hc-h">Pointing mode</div>
            <div class="hc-flow">${mark(model, {kind: "pointing", slot: slot.id})}
            <span>${esc(slot.displayName || `Slot ${slot.id + 1}`)}</span>
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
    return sections;
}

const helperText = (kind, helper) => helperWord(getModel(), kind, helper);
const described = (what, how) => [what, how].filter(Boolean).join(" · ");

const timing = (value) => Number(value) ? `${value} ms` : "keyboard default";

// Where a colour paints, in the vocabulary's words, read inside a sentence.
const localityText = (locality) => locality ? word(vocabulary(getModel()).localities, locality).toLowerCase() : "its locality";

// The card is drawn for whichever layer the screen is showing.
let layerIndex = () => 0;
export const bindLayerIndex = (fn) => { layerIndex = fn; };
const state = () => layerIndex();
