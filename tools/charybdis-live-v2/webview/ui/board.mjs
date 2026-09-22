// The board: the one constant surface. It paints the light the keyboard would
// show for this layer, draws the legend on top in whichever of black or white
// stays readable, and marks what each key reaches.

import {css, idealText, isOff} from "../lib/colour.mjs";
import {GEO, LED_INDEX, TRACKBALL_LED, fitText, keyFaceRows, keyVisual} from "../view/geometry.mjs";
import {behaviourFor, behaviourTiers, combosForKey, keyFace, keyMeaning} from "../view/keyface.mjs";
import {keyLight, stageEnabled, tierColour, trackballLight} from "../view/lighting.mjs";
import {el, esc} from "../lib/dom.mjs";
import {hideHover} from "./hover.mjs";

const TIER_KINDS = ["tap", "hold", "long"];

export function board(model, layer, options = {}) {
    const {selected, mode = "light", picks = [], inputs = [], reach = [], pdActive = null,
        faces = true, trackball = false, onKey, onOpen, onSwap, onTrackball} = options;
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

        const showMarks = faces && mode !== "leds";
        const tiers = showMarks ? behaviourTiers(behaviourFor(model, keyMeaning(position))) : [];
        const combos = showMarks ? combosForKey(model, position) : [];
        const sub = mode === "leds" ? "" : face.sub;
        const rows = keyFaceRows(visual.y,
            {tiers: tiers.length > 0, combos: combos.length > 0, sub: Boolean(sub)});

        let marks = "";
        const step = 9.6, startX = cx - ((tiers.length - 1) * step) / 2;
        marks += tiers.map((tier, order) => {
            const x = startX + order * step;
            const colour = tierColour(model, tier.kind);
            if (!feedbackOn || isOff(colour)) {
                return `<circle cx="${x}" cy="${rows.tierY}" r="3.9" fill="none" stroke="${text || "rgba(255,255,255,.9)"}" stroke-width="1.1" stroke-dasharray="2 1.6"></circle>`;
            }
            return `<circle cx="${x}" cy="${rows.tierY}" r="3.9" fill="${css(colour)}" stroke="${text || "rgba(255,255,255,.9)"}" stroke-width="1.1"></circle>`
                + (tier.count > 1 ? `<text x="${x}" y="${rows.tierY + 0.5}" class="kc-dotn" fill="${idealText(colour)}">${tier.count}</text>` : "");
        }).join("");

        if (combos.length) {
            // Each badge is as wide as its own text, and the row scales down
            // together when it would overrun the cap — three badges never reach
            // the edges the way a fixed width makes them.
            const gap = 1.5;
            const natural = combos.map((combo) => Math.max(11.5, 5 + String(combo.badge || "C").length * 3.2));
            const room = GEO.keyW - 8, between = gap * (combos.length - 1);
            const total = natural.reduce((sum, width) => sum + width, 0);
            const scale = total + between > room ? Math.max(0.68, (room - between) / total) : 1;
            const widths = natural.map((width) => width * scale);
            const fontSize = Math.max(4.9, 5.8 * scale);
            let x = cx - (widths.reduce((sum, width) => sum + width, 0) + between) / 2;
            marks += combos.map((combo, order) => {
                const width = widths[order];
                const badge = `<rect x="${x}" y="${rows.comboY}" width="${width}" height="${rows.comboHeight}" rx="3.5" fill="rgba(8,8,10,.86)" stroke="${comboLit ? css(comboColour) : "rgba(245,245,243,.86)"}" stroke-width="${comboLit ? 1.2 : 0.8}"></rect>`
                    + `<text x="${x + width / 2}" y="${rows.comboY + rows.comboHeight / 2 + 0.1}" class="kc-badgetext" font-size="${fontSize}">${esc(combo.badge || "C")}</text>`;
                x += width + gap;
                return badge;
            }).join("");
        }

        // Selection and combo-input rings ride on top: the light owns the key
        // face, so state is never told by recolouring it.
        const ring = (name) => `<rect class="${name}" x="${visual.x + 1.4}" y="${visual.y + 1.4}" width="${GEO.keyW - 2.8}" height="${GEO.keyH - 2.8}" rx="${GEO.radius - 1}"></rect>`;
        const main = mode === "leds" ? String(LED_INDEX[index] ?? index) : face.main;
        const above = (tiers.length ? 1 : 0) + (combos.length ? 1 : 0);
        // A legend is sized to the room its row leaves, so one typeface at one
        // weight carries every cap instead of stepping between faces by length.
        const mainFit = fitText(main, sub
            ? (above ? {max: 9.1, min: 6.4, width: GEO.keyW - 12} : {max: 12, min: 7, width: GEO.keyW - 12})
            : {max: 12, min: 7, width: GEO.keyW - 10});
        const subFit = sub ? fitText(sub, above
            ? {max: 6.4, min: 5.2, width: GEO.keyW - 12}
            : {max: 8, min: 6.2, width: GEO.keyW - 12}) : null;
        const squeeze = (fit) => fit.squeeze ? ` textLength="${fit.squeeze}" lengthAdjust="spacingAndGlyphs"` : "";

        keys += `<g class="${classes}${selected === index ? " sel" : ""}" data-key="${index}" tabindex="0" role="button"
            aria-label="${esc(keyMeaning(position))} at index ${index}"${transform}>
            <rect class="kc-rect" x="${visual.x}" y="${visual.y}" width="${GEO.keyW}" height="${GEO.keyH}" rx="${GEO.radius}"${fill}></rect>
            ${selected === index ? ring("kc-ring") : ""}${inputs.includes(index) ? ring("kc-inring") : ""}
            ${reach.includes(index) ? `<rect class="kc-reach" x="${visual.x - 3}" y="${visual.y - 3}" width="${GEO.keyW + 6}" height="${GEO.keyH + 6}" rx="${GEO.radius + 2}"></rect>` : ""}
            ${marks}
            ${sub ? `<line class="kc-sep" x1="${visual.x + 8}" y1="${rows.separatorY}" x2="${visual.x + GEO.keyW - 8}" y2="${rows.separatorY}" stroke="${text || "rgba(255,255,255,.9)"}" stroke-width="1"></line>` : ""}
            <text class="kc-label" x="${cx}" y="${rows.mainY}" font-size="${mainFit.size}"${squeeze(mainFit)}${text ? ` style="fill:${text}"` : ""}>${esc(main)}</text>
            ${sub ? `<text class="kc-sub" x="${cx}" y="${rows.subY}" font-size="${subFit.size}"${squeeze(subFit)}${text ? ` style="fill:${text}"` : ""}>${esc(sub)}</text>` : ""}
        </g>`;
    }

    // The trackball LED is index 56 and belongs to no key, but it is lit like
    // every other LED, so it is drawn with the light it emits rather than as an
    // outline of where the ball sits.
    const ball = trackballGlyph(model, layer, {mode, pdActive, trackball, clickable: Boolean(onTrackball)});
    if (ball.glow) glow += ball.glow;
    const node = el(`<div class="board ${options.picking ? "picking" : ""}">
        <svg viewBox="${GEO.viewBox}" xmlns="http://www.w3.org/2000/svg">${glow}${ball.markup}${keys}</svg></div>`);

    node.querySelectorAll("[data-key]").forEach((group) => {
        const index = Number(group.dataset.key);
        if (onKey) group.addEventListener("click", () => { if (!node.justDragged) onKey(index); });
        if (onOpen) group.addEventListener("dblclick", () => onOpen(index));
        if (onSwap) dragToSwap(node, group, index, onSwap);
    });
    node.querySelector("[data-trackball]")?.addEventListener("click", () => onTrackball());
    return node;
}

// Drag one key onto another to swap what they store. The key follows the
// pointer and the key under it is marked, so the swap is visible before it
// lands; a press that never moves stays a click.
const DRAG_THRESHOLD = 4;

function dragToSwap(node, group, index, onSwap) {
    const svg = node.querySelector("svg");
    const resting = group.getAttribute("transform") || "";
    let drag = null;

    const targetAt = (event) => document.elementsFromPoint(event.clientX, event.clientY)
        .map((hit) => hit.closest?.("[data-key]"))
        .find((hit) => hit && hit !== group) || null;
    const mark = (target) => {
        if (drag.target === target) return;
        drag.target?.classList.remove("drop-target");
        target?.classList.add("drop-target");
        drag.target = target;
    };
    const reset = () => {
        group.classList.remove("dragging");
        if (resting) group.setAttribute("transform", resting); else group.removeAttribute("transform");
        drag?.target?.classList.remove("drop-target");
        drag = null;
    };

    group.addEventListener("pointerdown", (event) => {
        if (event.button !== 0) return;
        drag = {x: event.clientX, y: event.clientY, moved: false, target: null};
        group.setPointerCapture?.(event.pointerId);
    });
    group.addEventListener("pointermove", (event) => {
        if (!drag) return;
        const dx = event.clientX - drag.x, dy = event.clientY - drag.y;
        if (!drag.moved) {
            if (Math.hypot(dx, dy) < DRAG_THRESHOLD) return;
            drag.moved = true;
            group.classList.add("dragging");
            hideHover();
            // Raise it above its neighbours; moving the node drops capture, so re-arm it.
            svg.appendChild(group);
            group.setPointerCapture?.(event.pointerId);
        }
        const scale = svg.viewBox.baseVal.width / svg.getBoundingClientRect().width;
        group.setAttribute("transform", `translate(${dx * scale} ${dy * scale}) ${resting}`.trim());
        mark(targetAt(event));
    });
    group.addEventListener("pointerup", (event) => {
        if (!drag) return;
        const moved = drag.moved || Math.hypot(event.clientX - drag.x, event.clientY - drag.y) >= DRAG_THRESHOLD;
        const target = moved ? targetAt(event) : null;
        reset();
        if (!moved) return;
        // The click that follows this release belongs to the drag, not a selection.
        node.justDragged = true;
        setTimeout(() => { node.justDragged = false; });
        if (target) onSwap(index, Number(target.dataset.key));
    });
    group.addEventListener("pointercancel", reset);
}

// The trackball's own LED. In light mode it shows what it emits; in the LED
// selector it shows its index and whether the pending group has picked it, the
// same two things every key shows there.
function trackballGlyph(model, layer, {mode, pdActive, trackball, clickable}) {
    const {x, y, r} = GEO.trackball;
    const light = trackballLight(model, layer, {pdActive});
    const paint = css(light.colour);
    const picked = mode === "leds"
        ? `fill:${trackball ? "var(--key-hi)" : "var(--key)"};stroke:var(--text);stroke-width:${trackball ? 2 : 1}`
        : `fill:${paint};stroke:rgba(255,255,255,.3)`;
    const text = mode === "leds" ? "" : idealText(light.colour);
    const markup = `<g class="kc kc-ball${trackball ? " sel" : ""}"${clickable ? ' data-trackball="1" tabindex="0" role="button"' : ""}
        aria-label="Trackball LED ${TRACKBALL_LED}">
        <circle cx="${x}" cy="${y}" r="${r}" style="${picked}"></circle>
        ${mode === "leds" ? `<text class="kc-label sm" x="${x}" y="${y}">${TRACKBALL_LED}</text>` : ""}
        <text x="${x}" y="${y + r + 12}" class="kc-badge">trackball</text></g>`;
    return {
        markup,
        glow: mode === "leds" ? "" : `<circle class="kc-glow" cx="${x}" cy="${y}" r="${r}" fill="${paint}"></circle>`,
    };
}

export const boardTierKinds = TIER_KINDS;
