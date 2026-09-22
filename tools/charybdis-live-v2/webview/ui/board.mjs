// The board: the one constant surface. It paints the light the keyboard would
// show for this layer, draws the legend on top in whichever of black or white
// stays readable, and marks what each key reaches.

import {css, idealText, isOff} from "../lib/colour.mjs";
import {GEO, LED_INDEX, keyVisual} from "../view/geometry.mjs";
import {behaviourFor, behaviourTiers, combosForKey, keyFace} from "../view/keyface.mjs";
import {keyLight, stageEnabled, tierColour} from "../view/lighting.mjs";
import {el, esc} from "../lib/dom.mjs";

const TIER_KINDS = ["tap", "hold", "long"];

export function board(model, layer, options = {}) {
    const {selected, mode = "light", picks = [], inputs = [], pdActive = null,
        faces = true, onKey, onOpen, onSwap} = options;
    const positions = layer?.positions || [];
    const feedbackOn = stageEnabled(model, "key");
    const comboColour = model?.rgb?.comboFeedback?.color;
    const comboLit = stageEnabled(model, "combo") && comboColour && !isOff(comboColour);

    let glow = "", keys = "";
    for (const position of positions) {
        const index = position.layoutIndex;
        const visual = keyVisual(index);
        const face = keyFace(position);
        const cx = visual.x + GEO.keyW / 2, cy = visual.y + GEO.keyH / 2;
        const transform = visual.angle ? ` transform="rotate(${visual.angle} ${cx} ${cy})"` : "";
        const classes = ["kc",
            face.kind === "transparent" ? "kc-trns" : "",
            face.kind === "disabled" ? "kc-none" : "",
            options.picking ? "pickable" : ""].filter(Boolean).join(" ");

        let fill = "", text = "";
        if (mode === "leds") {
            fill = picks.includes(index) ? ` style="fill:var(--key-hi);stroke:var(--text);stroke-width:2"` : "";
        } else {
            const light = keyLight(model, layer, position, {pdActive});
            text = idealText(light.colour);
            const paint = css(light.colour);
            fill = ` style="fill:${paint};stroke:rgba(255,255,255,${face.kind === "transparent" ? ".16" : ".3"})"`;
            glow += `<rect class="kc-glow" x="${visual.x}" y="${visual.y}" width="${GEO.keyW}" height="${GEO.keyH}" rx="${GEO.radius}" fill="${paint}"${transform}></rect>`;
        }

        let marks = "", lift = 0;
        if (faces && mode !== "leds") {
            const tiers = behaviourTiers(behaviourFor(model, position.keycode));
            const combos = combosForKey(model, position);
            const dotY = visual.y + (combos.length ? 6.6 : 8);
            const step = 9.6, startX = cx - ((tiers.length - 1) * step) / 2;
            marks += tiers.map((tier, order) => {
                const x = startX + order * step;
                const colour = tierColour(model, tier.kind);
                if (!feedbackOn || isOff(colour)) {
                    return `<circle cx="${x}" cy="${dotY}" r="3.9" fill="none" stroke="${text || "rgba(255,255,255,.9)"}" stroke-width="1.1" stroke-dasharray="2 1.6"></circle>`;
                }
                return `<circle cx="${x}" cy="${dotY}" r="3.9" fill="${css(colour)}" stroke="${text || "rgba(255,255,255,.9)"}" stroke-width="1.1"></circle>`
                    + (tier.count > 1 ? `<text x="${x}" y="${dotY + 0.4}" class="kc-dotn" fill="${idealText(colour)}">${tier.count}</text>` : "");
            }).join("");
            if (combos.length) {
                const width = 13, gap = 1.6;
                let x = cx - (combos.length * width + (combos.length - 1) * gap) / 2;
                const y = visual.y + (tiers.length ? 13.4 : 6.4);
                marks += combos.map((combo) => {
                    const badge = `<rect x="${x}" y="${y}" width="${width}" height="8.4" rx="3" fill="rgba(8,8,10,.86)" stroke="${comboLit ? css(comboColour) : "rgba(245,245,243,.86)"}" stroke-width="${comboLit ? 1.2 : 0.8}"></rect>`
                        + `<text x="${x + width / 2}" y="${y + 4.5}" class="kc-badgetext">${esc(combo.badge || "C")}</text>`;
                    x += width + gap;
                    return badge;
                }).join("");
            }
            lift = (tiers.length ? 5 : 0) + (combos.length ? 5 : 0);
        }

        // Selection and combo-input rings ride on top: the light owns the key
        // face, so state is never told by recolouring it.
        const ring = (name) => `<rect class="${name}" x="${visual.x + 1.4}" y="${visual.y + 1.4}" width="${GEO.keyW - 2.8}" height="${GEO.keyH - 2.8}" rx="${GEO.radius - 1}"></rect>`;
        const main = mode === "leds" ? String(LED_INDEX[index] ?? index) : face.main;
        const size = main.length > 5 ? "xs" : main.length > 3 ? "sm" : "";
        // Long device labels are squeezed to the keycap rather than cut: a
        // clipped legend would hide which key it is.
        const fit = main.length > 7 ? ` textLength="${GEO.keyW - 12}" lengthAdjust="spacingAndGlyphs"` : "";
        const labelY = cy + lift + (face.sub ? -5 : 0);
        keys += `<g class="${classes}${selected === index ? " sel" : ""}" data-key="${index}" tabindex="0" role="button"
            aria-label="${esc(position.keycode)} at index ${index}"${transform}>
            <rect class="kc-rect" x="${visual.x}" y="${visual.y}" width="${GEO.keyW}" height="${GEO.keyH}" rx="${GEO.radius}"${fill}></rect>
            ${selected === index ? ring("kc-ring") : ""}${inputs.includes(index) ? ring("kc-inring") : ""}
            ${marks}
            <text class="kc-label ${size}" x="${cx}" y="${labelY}"${fit}${text ? ` style="fill:${text}"` : ""}>${esc(main)}</text>
            ${face.sub && mode !== "leds" ? `<text class="kc-sub" x="${cx}" y="${labelY + 13}"${text ? ` style="fill:${text};opacity:.72"` : ""}>${esc(face.sub)}</text>` : ""}
        </g>`;
    }

    const ball = `<g><circle cx="${GEO.trackball.x}" cy="${GEO.trackball.y}" r="${GEO.trackball.r}" fill="none" stroke="var(--line-2)" stroke-dasharray="3 4"></circle>
        <text x="${GEO.trackball.x}" y="${GEO.trackball.y + 40}" class="kc-badge">trackball</text></g>`;
    const node = el(`<div class="board ${options.picking ? "picking" : ""}">
        <svg viewBox="${GEO.viewBox}" xmlns="http://www.w3.org/2000/svg">${glow}${ball}${keys}</svg></div>`);

    node.querySelectorAll("[data-key]").forEach((group) => {
        const index = Number(group.dataset.key);
        if (onKey) group.addEventListener("click", () => onKey(index));
        if (onOpen) group.addEventListener("dblclick", () => onOpen(index));
        if (onSwap) {
            group.addEventListener("pointerdown", (event) => { node.dragFrom = index; group.setPointerCapture?.(event.pointerId); });
            group.addEventListener("pointerup", (event) => {
                const target = document.elementFromPoint(event.clientX, event.clientY)?.closest("[data-key]");
                const to = target ? Number(target.dataset.key) : index;
                if (node.dragFrom !== undefined && node.dragFrom !== to) onSwap(node.dragFrom, to);
                node.dragFrom = undefined;
            });
        }
    });
    return node;
}

export const boardTierKinds = TIER_KINDS;
